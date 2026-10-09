# Firmware index

The `.ino` files are behavior-preserving reconstructions. Names were made
descriptive, but the source comments and logic were not modernized. “Recovered”
means the Arduino-generated preprocessing was removed; it does not mean the
firmware is known to work end to end.

## Main transmitter candidates

| Repository sketch | Recovered source | Target recorded by build cache | Role |
| --- | --- | --- | --- |
| `transmitter/device-esp2-recovered` | `DEVICE_ESP2.0.ino.cpp` | Generic ESP32-S2, core 3.0.5 | USB host attempt + protocol A ESP-NOW sender |
| `transmitter/device-esp3-recovered` | `DEVICE_ESP3.0.ino.cpp` | Generic ESP32-S2, core 3.0.5 | Descriptor-parsing USB host attempt + protocol B sender |

## Main receiver candidates

| Repository sketch | Recovered source | Target recorded by build cache | Computer output |
| --- | --- | --- | --- |
| `receiver/host-esp2-serial-recovered` | `HOST_ESP2.0.ino.cpp` | Generic ESP32-S2, core 3.0.5 | Raw MIDI bytes on `Serial`; not USB MIDI |
| `receiver/host-esp3-usb-midi-recovered` | `HOSTESP3.0.ino.cpp` | Generic ESP32-S2, core 3.0.5 | Control Surface `USBMIDI_Interface` attempt |

## Experiments

| Directory | Recovered source | What it demonstrates |
| --- | --- | --- |
| `device-ble-pairing` | `DeviceReceiverScript.ino.cpp` | C3 BLE GATT server, MAC exchange, NVS, ESP-NOW unpair struct; no USB or MIDI |
| `host-ble-pairing-oled` | `HostReceiverScript2.0.ino.cpp` | C3 BLE scan/client, OLED, NVS, ESP-NOW unpair receive; contains radio lifecycle and MAC-selection defects |
| `host-oled-ui` | `HostReceiverScript.ino.cpp` | SSD1306 status UI and button-state prototype only |
| `host-pairing-ui-state` | `HostReceiverSketch.ino.cpp` | Early C3 LED/button pairing-state prototype at 921600 baud |
| `c3-pairing-ui-state-115200` | `sketch_jul30a.ino.cpp` | Near-duplicate of the preceding prototype at 115200 baud |
| `s2-gpio-diagnostic` | `hostAliveAndWorking.ino.cpp` | S2 LED, button, and GPIO readback diagnostic |
| `c3-button-serial` | `SerialMonitorTest.ino.cpp` | C3 button/serial diagnostic; comments disagree with configured GPIO |
| `s2-led-blink` | `sketch_aug4a.ino.cpp` | S2 GPIO 15 LED blink smoke test |
| `esp-now-peer-a` / `esp-now-peer-b` | `test1.ino.cpp` / `test2.ino.cpp` | C3 ESP-NOW broadcast experiment with different pins; button condition is inverted for `INPUT_PULLUP` |

See [firmware versions](../docs/firmware-versions.md) for a source-by-source
assessment and [build instructions](../docs/build-and-flash.md) before trying
to flash any sketch.
