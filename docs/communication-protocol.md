# ESP-NOW communication protocols

The project has three unrelated payload families. There is no explicit protocol
version, byte-order marker, checksum, sequence number, acknowledgement, retry
queue, or packed wire schema. All versions send raw C/C++ structs, making them
dependent on compiler ABI, enum width, alignment, and endianness.

## Protocol A: `DEVICE_ESP2.0` ↔ `HOST_ESP2.0`

Both sides declare `MsgType : uint8_t`, use channel 6, and include a timestamp.
The recovered 32-bit ESP32 ABI yields 16 bytes:

| Offset | Size | Field | Notes |
| ---: | ---: | --- | --- |
| 0 | 1 | `type` | 1 announce, 2 response, 3 unpair, 4 heartbeat, 5 MIDI, 6 connection test |
| 1 | 6 | `mac` | Sender Wi-Fi MAC in most messages |
| 7 | 1 | `cable` | USB-MIDI virtual cable 0–15 |
| 8 | 1 | `status` | MIDI status byte |
| 9 | 1 | `data1` | MIDI data byte 1 |
| 10 | 1 | `data2` | MIDI data byte 2 |
| 11 | 1 | `length` | 1–3 |
| 12 | 4 | `timestamp` | Transmitter `millis()`; receiver does not use it |

Pairing begins when the host broadcasts `PAIR_ANNOUNCE`. A pairing transmitter
saves the advertised MAC, registers it, and unicasts `PAIR_RESPONSE`. Both
sides then exchange heartbeats. MIDI is unacknowledged application data;
ESP-NOW's send callback is not an end-to-end delivery acknowledgement.

## Protocol B: `DEVICE_ESP3.0` ↔ `HOSTESP3.0`

Both sides use an unscoped enum without an underlying type. In the recovered
build ABI the enum is four bytes and the struct is 16 bytes:

| Offset | Size | Field | Notes |
| ---: | ---: | --- | --- |
| 0 | 4 | `type` | Values 1–5; native little-endian enum representation |
| 4 | 6 | `mac` | Sender Wi-Fi MAC |
| 10 | 1 | `cable` | Ignored by receiver output |
| 11 | 1 | `status` | MIDI status byte |
| 12 | 1 | `data1` | MIDI data byte 1 |
| 13 | 1 | `data2` | MIDI data byte 2 |
| 14 | 1 | `length` | Sent but not validated by receiver |
| 15 | 1 | padding | ABI padding, not a protocol field |

The pair pins Wi-Fi and peer records to channel 6. It omits protocol A's
connection-test message and timestamp.

## Why A and B are incompatible

Their equal 16-byte size can let the `len == sizeof(MidiMsg)` guard pass. Their
field offsets do not match. A protocol B MIDI packet read as A obtains `0x90`
from offset 11 as the message length and corrupts the MAC and MIDI fields. A
protocol A packet read as B interprets `type` plus the first three MAC bytes as
a 32-bit enum, which normally matches no case. Pairing is likewise unsafe.

## BLE pairing experiment payload

The C3 experiment sends a separate 44-byte structure:

```cpp
char type[12];
char deviceName[32];
```

Only `UNPAIR_REQ` is implemented. BLE exchanges a textual host Wi-Fi MAC using
service UUID `d49e6f8a-0c69-497b-91c5-bc3446f3a1f6` and characteristic UUID
`34e6f3a8-c13b-4a6a-a491-44e0cfdfd1f7`. There is no authentication,
authorization, or integrity protection in the recovered application logic.

## Compatibility matrix

| Transmitter / experiment | `HOST_ESP2.0` | `HOSTESP3.0` | `HostReceiverScript2.0` |
| --- | --- | --- | --- |
| `DEVICE_ESP2.0` | **Protocol match (A)**; USB-host path has a blocking logic defect | Incompatible field layout | Incompatible payload/pairing design |
| `DEVICE_ESP3.0` | Incompatible field layout | **Protocol match (B)**; USB-host claim/transfer defects remain | Incompatible payload/pairing design |
| `DeviceReceiverScript` | Incompatible; no MIDI payload | Incompatible; no MIDI payload | Shared BLE UUIDs and unpair struct, but radio lifecycle/MAC defects prevent claiming interoperability |

“Protocol match” means declarations and state-machine messages align under the
recovered ABI. It does not claim successful hardware interoperability.
