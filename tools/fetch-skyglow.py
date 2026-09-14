#!/usr/bin/env python3
"""Sample David Lorenz's numeric atlas into an offline Yard site profile."""
import argparse
import gzip
import hashlib
import math
from pathlib import Path
import urllib.request
import zlib


def tile_location(latitude, longitude):
    if not math.isfinite(latitude) or not -65 <= latitude < 75:
        raise ValueError('atlas latitude must be in [-65, 75) degrees')
    if not math.isfinite(longitude) or not -180 <= longitude <= 180:
        raise ValueError('longitude must be in [-180, 180] degrees')
    x = (longitude + 180) % 360
    y = latitude + 65
    tx, ty = math.floor(x / 5), math.floor(y / 5)
    # Cell centers are at half a pixel; containing cell is nearest center.
    col = min(599, math.floor((x - tx * 5) * 120))
    row = min(599, math.floor((y - ty * 5) * 120))
    return tx + 1, ty + 1, col, row


def decode_sample(raw, col, row):
    if len(raw) != 360001 or not 0 <= col < 600 or not 0 <= row < 600:
        raise ValueError('invalid atlas tile or cell')
    a = memoryview(raw).cast('b')
    value = 128 * a[0] + a[1]
    value += sum(a[600 * i + 1] for i in range(1, row + 1))
    value += sum(a[600 * row + 1 + i] for i in range(1, col + 1))
    ratio = (5 / 195) * math.expm1(0.0195 * value)
    if not math.isfinite(ratio) or not 0 <= ratio <= 100000:
        raise ValueError('invalid decoded artificial/natural brightness ratio')
    return ratio


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--latitude', type=float, required=True)
    parser.add_argument('--longitude', type=float, required=True)
    parser.add_argument('--year', type=int, choices=[2016, 2020, 2022, 2023, 2024, 2025], default=2025)
    parser.add_argument('--tile', type=Path, help='use an already downloaded .dat.gz tile instead of network')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        tx, ty, col, row = tile_location(args.latitude, args.longitude)
        url = f'https://djlorenz.github.io/astronomy/binary_tiles/{args.year}/binary_tile_{tx}_{ty}.dat.gz'
        if args.tile:
            compressed = args.tile.read_bytes()
        else:
            with urllib.request.urlopen(url, timeout=30) as response:
                compressed = response.read(2_000_000)
        ratio = decode_sample(gzip.decompress(compressed), col, row)
        profile = (f'YARD_SKYGLOW_V1 {args.latitude:.10f} {args.longitude:.10f} {args.year} {ratio:.12g}\n'
                   '# David Lorenz, Light Pollution Atlas; artificial/natural zenith brightness ratio.\n'
                   f'# Source: {url}\n'
                   f'# Tile SHA-256: {hashlib.sha256(compressed).hexdigest()}\n'
                   f'# Zero-based cell (west to east, south to north): {col}, {row}\n')
        args.output.write_text(profile)
        print(f'{args.output}: artificial skyglow {ratio:.4f} times natural sky brightness')
    except (OSError, EOFError, ValueError, OverflowError, zlib.error) as error:
        parser.exit(1, f'skyglow: {error}\n')


if __name__ == '__main__':
    main()
