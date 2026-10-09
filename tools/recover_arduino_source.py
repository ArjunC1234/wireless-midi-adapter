#!/usr/bin/env python3
"""Recover an Arduino sketch from an Arduino-generated .ino.cpp file.

The Arduino preprocessor adds an Arduino.h include, #line directives, and a
block of function prototypes. This tool removes only those generated pieces.
It can also write a generated-source snapshot whose #line paths are redacted.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path


LINE_DIRECTIVE = re.compile(r'^\s*#line\s+(\d+)\s+"[^"]*"\s*$')


def generated_prototype_span(lines: list[str]) -> tuple[int, int] | None:
    """Return the inclusive generated-prototype span, if recognizable."""
    directives: list[tuple[int, int]] = []
    for index, line in enumerate(lines):
        match = LINE_DIRECTIVE.match(line)
        if match:
            directives.append((index, int(match.group(1))))

    # Ignore the initial #line 1 that marks the start of original sketch text.
    seen: dict[int, int] = {}
    for index, number in directives[1:]:
        if number in seen:
            return seen[number], index
        seen[number] = index
    return None


def recover(text: str) -> str:
    lines = text.splitlines()
    span = generated_prototype_span(lines)
    if span:
        start, end = span
        lines = lines[:start] + lines[end + 1 :]

    output: list[str] = []
    removed_generated_include = False
    for line in lines:
        if LINE_DIRECTIVE.match(line):
            continue
        if not removed_generated_include and line.strip() == "#include <Arduino.h>":
            removed_generated_include = True
            continue
        output.append(line)

    while output and output[0] == "":
        output.pop(0)
    return "\n".join(output).rstrip() + "\n"


def redact_generated_snapshot(text: str) -> str:
    return re.sub(
        r'(?m)^(\s*#line\s+\d+\s+)"[^"]*"\s*$',
        r'\1"<recovered-sketch-path>"',
        text,
    ).rstrip() + "\n"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("clean_output", type=Path)
    parser.add_argument("--snapshot-output", type=Path)
    args = parser.parse_args()

    text = args.source.read_text(encoding="utf-8")
    args.clean_output.parent.mkdir(parents=True, exist_ok=True)
    args.clean_output.write_text(recover(text), encoding="utf-8", newline="\n")

    if args.snapshot_output:
        args.snapshot_output.parent.mkdir(parents=True, exist_ok=True)
        args.snapshot_output.write_text(
            redact_generated_snapshot(text), encoding="utf-8", newline="\n"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
