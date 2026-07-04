#!/usr/bin/env python3
"""Audit legacy WS2812FX matrix effect metadata.

The legacy engine intentionally contains hard blink, strobe, sparkle and random
effects. This audit keeps those available while preventing them from being
promoted as calm/recommended effects and checks that the UI/backend mode range
stays aligned with the vendor mode table.
"""

from __future__ import annotations

import json
import re
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
MODES_PATH = REPO_ROOT / "lib" / "WS2812FX" / "modes_esp.h"
MODEL_PATH = REPO_ROOT / "interface" / "src" / "lib" / "features" / "system" / "matrix" / "matrixModel.ts"
OUTPUT_PATH = REPO_ROOT / ".pio" / "matrix3d_audit" / "matrix_legacy_effect_audit.json"
EXPECTED_MODE_COUNT = 70


SMOOTH_MODES = {
    0,   # Static
    2,   # Breath
    11,  # Rainbow
    12,  # Rainbow Cycle
    15,  # Fade
    18,  # Running Lights
    40,  # Running Color
    41,  # Running Red Blue
    64,  # Trifade
}

STEPPED_DETERMINISTIC_MODES = {
    3,   # Color Wipe
    4,   # Color Wipe Inverse
    5,   # Color Wipe Reverse
    6,   # Color Wipe Reverse Inverse
    13,  # Scan
    14,  # Dual Scan
    16,  # Theater Chase
    17,  # Theater Chase Rainbow
    30,  # Chase White
    31,  # Chase Color
    33,  # Chase Rainbow
    36,  # Chase Rainbow White
    37,  # Chase Blackout
    38,  # Chase Blackout Rainbow
    43,  # Larson Scanner
    44,  # Comet
    47,  # Merry Christmas
    51,  # Circus Combustus
    52,  # Halloween
    53,  # Bicolor Chase
    54,  # Tricolor Chase
    58,  # ICU
    59,  # Dual Larson
    61,  # Filler Up
    62,  # Rainbow Larson
    67,  # Multi Comet
    69,  # Oscillator
}

INTENTIONAL_HARD_OR_RANDOM_MODES = {
    1,   # Blink
    7,   # Color Wipe Random
    8,   # Random Color
    9,   # Single Dynamic
    10,  # Multi Dynamic
    19,  # Twinkle
    20,  # Twinkle Random
    21,  # Twinkle Fade
    22,  # Twinkle Fade Random
    23,  # Sparkle
    24,  # Flash Sparkle
    25,  # Hyper Sparkle
    26,  # Strobe
    27,  # Strobe Rainbow
    28,  # Multi Strobe
    29,  # Blink Rainbow
    32,  # Chase Random
    34,  # Chase Flash
    35,  # Chase Flash Random
    39,  # Color Sweep Random
    42,  # Running Random
    45,  # Fireworks
    46,  # Fireworks Random
    48,  # Fire Flicker
    49,  # Fire Flicker Soft
    50,  # Fire Flicker Intense
    55,  # TwinkleFOX
    56,  # Rain
    57,  # Block Dissolve
    60,  # Running Random2
    63,  # Rainbow Fireworks
    65,  # Heartbeat
    66,  # Bits
    68,  # Popcorn
}


def parse_mode_names(text: str) -> dict[int, str]:
    names: dict[int, str] = {}
    for match in re.finditer(r'const char name_(\d+)\[\] PROGMEM = "([^"]+)";', text):
        names[int(match.group(1))] = match.group(2)
    return names


def parse_mode_defines(text: str) -> dict[int, str]:
    defines: dict[int, str] = {}
    for match in re.finditer(r"#define\s+(FX_MODE_[A-Z0-9_]+)\s+(\d+)", text):
        defines[int(match.group(2))] = match.group(1)
    return defines


def parse_ui_categories(text: str) -> dict[str, list[int]]:
    start_marker = "export const MATRIX_EFFECT_CATEGORIES"
    end_marker = "export const MATRIX_NATIVE_3D_EFFECT_CATEGORIES"
    start = text.index(start_marker)
    end = text.index(end_marker, start)
    legacy_categories_text = text[start:end]

    categories: dict[str, list[int]] = {}
    for match in re.finditer(
        r"value:\s*'([^']+)'\s*,\s*effectIds:\s*\[([^\]]*)\]",
        legacy_categories_text,
        flags=re.MULTILINE,
    ):
        value = match.group(1)
        raw_ids = match.group(2)
        ids = [int(item) for item in re.findall(r"\d+", raw_ids)]
        categories[value] = ids
    return categories


def classify_mode(mode_id: int) -> str:
    if mode_id in SMOOTH_MODES:
        return "smooth"
    if mode_id in STEPPED_DETERMINISTIC_MODES:
        return "stepped"
    if mode_id in INTENTIONAL_HARD_OR_RANDOM_MODES:
        return "intentional_hard_or_random"
    raise KeyError(mode_id)


def main() -> int:
    modes_text = MODES_PATH.read_text(encoding="utf-8")
    model_text = MODEL_PATH.read_text(encoding="utf-8")

    names = parse_mode_names(modes_text)
    defines = parse_mode_defines(modes_text)
    categories = parse_ui_categories(model_text)
    expected_ids = set(range(EXPECTED_MODE_COUNT))

    errors: list[str] = []
    if set(names) != expected_ids:
        errors.append(f"legacy mode names are not contiguous 0..{EXPECTED_MODE_COUNT - 1}")
    if set(defines) != expected_ids:
        errors.append(f"legacy mode defines are not contiguous 0..{EXPECTED_MODE_COUNT - 1}")

    classified_ids = SMOOTH_MODES | STEPPED_DETERMINISTIC_MODES | INTENTIONAL_HARD_OR_RANDOM_MODES
    if classified_ids != expected_ids:
        missing = sorted(expected_ids - classified_ids)
        extra = sorted(classified_ids - expected_ids)
        errors.append(f"legacy classification mismatch missing={missing} extra={extra}")

    hard_ids = INTENTIONAL_HARD_OR_RANDOM_MODES
    for category_name in ("recommended", "calm"):
        bad = sorted(set(categories.get(category_name, [])) & hard_ids)
        if bad:
            labeled = ", ".join(f"{mode_id}:{names.get(mode_id, '?')}" for mode_id in bad)
            errors.append(f"{category_name} includes hard/random legacy modes: {labeled}")

    report = {
        "mode_count": len(names),
        "categories": {
            "smooth": sorted(SMOOTH_MODES),
            "stepped": sorted(STEPPED_DETERMINISTIC_MODES),
            "intentional_hard_or_random": sorted(INTENTIONAL_HARD_OR_RANDOM_MODES),
        },
        "modes": [
            {
                "id": mode_id,
                "define": defines.get(mode_id),
                "name": names.get(mode_id),
                "audit_class": classify_mode(mode_id),
            }
            for mode_id in sorted(expected_ids)
        ],
        "ui_categories": categories,
        "errors": errors,
    }

    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_PATH.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    for mode in report["modes"]:
        print(f"{mode['id']:02d} {mode['audit_class']:28s} {mode['name']}")
    print(f"wrote {OUTPUT_PATH}")

    if errors:
        for error in errors:
            print(f"ERROR: {error}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
