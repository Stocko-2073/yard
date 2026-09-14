# Night sky data

`default-site.txt` is a nearest-cell sample of David Lorenz's **2025 Light
Pollution Atlas**, retrieved September 14, 2026. The numeric value is artificial
zenith brightness divided by natural zenith brightness (LPI), **not a Bortle
class**, direct ground illumination, or a local light fixture measurement.
At 32.8908277, -84.3271342 the LPI is 5.44249805476, so total natural plus
artificial zenith brightness is about 6.44 times the atlas's natural reference.
The compiled default in `src/skyglow.c` is checked against this profile by tests.

Sources and attribution:

- [David Lorenz, Light Pollution Atlas](https://djlorenz.github.io/astronomy/lp/):
  model of atmospheric light transfer using annual cloud-free VIIRS nighttime
  lights from the Earth Observation Group, Colorado School of Mines / NOAA.
- [Atlas quantities and color scale](https://djlorenz.github.io/astronomy/lp/colors.html):
  LPI uses a natural reference of 22.0 mag/arcsec²; it is distinct from Bortle class.
- [Author's interactive map and numeric decoder](https://djlorenz.github.io/astronomy/lp/overlay/dark.html).
- [Original numeric tile](https://djlorenz.github.io/astronomy/binary_tiles/2025/binary_tile_20_20.dat.gz).
  Its SHA-256 and sampled cell are recorded in the profile. The atlas estimates
  clear-sky zenith brightness; actual conditions and lighting can differ.

The importer samples the numeric grid, not map colors. Tiles span 5° × 5° with
600 × 600 cells at 1/120°. Cell order runs west to east and south to north.
The gzip payload contains 360001 signed bytes: the first value is 128*a[0]+a[1];
each subsequent row starts with a delta from the preceding row's first value,
and other values are longitude deltas. The author's conversion is
`LPI = (5/195) * (exp(0.0195*value) - 1)`. Yard selects the containing cell,
equivalent to the nearest cell center, with explicit handling of tile edges
and longitude wrapping. It does not interpolate across tiles.

To reproduce the bundled profile (Python 3, network required):

```sh
python3 tools/fetch-skyglow.py --latitude 32.8908277 --longitude -84.3271342 --year 2025 --output data/default-site.txt
```

If updating this sample, also update the compiled constant in `src/skyglow.c`.
For offline reproduction, download the source tile and pass `--tile FILE.dat.gz`;
that option trusts that the supplied tile matches the requested coordinates/year.
The numeric atlas has no versioned checksum manifest; each profile records the
retrieved tile hash for provenance. Normal builds and app launches never fetch.

A site profile's first line is:

```
YARD_SKYGLOW_V1 latitude longitude atlas-year artificial-to-natural-ratio
```

Subsequent lines are `#` provenance comments or empty lines. Supported coordinates
are latitude [-65,75) and longitude [-180,180]. Download failures, invalid data,
and unsupported coordinates fail explicitly; no arbitrary skyglow is substituted.

The renderer adds achromatic artificial skyglow with luminance `LPI` times its
existing blue natural-sky floor. This preserves the modeled luminance ratio
without inventing a lamp spectrum. Both contributions fade smoothly from full
strength at solar elevation -18° to zero at -6°. Surface ambient uses π times
the sky radiance, the uniform-hemisphere irradiance approximation, followed by
the existing normal-dependent ambient factor. The sky and distant haze receive
the same radiance. Solar scattering and directional sunlight are unchanged.

This is relative lighting passed through the configurable manual camera exposure
(see [camera model](../design/CAMERA.md)), not a lux-calibrated sensor model. The atlas supplies no directional light domes, lamp spectra, cloud
amplification, or lunar contribution. A site's atlas year stays fixed while
simulation time advances; it is not a historical or future lighting prediction.
