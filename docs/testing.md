# Validation record

Validation levels are intentionally separate. A build result is not a hardware
or end-to-end result.

## Historical cache evidence

All 14 recovered sources have matching `.o`, linked `.elf`, `.bin`, merged
image, map, and partition artifacts in the Arduino cache. SHA-256 hashes and
sizes are recorded in `archive/artifact-manifest.csv`; binaries are not
committed. This establishes historical compilation/linking in the environments
recorded by `archive/build-environments.csv`.

## Reconstruction rebuilds

Rebuild host: Windows, Arduino CLI 1.2.0 bundled with Arduino IDE,
Arduino-ESP32 3.0.5, recovered user libraries, October 9, 2026.

| Reconstructed sketch | Result | Flash | Global RAM |
| --- | --- | ---: | ---: |
| `device-esp2-recovered` | Pass | 900,050 bytes (68%) | 36,952 bytes (11%) |
| `device-esp3-recovered` | Pass | 912,342 bytes (69%) | 49,048 bytes (14%) |
| `host-esp2-serial-recovered` | Pass | 882,306 bytes (67%) | 48,992 bytes (14%) |
| `host-esp3-usb-midi-recovered` | Pass after using complete cached Control Surface archive | 960,406 bytes (73%) | 49,960 bytes (15%) |

The first USB-MIDI receiver rebuild stopped at
`AH/Arduino-Wrapper.h: No such file or directory`: the installed Control
Surface directory contained incomplete/broken linked subdirectories. A complete
`Control_Surface-2.0.0.zip` from Arduino's existing staging cache was extracted
into the ignored `.build` directory and selected with `--library`; the retry
compiled successfully. The user's installed library was not modified and no
network download was used.

The C3 experiments were not rebuilt because the active CLI installation
registered Arduino-ESP32 3.0.5, while their recovered build metadata uses
3.3.0. Their historical linked outputs remain recorded. No dependency version
was changed merely to force a modern build.

## Host-side simulations

Command:

```powershell
python -m unittest discover -s tests -v
```

Result: 6/6 tests passed. They verify the two 16-byte layouts, demonstrate
cross-layout incompatibility, assert important field offsets, and capture the
recovered protocol A CIN behavior including omitted SysEx CIN values.

## Repository audit

Command:

```powershell
python tools/audit_repository.py
```

The audit checks for exactly 14 clean sketches and 14 path-redacted snapshots,
generated `#line` directives in primary firmware, missing `setup()`/`loop()`,
and machine-specific paths in publishable text.

## Not performed

- No firmware was flashed during reconstruction.
- No USB controller was attached or enumerated.
- No ESP-NOW packet was transmitted over hardware.
- No computer observed a USB-MIDI interface.
- No serial-to-MIDI bridge or DAW was exercised.
- No latency, packet loss, reconnect reliability, battery runtime, charging,
  power, or thermal measurement was made.

Those results must not be inferred from successful builds or simulations.
