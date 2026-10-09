# Wireless USB-MIDI Adapter — Recovered ESP32 Prototype

A prototype wireless USB-MIDI bridge built around ESP32 microcontrollers,
exploring USB host enumeration, MIDI event-packet decoding, ESP-NOW transport,
peer persistence/reconnection, and computer-side MIDI forwarding.

This repository reconstructs a 2025 experimental project from Arduino build
artifacts. It is presented as engineering work in progress: every recovered
sketch had a linked historical build, and the primary S2 sketches were rebuilt
during reconstruction, but no evidence was available to claim a fully working
controller-to-DAW path.

## System concept

```mermaid
flowchart LR
    A[USB MIDI controller] -->|USB| B[Battery-powered ESP32-S2\nUSB host / transmitter]
    B -->|ESP-NOW| C[ESP32-S2 receiver]
    C -->|USB MIDI attempt or serial bridge| D[Computer / DAW]
```

The firmware evolved along two main, mutually incompatible branches:

- **Protocol A (`DEVICE_ESP2.0` + `HOST_ESP2.0`)** adds connection diagnostics
  and sends raw MIDI bytes over a serial port for external bridge software.
- **Protocol B (`DEVICE_ESP3.0` + `HOSTESP3.0`)** improves descriptor discovery
  and uses Control Surface to attempt a class-compliant USB-MIDI receiver.

The second branch is closest to the intended product architecture, but its USB
host implementation still omits interface claiming and robust transfer setup.

## Engineering highlights

- Used the ESP-IDF USB Host API from Arduino-ESP32 to inspect configuration,
  interface, and endpoint descriptors for USB Audio/MIDIStreaming devices.
- Parsed four-byte USB-MIDI event packets and mapped channel voice messages to
  compact ESP-NOW payloads.
- Built broadcast discovery, MAC-based peer registration, NVS persistence,
  heartbeats, reconnection states, timeouts, and manual unpair behavior.
- Explored two computer output approaches: CDC serial bytes through an external
  MIDI bridge and native USB MIDI through Control Surface/TinyUSB.
- Prototyped BLE-assisted discovery, SSD1306 status UI, GPIO diagnostics, and
  ESP-NOW peer tests on ESP32-C3 hardware.
- Recovered 14 Arduino sketches without rewriting their historical behavior,
  retained auditable generated-source snapshots, and added protocol-layout
  simulations and publication checks.

## Evidence-based project status

| Area | What the repository demonstrates | Current limitation |
| --- | --- | --- |
| USB host | Host stack setup, device events, descriptor walking, transfer callbacks | One branch never reaches setup; the other does not claim the interface |
| USB-MIDI parsing | Channel voice CIN decoding and forwarding | SysEx/system common coverage is incomplete |
| ESP-NOW | Two pairing/heartbeat protocols, channel pinning, NVS peers | Raw ABI structs, no version/sequence/acknowledgement |
| Computer MIDI | Serial-byte bridge and Control Surface USB-MIDI approaches | No recovered USB enumeration or DAW test record |
| BLE/OLED | Separate C3 pairing and UI experiments | Not integrated with the S2 MIDI path |
| Hardware/power | Board roles and several GPIO assignments | No verified schematic, wiring record, or power measurements |
| HID | Long-term concept only | No HID implementation recovered |

## Repository map

```text
firmware/
  transmitter/                 Reconstructed S2 USB-host candidates
  receiver/                    Reconstructed serial and USB-MIDI receivers
  experiments/                 C3 pairing/UI, ESP-NOW, GPIO, and button tests
archive/
  recovered-sources/           Path-redacted generated .ino.cpp snapshots
  artifact-manifest.csv        Original artifact hashes and cache evidence
  build-environments.csv       Exact historical board/core configuration
docs/                          Architecture, protocol, build, history, and audit
hardware/                      Evidence-graded hardware and power notes
tests/                         Host-side protocol/CIN simulations
tools/                         Source-recovery and repository-audit utilities
```

## Build and inspect

The primary sketches were built historically with Arduino-ESP32 3.0.5 for the
generic ESP32-S2 target. The USB-MIDI receiver additionally used Control Surface
2.0.0. Start with [build and flash](docs/build-and-flash.md); exact recovered
FQBN strings are in `archive/build-environments.csv`.

Run the hardware-independent validation with:

```powershell
python -m unittest discover -s tests -v
python tools/audit_repository.py
```

Do not begin with battery power or a valuable USB controller. The original
power path and USB VBUS circuit were not recovered.

## Documentation

- [Architecture](docs/architecture.md)
- [Firmware versions](docs/firmware-versions.md)
- [ESP-NOW protocols and compatibility matrix](docs/communication-protocol.md)
- [USB and MIDI analysis](docs/usb-midi.md)
- [Pairing and reconnection](docs/pairing-and-reconnection.md)
- [Build and flash](docs/build-and-flash.md)
- [Development history](docs/development-history.md)
- [Testing evidence](docs/testing.md)
- [Known limitations](docs/known-limitations.md)
- [Troubleshooting](docs/troubleshooting.md)
- [Hardware evidence](hardware/README.md)
- [Manual GitHub publication](docs/publishing.md)

## Provenance and license

The initial Git history should describe this as a reconstruction from recovered
artifacts, not as the original development history. No repository-wide license
has been applied because source ownership and attribution are not fully
resolved; see [licensing and provenance](LICENSES.md).
