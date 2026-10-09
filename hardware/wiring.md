# Wiring evidence and unknowns

## Source-confirmed logical assignments

| Branch | Signal | GPIO / setting |
| --- | --- | --- |
| S2 integrated candidates | status LED | 15 |
| S2 transmitter candidates | active-low button | 0 |
| C3 host UI/BLE experiments | status LED | 1 |
| C3 host UI/BLE experiments | active-low button | 2 |
| C3 OLED experiments | SDA | 8 |
| C3 OLED experiments | SCL | 9 |
| C3 OLED experiments | I2C address | `0x3C` |
| C3 device BLE experiment | LED / button | 0 / 1 |

The early serial test configures button GPIO 2 but prints an instruction naming
GPIO 3. Several source comments similarly say “GPIO2” or “GPIO3” next to
constants 1 and 2. The constants are the executable evidence; the physical
wiring remains unverified.

## USB wiring

The S2 transmitter requires the controller to see a powered USB host. The
source does not define VBUS enable, current limiting, over-current detection,
connector pinout, or whether an external PHY/power switch was used. The S2
receiver intends the native USB data pins/connector, not merely a USB-to-UART
programming port.

## Not established

There is no evidence-backed netlist for battery, charger, IP5310, 5 V USB VBUS,
3.3 V regulation, OLED power, buttons, or LEDs. Do not turn this page into a
construction diagram without inspecting the original assembly or measuring the
boards.
