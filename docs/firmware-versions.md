# Firmware versions and evidence

## Evidence levels used here

- **Source-confirmed:** directly present in a recovered translation unit.
- **Build-confirmed:** the Arduino cache contains `.o`, linked `.elf`, and
  `.bin` outputs for that source. This proves a historical compile/link, not
  execution on a board.
- **Rebuild-confirmed:** the reconstructed `.ino` compiled during this
  repository reconstruction; see [testing](testing.md).
- **Hardware-unverified:** no serial log, USB capture, radio capture, DAW
  screenshot, or test record was supplied.

All 14 sources are build-confirmed by recovered cache artifacts. The cache
timestamps are useful forensic evidence, but they are not a definitive project
timeline.

## Integrated S2 candidates

### `DEVICE_ESP2.0`

Source-confirmed features include a USB Host client, ESP-NOW channel 6,
pairing broadcasts/responses, NVS peer persistence, heartbeat/reconnect states,
a connection-test message, LED/button handling, and decoding of selected
four-byte USB-MIDI event packets into protocol A.

Critical issue: `isMIDI` is set only by `check_interface_desc_MIDI()`, but that
function is never called. The new-device callback does not open or inspect the
device. Consequently the main loop has no source-visible path to call
`handle_usb_device()`. That handler also hard-codes device address 1, guesses
interfaces 1 then 0, and guesses endpoints `0x81`–`0x84`.

### `HOST_ESP2.0`

Source-confirmed protocol A receiver. It sends 1–3 raw MIDI bytes through
`Serial` at 115200 baud and explicitly expects external serial-to-MIDI bridge
software. It is not a class-compliant USB-MIDI device.

Critical issue: human-readable status/debug output and raw MIDI bytes share the
same stream. Those text bytes can be interpreted as MIDI by a transparent
serial bridge. MIDI processing also blocks for a 50 ms LED pulse and flushes
the serial stream after every event.

### `DEVICE_ESP3.0`

Source-confirmed improvements include using the address supplied by the USB
new-device event and walking the active configuration descriptor to find an IN
endpoint within an Audio/MIDIStreaming interface.

Critical issues: it does not retain the MIDI interface number, select an
alternate setting, or call `usb_host_interface_claim()` before submitting
transfers. Allocation/submission errors are not used to decide readiness, and
the sketch sets `isMIDIReady = true` unconditionally. Transfer/device resources
are not released on disconnect. The CIN switch omits SysEx and several system
common packet forms.

### `HOSTESP3.0`

Source-confirmed protocol B receiver using Control Surface's
`USBMIDI_Interface`. It maps the seven channel-voice status families to
library calls. System messages are ignored, cable numbers are discarded, and
the received `length` is not validated or used. The source intends an S2/S3
native USB port and TinyUSB/USB-OTG configuration; enumeration on a computer
was not evidenced.

## C3 pairing and user-interface branch

`DeviceReceiverScript` and `HostReceiverScript2.0` share BLE service and
characteristic UUIDs and a 44-byte unpair structure. They target an ESP32-C3
Super Mini variant under Arduino-ESP32 3.3.0. Neither processes MIDI.

The host deinitializes ESP-NOW when BLE scanning starts and never initializes
it again before attempting to add the discovered peer. It also stores the BLE
address returned by the scanner as the ESP-NOW peer address, while the device
advertises/writes Wi-Fi MAC data separately. These address domains are not
shown to be equivalent. Wi-Fi is not explicitly pinned to the peer's declared
channel 1.

`HostReceiverScript` is an OLED/button UI prototype without BLE, ESP-NOW, or
MIDI. The earlier host-state sketches contain a debounce logic defect: the same
variable is used as both the observed and stable state, continuously resetting
the debounce timer while the input differs.

## Diagnostics

The remaining sketches capture useful engineering exploration—GPIO readback,
LED smoke testing, serial button testing, and two-peer ESP-NOW broadcasts—but
none is an end-to-end MIDI adapter. The GPIO diagnostic labels a pin “DAMAGED”
from a digital readback test; that conclusion is not electrically sufficient
and must not be treated as a board-health certification.
