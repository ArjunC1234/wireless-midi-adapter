# Components

## Source/build-confirmed

- **ESP32-S2:** target for the two USB-host transmitter candidates and two
  computer-side receiver candidates. The exact S2 Mini vendor/revision is not
  encoded; the build used Arduino's generic ESP32-S2 definition, 4 MB flash,
  no PSRAM, and 240 MHz CPU setting.
- **ESP32-C3 Super Mini:** target for BLE pairing, OLED, ESP-NOW peer, button,
  and serial experiments. The C3 has native USB Serial/JTAG, but the recovered
  C3 sources do not implement USB host or USB MIDI.
- **SSD1306 128×64 OLED:** I2C display initialized at address `0x3C` using
  Adafruit SSD1306/GFX.
- **Buttons and LEDs:** active-low buttons use `INPUT_PULLUP`; LEDs are driven
  directly by GPIO in source. Electrical polarity and series-resistor values
  are not documented.

## Historical context, not file-corroborated

The project description identifies an approximately 1000 mAh LiPo cell, TP4056
charger, IP5310 power-management hardware, USB connections, and soldered
prototype assembly. No recovered document establishes which revision used
which module or how they were interconnected.

## Unknown selection details

Part numbers/manufacturers for the ESP32 boards, OLED module voltage tolerance,
USB receptacles/adapters, battery connector, switches, passives, ESD parts,
fuses, and VBUS power switch are unknown.
