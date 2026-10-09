# Host-side validation

Run from the repository root:

```powershell
python -m unittest discover -s tests -v
```

The tests model the two recovered ESP-NOW packet ABIs and selected USB-MIDI
CIN handling. They are simulations and static compatibility checks, not radio,
USB, latency, DAW, or battery tests.
