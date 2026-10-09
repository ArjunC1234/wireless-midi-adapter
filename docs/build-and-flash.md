# Build and flash

## Recovered environment

| Firmware group | Board/core evidence | Additional libraries |
| --- | --- | --- |
| Four S2 integrated candidates | `esp32:esp32:esp32s2`, Arduino-ESP32 3.0.5 | Control Surface 2.0.0 for `HOSTESP3.0` |
| C3 BLE/OLED/ESP-NOW experiments | `nologo_esp32c3_super_mini` or generic `esp32c3`, Arduino-ESP32 3.3.0 | Adafruit SSD1306 2.5.15, GFX 1.12.1, BusIO 1.17.2 as applicable |

The exact menu-option strings are preserved in
`archive/build-environments.csv`. API compatibility with other versions is not
assumed.

## Install with Arduino CLI

Install Arduino CLI, configure Espressif's board index through the normal
Arduino instructions, then install the recovered versions:

```powershell
arduino-cli core install esp32:esp32@3.0.5
arduino-cli lib install "Control Surface@2.0.0"
arduino-cli lib install "Adafruit SSD1306@2.5.15"
arduino-cli lib install "Adafruit GFX Library@1.12.1"
arduino-cli lib install "Adafruit BusIO@1.17.2"
```

Arduino-ESP32 3.3.0 is needed to reproduce the C3 cache builds. Installing it
may make the ESP-NOW send-callback signature incompatible with the S2 3.0.5
sources, so keep separate CLI configurations or reinstall the required core
version between groups.

## Compile the main S2 candidates

Run from the repository root. Long FQBN strings intentionally mirror the
recovered build settings.

```powershell
arduino-cli compile --fqbn "esp32:esp32:esp32s2:UploadSpeed=921600,CDCOnBoot=default,MSCOnBoot=default,DFUOnBoot=default,UploadMode=default,CPUFreq=240,FlashFreq=80,FlashMode=qio,FlashSize=4M,PartitionScheme=default,DebugLevel=none,PSRAM=disabled,EraseFlash=none,JTAGAdapter=default,ZigbeeMode=default" firmware/transmitter/device-esp2-recovered

arduino-cli compile --fqbn "esp32:esp32:esp32s2:UploadSpeed=921600,CDCOnBoot=cdc,MSCOnBoot=default,DFUOnBoot=default,UploadMode=default,CPUFreq=240,FlashFreq=80,FlashMode=qio,FlashSize=4M,PartitionScheme=default,DebugLevel=none,PSRAM=disabled,EraseFlash=none,JTAGAdapter=default,ZigbeeMode=default" firmware/transmitter/device-esp3-recovered

arduino-cli compile --fqbn "esp32:esp32:esp32s2:UploadSpeed=921600,CDCOnBoot=cdc,MSCOnBoot=default,DFUOnBoot=default,UploadMode=default,CPUFreq=240,FlashFreq=80,FlashMode=qio,FlashSize=4M,PartitionScheme=default,DebugLevel=none,PSRAM=disabled,EraseFlash=none,JTAGAdapter=default,ZigbeeMode=default" firmware/receiver/host-esp2-serial-recovered

arduino-cli compile --fqbn "esp32:esp32:esp32s2:UploadSpeed=921600,CDCOnBoot=default,MSCOnBoot=default,DFUOnBoot=default,UploadMode=default,CPUFreq=240,FlashFreq=80,FlashMode=qio,FlashSize=4M,PartitionScheme=default,DebugLevel=none,PSRAM=disabled,EraseFlash=none,JTAGAdapter=default,ZigbeeMode=default" firmware/receiver/host-esp3-usb-midi-recovered
```

## Arduino IDE

Use the board/core versions above. For the S2 USB-MIDI receiver, the recovered
source specifically calls for the native USB connector and an IDE USB mode that
uses USB-OTG/TinyUSB. Menu labels vary across board packages; do not assume the
UART/programming connector exposes MIDI. The recovered cache FQBN alone does
not verify successful USB-MIDI mode selection.

## Upload

List ports, put the board into its required bootloader mode, and upload the
chosen sketch:

```powershell
arduino-cli board list
arduino-cli compile --upload -p COM_NUMBER --fqbn "YOUR_EXACT_FQBN" path/to/sketch
```

Replace `COM_NUMBER` and use the matching recovered FQBN. Native USB boards can
enumerate under a different port during reset. Do not flash transmitter and
receiver sketches from different protocol rows in the compatibility matrix.

## Host software

- Protocol A receiver: external serial-to-MIDI bridge software is required at
  115200 baud. Debug text must be separated or disabled before treating the
  stream as MIDI.
- Protocol B receiver: a compatible native USB/TinyUSB configuration and
  Control Surface are required. No extra serial bridge should be needed if it
  enumerates correctly.
- A DAW such as FL Studio is an intended consumer, not a validation result in
  this repository.
