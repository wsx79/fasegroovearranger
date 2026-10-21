#!/usr/bin/env python3
"""Convert the existing Arranger brand into the family's installer asset sizes."""
# Copyright (C) 2026 Fabio 'Pitch' Serra
# SPDX-License-Identifier: AGPL-3.0-or-later
from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parent
logo = Image.open(root.parents[1] / 'Resources/arranger.png').convert('RGBA')
logo.save(root / 'FaseGrooveArranger.ico', sizes=[(s, s) for s in [16, 24, 32, 48, 64, 128, 256]])
for scale in [1, 2]:
    large = Image.new('RGB', (164 * scale, 314 * scale), '#17111c')
    icon = logo.resize((96 * scale, 96 * scale), Image.Resampling.LANCZOS)
    large.paste(icon, (34 * scale, 58 * scale), icon)
    for x in range(large.width):
        k = x / max(1, large.width - 1)
        colour = tuple(round(a + (b - a) * k) for a, b in zip((255, 179, 92), (255, 122, 61)))
        for y in range(309 * scale, 314 * scale):
            large.putpixel((x, y), colour)
    small = Image.new('RGB', (55 * scale, 58 * scale), '#17111c')
    icon = logo.resize((48 * scale, 48 * scale), Image.Resampling.LANCZOS)
    small.paste(icon, (4 * scale, 5 * scale), icon)
    suffix = '-2x' if scale == 2 else ''
    large.save(root / f'wizard-large{suffix}.bmp')
    small.save(root / f'wizard-small{suffix}.bmp')
print('PASS Arranger icon and family installer bitmaps at 1x/2x')
