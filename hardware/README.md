# Hardware evidence

The recovered repository contains no photographs, schematic, PCB/CAD files,
bill of materials, wiring table, or measurement logs. Hardware facts below are
classified by evidence source.

| Item | Evidence |
| --- | --- |
| ESP32-S2 used for both main USB/MIDI roles | Source/build-confirmed by the generic `esp32s2` variant in all four integrated candidates |
| ESP32-C3 Super Mini used in pairing/OLED experiments | Source/build-confirmed by the recovered variant |
| SSD1306 128×64 at I2C address `0x3C` | Source-confirmed in two C3 host experiments |
| OLED SDA 8 / SCL 9 | Source-confirmed in those C3 experiments only |
| S2 LED 15 and button 0 | Source-confirmed in integrated S2 firmware |
| C3 LED 1 and button 2 | Source-confirmed in host UI/pairing experiments; comments sometimes name different GPIOs |
| LiPo around 1000 mAh, TP4056, IP5310, soldered prototype wiring | Supplied historical context; not corroborated by recovered files |
| Exact USB connectors, VBUS switching, grounds, protection, charging path, and battery wiring | Unknown |

See [components](components.md), [assembly](assembly.md), [wiring](wiring.md),
and [power](power.md). None of these pages is a verified construction guide.
