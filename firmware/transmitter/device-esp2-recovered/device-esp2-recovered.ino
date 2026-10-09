// Reconstructed from Arduino-generated source. Historical comments below are
// preserved as source evidence, not as independent validation claims.
/*
  DeviceReceiver_Complete_Fixed.ino - IMPROVED VERSION
  ESP32-S2 Mini as USB Host for MIDI controllers
  Reads MIDI from USB controllers and transmits via ESP-NOW
  
  IMPROVEMENTS:
  - Better heartbeat tolerance and connection stability
  - Improved USB MIDI device detection
  - Enhanced error recovery
  - Multiple MIDI interface support
  - Optimized MIDI packet processing
*/

#include <WiFi.h>
#include <esp_now.h>
#include <Preferences.h>
#include <usb/usb_host.h>

// ───── CONFIG ──────────────────────────────────────────
#define LED_PIN               15
#define BUTTON_PIN            0

const uint32_t PAIR_TIMEOUT_MS        = 30000;  // 30 seconds pairing timeout
const uint32_t HEARTBEAT_INTERVAL_MS  = 3000;   // Send heartbeat every 3 seconds (increased)
const uint32_t HEARTBEAT_TIMEOUT_MS   = 25000;  // Consider disconnected after 25 seconds (more tolerant)
const uint32_t BLINK_INTERVAL_MS      = 250;    // LED blink rate
const uint32_t BUTTON_HOLD_TIME_MS    = 1000;   // Button hold time for action
const uint32_t CONNECTION_RETRY_MS    = 5000;   // Retry connection every 5 seconds

// ESP-NOW message types (must match HostReceiver)
enum MsgType : uint8_t {
  PAIR_ANNOUNCE  = 0x01,
  PAIR_RESPONSE  = 0x02,
  UNPAIR_NOTIFY  = 0x03,
  HEARTBEAT      = 0x04,
  MIDI_DATA      = 0x05,
  CONNECTION_TEST = 0x06  // New: Test connection quality
};

// MIDI message structure (must match HostReceiver)
struct MidiMsg {
  MsgType  type;
  uint8_t  mac[6];
  uint8_t  cable;       // MIDI cable number (0-15)
  uint8_t  status;      // MIDI status byte
  uint8_t  data1;       // First data byte
  uint8_t  data2;       // Second data byte
  uint8_t  length;      // Message length (1-3 bytes)
  uint32_t timestamp;   // Add timestamp for better sync
};

// Device states
enum DevState { 
  DS_IDLE,         // Not paired, not pairing
  DS_PAIRING,      // Actively looking for host
  DS_PAIRED,       // Connected and operational
  DS_RECONNECTING, // Trying to reconnect to saved peer
  DS_ERROR         // Error state
};

// ───── GLOBALS ─────────────────────────────────────────
DevState    state            = DS_IDLE;
bool        pairingMode      = false;
bool        connected        = false;
uint8_t     peerMac[6]       = {0};

uint32_t    pairingStartTime = 0;
uint32_t    lastHeartbeatSent= 0;
uint32_t    lastHeartbeatRecv= 0;
uint32_t    lastConnectionTest= 0;
uint32_t    lastBlink        = 0;
uint32_t    reconnectAttempts= 0;

bool        ledOn            = false;
bool        btnHeld          = false;
uint32_t    btnPressTime     = 0;

// Connection quality tracking
uint32_t    packetsLost      = 0;
uint32_t    packetsSent      = 0;
uint32_t    lastQualityCheck = 0;

Preferences prefs;

// ───── USB HOST MIDI GLOBALS ───────────────────────────
bool isMIDI = false;
bool isMIDIReady = false;
usb_host_client_handle_t Client_Handle;
usb_device_handle_t Device_Handle;
uint8_t midiInterfaceCount = 0;
uint8_t activeMidiEndpoints[4] = {0}; // Support up to 4 MIDI endpoints

const size_t MIDI_IN_BUFFERS = 8;
usb_transfer_t *MIDIIn[MIDI_IN_BUFFERS] = {NULL};

// ───── UTILS ───────────────────────────────────────────
void setLed(bool on) {
  digitalWrite(LED_PIN, on ? HIGH : LOW);
  ledOn = on;
}

void printMac(const uint8_t *mac) {
  char buf[18];
  sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.print(buf);
}

void resetConnectionStats() {
  packetsLost = 0;
  packetsSent = 0;
  lastQualityCheck = millis();
}

float getConnectionQuality() {
  if (packetsSent == 0) return 100.0;
  return ((float)(packetsSent - packetsLost) / packetsSent) * 100.0;
}

// ───── MIDI PROCESSING ─────────────────────────────────
void sendMidiToHost(uint8_t cable, uint8_t status, uint8_t data1, uint8_t data2, uint8_t length) {
  if (state != DS_PAIRED) {
    Serial.println("Not paired - MIDI dropped");
    return;
  }
  
  // Validate MIDI data
  if (length == 0 || length > 3) {
    Serial.printf("Invalid MIDI length: %d\n", length);
    return;
  }
  
  // VISUAL MIDI ACTIVITY INDICATOR - Flash LED to show MIDI received from controller
  bool wasLedOn = ledOn;
  setLed(false);        // Turn off briefly
  delay(50);            // 50ms flash
  setLed(wasLedOn);     // Return to previous state
  
  MidiMsg msg = { MIDI_DATA, {0} };
  WiFi.macAddress(msg.mac);
  msg.cable = cable;
  msg.status = status;
  msg.data1 = data1;
  msg.data2 = data2;
  msg.length = length;
  msg.timestamp = millis();
  
  esp_err_t result = esp_now_send(peerMac, (uint8_t*)&msg, sizeof(msg));
  packetsSent++;
  
  if (result == ESP_OK) {
    Serial.printf("🎵 MIDI->Host: Cable:%d Status:0x%02X Data:0x%02X 0x%02X (len:%d)\n", 
                  cable, status, data1, data2, length);
  } else {
    packetsLost++;
    Serial.printf("❌ MIDI send failed: %s\n", esp_err_to_name(result));
  }
}

// ───── USB HOST MIDI CALLBACKS ─────────────────────────
static void midi_transfer_cb(usb_transfer_t *transfer) {
  if (Device_Handle == transfer->device_handle) {
    int in_xfer = transfer->bEndpointAddress & USB_B_ENDPOINT_ADDRESS_EP_DIR_MASK;
    if ((transfer->status == 0) && in_xfer) {
      uint8_t *const p = transfer->data_buffer;
      
      // Process MIDI packets (4 bytes each)
      for (int i = 0; i < transfer->actual_num_bytes; i += 4) {
        if ((p[i] + p[i+1] + p[i+2] + p[i+3]) == 0) break;
        
        // Parse USB MIDI packet
        uint8_t cable = (p[i] >> 4) & 0x0F;  // Cable number
        uint8_t cin = p[i] & 0x0F;           // Code Index Number
        uint8_t midi_0 = p[i+1];             // Status byte
        uint8_t midi_1 = p[i+2];             // Data byte 1
        uint8_t midi_2 = p[i+3];             // Data byte 2
        
        // Skip empty packets
        if (cin == 0x0 && midi_0 == 0x00) continue;
        
        // Determine message length from CIN
        uint8_t length = 0;
        switch (cin) {
          case 0x8: case 0x9: case 0xA: case 0xB: case 0xE: // 3-byte messages
            length = 3;
            break;
          case 0xC: case 0xD: // 2-byte messages  
            length = 2;
            break;
          case 0xF: // System messages
            if (midi_0 == 0xF6 || midi_0 == 0xF8 || midi_0 == 0xFA || 
                midi_0 == 0xFB || midi_0 == 0xFC || midi_0 == 0xFE || midi_0 == 0xFF) {
              length = 1; // Single byte real-time messages
            } else if (midi_0 == 0xF1 || midi_0 == 0xF3) {
              length = 2; // 2-byte system messages
            } else {
              length = 3; // Other system messages
            }
            break;
          default:
            Serial.printf("Unknown CIN: 0x%02X\n", cin);
            continue; // Skip invalid packets
        }
        
        // Send MIDI data via ESP-NOW
        sendMidiToHost(cable, midi_0, midi_1, midi_2, length);
      }
      
      // Resubmit transfer for more data
      esp_err_t err = usb_host_transfer_submit(transfer);
      if (err != ESP_OK) {
        Serial.printf("❌ usb_host_transfer_submit failed: %s\n", esp_err_to_name(err));
      }
    }
  }
}

void check_interface_desc_MIDI(const void *p) {
  const usb_intf_desc_t *intf = (const usb_intf_desc_t *)p;

  if ((intf->bInterfaceClass == USB_CLASS_AUDIO) &&
      (intf->bInterfaceSubClass == 3)) {
    isMIDI = true;
    midiInterfaceCount++;
    Serial.printf("🎹 MIDI Interface %d detected!\n", midiInterfaceCount);
    
    // LED DIAGNOSTIC: Single long blink = MIDI interface detected
    setLed(true); delay(1000); setLed(false); delay(200);
  }
}

static void client_event_cb(const usb_host_client_event_msg_t *event_msg, void *arg) {
  switch (event_msg->event) {
    case USB_HOST_CLIENT_EVENT_NEW_DEV:
      Serial.printf("🔌 New USB device address: %d\n", event_msg->new_dev.address);
      // LED DIAGNOSTIC: 2 short blinks = USB device connected
      for(int i = 0; i < 2; i++) {
        setLed(true); delay(150); setLed(false); delay(150);
      }
      break;
    case USB_HOST_CLIENT_EVENT_DEV_GONE:
      Serial.printf("📤 USB device disconnected\n");
      isMIDI = false;
      isMIDIReady = false;
      midiInterfaceCount = 0;
      // Clear endpoint tracking
      memset(activeMidiEndpoints, 0, sizeof(activeMidiEndpoints));
      // LED DIAGNOSTIC: 3 short blinks = USB device disconnected
      for(int i = 0; i < 3; i++) {
        setLed(true); delay(150); setLed(false); delay(150);
      }
      break;
    default:
      break;
  }
}

void usb_host_task(void *arg) {
  while (1) {
    uint32_t event_flags;
    usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
    
    if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
      Serial.printf("No more USB clients\n");
      usb_host_device_free_all();
    }
    if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
      Serial.printf("All USB devices freed\n");
    }
  }
}

void setup_usb_host() {
  // Install USB Host driver
  usb_host_config_t host_config = {
    .skip_phy_setup = false,
    .intr_flags = ESP_INTR_FLAG_LEVEL1,
  };
  
  esp_err_t err = usb_host_install(&host_config);
  if (err != ESP_OK) {
    Serial.printf("❌ usb_host_install failed: %s\n", esp_err_to_name(err));
    return;
  }

  // Create a task for USB host events
  xTaskCreate(usb_host_task, "usb_host", 4096, NULL, 2, NULL);

  // Register USB Host client
  usb_host_client_config_t client_config = {
    .is_synchronous = false,
    .max_num_event_msg = 5,
    .async = {
      .client_event_callback = client_event_cb,
      .callback_arg = NULL
    }
  };
  
  err = usb_host_client_register(&client_config, &Client_Handle);
  if (err != ESP_OK) {
    Serial.printf("❌ usb_host_client_register failed: %s\n", esp_err_to_name(err));
  }
}

void handle_usb_device() {
  if (!isMIDI || isMIDIReady) return;
  
  Serial.println("🔧 Setting up USB MIDI device...");
  
  // LED DIAGNOSTIC: 2 quick blinks = Starting USB setup
  for(int i = 0; i < 2; i++) {
    setLed(true); delay(100); setLed(false); delay(100);
  }
  
  // Open device  
  esp_err_t err = usb_host_device_open(Client_Handle, 1, &Device_Handle);
  if (err != ESP_OK) {
    Serial.printf("❌ usb_host_device_open failed: %s\n", esp_err_to_name(err));
    // LED DIAGNOSTIC: 5 rapid blinks = Device open failed
    for(int i = 0; i < 5; i++) {
      setLed(true); delay(50); setLed(false); delay(50);
    }
    return;
  }

  // Get device descriptor
  const usb_device_desc_t *dev_desc;
  err = usb_host_get_device_descriptor(Device_Handle, &dev_desc);
  if (err == ESP_OK) {
    Serial.printf("🎹 USB MIDI Device: VID=0x%04X PID=0x%04X Interfaces=%d\n", 
                  dev_desc->idVendor, dev_desc->idProduct, midiInterfaceCount);
    // LED DIAGNOSTIC: 3 slow blinks = Device descriptor OK
    for(int i = 0; i < 3; i++) {
      setLed(true); delay(300); setLed(false); delay(200);
    }
  }

  // Try to claim MIDI interface (usually interface 1, but could be different)
  uint8_t interface_to_claim = 1;
  err = usb_host_interface_claim(Client_Handle, Device_Handle, interface_to_claim, 0);
  if (err != ESP_OK) {
    Serial.printf("❌ usb_host_interface_claim failed for interface %d: %s\n", 
                  interface_to_claim, esp_err_to_name(err));
    // Try interface 0 as backup
    interface_to_claim = 0;
    err = usb_host_interface_claim(Client_Handle, Device_Handle, interface_to_claim, 0);
    if (err != ESP_OK) {
      Serial.printf("❌ usb_host_interface_claim failed for interface %d: %s\n", 
                    interface_to_claim, esp_err_to_name(err));
      // LED DIAGNOSTIC: 10 rapid blinks = Interface claim failed
      for(int i = 0; i < 10; i++) {
        setLed(true); delay(50); setLed(false); delay(50);
      }
      return;
    }
  }
  
  Serial.printf("✅ Claimed MIDI interface %d\n", interface_to_claim);
  // LED DIAGNOSTIC: 4 slow blinks = Interface claimed successfully
  for(int i = 0; i < 4; i++) {
    setLed(true); delay(300); setLed(false); delay(200);
  }

  // Allocate MIDI input transfers - try multiple endpoints
  uint8_t endpoints_to_try[] = {0x81, 0x82, 0x83, 0x84}; // Common MIDI IN endpoints
  int successful_transfers = 0;
  
  for (int ep = 0; ep < 4 && successful_transfers < MIDI_IN_BUFFERS; ep++) {
    uint8_t endpoint = endpoints_to_try[ep];
    
    for (int i = successful_transfers; i < MIDI_IN_BUFFERS && (i - successful_transfers) < 2; i++) {
      err = usb_host_transfer_alloc(64, 0, &MIDIIn[i]);
      if (err == ESP_OK) {
        MIDIIn[i]->device_handle = Device_Handle;
        MIDIIn[i]->bEndpointAddress = endpoint;
        MIDIIn[i]->callback = midi_transfer_cb;
        MIDIIn[i]->context = (void *)i;
        MIDIIn[i]->num_bytes = 64;
        
        err = usb_host_transfer_submit(MIDIIn[i]);
        if (err == ESP_OK) {
          activeMidiEndpoints[ep] = endpoint;
          successful_transfers++;
          Serial.printf("✅ Transfer %d allocated for endpoint 0x%02X\n", i, endpoint);
        } else {
          Serial.printf("⚠️  Transfer submit failed for endpoint 0x%02X: %s\n", 
                       endpoint, esp_err_to_name(err));
          usb_host_transfer_free(MIDIIn[i]);
          MIDIIn[i] = NULL;
          break; // Try next endpoint
        }
      } else {
        Serial.printf("❌ usb_host_transfer_alloc failed: %s\n", esp_err_to_name(err));
        break;
      }
    }
  }

  if (successful_transfers > 0) {
    isMIDIReady = true;
    Serial.printf("✅ USB MIDI Host ready with %d active transfers\n", successful_transfers);
    // LED DIAGNOSTIC: 5 long blinks = USB MIDI Ready!
    for(int i = 0; i < 5; i++) {
      setLed(true); delay(500); setLed(false); delay(200);
    }
  } else {
    Serial.println("❌ No MIDI transfers could be established");
    // LED DIAGNOSTIC: 15 rapid blinks = No transfers established
    for(int i = 0; i < 15; i++) {
      setLed(true); delay(50); setLed(false); delay(50);
    }
  }
}

// ───── ESP-NOW CALLBACKS ───────────────────────────────
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (status != ESP_NOW_SEND_SUCCESS) {
    packetsLost++;
    Serial.printf("❌ ESP-NOW send failed to ");
    printMac(mac_addr);
    Serial.println();
  }
}

void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len != sizeof(MidiMsg)) {
    Serial.printf("Invalid message size: %d (expected %d)\n", len, sizeof(MidiMsg));
    return;
  }
  
  auto *m = (const MidiMsg*)data;
  
  switch (m->type) {
    case PAIR_ANNOUNCE:
      if (state == DS_PAIRING) {
        // Save peer and respond
        memcpy(peerMac, m->mac, 6);
        prefs.begin("pair", false);
        prefs.putBytes("peer", peerMac, 6);
        prefs.end();

        // Add peer for unicast
        esp_now_peer_info_t pi = {};
        memcpy(pi.peer_addr, peerMac, 6);
        pi.channel = 0; 
        pi.ifidx = WIFI_IF_STA; 
        pi.encrypt = false;
        esp_now_add_peer(&pi);

        // Send pairing response
        MidiMsg resp = { PAIR_RESPONSE, {0} };
        WiFi.macAddress(resp.mac);
        esp_now_send(peerMac, (uint8_t*)&resp, sizeof(resp));

        connected = true;
        pairingMode = false;
        state = DS_PAIRED;
        setLed(true);
        lastHeartbeatSent = lastHeartbeatRecv = millis();
        pairingStartTime = 0;
        reconnectAttempts = 0;
        resetConnectionStats();
        
        Serial.println("🎵 PAIRED with Host Receiver!");
        Serial.print("Host MAC: ");
        printMac(peerMac);
        Serial.println();
      }
      break;

    case UNPAIR_NOTIFY:
      if (state == DS_PAIRED || state == DS_RECONNECTING) {
        // Clear saved pairing
        prefs.begin("pair", false);
        prefs.remove("peer");
        prefs.end();
        esp_now_del_peer(peerMac);

        connected = false;
        pairingMode = false;
        state = DS_IDLE;
        setLed(false);
        reconnectAttempts = 0;
        Serial.println("📤 Unpaired by host");
      }
      break;

    case HEARTBEAT:
      lastHeartbeatRecv = millis();
      if (state == DS_RECONNECTING) {
        connected = true;
        state = DS_PAIRED;
        setLed(true);
        reconnectAttempts = 0;
        Serial.println("🔄 Reconnected to host");
      }
      break;

    case CONNECTION_TEST:
      // Respond to connection test
      {
        MidiMsg resp = { CONNECTION_TEST, {0} };
        WiFi.macAddress(resp.mac);
        esp_now_send(peerMac, (uint8_t*)&resp, sizeof(resp));
      }
      break;

    default:
      Serial.printf("Unknown message type: 0x%02X\n", m->type);
      break;
  }
}

// ───── SETUP ───────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== 🎹 DeviceReceiver (Enhanced) starting ===");

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  setLed(false);

  // Initialize USB Host
  setup_usb_host();
  Serial.println("USB Host initialized");

  // Wi-Fi + ESP-NOW setup
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  WiFi.setChannel(6);

  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW init failed");
    state = DS_ERROR;
    while (1) {
      setLed(!ledOn);
      delay(100); // Rapid blink = error
    }
  }
  
  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataRecv);
  Serial.println("ESP-NOW initialized");

  // Add broadcast peer for pairing
  esp_now_peer_info_t bp = {};
  memset(bp.peer_addr, 0xFF, 6);
  bp.channel = 0; 
  bp.ifidx = WIFI_IF_STA; 
  bp.encrypt = false;
  esp_now_add_peer(&bp);

  // Load saved peer for auto-reconnect OR auto-pair
  prefs.begin("pair", true);
  if (prefs.isKey("peer")) {
    prefs.getBytes("peer", peerMac, 6);
    Serial.print("Found saved peer: ");
    printMac(peerMac);
    Serial.println(" - attempting reconnection...");

    // Add saved peer for unicast
    esp_now_peer_info_t pi = {};
    memcpy(pi.peer_addr, peerMac, 6);
    pi.channel = 0; 
    pi.ifidx = WIFI_IF_STA; 
    pi.encrypt = false;
    esp_now_add_peer(&pi);

    state = DS_RECONNECTING;
    lastHeartbeatRecv = millis();
    Serial.println("🔄 RECONNECTING...");
  } else {
    // AUTO-PAIR: Enter pairing mode automatically if no saved peer
    Serial.println("No saved peer - AUTO-PAIRING...");
    pairingMode = true;
    state = DS_PAIRING;
    pairingStartTime = millis();
    lastBlink = millis();
  }
  prefs.end();

  resetConnectionStats();

  // STARTUP DIAGNOSTIC: Show the device is working
  // 6 quick blinks = ESP32 Device startup complete
  for(int i = 0; i < 6; i++) {
    setLed(true); delay(100); setLed(false); delay(100);
  }

  Serial.println("✅ Setup complete!");
  Serial.println("Connect USB MIDI controller now");
  Serial.println("LED Diagnostic Codes:");
  Serial.println("  • 2 short blinks = USB device connected");
  Serial.println("  • 1 long blink = MIDI interface detected");
  Serial.println("  • 2 quick blinks = Starting USB setup");
  Serial.println("  • 3 slow blinks = Device descriptor OK");
  Serial.println("  • 4 slow blinks = Interface claimed");
  Serial.println("  • 5 long blinks = USB MIDI Ready!");
  Serial.println("  • 5 rapid blinks = Device open failed");
  Serial.println("  • 10 rapid blinks = Interface claim failed");
  Serial.println("  • 15 rapid blinks = No transfers established");
  Serial.println("");
  Serial.println("Normal Operation:");
  Serial.println("  • Blinking = Pairing/Reconnecting");  
  Serial.println("  • Solid = Paired and Ready");
  Serial.println("  • Brief OFF = MIDI Activity");
  Serial.println("  • Rapid = Error");
  Serial.println("Button: Hold 1+ seconds to pair/unpair");
}

// ───── LOOP ────────────────────────────────────────────
void loop() {
  uint32_t now = millis();
  
  // Handle USB device events
  usb_host_client_handle_events(Client_Handle, 0);
  
  // Check for new MIDI devices
  if (isMIDI && !isMIDIReady) {
    handle_usb_device();
  }

  // Connection quality monitoring
  if (state == DS_PAIRED && (now - lastQualityCheck > 10000)) {
    float quality = getConnectionQuality();
    Serial.printf("📊 Connection quality: %.1f%% (Sent: %lu, Lost: %lu)\n", 
                  quality, packetsSent, packetsLost);
    lastQualityCheck = now;
    
    // Reset stats periodically
    if (packetsSent > 1000) {
      resetConnectionStats();
    }
  }

  // State machine
  switch (state) {
    case DS_IDLE:
      // Ensure LED is off when idle
      if (ledOn) {
        setLed(false);
      }
      break;

    case DS_PAIRING:
      // Pairing timeout
      if (pairingStartTime > 0 && (now - pairingStartTime >= PAIR_TIMEOUT_MS)) {
        Serial.println("⏱️  Pairing timeout - going to IDLE");
        state = DS_IDLE;
        pairingMode = false;
        setLed(false);
        pairingStartTime = 0;
      }

      // Blink LED during pairing
      if (now - lastBlink >= BLINK_INTERVAL_MS) {
        lastBlink = now;
        setLed(!ledOn);
      }
      break;

    case DS_PAIRED:
      // Send heartbeat
      if (now - lastHeartbeatSent >= HEARTBEAT_INTERVAL_MS) {
        MidiMsg hb = { HEARTBEAT, {0} };
        WiFi.macAddress(hb.mac);
        esp_now_send(peerMac, (uint8_t*)&hb, sizeof(hb));
        lastHeartbeatSent = now;
      }

      // Send periodic connection test
      if (now - lastConnectionTest >= 15000) {
        MidiMsg ct = { CONNECTION_TEST, {0} };
        WiFi.macAddress(ct.mac);
        esp_now_send(peerMac, (uint8_t*)&ct, sizeof(ct));
        lastConnectionTest = now;
      }

      // Check heartbeat timeout
      if (now - lastHeartbeatRecv > HEARTBEAT_TIMEOUT_MS) {
        Serial.println("💔 Heartbeat lost - attempting reconnection");
        connected = false;
        state = DS_RECONNECTING;
        pairingMode = false;
        lastBlink = now;
        reconnectAttempts = 0;
      }

      // Ensure solid LED when paired
      if (!ledOn) {
        setLed(true);
      }
      break;

    case DS_RECONNECTING:
      // Send heartbeat to try reconnecting
      if (now - lastHeartbeatSent >= HEARTBEAT_INTERVAL_MS) {
        MidiMsg hb = { HEARTBEAT, {0} };
        WiFi.macAddress(hb.mac);
        esp_now_send(peerMac, (uint8_t*)&hb, sizeof(hb));
        lastHeartbeatSent = now;
        reconnectAttempts++;
      }

      // Blink LED during reconnection
      if (now - lastBlink >= BLINK_INTERVAL_MS) {
        lastBlink = now;
        setLed(!ledOn);
      }

      // If reconnection takes too long, go back to pairing
      if (reconnectAttempts > 10) { // 10 attempts = ~30 seconds
        Serial.println("🔄 Reconnection timeout - clearing saved peer and going to pairing");
        prefs.begin("pair", false);
        prefs.remove("peer");
        prefs.end();
        esp_now_del_peer(peerMac);
        pairingMode = true;
        state = DS_PAIRING;
        pairingStartTime = millis();
        reconnectAttempts = 0;
      }
      break;

    case DS_ERROR:
      // Rapid blink in error state
      if (now - lastBlink >= 100) {
        lastBlink = now;
        setLed(!ledOn);
      }
      break;
  }

  // Button handling for manual pairing/unpairing
  if (digitalRead(BUTTON_PIN) == LOW) {
    if (!btnHeld) {
      btnHeld = true;
      btnPressTime = now;
    }
    else if ((now - btnPressTime > BUTTON_HOLD_TIME_MS) && btnHeld) {
      btnHeld = false; // Prevent multiple triggers
      
      if (state != DS_PAIRED) {
        // Enter pairing mode
        pairingMode = true;
        state = DS_PAIRING;
        pairingStartTime = now;
        lastBlink = now;
        reconnectAttempts = 0;
        Serial.println("🔘 Button: Entering PAIRING mode");
      } else {
        // Unpair
        MidiMsg m = { UNPAIR_NOTIFY, {0} };
        memcpy(m.mac, peerMac, 6);
        esp_now_send(peerMac, (uint8_t*)&m, sizeof(m));

        prefs.begin("pair", false);
        prefs.remove("peer");
        prefs.end();
        esp_now_del_peer(peerMac);

        connected = false;
        pairingMode = false;
        state = DS_IDLE;
        setLed(false);
        reconnectAttempts = 0;
        Serial.println("🔘 Button: Unpaired");
      }
    }
  } else {
    btnHeld = false;
  }
}
