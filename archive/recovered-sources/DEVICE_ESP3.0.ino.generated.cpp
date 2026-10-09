#include <Arduino.h>
#line 1 "<recovered-sketch-path>"
/*
  DEVICE_ESP2.0_v2 — USB‑MIDI Host → ESP‑NOW sender

  Fixes vs original:
  - Uses actual USB device address reported by NEW_DEV (no hard‑coded addr=1).
  - Parses active configuration to discover true MIDI IN endpoint (no hard‑coded 0x81).
  - Sets ESP‑NOW peer channel to match Wi‑Fi channel (prevents missed heartbeats).
  - Calmer reconnection: longer grace before wiping saved peer; steadier LED.

  Notes:
  - Requires ESP32‑S2/S3 with USB Host capability and Arduino‑ESP32 core with usb/usb_host.h.
*/

#include <WiFi.h>
#include <esp_now.h>
#include <Preferences.h>
#include <usb/usb_host.h>

// ===== Config =====
#define LED_PIN                 15
#define BUTTON_PIN              0
static const uint8_t ESPNOW_CHANNEL   = 6;      // keep both sides pinned

static const uint32_t PAIR_TIMEOUT_MS       = 30000;
static const uint32_t HEARTBEAT_INTERVAL_MS = 2000;
static const uint32_t HEARTBEAT_TIMEOUT_MS  = 15000;
static const uint32_t RECONNECT_CLEAR_MS    = 60000; // wait 60s before clearing saved peer
static const uint32_t BLINK_INTERVAL_MS     = 250;
static const uint32_t BUTTON_HOLD_TIME_MS   = 1000;

// ===== Protocol =====
enum MsgType { PAIR_ANNOUNCE=1, PAIR_RESPONSE, UNPAIR_NOTIFY, HEARTBEAT, MIDI_DATA };
struct MidiMsg {
  MsgType  type;
  uint8_t  mac[6];
  uint8_t  cable;
  uint8_t  status;
  uint8_t  data1;
  uint8_t  data2;
  uint8_t  length;
};

enum DevState { DS_IDLE, DS_PAIRING, DS_PAIRED, DS_RECONNECTING, DS_ERROR };

// ===== State =====
Preferences prefs;
DevState  state = DS_IDLE;
bool      connected = false;
bool      pairingMode = false;
bool      ledOn = false;
uint8_t   peerMac[6] = {0};
uint32_t  lastHeartbeatSent = 0;
uint32_t  lastHeartbeatRecv = 0;
uint32_t  lastBlink = 0;
uint32_t  pairingStartTime = 0;
bool      btnHeld = false;

// USB host
usb_host_client_handle_t Client_Handle = nullptr;
usb_device_handle_t      Device_Handle = nullptr;
uint8_t  current_dev_addr = 0;
bool     isMIDI = false;
bool     isMIDIReady = false;
uint8_t  ep_midi_in = 0;            // discovered IN EP
usb_transfer_t* MIDIIn[2] = {nullptr, nullptr};

// ===== Utils =====
#line 68 "<recovered-sketch-path>"
static void setLed(bool on);
#line 69 "<recovered-sketch-path>"
static void printMac(const uint8_t* m);
#line 72 "<recovered-sketch-path>"
static void send_midi(uint8_t cable, uint8_t s, uint8_t d1, uint8_t d2, uint8_t len);
#line 81 "<recovered-sketch-path>"
static bool find_midi_endpoints(usb_device_handle_t h, uint8_t& ep_in);
#line 105 "<recovered-sketch-path>"
static void midi_transfer_cb(usb_transfer_t* t);
#line 121 "<recovered-sketch-path>"
static void client_event_cb(const usb_host_client_event_msg_t* e, void*);
#line 136 "<recovered-sketch-path>"
static void usb_host_task(void*);
#line 143 "<recovered-sketch-path>"
static void setup_usb_host();
#line 152 "<recovered-sketch-path>"
static void handle_usb_device();
#line 172 "<recovered-sketch-path>"
static void onDataSent(const uint8_t* mac, esp_now_send_status_t s);
#line 176 "<recovered-sketch-path>"
static void onDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len);
#line 211 "<recovered-sketch-path>"
void setup();
#line 242 "<recovered-sketch-path>"
void loop();
#line 68 "<recovered-sketch-path>"
static inline void setLed(bool on) { digitalWrite(LED_PIN, on ? HIGH : LOW); ledOn = on; }
static void printMac(const uint8_t* m){ for(int i=0;i<6;i++){ Serial.printf("%02X%s", m[i], (i<5?":":"")); } }

// ===== ESP‑NOW send MIDI =====
static void send_midi(uint8_t cable, uint8_t s, uint8_t d1, uint8_t d2, uint8_t len){
  if(!connected || len==0 || len>3) return;
  MidiMsg msg; memset(&msg, 0, sizeof(msg));
  msg.type = MIDI_DATA; WiFi.macAddress(msg.mac);
  msg.cable=cable; msg.status=s; msg.data1=d1; msg.data2=d2; msg.length=len;
  esp_now_send(peerMac,(uint8_t*)&msg,sizeof(msg));
}

// ===== USB host: parse config to find MIDI =====
static bool find_midi_endpoints(usb_device_handle_t h, uint8_t& ep_in){
  ep_in = 0;
  const usb_config_desc_t* cfg = nullptr;
  if(usb_host_get_active_config_descriptor(h, &cfg) != ESP_OK || !cfg) return false;
  const uint8_t* p = (const uint8_t*)cfg;
  size_t len = cfg->wTotalLength;
  size_t i = 0; bool if_is_midi = false;
  while(i + 2 <= len){
    uint8_t bLen = p[i];
    uint8_t bType = p[i+1];
    if(bLen == 0) break;
    if(bType == USB_B_DESCRIPTOR_TYPE_INTERFACE && bLen >= sizeof(usb_intf_desc_t)){
      const usb_intf_desc_t* id = (const usb_intf_desc_t*)(p + i);
      if_is_midi = (id->bInterfaceClass == 0x01 && id->bInterfaceSubClass == 0x03); // Audio, MIDI streaming
    } else if(bType == USB_B_DESCRIPTOR_TYPE_ENDPOINT && bLen >= sizeof(usb_ep_desc_t) && if_is_midi){
      const usb_ep_desc_t* ed = (const usb_ep_desc_t*)(p + i);
      if(ed->bEndpointAddress & 0x80){ ep_in = ed->bEndpointAddress; }
    }
    i += bLen;
  }
  return ep_in != 0; 
}

// ===== USB callbacks =====
static void midi_transfer_cb(usb_transfer_t* t){
  if(t->status==0 && (t->bEndpointAddress & 0x80) && (t->actual_num_bytes % 4 == 0)){
    uint8_t* p = t->data_buffer; int n = t->actual_num_bytes;
    for(int i=0;i<n;i+=4){
      uint8_t cin = p[i] & 0x0F, cable = (p[i]>>4) & 0x0F;
      uint8_t b1 = p[i+1], b2 = p[i+2], b3 = p[i+3];
      switch(cin){
        case 0x8: case 0x9: case 0xA: case 0xB: case 0xE: case 0xF: send_midi(cable,b1,b2,b3,3); break;
        case 0xC: case 0xD: send_midi(cable,b1,b2,0,2); break;
        default: break;
      }
    }
  }
  t->num_bytes = 64; usb_host_transfer_submit(t);
}

static void client_event_cb(const usb_host_client_event_msg_t* e, void*){
  switch(e->event){
    case USB_HOST_CLIENT_EVENT_NEW_DEV:
      current_dev_addr = e->new_dev.address;
      isMIDI = true; // try to set up when we handle in loop
      Serial.printf("🔌 New USB device addr=%u\n", current_dev_addr);
      break;
    case USB_HOST_CLIENT_EVENT_DEV_GONE:
      isMIDI = false; isMIDIReady = false; current_dev_addr = 0; ep_midi_in = 0;
      Serial.println("📤 USB device disconnected");
      break;
    default: break;
  }
}

static void usb_host_task(void*){
  while(1){
    uint32_t f; usb_host_lib_handle_events(portMAX_DELAY, &f);
    if(f & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) usb_host_device_free_all();
  }
}

static void setup_usb_host(){
  usb_host_config_t hc; memset(&hc, 0, sizeof(hc)); hc.skip_phy_setup = false; hc.intr_flags = ESP_INTR_FLAG_LEVEL1;
  if(usb_host_install(&hc) != ESP_OK){ Serial.println("usb_host_install failed"); return; }
  xTaskCreate(usb_host_task, "usb_host", 4096, nullptr, 2, nullptr);
  usb_host_client_config_t cc; memset(&cc, 0, sizeof(cc));
  cc.is_synchronous = false; cc.max_num_event_msg = 5; cc.async.client_event_callback = client_event_cb; cc.async.callback_arg = nullptr;
  usb_host_client_register(&cc, &Client_Handle);
}

static void handle_usb_device(){
  if(isMIDIReady || !isMIDI) return;
  if(!current_dev_addr){ Serial.println("No device address yet"); return; }
  Serial.println("🔧 Setting up USB MIDI (v2)");
  if(usb_host_device_open(Client_Handle, current_dev_addr, &Device_Handle) != ESP_OK){ Serial.println("open failed"); return; }
  if(!find_midi_endpoints(Device_Handle, ep_midi_in)){ Serial.println("No MIDI endpoints found"); return; }
  for(int i=0;i<2;i++){
    if(usb_host_transfer_alloc(64, 0, &MIDIIn[i]) == ESP_OK){
      MIDIIn[i]->device_handle = Device_Handle;
      MIDIIn[i]->bEndpointAddress = ep_midi_in;
      MIDIIn[i]->callback = midi_transfer_cb;
      MIDIIn[i]->context = (void*)i;
      MIDIIn[i]->num_bytes = 64;
      usb_host_transfer_submit(MIDIIn[i]);
    }
  }
  isMIDIReady = true; Serial.printf("🎹 MIDI ready on EP IN=0x%02X\n", ep_midi_in);
}

// ===== ESP‑NOW callbacks =====
static void onDataSent(const uint8_t* mac, esp_now_send_status_t s){
  if(s != ESP_NOW_SEND_SUCCESS){ Serial.print("❌ send fail "); printMac(mac); Serial.println(); }
}

static void onDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len){
  if(len != sizeof(MidiMsg)) return; const MidiMsg* m = (const MidiMsg*)data;
  switch(m->type){
    case PAIR_ANNOUNCE:
      if(!connected && pairingMode){
        memcpy(peerMac, m->mac, 6);
        prefs.begin("pair", false); prefs.putBytes("peer", peerMac, 6); prefs.end();
        esp_now_peer_info_t pi; memset(&pi, 0, sizeof(pi));
        memcpy(pi.peer_addr, peerMac, 6); pi.channel = ESPNOW_CHANNEL; pi.ifidx = WIFI_IF_STA; pi.encrypt = false;
        esp_now_add_peer(&pi);
        {
          MidiMsg resp; memset(&resp, 0, sizeof(resp)); resp.type = PAIR_RESPONSE; WiFi.macAddress(resp.mac);
          esp_now_send(peerMac, (uint8_t*)&resp, sizeof(resp));
        }
        connected = true; pairingMode = false; state = DS_PAIRED; setLed(true);
        lastHeartbeatSent = lastHeartbeatRecv = millis(); pairingStartTime = 0;
        Serial.println("🎵 PAIRED (v2)");
      }
      break;
    case UNPAIR_NOTIFY:
      if(connected){
        prefs.begin("pair", false); prefs.remove("peer"); prefs.end();
        esp_now_del_peer(peerMac);
        connected = false; state = DS_IDLE; setLed(false);
      }
      break;
    case HEARTBEAT:
      lastHeartbeatRecv = millis();
      if(state == DS_RECONNECTING){ connected = true; state = DS_PAIRED; setLed(true); Serial.println("🔄 Reconnected (v2)"); }
      break;
    default: break;
  }
}

// ===== Setup / Loop =====
void setup(){
  pinMode(LED_PIN, OUTPUT); pinMode(BUTTON_PIN, INPUT_PULLUP); setLed(false);
  Serial.begin(115200); delay(50);
  setup_usb_host();

  WiFi.mode(WIFI_STA); WiFi.disconnect(); delay(100); WiFi.setChannel(ESPNOW_CHANNEL);
  if(esp_now_init() != ESP_OK){ while(1){ setLed(!ledOn); delay(100); }}
  esp_now_register_send_cb(onDataSent); esp_now_register_recv_cb(onDataRecv);

  // Broadcast peer for pairing beacons
  esp_now_peer_info_t bp; memset(&bp, 0, sizeof(bp));
  uint8_t bcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
  memcpy(bp.peer_addr, bcast, 6); bp.channel = ESPNOW_CHANNEL; bp.ifidx = WIFI_IF_STA; bp.encrypt = false;
  esp_now_add_peer(&bp);

  // Load saved peer
  prefs.begin("pair", true);
  if(prefs.isKey("peer")){
    prefs.getBytes("peer", peerMac, 6);
    esp_now_peer_info_t pi; memset(&pi, 0, sizeof(pi));
    memcpy(pi.peer_addr, peerMac, 6); pi.channel = ESPNOW_CHANNEL; pi.ifidx = WIFI_IF_STA; pi.encrypt = false;
    esp_now_add_peer(&pi);
    state = DS_RECONNECTING; lastHeartbeatRecv = millis();
    Serial.println("🔄 RECONNECTING (v2)...");
  } else {
    pairingMode = true; state = DS_PAIRING; pairingStartTime = millis(); lastBlink = millis();
    Serial.println("AUTO‑PAIR (v2)");
  }
  prefs.end();
}

void loop(){
  uint32_t now = millis();
  usb_host_client_handle_events(Client_Handle, 0);
  if(isMIDI && !isMIDIReady) handle_usb_device();

  // Button: long hold to unpair
  if(digitalRead(BUTTON_PIN) == LOW){
    if(!btnHeld){
      btnHeld = true; uint32_t t0 = millis();
      while(digitalRead(BUTTON_PIN) == LOW) { delay(10); }
      uint32_t dur = millis() - t0;
      if(dur >= BUTTON_HOLD_TIME_MS){
        prefs.begin("pair", false); prefs.remove("peer"); prefs.end();
        esp_now_del_peer(peerMac); connected = false; pairingMode = false; state = DS_IDLE; setLed(false);
        Serial.println("🔘 Unpaired (manual)");
      }
    }
  } else { btnHeld = false; }

  switch(state){
    case DS_IDLE:
      if(ledOn) setLed(false);
      break;
    case DS_PAIRING:
      if(pairingStartTime && (now - pairingStartTime >= PAIR_TIMEOUT_MS)){
        pairingMode = false; state = DS_IDLE; setLed(false);
      }
      if(now - lastBlink >= BLINK_INTERVAL_MS){ lastBlink = now; setLed(!ledOn); }
      break;
    case DS_PAIRED:
      if(now - lastHeartbeatSent >= HEARTBEAT_INTERVAL_MS){
        MidiMsg hb; memset(&hb,0,sizeof(hb)); hb.type = HEARTBEAT; WiFi.macAddress(hb.mac);
        esp_now_send(peerMac, (uint8_t*)&hb, sizeof(hb)); lastHeartbeatSent = now;
      }
      if(now - lastHeartbeatRecv > HEARTBEAT_TIMEOUT_MS){ connected = false; state = DS_RECONNECTING; lastBlink = now; }
      if(!ledOn) setLed(true);
      break;
    case DS_RECONNECTING:
      if(now - lastHeartbeatSent >= HEARTBEAT_INTERVAL_MS){
        MidiMsg hb; memset(&hb,0,sizeof(hb)); hb.type = HEARTBEAT; WiFi.macAddress(hb.mac);
        esp_now_send(peerMac, (uint8_t*)&hb, sizeof(hb)); lastHeartbeatSent = now;
      }
      if(now - lastBlink >= BLINK_INTERVAL_MS){ lastBlink = now; setLed(!ledOn); }
      if(now - lastHeartbeatRecv > RECONNECT_CLEAR_MS){
        prefs.begin("pair", false); prefs.remove("peer"); prefs.end();
        esp_now_del_peer(peerMac); pairingMode = true; state = DS_PAIRING; pairingStartTime = millis();
      }
      break;
    case DS_ERROR:
      if(now - lastBlink >= 100){ lastBlink = now; setLed(!ledOn); }
      break;
  }
}
