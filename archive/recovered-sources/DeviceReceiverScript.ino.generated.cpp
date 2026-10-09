#include <Arduino.h>
#line 1 "<recovered-sketch-path>"
#include <esp_now.h>
#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <Preferences.h>
#include <string>


// Pin definitions
#define LED_PIN 0
#define BUTTON_PIN 1

// BLE service/char UUIDs
#define SERVICE_UUID        "d49e6f8a-0c69-497b-91c5-bc3446f3a1f6"
#define CHAR_HOSTMAC_UUID   "34e6f3a8-c13b-4a6a-a491-44e0cfdfd1f7"
#define CHAR_DEVICEMAC_UUID "726ef22e-73a9-48a5-85f0-2537dbb4f62d"

// Forward declarations
bool registerEspNowPeer(const uint8_t* mac);
void saveMacToFlash(const uint8_t* mac);
void startBLEPairing();
void stopBLEPairing();
void updateLed();

// BLE objects
BLEServer* pServer = nullptr;
BLECharacteristic* hostMacChar = nullptr;
BLECharacteristic* deviceMacChar = nullptr;
BLEAdvertising* pAdvertising = nullptr;
bool deviceConnected = false;

Preferences preferences;
uint8_t hostMac[6];
bool isPaired = false;
bool isPairingMode = false;

unsigned long lastBlinkTime = 0;
bool currentLedState = false;
const unsigned long BLINK_INTERVAL = 250;

// Button tracking
bool buttonHeld = false;
unsigned long buttonPressTime = 0;
bool actionTriggered = false;

// Data Structure
typedef struct struct_message {
  char type[12]; // e.g. "UNPAIR_REQ"
  char deviceName[32];
} struct_message;

struct_message outgoingMessage;

// === BLE Server Callbacks ===
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    deviceConnected = true;
    Serial.println("[BLE] Client connected");
  }
  void onDisconnect(BLEServer* pServer) override {
    deviceConnected = false;
    Serial.println("[BLE] Client disconnected");
  }
};

class HostMacCharCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pChar) override {
    String val = String(pChar->getValue().c_str());
    Serial.print("[BLE] Host MAC write received: ");
    Serial.println(val);

    if (val.length() == 17) { // "AA:BB:CC:DD:EE:FF"
      int values[6];
      int parsed = sscanf(val.c_str(), "%x:%x:%x:%x:%x:%x",
             &values[0], &values[1], &values[2], &values[3], &values[4], &values[5]);
      if (parsed == 6) {
        for (int i = 0; i < 6; i++) hostMac[i] = (uint8_t)values[i];

        Serial.print("[BLE] Parsed Host MAC: ");
        for (int i = 0; i < 6; i++) {
          if (i > 0) Serial.print(":");
          Serial.printf("%02X", hostMac[i]);
        }
        Serial.println();

        if (registerEspNowPeer(hostMac)) {
          saveMacToFlash(hostMac);
          isPaired = true;
          isPairingMode = false;
          updateLed();
          stopBLEPairing();
          Serial.println("[BLE] Pairing complete, advertising stopped.");
        } else {
          Serial.println("[BLE] Failed to register ESP-NOW peer.");
        }
      } else {
        Serial.println("[BLE] Failed to parse MAC address properly.");
      }
    } else {
      Serial.println("[BLE] Invalid MAC length received via BLE.");
    }
  }
};

#line 106 "<recovered-sketch-path>"
void setup();
#line 143 "<recovered-sketch-path>"
void loop();
#line 281 "<recovered-sketch-path>"
bool loadMacFromFlash(uint8_t* mac);
#line 294 "<recovered-sketch-path>"
void clearFlashPairing();
#line 301 "<recovered-sketch-path>"
void sendUnpairRequest();
#line 106 "<recovered-sketch-path>"
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Serial.println("[Setup] Initializing WiFi and ESP-NOW...");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("[Error] ESP-NOW init failed!");
    while (true);
  }

  esp_now_register_recv_cb([](const esp_now_recv_info_t* info, const uint8_t* data, int len){
    // Not used here yet
  });
  esp_now_register_send_cb([](const wifi_tx_info_t* info, esp_now_send_status_t status){
    Serial.printf("[ESP-NOW] Send status: %s\n", status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
  });
  BLEDevice::init("ReceiverBLE");

  // Attempt to load saved MAC and register peer
  if (loadMacFromFlash(hostMac)) {
    Serial.println("[Setup] Loaded MAC from flash, registering peer...");
    isPaired = registerEspNowPeer(hostMac);
    if (isPaired) {
      Serial.println("[Setup] Peer registered, device paired.");
      updateLed();
    } else {
      Serial.println("[Setup] Failed to register peer from flash.");
    }
  } else {
    Serial.println("[Setup] No MAC found in flash.");
  }
}

void loop() {
  unsigned long now = millis();
  bool buttonState = digitalRead(BUTTON_PIN) == LOW;

  if (buttonState) {
    if (!buttonHeld) {
      // Button just pressed
      buttonHeld = true;
      buttonPressTime = now;
      actionTriggered = false;
      Serial.println("[Button] Press detected");
    } else {
      // Button held down
      unsigned long heldDuration = now - buttonPressTime;

      if (isPaired && !isPairingMode && heldDuration >= 1000 && !actionTriggered) {
        Serial.println("[Button] Hold 1s while paired: Sending UNPAIR request...");
        sendUnpairRequest();
        actionTriggered = true;
        updateLed();
      }

      if (heldDuration >= 2000 && !actionTriggered) {
        if (isPairingMode) {
          Serial.println("[Button] Hold 2s: Cancelling pairing mode...");
          isPairingMode = false;
          stopBLEPairing();
        } else {
          Serial.println("[Button] Hold 2s: Entering BLE pairing mode...");
          isPairingMode = true;
          startBLEPairing();
        }
        actionTriggered = true;
      }
    }
  } else {
    if (buttonHeld) {
      Serial.println("[Button] Released");
    }
    buttonHeld = false;
  }

  // LED blinking & state update
  updateLed();
}

void startBLEPairing() {
  Serial.println("[BLE] Starting BLE pairing mode...");
  if (!pServer) {
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService* service = pServer->createService(SERVICE_UUID);

    hostMacChar = service->createCharacteristic(CHAR_HOSTMAC_UUID, BLECharacteristic::PROPERTY_WRITE);
    hostMacChar->setCallbacks(new HostMacCharCallbacks());

    deviceMacChar = service->createCharacteristic(CHAR_DEVICEMAC_UUID, BLECharacteristic::PROPERTY_READ);

    // Provide our MAC address string
    char macStr[18];
    uint8_t mac[6];
    WiFi.macAddress(mac);
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    deviceMacChar->setValue(macStr);

    service->start();

    pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinInterval(0x20);  // adjust advertising interval if needed
    pAdvertising->setMaxInterval(0x40);
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->start();

    Serial.println("[BLE] BLE server started, advertising.");
  } else {
    // Already initialized, just restart advertising
    if (pAdvertising) {
      pAdvertising->start();
      Serial.println("[BLE] BLE advertising restarted.");
    }
  }
}

void stopBLEPairing() {
  Serial.println("[BLE] Stopping BLE pairing mode...");
  if (pAdvertising) {
    pAdvertising->stop();
    Serial.println("[BLE] Advertising stopped.");
  }
  // Do NOT call BLEDevice::deinit() to keep BLE server ready for next pairing session
}

void updateLed() {
  unsigned long now = millis();

  if (isPairingMode && !isPaired) {
    if (now - lastBlinkTime >= BLINK_INTERVAL) {
      currentLedState = !currentLedState;
      digitalWrite(LED_PIN, currentLedState);
      lastBlinkTime = now;
    }
  } else if (isPaired) {
    digitalWrite(LED_PIN, HIGH);
  } else {
    digitalWrite(LED_PIN, LOW);
  }
}

bool registerEspNowPeer(const uint8_t* mac) {
  if (esp_now_is_peer_exist(mac)) {
    Serial.println("[ESP-NOW] Peer already exists.");
    return true;
  }

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, mac, 6);
  peerInfo.channel = 1; // Auto channel
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) == ESP_OK) {
    Serial.println("[ESP-NOW] Peer registered.");
    return true;
  } else {
    Serial.println("[ESP-NOW] Failed to register peer.");
    return false;
  }
}

void saveMacToFlash(const uint8_t* mac) {
  preferences.begin("pairing", false);
  preferences.putBytes("hostmac", mac, 6);
  preferences.end();
  Serial.println("[Flash] Host MAC saved to flash.");
}

bool loadMacFromFlash(uint8_t* mac) {
  preferences.begin("pairing", true);
  if (preferences.isKey("hostmac")) {
    preferences.getBytes("hostmac", mac, 6);
    preferences.end();
    Serial.println("[Flash] Loaded MAC from flash.");
    return true;
  }
  preferences.end();
  Serial.println("[Flash] No MAC found in flash.");
  return false;
}

void clearFlashPairing() {
  preferences.begin("pairing", false);
  preferences.remove("hostmac");
  preferences.end();
  Serial.println("[Flash] Host MAC cleared from flash.");
}

void sendUnpairRequest() {
  strcpy(outgoingMessage.type, "UNPAIR_REQ");
  strcpy(outgoingMessage.deviceName, "DeviceReceiver");

  Serial.println("[ESP-NOW] Sending unpair request...");
  if (esp_now_send(hostMac, (uint8_t*)&outgoingMessage, sizeof(outgoingMessage)) == ESP_OK) {
    Serial.println("[ESP-NOW] Unpair request sent successfully.");
  } else {
    Serial.println("[ESP-NOW] Failed to send unpair request.");
  }

  delay(100); // Give time for sending before clearing

  clearFlashPairing();
  isPaired = false;
  updateLed();
}
