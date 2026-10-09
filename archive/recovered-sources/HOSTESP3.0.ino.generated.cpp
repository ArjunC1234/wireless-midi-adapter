#include <Arduino.h>
#line 1 "<recovered-sketch-path>"
/*
  HOST_ESP2.0_v2 — ESP‑NOW receiver → USB‑MIDI device (Control Surface backend)

  Why this change?
  Your computer didn’t see a MIDI device because the previous sketch started TinyUSB
  but didn’t actually publish a MIDI interface (no MIDI descriptors). This version uses
  the Control Surface library’s USBMIDI_Interface, which exposes a class‑compliant
  USB‑MIDI device on ESP32‑S2/S3 when Tools→USB Mode is set to “USB‑OTG (TinyUSB)”.

  Requirements
  - Arduino‑ESP32 core ≥ 3.0.x
  - Board: ESP32‑S2 or ESP32‑S3, plugged into the NATIVE USB port
  - Tools → USB Mode: USB‑OTG (TinyUSB)
  - Library Manager: install “Control Surface” by tttapa (Pieter P.)

  Notes
  - We forward MIDI from ESP‑NOW to the USB device by mapping status bytes to
    Control Surface API calls. (No hardcoded USB packets/descriptors needed.)
*/

#include <WiFi.h>
#include <esp_now.h>
#include <Preferences.h>
#include <Control_Surface.h>   // <— add from Library Manager

// Instantiate a class‑compliant USB‑MIDI interface
USBMIDI_Interface usbMIDI;

// ===== Config =====
#define LED_PIN                 15
static const uint8_t ESPNOW_CHANNEL    = 6;

static const uint32_t PAIR_BROADCAST_INTERVAL = 1000;
static const uint32_t HEARTBEAT_INTERVAL_MS   = 2000;
static const uint32_t HEARTBEAT_TIMEOUT_MS    = 15000;
static const uint32_t RECONNECT_CLEAR_MS      = 60000; // 60s before wiping saved peer
static const uint32_t BLINK_INTERVAL_MS       = 250;

// ===== Protocol =====
enum MsgType { PAIR_ANNOUNCE=1, PAIR_RESPONSE, UNPAIR_NOTIFY, HEARTBEAT, MIDI_DATA };
struct MidiMsg {
  MsgType  type;
  uint8_t  mac[6];
  uint8_t  cable;
  uint8_t  status;
  uint8_t  data1;
  uint8_t  data2;
  uint8_t  length;  // 2 or 3 for channel voice; other types ignored here
};

enum HostState { HS_PAIRING, HS_PAIRED, HS_RECONNECTING, HS_ERROR };

// ===== State =====
Preferences prefs;
HostState state = HS_PAIRING;
bool      connected = false;
bool      ledOn = false;
uint8_t   peerMac[6] = {0};
uint32_t  lastBroadcast = 0;
uint32_t  lastHeartbeatSent = 0;
uint32_t  lastHeartbeatRecv = 0;
uint32_t  lastBlink = 0;

#line 64 "<recovered-sketch-path>"
static void setLed(bool on);
#line 65 "<recovered-sketch-path>"
static void printMac(const uint8_t* m);
#line 68 "<recovered-sketch-path>"
static void midi_send_packet(uint8_t cable, uint8_t status, uint8_t d1, uint8_t d2, uint8_t length);
#line 102 "<recovered-sketch-path>"
static void onDataSent(const uint8_t* mac, esp_now_send_status_t s);
#line 104 "<recovered-sketch-path>"
static void onDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len);
#line 138 "<recovered-sketch-path>"
void setup();
#line 170 "<recovered-sketch-path>"
void loop();
#line 64 "<recovered-sketch-path>"
static inline void setLed(bool on){ pinMode(LED_PIN, OUTPUT); digitalWrite(LED_PIN, on?HIGH:LOW); ledOn = on; }
static void printMac(const uint8_t* m){ for(int i=0;i<6;i++){ Serial.printf("%02X%s", m[i], (i<5?":":"")); } }

// ===== USB‑MIDI send (maps raw status/data to Control Surface calls) =====
static inline void midi_send_packet(uint8_t cable, uint8_t status, uint8_t d1, uint8_t d2, uint8_t length){
  (void)cable; // Control Surface exposes a single virtual cable; ignore here
  uint8_t st = status & 0xF0;
  uint8_t ch = (status & 0x0F) + 1; // Control Surface channels are 1..16
  switch(st){
    case 0x80: // Note Off
      usbMIDI.sendNoteOff({d1, Channel(ch)}, d2);
      break;
    case 0x90: // Note On (velocity 0 = Note Off OK too)
      usbMIDI.sendNoteOn({d1, Channel(ch)}, d2);
      break;
    case 0xA0: // Poly Aftertouch
      usbMIDI.sendKeyPressure({d1, Channel(ch)}, d2);
      break;
    case 0xB0: // Control Change
      usbMIDI.sendControlChange({d1, Channel(ch)}, d2);
      break;
    case 0xC0: // Program Change
      usbMIDI.sendProgramChange({d1, Channel(ch)});
      break;
    case 0xD0: // Channel Aftertouch
      usbMIDI.sendChannelPressure(Channel(ch), d1);
      break;
    case 0xE0: { // Pitch Bend (14‑bit)
      uint16_t pb = (uint16_t(d2) << 7) | d1; // 0..16383
      usbMIDI.sendPitchBend(Channel(ch), pb);
      break; }
    default:
      // System messages (0xF*) ignored for this bridge; extend if needed
      break;
  }
}

// ===== ESP‑NOW callbacks =====
static void onDataSent(const uint8_t* mac, esp_now_send_status_t s){ if(s != ESP_NOW_SEND_SUCCESS){ Serial.print("❌ send fail "); printMac(mac); Serial.println(); }}

static void onDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len){
  if(len != sizeof(MidiMsg)) return; const MidiMsg* m = (const MidiMsg*)data;
  switch(m->type){
    case PAIR_RESPONSE:
      if(state==HS_PAIRING || state==HS_RECONNECTING){
        memcpy(peerMac, info->src_addr, 6);
        prefs.begin("pair", false); prefs.putBytes("peer", peerMac, 6); prefs.end();
        esp_now_peer_info_t pi; memset(&pi, 0, sizeof(pi));
        memcpy(pi.peer_addr, peerMac, 6); pi.channel = ESPNOW_CHANNEL; pi.ifidx = WIFI_IF_STA; pi.encrypt = false;
        esp_now_add_peer(&pi);
        connected = true; state = HS_PAIRED; setLed(true);
        lastHeartbeatSent = lastHeartbeatRecv = millis();
        Serial.println("✅ Paired (USB‑MIDI ready)");
      }
      break;
    case UNPAIR_NOTIFY:
      if(connected){
        prefs.begin("pair", false); prefs.remove("peer"); prefs.end();
        esp_now_del_peer(peerMac);
        state = HS_PAIRING; connected = false; setLed(false);
      }
      break;
    case HEARTBEAT:
      lastHeartbeatRecv = millis();
      if(state == HS_RECONNECTING){ connected = true; state = HS_PAIRED; setLed(true); }
      break;
    case MIDI_DATA:
      if(state == HS_PAIRED){ midi_send_packet(m->cable, m->status, m->data1, m->data2, m->length); }
      break;
    default: break;
  }
}

// ===== Setup / Loop =====
void setup(){
  Serial.begin(115200);
  setLed(false);

  // Start USB‑MIDI (Control Surface)
  Control_Surface.begin();

  // Radio / ESP‑NOW
  WiFi.mode(WIFI_STA); WiFi.disconnect(); delay(100); WiFi.setChannel(ESPNOW_CHANNEL);
  if(esp_now_init() != ESP_OK){ while(1){ setLed(true); delay(100); setLed(false); delay(100); }}
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
    state = HS_RECONNECTING; lastHeartbeatRecv = millis();
  } else {
    state = HS_PAIRING;
  }
  prefs.end();
}

void loop(){
  Control_Surface.loop(); // service USB‑MIDI

  uint32_t now = millis();

  switch(state){
    case HS_PAIRING:
      if(now - lastBroadcast >= PAIR_BROADCAST_INTERVAL){
        MidiMsg m; memset(&m, 0, sizeof(m)); m.type = PAIR_ANNOUNCE; WiFi.macAddress(m.mac);
        uint8_t bcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
        esp_now_send(bcast, (uint8_t*)&m, sizeof(m));
        lastBroadcast = now;
      }
      if(now - lastBlink >= BLINK_INTERVAL_MS){ lastBlink = now; setLed(!ledOn); }
      break;

    case HS_PAIRED:
      if(now - lastHeartbeatSent >= HEARTBEAT_INTERVAL_MS){
        MidiMsg hb; memset(&hb, 0, sizeof(hb)); hb.type = HEARTBEAT; WiFi.macAddress(hb.mac);
        esp_now_send(peerMac, (uint8_t*)&hb, sizeof(hb));
        lastHeartbeatSent = now;
      }
      if(now - lastHeartbeatRecv >= HEARTBEAT_TIMEOUT_MS){ connected = false; state = HS_RECONNECTING; lastBlink = now; }
      if(!ledOn) setLed(true);
      break;

    case HS_RECONNECTING:
      if(now - lastHeartbeatSent >= HEARTBEAT_INTERVAL_MS){
        MidiMsg hb; memset(&hb, 0, sizeof(hb)); hb.type = HEARTBEAT; WiFi.macAddress(hb.mac);
        esp_now_send(peerMac, (uint8_t*)&hb, sizeof(hb));
        lastHeartbeatSent = now;
      }
      if(now - lastBlink >= BLINK_INTERVAL_MS){ lastBlink = now; setLed(!ledOn); }
      if(now - lastHeartbeatRecv > RECONNECT_CLEAR_MS){
        prefs.begin("pair", false); prefs.remove("peer"); prefs.end();
        esp_now_del_peer(peerMac); state = HS_PAIRING; lastBroadcast = 0;
      }
      break;

    case HS_ERROR:
      if(now - lastBlink >= 100){ lastBlink = now; setLed(!ledOn); }
      break;
  }
}
