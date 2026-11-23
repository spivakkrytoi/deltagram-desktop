#!/usr/bin/env python3
"""Package a native Telegram Desktop theme, preserving upstream palette keys."""
from pathlib import Path
import re
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "Telegram/Resources/night.tdesktop-theme"
OVERRIDES = ROOT / "deltagram/theme-overrides.txt"
OUTPUT = ROOT / "deltagram/DeltaGram-Graphite-Rose.tdesktop-theme"
LINE = re.compile(r"^([A-Za-z][A-Za-z0-9]*):\s*(.*?);(.*)$")
COLOR = re.compile(r"#[0-9a-fA-F]{6}(?:[0-9a-fA-F]{2})?$")

with zipfile.ZipFile(SOURCE) as upstream:
    palette = upstream.read("colors.tdesktop-theme").decode("utf-8")
    background = upstream.read("background.png")

overrides = {}
for line in OVERRIDES.read_text().splitlines():
    if not line.strip() or line.lstrip().startswith("#"):
        continue
    match = LINE.fullmatch(line)
    if not match or not COLOR.fullmatch(match.group(2)):
        raise ValueError(f"Invalid theme override: {line!r}")
    if match.group(1) in overrides:
        raise ValueError(f"Duplicate key: {match.group(1)}")
    overrides[match.group(1)] = match.group(2)

seen = set()
lines = []
for line in palette.splitlines(keepends=True):
    match = LINE.match(line.rstrip("\r\n"))
    if match and match.group(1) in overrides:
        key = match.group(1)
        lines.append(f"{key}: {overrides[key]};{match.group(3)}\n")
        seen.add(key)
    else:
        lines.append(line)
missing = overrides.keys() - seen
if missing:
    raise ValueError(f"Unknown upstream palette keys: {sorted(missing)}")

with zipfile.ZipFile(OUTPUT, "w", compression=zipfile.ZIP_DEFLATED) as theme:
    theme.writestr("colors.tdesktop-theme", "".join(lines).encode("utf-8"))
    theme.writestr("background.png", background)
print(f"{OUTPUT}: {len(seen)} overrides, {len(lines)} palette lines")
