/*
  ESP-NOW Multi Unit Demo (Updated for Arduino-ESP32 v3.3.0+)
  DroneBot Workshop 2022 (Adapted by ChatGPT 2025)
*/

#include <WiFi.h>
#include <esp_now.h>

// Define LED and pushbutton pins (change if needed for your board)
#define STATUS_LED     1  // change to valid GPIO on your board
#define STATUS_BUTTON   2  // change to valid GPIO on your board

bool buttonDown = false;
bool ledOn = false;

// Format MAC for printing
void formatMacAddress(const uint8_t *macAddr, char *buffer, int maxLength) {
  snprintf(buffer, maxLength, "%02X:%02X:%02X:%02X:%02X:%02X",
           macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
}

// Updated receive callback for ESP32 Arduino v3.3.0+
void receiveCallback(const esp_now_recv_info_t *info, const uint8_t *data, int dataLen) {
  char buffer[ESP_NOW_MAX_DATA_LEN + 1];
  int msgLen = min(ESP_NOW_MAX_DATA_LEN, dataLen);
  strncpy(buffer, (const char *)data, msgLen);
  buffer[msgLen] = 0;

  char macStr[18];
  formatMacAddress(info->src_addr, macStr, 18);
  Serial.printf("Received message from %s: %s\n", macStr, buffer);

  if (strcmp(buffer, "on") == 0) {
    ledOn = true;
  } else {
    ledOn = false;
  }
  digitalWrite(STATUS_LED, ledOn);
}

// Updated send callback
void sentCallback(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  char macStr[18];
  formatMacAddress(info->des_addr, macStr, 18);
  Serial.printf("Sent to %s: %s\n", macStr, status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

// Broadcast message
void broadcast(const String &message) {
  uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  // match current channel
  peerInfo.ifidx = WIFI_IF_STA;
  peerInfo.encrypt = false;

  if (!esp_now_is_peer_exist(broadcastAddress)) {
    esp_now_add_peer(&peerInfo);
  }

  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)message.c_str(), message.length());
  if (result == ESP_OK) {
    Serial.println("Broadcast sent");
  } else {
    Serial.printf("Broadcast failed: %d\n", result);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_STA);
  Serial.println("ESP-NOW Test (Arduino 3.3.0+)");
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    ESP.restart();
  }

  esp_now_register_recv_cb(receiveCallback);
  esp_now_register_send_cb(sentCallback);

  pinMode(STATUS_BUTTON, INPUT_PULLUP);
  pinMode(STATUS_LED, OUTPUT);
}

void loop() {
  if (digitalRead(STATUS_BUTTON)) {
    if (!buttonDown) {
      buttonDown = true;
      ledOn = !ledOn;
      digitalWrite(STATUS_LED, ledOn);
      broadcast(ledOn ? "on" : "off");
    }
    delay(500);  // debounce
  } else {
    buttonDown = false;
  }
}
