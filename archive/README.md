# Recovery archive

`recovered-sources/` contains all 14 recovered Arduino-generated translation
units. Their source logic and generated prototypes are retained, but absolute
local paths in `#line` directives were replaced with
`<recovered-sketch-path>` to avoid publishing machine-specific information.

`artifact-manifest.csv` records names, sizes, local cache timestamps, and
SHA-256 hashes for the original `.ino.cpp`, `.d`, `.o`, linked firmware, map,
and build-metadata artifacts. Generated binaries and object files are not
committed because they are reproducible build outputs and would add roughly
hundreds of megabytes.

`build-environments.csv` records the exact recovered FQBN, Arduino-ESP32 core,
board variant, and cache identifier for each sketch. Cache timestamps establish
the order of these recovered builds, not necessarily the order in which the
original sketches were authored or tested.

The clean `.ino` counterparts are under `firmware/`. The recovery procedure is
implemented in `tools/recover_arduino_source.py`.
