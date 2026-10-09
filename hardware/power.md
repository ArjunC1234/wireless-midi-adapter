# Power subsystem

The supplied history names an approximately 1000 mAh LiPo cell, a TP4056 charge
module, and IP5310 power-management hardware. The recovered firmware and build
artifacts contain no electrical description or measurement of this subsystem.

Unknowns include:

- whether the cell had integrated protection;
- the TP4056 charge-current resistor and thermal conditions;
- whether charging and load sharing were supported simultaneously;
- IP5310 module topology, configuration, efficiency, and idle behavior;
- how 5 V controller VBUS was generated and current-limited;
- reverse-current paths when USB and battery power were both present;
- grounds, fusing, ESD protection, cable current, and enclosure temperature;
- actual current draw and battery runtime.

Accordingly, no runtime, charge time, output-current capability, or safe wiring
claim is made. Reconstruct the schematic from the physical prototype and the
exact module datasheets before connecting a LiPo cell or USB controller. Use a
current-limited bench supply for first power-up and have the circuit reviewed
by someone qualified for battery-powered hardware.
