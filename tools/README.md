# Recovery tools

`recover_arduino_source.py` reconstructs an `.ino` file from Arduino's
generated `.ino.cpp` translation unit. It removes the generated `Arduino.h`
include, generated `#line` directives, and the recognizable prototype block.
It does not reformat or intentionally change implementation logic.

Example:

```powershell
python tools/recover_arduino_source.py recovered.ino.cpp recovered.ino `
  --snapshot-output archived.generated.cpp
```

The optional snapshot retains the generated translation unit but replaces
machine-specific paths in `#line` directives with
`<recovered-sketch-path>`.
