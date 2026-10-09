# Pairing and reconnection

## ESP-NOW channel-6 state machines

The host-side sketches start in pairing when no NVS peer exists and broadcast
once per second. A transmitter in pairing stores the announced MAC, registers
the peer, and replies. Both sides then send periodic heartbeats and move to a
reconnecting state after a timeout.

Protocol A uses three-second heartbeats, a 25-second timeout, and retry-count
based peer deletion (roughly 30 seconds on the transmitter and 45 seconds on
the receiver). Protocol B uses two-second heartbeats, a 15-second timeout, and
a 60-second elapsed-time window before deleting the peer.

These state machines do not authenticate a peer. The MAC carried inside a
message is trusted in several paths instead of consistently using the ESP-NOW
callback source address. Broadcast announcements can therefore select an
unexpected device when multiple units pair at once.

## Persistence

The S2 pairs use Preferences namespace `pair`, key `peer`. The C3 BLE branch
uses namespace `pairing`, with `hostmac` on the device and `mac` on the host.
These formats are unrelated.

No schema version or migration logic exists. Reflashing between branches can
leave stale records whose meaning differs.

## BLE-assisted C3 experiment

The device advertises a custom service and accepts a textual host Wi-Fi MAC.
The host scans for that service, connects, and writes its Wi-Fi MAC. The source
then stores the advertised BLE address as the device ESP-NOW address. The code
does not demonstrate that the BLE and Wi-Fi station addresses are identical.

`startBLEPairing()` calls `esp_now_deinit()` to free the shared radio. After the
BLE exchange, the host calls `esp_now_add_peer()` without first calling
`esp_now_init()` and without re-registering callbacks. That is a blocking defect
in the recovered flow. The source also declares channel 1 in peer records but
does not explicitly set the Wi-Fi station channel to 1.

## Hardening direction

A future, separate implementation should define a versioned byte protocol,
derive peer identity from callback metadata, authenticate or physically gate
pairing, distinguish discovery from an established session, retain sequence
numbers, and test recovery from reboot, radio loss, peer replacement, and NVS
corruption. Those changes are recommendations, not claims about recovered code.
