#!/usr/bin/env python3
"""Fail on common reconstruction and publication mistakes."""

from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
TEXT_SUFFIXES = {".md", ".ino", ".py", ".csv", ".cpp", ".h"}
FORBIDDEN = [
    re.compile(r"C:\\Users\\", re.IGNORECASE),
    re.compile(r"AppData[\\/]", re.IGNORECASE),
    re.compile(r"New OneDrive", re.IGNORECASE),
]


def main() -> int:
    errors: list[str] = []
    sketches = list((ROOT / "firmware").rglob("*.ino"))
    snapshots = list((ROOT / "archive" / "recovered-sources").glob("*.cpp"))
    if len(sketches) != 14:
        errors.append(f"expected 14 clean sketches, found {len(sketches)}")
    if len(snapshots) != 14:
        errors.append(f"expected 14 generated snapshots, found {len(snapshots)}")

    for path in ROOT.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in TEXT_SUFFIXES:
            continue
        relative = path.relative_to(ROOT)
        if relative.parts[0] in {".build", ".git"} or path == Path(__file__).resolve():
            continue
        text = path.read_text(encoding="utf-8")
        for pattern in FORBIDDEN:
            if pattern.search(text):
                errors.append(f"machine-specific path in {relative}")
                break
        if path.suffix == ".ino":
            if "#line " in text:
                errors.append(f"generated #line remains in {relative}")
            if "void setup(" not in text or "void loop(" not in text:
                errors.append(f"missing setup/loop in {relative}")
        if path.suffix == ".md":
            for target in re.findall(r"\[[^]]+\]\(([^)]+)\)", text):
                if target.startswith(("http://", "https://", "#", "mailto:")):
                    continue
                local_target = target.split("#", 1)[0]
                if local_target and not (path.parent / local_target).resolve().exists():
                    errors.append(f"broken Markdown link in {relative}: {target}")

    if errors:
        print("Repository audit failed:")
        for error in errors:
            print(f"- {error}")
        return 1
    print(f"Repository audit passed: {len(sketches)} sketches, {len(snapshots)} snapshots")
    return 0


if __name__ == "__main__":
    sys.exit(main())
