# Physical assembly

Historical context says the adapter was assembled as a soldered prototype with
an ESP32 board, USB connections, buttons/LEDs, display in some experiments, and
battery/power modules. No photographs or assembly notes were available in the
materials inspected for this reconstruction.

Before recreating hardware, document at minimum:

1. board vendor and revision for each ESP32;
2. native-USB versus UART/programming connector used on each board;
3. controller-facing USB receptacle and VBUS source/switching;
4. common-ground topology and signal voltage levels;
5. button/LED polarity and resistor values;
6. OLED module voltage, address, and physical pin order;
7. battery connector polarity, protection, charger, load-sharing, and power
   switch behavior.

The firmware's GPIO constants are historical evidence, not a complete assembly
definition. Validate them against the exact board silkscreen and schematic.
