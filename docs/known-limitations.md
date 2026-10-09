# Known limitations

## Highest-priority blockers

1. `DEVICE_ESP2.0` never calls its MIDI descriptor checker, so its USB setup
   path is unreachable from the source-visible state transitions.
2. `DEVICE_ESP3.0` discovers an endpoint but does not claim the containing
   interface before submitting transfers and reports readiness without checking
   successful submissions.
3. The two 16-byte ESP-NOW structs have incompatible field layouts and no
   protocol version.
4. `HOST_ESP2.0` mixes binary MIDI and debug text on one serial stream.
5. No recovered evidence demonstrates a class-compliant MIDI enumeration,
   controller-to-DAW message, or reconnect under real hardware.

## MIDI coverage

- No SysEx transport exists in the fixed three-byte MIDI payload.
- Several system common USB-MIDI CIN values are ignored.
- Protocol B treats CIN `0xF` as three bytes on the transmitter while the
  receiver drops all `0xF*` statuses.
- Protocol B collapses all USB virtual cables to one output.
- There is no queue or backpressure policy; callbacks can drop data.

## Wireless behavior

- MIDI messages have no sequence number, retry, duplicate detection, or
  end-to-end acknowledgement.
- Pairing is unauthenticated and broadcast-based.
- Raw structs depend on the compiler ABI and little-endian representation.
- `DEVICE_ESP2.0` uses blocking 50 ms LED delays in the MIDI path.
- “Connection quality” counters do not measure RF packet-loss rate rigorously.

## USB lifecycle

- Multi-device operation is not implemented.
- Interface alternate settings and endpoint transfer types are not handled
  robustly.
- Resource release on disconnect is incomplete.
- Endpoint packet size is assumed to be 64 bytes.

## Hardware and product scope

- Battery capacity/runtime, charging, 5 V VBUS delivery, and thermal behavior
  are unmeasured.
- GPIO mappings conflict with some comments and are not verified against a
  schematic.
- HID support is a planned extension only.
- BLE/OLED experiments are not integrated into the main S2 firmware.
- No enclosure, EMC, ESD, or USB compliance work is documented.
