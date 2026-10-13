# Night sky data

`default-site.txt` samples David Lorenz's 2025 [Light Pollution Atlas](https://djlorenz.github.io/astronomy/lp/)
at Yard's default coordinates, retrieved September 14, 2026. Its value is the
light pollution index (LPI): artificial zenith brightness divided by the natural
zenith brightness of 22.0 mag/arcsec². It is **not a Bortle class**, ground
illumination, or a fixture measurement. Here LPI is 5.44, so the whole sky is
about 6.44 times as bright as the natural reference. `src/skyglow.cpp` compiles
the same values in as the default site.

The atlas models clear-sky zenith brightness from annual cloud-free VIIRS
nighttime lights (Earth Observation Group, Colorado School of Mines / NOAA). See
its [quantities and color scale](https://djlorenz.github.io/astronomy/lp/colors.html)
and the author's [interactive map](https://djlorenz.github.io/astronomy/lp/overlay/dark.html).

`tools/fetch-skyglow.py` samples the atlas's numeric 5° tiles, not the map
colors, taking the 1/120° cell that contains the site. To reproduce the default
(network required):

```sh
python3 tools/fetch-skyglow.py --latitude 32.8908277 --longitude -84.3271342 --year 2025 --output data/default-site.txt
```

`--tile FILE.dat.gz` reads an already downloaded tile instead, trusting that it
matches the coordinates and year. Each profile records its source URL, tile
SHA-256 and cell. Builds and app launches never fetch.

A site profile's first line is

```
YARD_SKYGLOW_V1 latitude longitude atlas-year artificial-to-natural-ratio
```

followed only by `#` comments or blank lines. Latitude must be in [-65, 75) and
longitude in [-180, 180].

Skyglow is rendered gray, at the atlas ratio times the natural sky's luminance,
and fades in as the sun drops from -6° to -18°. The atlas year stays fixed as
simulation time advances. There are no light domes, lamp spectra, clouds or
moonlight, and brightness is relative, not lux-calibrated.
