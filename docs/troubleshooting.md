# Troubleshooting

## The controller is not detected

Confirm the transmitter is an S2/S3 with USB OTG capability and that the
controller receives a safe 5 V VBUS supply. Then capture the USB host event and
descriptor logs. `DEVICE_ESP2.0` has an unreachable setup path; start analysis
with `DEVICE_ESP3.0`, but fix interface selection/claiming before expecting
transfers.

## A MIDI interface does not appear on the computer

`HOST_ESP2.0` is serial, not USB MIDI. For `HOSTESP3.0`, use the native USB port,
the correct USB-OTG/TinyUSB board setting, and Control Surface 2.0.0. Inspect the
operating system's USB descriptors rather than treating a serial port as proof
of MIDI enumeration.

## Serial bridge produces random MIDI events

The serial receiver sends status logs and raw MIDI on the same stream. Text
bytes are valid byte values to a serial bridge and can become spurious MIDI.
Move diagnostics to another UART or compile them out in a separately documented
fix before testing.

## ESP-NOW peers do not pair

Use only a matched protocol pair, erase stale NVS peer data on both boards, and
confirm both station interfaces are on channel 6. Log the callback source MAC
and compare it with the stored peer. Do not mix the C3 BLE branch with the S2
protocols.

## BLE pairing finds a device but ESP-NOW fails

The recovered C3 host deinitializes ESP-NOW during scan and does not initialize
it afterward. It also uses a BLE address as an ESP-NOW address. Both issues need
a traceable corrected branch and hardware verification.

## Build fails after changing Arduino-ESP32 versions

The recovered S2 firmware used 3.0.5; C3 experiments used 3.3.0. ESP-NOW callback
types differ across these APIs. Reproduce the matching environment first, then
port intentionally instead of editing historical sketches in place.
