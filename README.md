# Wireless USB-MIDI Adapter

I built this ESP32-based prototype to explore how a conventional USB MIDI
controller could communicate with a computer wirelessly. The project combines
USB host enumeration, USB-MIDI packet decoding, ESP-NOW transport, peer
management, and computer-side MIDI forwarding.

The work is organized as an engineering prototype rather than a finished
product. Individual subsystems compile and the protocol behavior is covered by
host-side tests, but I have not documented a verified end-to-end
controller-to-DAW test. Known gaps and unsuccessful approaches are retained
because they show the design decisions and debugging process behind the
project.

## System concept

```mermaid
flowchart LR
    A[USB MIDI controller] -->|USB| B[Battery-powered ESP32-S2<br/>USB host / transmitter]
    B -->|ESP-NOW| C[ESP32-S2 receiver]
    C -->|USB MIDI or serial bridge| D[Computer / DAW]
```

The firmware evolved along two main, mutually incompatible branches:

- **Protocol A (`DEVICE_ESP2.0` + `HOST_ESP2.0`)** adds connection diagnostics
  and forwards raw MIDI bytes over a serial port for use with external bridge
  software.
- **Protocol B (`DEVICE_ESP3.0` + `HOSTESP3.0`)** improves USB descriptor
  discovery and uses Control Surface to present MIDI to the computer over USB.

These branches use different in-memory packet layouts and are not mutually
compatible. Protocol B is closer to the intended architecture, although its
USB host path still needs interface claiming and more robust transfer and error
handling.

## What I implemented

- Used the ESP-IDF USB Host API within Arduino-ESP32 to inspect configuration,
  interface, and endpoint descriptors for USB Audio/MIDIStreaming devices.
- Parsed four-byte USB-MIDI event packets and mapped channel voice messages to
  compact ESP-NOW payloads.
- Implemented broadcast discovery, MAC-based peer registration, NVS peer
  persistence, heartbeats, reconnection states, connection timeouts, and manual
  unpairing.
- Explored two computer-output paths: CDC serial bytes through an external
  MIDI bridge and native USB MIDI through Control Surface/TinyUSB.
- Prototyped BLE-assisted discovery, an SSD1306 status interface, GPIO
  diagnostics, and ESP-NOW peer tests on ESP32-C3 hardware.
- Added host-side checks for packet layout compatibility and USB-MIDI message
  length handling.

## Project status

| Area | Implemented | Remaining work |
| --- | --- | --- |
| USB host | Host-stack setup, device events, descriptor walking, and transfer callbacks | One branch does not reach setup; the later branch does not claim the MIDI interface |
| USB-MIDI parsing | Channel-voice Code Index Number decoding and forwarding | SysEx and system-common coverage is incomplete |
| ESP-NOW | Pairing, heartbeat, fixed-channel operation, and NVS peer storage | Replace raw ABI structs with a versioned wire format; add sequencing and acknowledgements |
| Computer MIDI | Serial bridge and Control Surface USB-MIDI approaches | Validate enumeration and DAW input on target hardware |
| BLE and OLED | Separate C3 pairing and status-interface experiments | Integrate with the S2 MIDI data path |
| Hardware and power | Board roles and several firmware-visible GPIO assignments | Document and electrically validate the complete schematic, USB VBUS path, and battery circuit |
| HID | Design consideration only | No HID transport is implemented |

## Repository layout

```text
firmware/
  transmitter/                 ESP32-S2 USB-host transmitter iterations
  receiver/                    Serial and USB-MIDI receiver iterations
  experiments/                 C3 pairing/UI, ESP-NOW, GPIO, and button tests
archive/                       Generated source snapshots and build evidence
docs/                          Architecture, protocol, build, and design analysis
hardware/                      Hardware, assembly, wiring, and power notes
tests/                         Host-side protocol and MIDI-decoding tests
tools/                         Source-processing and repository-audit utilities
```

## Build and validation

The four primary ESP32-S2 sketches compile with Arduino-ESP32 3.0.5. The native
USB-MIDI receiver also requires Control Surface 2.0.0. See
[Build and flash](docs/build-and-flash.md) for board settings, dependencies,
commands, and recorded validation results.

Run the hardware-independent checks with:

```powershell
python -m unittest discover -s tests -v
python tools/audit_repository.py
```

The tests simulate protocol layout and selected MIDI decoding behavior; they do
not replace hardware validation. Before powering the prototype from a LiPo or
connecting a valuable USB controller, verify the USB VBUS and battery wiring
against the actual assembly.

## Documentation

- [Architecture](docs/architecture.md)
- [Firmware versions and compatibility](docs/firmware-versions.md)
- [ESP-NOW communication protocols](docs/communication-protocol.md)
- [USB host and MIDI analysis](docs/usb-midi.md)
- [Pairing and reconnection](docs/pairing-and-reconnection.md)
- [Build and flash](docs/build-and-flash.md)
- [Development history](docs/development-history.md)
- [Testing and validation](docs/testing.md)
- [Known limitations](docs/known-limitations.md)
- [Troubleshooting](docs/troubleshooting.md)
- [Hardware notes](hardware/README.md)

## License status

No repository-wide license is currently applied. Some dependencies and copied
library material have their own license requirements; see
[Licensing notes](LICENSES.md) before reusing or redistributing the source.
