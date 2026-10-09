# Reconstructed development history

This history is inferred from source scope, cache modification times, and build
metadata. It intentionally avoids claiming dates of hardware milestones or
successful tests.

1. **C3 input and state experiments (July 30, 2025 cache records).** A button
   serial sketch and two near-identical LED/pairing-state sketches explored
   press/hold behavior, timeouts, and visible states. Their logic contains pin
   comment mismatches and a debounce defect.
2. **OLED and BLE-assisted pairing (July 31).** An SSD1306 prototype added
   centered status text. A device BLE server and host BLE scanner exchanged a
   host Wi-Fi MAC and persisted pairing records. The later host experiment added
   ESP-NOW unpair reception, but deinitialized ESP-NOW without restoring it and
   confused BLE and Wi-Fi peer addresses.
3. **Basic ESP-NOW peer experiments (August 1).** Two C3 sketches broadcast
   `on`/`off` strings and print source/destination MACs. They differ mainly in
   LED/button pins. The `INPUT_PULLUP` button test is inverted, so the nominal
   send action occurs when the input is high.
4. **S2 hardware smoke tests (August 4–9).** A GPIO 15 blink sketch and broader
   S2 GPIO/button diagnostic show a transition toward the USB-capable board.
   The diagnostic's readback cannot by itself establish whether a GPIO is
   electrically healthy.
5. **USB host and MIDI transport candidates (August 9).** `DEVICE_ESP3.0` and
   `HOSTESP3.0` form protocol B and attempt descriptor-driven USB host input plus
   Control Surface USB-MIDI output. Later cache timestamps for the more verbose
   `HOST_ESP2.0` / `DEVICE_ESP2.0` pair show protocol A, extensive LED/logging,
   connection-test messages, and serial forwarding. The “2.0” and “3.0” labels
   therefore do not establish a simple chronological or quality ranking.

Every sketch has an object file and linked binary in the recovered cache. That
shows that its historical source compiled and linked in the recorded Arduino
environment. It does not show that USB enumeration, ESP-NOW exchange, MIDI
delivery, OLED behavior, or battery operation succeeded on physical hardware.

No Git history existed for these artifacts. This repository is a transparent
reconstruction and does not fabricate earlier commits.
