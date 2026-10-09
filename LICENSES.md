# Licensing and provenance

No repository-wide open-source license has been applied. The recovered
firmware did not include a license notice, and two ESP-NOW experiments credit
“DroneBot Workshop 2022 (Adapted by ChatGPT 2025)” without enough provenance
to determine reuse terms. Until ownership and source provenance are reviewed,
the recovered project code should be treated as **all rights reserved**.

Third-party libraries are dependencies only; their source is not vendored:

| Dependency in recovered build | Recovered version | License observed in the installed copy |
| --- | ---: | --- |
| Control Surface by Pieter P | 2.0.0 | GNU GPL v3 |
| Adafruit SSD1306 | 2.5.15 | BSD license |
| Adafruit GFX Library | 1.12.1 | BSD license |
| Adafruit BusIO | 1.17.2 | MIT license |
| Arduino-ESP32 | 3.0.5 and 3.3.0 build records | Upstream project terms; not copied here |

The dependency versions above come from recovered `.d` files and the local
library manifests that were available during reconstruction. They are not a
promise that other versions are compatible.
