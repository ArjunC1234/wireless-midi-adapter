// Reconstructed from Arduino-generated source. Historical comments below are
// preserved as source evidence, not as independent validation claims.
/*
  HostReceiver_Serial_MIDI_Bridge.ino - SIMPLE & RELIABLE VERSION
  Works with ANY ESP32-S2 (including clones)
  Sends MIDI data over Serial - use with bridge software
  
  BRIDGE SOFTWARE NEEDED:
  - Windows: Hairless MIDI<->Serial Bridge (free)
  - Download: https://projectgus.github.io/hairless-midiserial/
  
  SETUP:
  1. Upload this code to ESP32-S2
  2. Install Hairless MIDI<->Serial Bridge
  3. In Hairless: Select ESP32's COM port, set baud to 115200
  4. FL Studio will see "Hairless MIDI" as input device
*/

#include <WiFi.h>
#include <esp_now.h>
#include <Preferences.h>

// ───── CONFIG ──────────────────────────────────────────
#define LED_PIN              15

const uint32_t PAIR_BROADCAST_INTERVAL = 1000;
const uint32_t HEARTBEAT_INTERVAL_MS   = 3000;
const uint32_t HEARTBEAT_TIMEOUT_MS    = 25000;
const uint32_t BLINK_INTERVAL_MS       = 250;
const uint32_t CONNECTION_TEST_INTERVAL = 15000;

// ───── ESP-NOW MESSAGE TYPES ──────────────────────────
enum MsgType : uint8_t {
  PAIR_ANNOUNCE  = 0x01,
  PAIR_RESPONSE  = 0x02,
  UNPAIR_NOTIFY  = 0x03,
  HEARTBEAT      = 0x04,
  MIDI_DATA      = 0x05,
  CONNECTION_TEST = 0x06
};

struct MidiMsg {
  MsgType type;
  uint8_t mac[6];
  uint8_t cable;
  uint8_t status;
  uint8_t data1;
  uint8_t data2;
  uint8_t length;
  uint32_t timestamp;
};

enum HostState { 
  HS_PAIRING,
  HS_PAIRED,
  HS_RECONNECTING,
  HS_ERROR
};

// ───── GLOBALS ─────────────────────────────────────────
HostState   state             = HS_PAIRING;
bool        connected         = false;
uint8_t     peerMac[6]        = {0};
uint32_t    lastBroadcast     = 0;
uint32_t    lastHeartbeatSent = 0;
uint32_t    lastHeartbeatRecv = 0;
uint32_t    lastConnectionTest= 0;
uint32_t    lastBlink         = 0;
uint32_t    reconnectAttempts = 0;
bool        ledOn             = false;

// Connection quality tracking
uint32_t    packetsReceived   = 0;
uint32_t    packetsProcessed  = 0;
uint32_t    lastQualityCheck  = 0;
uint32_t    midiNotesPlayed   = 0;
uint32_t    lastDebugPrint    = 0;

Preferences prefs;

// ───── MIDI OVER SERIAL IMPLEMENTATION ────────────────
// We'll use a simple protocol: send raw MIDI bytes over Serial
// The bridge software will convert these to virtual MIDI

void initSerialMIDI() {
  Serial.begin(115200);
  delay(1000);
  
  // Send startup message
  Serial.println("=== ESP32 MIDI Bridge Ready ===");
  Serial.println("Connect Hairless MIDI<->Serial Bridge");
  Serial.println("Set baud rate to 115200");
  Serial.println("=====================================");
}

void sendMIDIOverSerial(uint8_t status, uint8_t data1, uint8_t data2, uint8_t length) {
  // Send raw MIDI bytes - bridge software will handle them
  switch (length) {
    case 1:
      Serial.write(status);
      break;
    case 2:
      Serial.write(status);
      Serial.write(data1);
      break;
    case 3:
      Serial.write(status);
      Serial.write(data1);
      Serial.write(data2);
      break;
  }
  Serial.flush();
}

// ───── UTILS ───────────────────────────────────────────
void setLed(bool on) {
  digitalWrite(LED_PIN, on ? HIGH : LOW);
  ledOn = on;
}

void debugBlink(int count, int delayMs = 200) {
  for(int i = 0; i < count; i++) {
    setLed(true);
    delay(delayMs);
    setLed(false);
    delay(delayMs);
  }
}

void printMac(const uint8_t *mac) {
  char buf[18];
  sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.print(buf);
}

void resetConnectionStats() {
  packetsReceived = 0;
  packetsProcessed = 0;
  lastQualityCheck = millis();
}

float getConnectionQuality() {
  if (packetsReceived == 0) return 100.0;
  return ((float)packetsProcessed / packetsReceived) * 100.0;
}

// ───── MIDI PROCESSING ─────────────────────────────────
void sendMidiToComputer(const MidiMsg* msg) {
  uint8_t status = msg->status;
  uint8_t data1 = msg->data1;
  uint8_t data2 = msg->data2;
  uint8_t length = msg->length;
  
  if (length == 0 || length > 3) {
    return; // Skip invalid MIDI
  }
  
  packetsReceived++;
  
  // VISUAL MIDI ACTIVITY INDICATOR - Flash LED to show MIDI received via ESP-NOW
  bool wasLedOn = ledOn;
  setLed(false);        // Turn off briefly
  delay(50);            // 50ms flash
  setLed(wasLedOn);     // Return to previous state
  
  // Send MIDI over Serial for bridge software
  sendMIDIOverSerial(status, data1, data2, length);
  packetsProcessed++;
  
  // Count note events for statistics
  if ((status & 0xF0) == 0x90 || (status & 0xF0) == 0x80) {
    midiNotesPlayed++;
  }
}

// ───── ESP-NOW CALLBACKS ───────────────────────────────
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Silent operation for MIDI performance
}

void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len != sizeof(MidiMsg)) {
    return; // Invalid message
  }
  
  auto *m = (const MidiMsg*)data;

  switch (m->type) {
    case PAIR_RESPONSE:
      if (state == HS_PAIRING) {
        memcpy(peerMac, m->mac, 6);
        prefs.begin("pair", false);
        prefs.putBytes("peer", peerMac, 6);
        prefs.end();

        esp_now_peer_info_t pi = {};
        memcpy(pi.peer_addr, peerMac, 6);
        pi.channel = 0; 
        pi.ifidx = WIFI_IF_STA; 
        pi.encrypt = false;
        esp_now_add_peer(&pi);

        connected = true;
        state = HS_PAIRED;
        setLed(true);
        lastHeartbeatSent = lastHeartbeatRecv = millis();
        reconnectAttempts = 0;
        resetConnectionStats();
        
        Serial.println("🎵 PAIRED with Device!");
        Serial.print("Device MAC: ");
        printMac(peerMac);
        Serial.println();
        
        // PAIRED! - 3 long blinks
        for(int i = 0; i < 3; i++) {
          setLed(true);
          delay(400);
          setLed(false);
          delay(200);
        }
        setLed(true); // Stay solid
      }
      break;

    case UNPAIR_NOTIFY:
      if (state == HS_PAIRED || state == HS_RECONNECTING) {
        prefs.begin("pair", false);
        prefs.remove("peer");
        prefs.end();
        esp_now_del_peer(peerMac);
        connected = false;
        state = HS_PAIRING;
        reconnectAttempts = 0;
        Serial.println("📤 Unpaired by device");
        debugBlink(5, 100);
      }
      break;

    case HEARTBEAT:
      lastHeartbeatRecv = millis();
      if (state == HS_RECONNECTING) {
        connected = true;
        state = HS_PAIRED;
        setLed(true);
        reconnectAttempts = 0;
        Serial.println("🔄 Reconnected to device");
        debugBlink(2, 400);
        setLed(true);
      }
      break;

    case MIDI_DATA:
      if (state == HS_PAIRED) {
        sendMidiToComputer(m);
      }
      break;
      
    case CONNECTION_TEST:
      // Respond to connection test from device
      {
        MidiMsg resp = { CONNECTION_TEST, {0} };
        WiFi.macAddress(resp.mac);
        esp_now_send(peerMac, (uint8_t*)&resp, sizeof(resp));
      }
      break;
  }
}

// ───── SETUP ────────────────────────────────────────────
void setup() {
  pinMode(LED_PIN, OUTPUT);
  setLed(false);
  debugBlink(3, 150);
  
  // Initialize Serial MIDI bridge
  initSerialMIDI();
  debugBlink(1, 500);

  // Initialize WiFi
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  WiFi.setChannel(6);
  debugBlink(2, 500);

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW init failed");
    while (1) {
      setLed(true);
      delay(100);
      setLed(false);
      delay(100);
    }
  }
  
  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataRecv);
  Serial.println("✅ ESP-NOW initialized");
  debugBlink(3, 500);

  // Add broadcast peer for pairing
  esp_now_peer_info_t bp = {};
  memset(bp.peer_addr, 0xFF, 6);
  bp.channel = 0; 
  bp.ifidx = WIFI_IF_STA; 
  bp.encrypt = false;
  
  if (esp_now_add_peer(&bp) != ESP_OK) {
    Serial.println("❌ Failed to add broadcast peer");
    while (1) {
      setLed(true);
      delay(500);
      setLed(false);
      delay(500);
    }
  }
  debugBlink(4, 500);

  // Check for saved peer
  prefs.begin("pair", true);
  if (prefs.isKey("peer")) {
    prefs.getBytes("peer", peerMac, 6);
    Serial.print("Found saved peer: ");
    printMac(peerMac);
    Serial.println(" - attempting reconnection...");
    
    esp_now_peer_info_t pi = {};
    memcpy(pi.peer_addr, peerMac, 6);
    pi.channel = 0; 
    pi.ifidx = WIFI_IF_STA; 
    pi.encrypt = false;
    esp_now_add_peer(&pi);
    state = HS_RECONNECTING;
    lastHeartbeatRecv = millis();
    debugBlink(5, 500);
  } else {
    state = HS_PAIRING;
    Serial.println("No saved peer - entering pairing mode");
    debugBlink(6, 500);
  }
  prefs.end();

  // Startup sequence complete
  for(int i = 0; i < 10; i++) {
    setLed(true);
    delay(50);
    setLed(false); 
    delay(50);
  }
  
  lastBlink = millis();
  resetConnectionStats();
  
  Serial.println("✅ Setup complete!");
  Serial.println("Ready to bridge MIDI data to Serial");
  Serial.println("Install Hairless MIDI<->Serial Bridge software");
  Serial.println("LED States:");
  Serial.println("  • Blinking = Pairing/Reconnecting");  
  Serial.println("  • Solid = Paired and Ready");
  Serial.println("  • Flash = MIDI Activity");
}

// ───── LOOP ─────────────────────────────────────────────
void loop() {
  uint32_t now = millis();

  // Periodic status update (every 30 seconds)
  if (state == HS_PAIRED && (now - lastDebugPrint > 30000)) {
    float quality = getConnectionQuality();
    Serial.printf("📊 Status: %.1f%% quality | %lu MIDI notes played\n", 
                  quality, midiNotesPlayed);
    lastDebugPrint = now;
    
    // Reset stats periodically
    if (packetsReceived > 1000) {
      resetConnectionStats();
      midiNotesPlayed = 0;
    }
  }

  switch (state) {
    case HS_PAIRING:
      // Broadcast pairing announcement
      if (now - lastBroadcast >= PAIR_BROADCAST_INTERVAL) {
        MidiMsg m = { PAIR_ANNOUNCE, {0} };
        WiFi.macAddress(m.mac);
        esp_now_send((uint8_t*)"\xFF\xFF\xFF\xFF\xFF\xFF", (uint8_t*)&m, sizeof(m));
        lastBroadcast = now;
      }
      
      // Blink LED during pairing
      if (now - lastBlink >= BLINK_INTERVAL_MS) {
        lastBlink = now;
        setLed(!ledOn);
      }
      break;

    case HS_PAIRED:
      // Send heartbeat
      if (now - lastHeartbeatSent >= HEARTBEAT_INTERVAL_MS) {
        MidiMsg hb = { HEARTBEAT, {0} };
        WiFi.macAddress(hb.mac);
        esp_now_send(peerMac, (uint8_t*)&hb, sizeof(hb));
        lastHeartbeatSent = now;
      }

      // Send periodic connection test
      if (now - lastConnectionTest >= CONNECTION_TEST_INTERVAL) {
        MidiMsg ct = { CONNECTION_TEST, {0} };
        WiFi.macAddress(ct.mac);
        esp_now_send(peerMac, (uint8_t*)&ct, sizeof(ct));
        lastConnectionTest = now;
      }

      // Check heartbeat timeout
      if (now - lastHeartbeatRecv >= HEARTBEAT_TIMEOUT_MS) {
        Serial.println("💔 Heartbeat lost - attempting reconnection");
        connected = false;
        state = HS_RECONNECTING;
        lastBlink = now;
        reconnectAttempts = 0;
      }

      // Ensure solid LED when paired
      if (!ledOn) {
        setLed(true);
      }
      break;

    case HS_RECONNECTING:
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

      // If reconnection takes too long, clear saved peer and go to pairing
      if (reconnectAttempts > 15) { // 15 attempts = ~45 seconds
        Serial.println("🔄 Reconnection timeout - clearing saved peer");
        prefs.begin("pair", false);
        prefs.remove("peer");
        prefs.end();
        esp_now_del_peer(peerMac);
        state = HS_PAIRING;
        lastBroadcast = 0;
        reconnectAttempts = 0;
      }
      break;

    case HS_ERROR:
      // Rapid blink in error state
      if (now - lastBlink >= 100) {
        lastBlink = now;
        setLed(!ledOn);
      }
      break;
  }
}
