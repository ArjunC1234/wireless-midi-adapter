# USB and MIDI analysis

## Controller-facing USB host

### `DEVICE_ESP2.0`

The sketch installs `usb_host`, registers an async client, allocates transfers,
and includes an Audio class / MIDIStreaming subclass check. That check is not
invoked. The new-device event does not store its address, while device setup
hard-codes address 1. Interface and endpoint selection are guesses.

The packet loop handles channel-voice CIN values `0x8`, `0x9`, `0xA`, `0xB`,
`0xC`, `0xD`, and `0xE`. Its `0xF` branch tries to infer several system message
lengths from the status byte. USB-MIDI 1.0 uses other CIN values for system
common and SysEx packets, and those values are skipped. Running Status is not a
USB event-packet concern, but SysEx assembly would require more than the current
three-byte transport fields.

### `DEVICE_ESP3.0`

The sketch walks the active configuration descriptor and remembers the last IN
endpoint encountered while within a MIDIStreaming interface. It does not retain
or claim that interface and does not inspect transfer type or maximum packet
size. It submits two fixed 64-byte transfers, ignores submit failures, and
marks the device ready even if neither transfer is active. Disconnect cleanup
does not free transfers, release the interface, or close the device.

Its CIN switch forwards five three-byte channel-voice types, the two two-byte
channel-voice types, and every `0xF` packet as three bytes. It omits CIN values
used for SysEx and system common messages and treats one-byte CIN `0xF` as a
three-byte message.

## Computer-facing output

### Serial bridge receiver

`HOST_ESP2.0` sends raw MIDI bytes using `Serial.write()` at 115200 baud. The
recovered FQBN enables CDC on boot, but the interface remains a serial port.
External software must translate that byte stream into an operating-system MIDI
port. The comments name Hairless MIDI<->Serial Bridge, but no copy is included
and no runtime test was recovered.

Status text, emoji, connection logs, and raw MIDI all use the same `Serial`
object. A transparent bridge cannot distinguish diagnostic bytes from MIDI.

### USB-MIDI receiver

`HOSTESP3.0` uses Control Surface 2.0.0's `USBMIDI_Interface` and calls
`Control_Surface.begin()` / `Control_Surface.loop()`. It maps note off/on, poly
pressure, control change, program change, channel pressure, and pitch bend.
System messages are dropped; cable numbers are collapsed to one interface.

The source comment requires the native USB connector and USB-OTG/TinyUSB mode.
The recovered generic ESP32-S2 FQBN records `CDCOnBoot=default` and does not by
itself prove the IDE menu selection described in the comment. A class-compliant
enumeration claim therefore remains unverified until the device is observed on
a USB host.

## Validation needed

Capture and retain:

1. USB device/configuration descriptors for the controller and receiver.
2. USB Host logs showing the selected interface, alternate setting, endpoint,
   transfer type, and successful transfer completion.
3. Computer USB enumeration showing an Audio/MIDIStreaming function.
4. A MIDI monitor trace for note, CC, program change, pressure, pitch bend,
   realtime, and SysEx cases.
5. Disconnect/reconnect tests with transfer resources checked for leaks.
