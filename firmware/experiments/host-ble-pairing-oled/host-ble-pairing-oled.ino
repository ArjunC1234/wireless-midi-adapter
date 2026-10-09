#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <esp_now.h>
#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <Preferences.h>
#include <esp_wifi.h>

// ==== CONFIG ====
#define LED_PIN     1
#define BUTTON_PIN  2
#define OLED_ADDR   0x3C
#define ESP_NOW_CHANNEL 1
#define MAC_ADDR_LEN 6

#define SERVICE_UUID        "d49e6f8a-0c69-497b-91c5-bc3446f3a1f6"
#define CHAR_HOSTMAC_UUID   "34e6f3a8-c13b-4a6a-a491-44e0cfdfd1f7"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

#define DEBOUNCE_DELAY 50
#define HOLD_DURATION 2000
#define BLINK_INTERVAL 250

// ==== GLOBALS ====
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Preferences prefs;

bool isPairingMode = false;
bool isPaired = false;
bool currentLedState = LOW;
bool displayNeedsUpdate = false;

String deviceName = "None";
uint8_t savedMac[MAC_ADDR_LEN];

unsigned long lastBlink = 0;

// For button state tracking
bool lastRaw = HIGH, stableRaw = HIGH;
unsigned long pressStart = 0;

// BLE scanning globals
BLEScan* scan = nullptr;
bool pairingInProgress = false;

// ==== STRUCT ====
typedef struct struct_message {
  char type[12];
  char deviceName[32];
} struct_message;
struct_message outgoingMessage;

// ==== FORWARD DECLARATIONS ====
void updateDisplay();
void printCenteredText(const String &text, int16_t y);
void startBLEPairing();
bool registerEspNowPeer(const uint8_t* mac);
void saveMacToFlash(const uint8_t* mac);
bool loadMacFromFlash(uint8_t* mac);
void deleteMacFromFlash();
void clearHostFlash();
void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len);
void onDataSent(const wifi_tx_info_t* info, esp_now_send_status_t status);
void scanCompleteCB(BLEScanResults scanResults);

void setup() {
  Serial.begin(115200);
  Wire.begin(8, 9);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) while(1);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Booting...");
  display.display();
  delay(500);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  digitalWrite(LED_PIN, LOW);

  WiFi.mode(WIFI_STA);
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM); // <-- Add this for better BLE/WiFi coexistence
  WiFi.disconnect();
  if (esp_now_init() != ESP_OK) while(1);
  esp_now_register_recv_cb(onDataRecv);
  esp_now_register_send_cb(onDataSent);
  Serial.println("Starting BLE pairing mode...");
  BLEDevice::init("Host");
  BLEDevice::setMTU(53); // <-- Add this line. This is the key fix.

  esp_now_peer_info_t bc = {};
  memset(bc.peer_addr, 0xFF, MAC_ADDR_LEN);
  bc.channel = ESP_NOW_CHANNEL;
  bc.encrypt = false;
  if (!esp_now_is_peer_exist(bc.peer_addr)) esp_now_add_peer(&bc);

  if (loadMacFromFlash(savedMac)) {
    if (registerEspNowPeer(savedMac)) {
      isPaired = true;
      deviceName = "Paired Device";
      digitalWrite(LED_PIN, HIGH);
    }
  }

  updateDisplay();
  display.display();
}

void loop() {
  unsigned long now = millis();

  // === Button handling with simple hold detection ===
  bool raw = digitalRead(BUTTON_PIN);
  if (raw != lastRaw) {
    // Debounce on button state change
    delay(DEBOUNCE_DELAY);
    raw = digitalRead(BUTTON_PIN);
    if (raw != lastRaw) {
      lastRaw = raw;
      if (raw == LOW) {
        // Button pressed
        stableRaw = LOW;
        pressStart = now;
      } else {
        // Button released
        stableRaw = HIGH;
        if (now - pressStart >= HOLD_DURATION) {
          isPairingMode = !isPairingMode;
          if (isPairingMode) {
            startBLEPairing();
            Serial.println("Entered pairing mode.");
          } else {
            pairingInProgress = false;
            Serial.println("Exited pairing mode.");
          }
          displayNeedsUpdate = true;
        }
      }
    }
  }

  // === BLE continuous scanning control ===
  if (pairingInProgress && scan) {
    // No action needed here; scanCompleteCB will handle next scans
  }

  // === LED blinking ===
  if (isPairingMode && !isPaired) {
    if (now - lastBlink >= BLINK_INTERVAL) {
      currentLedState = !currentLedState;
      digitalWrite(LED_PIN, currentLedState);
      lastBlink = now;
    }
  } else {
    digitalWrite(LED_PIN, isPaired ? HIGH : LOW);
  }

  // === Display update ===
  if (displayNeedsUpdate) {
    updateDisplay();
    displayNeedsUpdate = false;
  }

  // === Serial command handling ===
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.equalsIgnoreCase("reset")) {
      clearHostFlash();
    } else if (input.equalsIgnoreCase("status")) {
      Serial.println("📡 Host is running. Waiting for commands...");
    } else {
      Serial.println("❓ Unknown command. Try 'reset' or 'status'");
    }
  }
}
String getHostMacString() {
  char macStr[18]; // 17 chars + null terminator
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           savedMac[0], savedMac[1], savedMac[2],
           savedMac[3], savedMac[4], savedMac[5]);
  return String(macStr);
}
// === BLE scan complete callback ===
void scanCompleteCB(BLEScanResults scanResults) {
    Serial.printf("Scan complete: found %d devices\n", scanResults.getCount());

    for (int i = 0; i < scanResults.getCount(); i++) {
        BLEAdvertisedDevice dev = scanResults.getDevice(i);

        if (dev.haveServiceUUID() && dev.isAdvertisingService(BLEUUID(SERVICE_UUID))) {
            
            Serial.println("Target device found, stopping scan...");

            // Stop the scan to free up the radio. This is critical.
            scan->stop();

            BLEClient* client = BLEDevice::createClient();
            Serial.println("Attempting to connect...");

            // Use the simpler, single-argument connect function.
            if (client->connect(&dev)) {
                Serial.println("Connected successfully.");

                // Get the service
                BLERemoteService* svc = client->getService(BLEUUID(SERVICE_UUID));
                if (svc) {
                    Serial.println("Service found.");
                    BLERemoteCharacteristic* chr = svc->getCharacteristic(BLEUUID(CHAR_HOSTMAC_UUID));
                    if (chr && chr->canWrite()) {
                        Serial.println("Characteristic found and is writable.");

                        // Get the Host's actual WiFi MAC address
                        uint8_t myMac[MAC_ADDR_LEN];
                        WiFi.macAddress(myMac);
                        char macStr[18];
                        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X", myMac[0], myMac[1], myMac[2], myMac[3], myMac[4], myMac[5]);
                        String hostMacStr = String(macStr);

                        if (chr->writeValue((uint8_t*)hostMacStr.c_str(), hostMacStr.length())) {
                            Serial.println("Host MAC written successfully.");

                            BLEAddress deviceAddress = dev.getAddress();
                            memcpy(savedMac, deviceAddress.getNative(), MAC_ADDR_LEN);
                            saveMacToFlash(savedMac);
                            registerEspNowPeer(savedMac);

                            isPaired = true;
                            deviceName = "Paired Device";
                            displayNeedsUpdate = true;
                            pairingInProgress = false; // Pairing is done
                        } else {
                            Serial.println("Failed to write Host MAC.");
                        }
                    } else {
                        Serial.println("Characteristic NOT found or NOT writable.");
                    }
                } else {
                    Serial.println("Service NOT found.");
                }

                client->disconnect();

            } else {
                Serial.println("Failed to connect.");
            }

            // IMPORTANT: Delete the client object in all cases to prevent memory leaks
            delete client;

            // If pairing is complete, exit the scan loop entirely
            if (!pairingInProgress) {
                return;
            }

        } // end if target device found
    } // end for loop

    // If we finished the loop and are still in pairing mode, restart the scan
    if (pairingInProgress && scan) {
        Serial.println("Target not found, restarting scan...");
        scan->start(5, scanCompleteCB, false);
    }
}



void startBLEPairing() {
  if (!pairingInProgress) {
    esp_now_deinit();
    scan = BLEDevice::getScan();
    scan->setActiveScan(true);
    pairingInProgress = true;
    scan->start(5, scanCompleteCB, false); // 5 seconds, non-blocking
  }
}

// ==== ESP-NOW ====

void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    // --- FIX START ---
    if (len == sizeof(struct_message)) {
        struct_message receivedMessage;
        memcpy(&receivedMessage, data, sizeof(receivedMessage));

        if (strcmp(receivedMessage.type, "UNPAIR_REQ") == 0) {
            Serial.println("🔓 Unpair command received!");
            deleteMacFromFlash();
            // You may want to also remove the ESP-NOW peer here
            esp_now_del_peer(info->src_addr);
        } else {
            Serial.printf("📨 Received struct message of type: %s\n", receivedMessage.type);
        }
    } else {
        Serial.printf("📨 Received %d bytes (unknown format): %.*s\n", len, len, data);
    }
    // --- FIX END ---
}

void onDataSent(const wifi_tx_info_t* info, esp_now_send_status_t status) {
  Serial.printf("ESP-NOW send status: %s\n",
                status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

bool registerEspNowPeer(const uint8_t* mac) {
  esp_now_peer_info_t p = {};
  memcpy(p.peer_addr, mac, MAC_ADDR_LEN);
  p.channel = ESP_NOW_CHANNEL;
  p.encrypt = false;
  return esp_now_add_peer(&p) == ESP_OK;
}

// ==== FLASH ====

void saveMacToFlash(const uint8_t* mac) {
  prefs.begin("pairing", false);
  prefs.putBytes("mac", mac, MAC_ADDR_LEN);
  prefs.end();
}

bool loadMacFromFlash(uint8_t* mac) {
  prefs.begin("pairing", true);
  bool ok = prefs.isKey("mac");
  if (ok) prefs.getBytes("mac", mac, MAC_ADDR_LEN);
  prefs.end();
  return ok;
}

void deleteMacFromFlash() {
  prefs.begin("pairing", false);
  prefs.remove("mac");
  prefs.end();
  Serial.println("❌ Device MAC removed from flash.");
  deviceName = "None";
  isPaired = false;
  displayNeedsUpdate = true;
}

void clearHostFlash() {
  prefs.begin("pairing", false);
  prefs.clear();
  prefs.end();
  Serial.println("⚠️ Flash cleared. Rebooting...");
  delay(1000);
  ESP.restart();
}

// ==== DISPLAY ====

void updateDisplay() {
  display.clearDisplay();
  printCenteredText("Device: " + deviceName, 4);
  String tip = isPairingMode ? "BLE Pairing..." : "Hold 2s to pair";
  printCenteredText(tip, 28);
  display.display();
}

void printCenteredText(const String &text, int16_t y) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, y);
  display.println(text);
}
