# Yard

A virtual yard for your yard robot to tend.

Yard aims to be a rich, active simulation of a yard, with optional photorealistic
appearance. The environment is intended to evolve over time and respond to the
work robots do in it.

The planned environment includes:

- Grass, weeds, and overgrowth.
- Trees, shrubs, mulch, and autumn leaves.
- Fences, pools, and concrete pads.
- Building exteriors.
- Garden sheds, including their interiors.

## Running the scaffold on macOS

The application and native tests use C++20; the Sokol implementation uses
Objective-C with ARC. Requires Apple's command-line developer tools (`xcode-select --install`) or
Xcode, and an Apple Silicon Mac with Metal support. Install the pinned shader
compiler once with `make setup-tools` (requires network). Subsequent builds work
offline; the Sokol headers are vendored.

```sh
make setup-tools
make
make run
```

The app opens a resizable Metal window with an atmospheric sky, date-driven sun
and moon, a lit cube, and a ground plane with a metre grid and a directional
sun shadow. The default camera produces a fixed **800×600, 4:3** image at up to **30 fps**,
matching the OV2640's SVGA output dimensions and maximum nominal frame rate
([sensor datasheet](https://files.waveshare.com/wiki/common/OV2640DS_en.pdf)).
The window starts at 800×600 logical pixels. Resizing letterboxes the same camera
image; Retina displays magnify it without increasing sensor resolution. Preview
scaling uses nearest-neighbor sampling so individual sensor pixels remain visible.

The medium-term target module is the original **XIAO ESP32-S3 Sense with OV2640**.
Its built-in camera profile defaults to SVGA; profiles keep other cameras configurable.
[Seeed's documentation](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/#for-seeed-studio-xiao-esp32-s3-sense-camera)
provides the OV2640 sensor datasheet, but a stock-lens horizontal/vertical FOV
could not be verified there. The projection therefore retains an uncalibrated **60° vertical FOV** (75.18° horizontal at
4:3). Set `--vfov DEGREES` to supply a measured lens FOV; accepted values are 1–170°.
For a centered, flat target filling the image vertically, measure its height H
and perpendicular distance D: vertical FOV = 2 atan(H / (2D)). A camera
calibration will also account for lens distortion.

This models resolution, aspect ratio, capture cadence, and manual shutter/gain
response. Lens distortion, automatic exposure/gain, temporal shutter integration,
rolling shutter, Bayer sampling, noise, and JPEG/RGB565 output are not simulated. The camera target is single-sample
RGBA8, with no MSAA. Sun and moon retain their physical angular sizes.

The default location is **Thomaston, Georgia, USA**: 32.8908277° N, 84.3271342° W
([US Census city coordinates](https://tigerweb.geo.census.gov/tigerwebmain/Files/acs25/tigerweb_acs25_incplace_2025_bas25_ga.html)).
The simulation starts at the current date/time. The title shows America/New_York calendar
time, EST/EDT, and the moon's illuminated percentage and waxing/waning state.
The internal clock advances in UTC; local display and calendar input use the
system's `America/New_York` timezone database, including daylight saving.

- **Left/Right**: scrub time backward/forward (two simulated hours per second).
- **Space**: pause/resume the clock (one day in four minutes).
- **[ / ]**: step back/forward one local calendar day and pause.
- **W/A/S/D**: move forward/left/back/right relative to the viewing direction,
  along the ground at 3 m/s. **Shift** moves at 9 m/s. Diagonal speed is normalized.
- **Left click**: capture the mouse; move the mouse to look. **Escape** releases
  it; press Escape again while released to quit. Losing focus releases the mouse.
  Movement and mouse look cancel moon tracking and work while time is paused.
  This is viewer navigation at a fixed 2.5 m eye height, without collision handling.
- **M**: toggle moon tracking. A moon below the horizon remains hidden by ground;
  its status appears in the title. Step time forward to see it rise.
- **Z**: toggle 8× digital preview magnification of the captured image; it does
  not change the camera projection or add image detail.
- **1/2/3/4**: select 07:00/noon/19:00/midnight on the current local date and pause.
  These are clock presets, not calculated sunrise/sunset times.
- **R**: reset the view and resume from the current real date/time. **Escape**: release mouse/quit.

Set a date and decimal local hour to start paused, or omit either to use today's
date/current local clock time. Dates from 1900 through 2100 and hours in [0,24)
are supported. Invalid dates and nonexistent local times during the spring DST
transition are rejected. Repeated fall DST times select standard time. Calendar
day stepping preserves the local clock where possible; a spring DST gap is
normalized forward by the operating system.

```sh
./build/yard --date 2026-01-21 --time 19 --moon --zoom  # waxing crescent
./build/yard --date 2026-01-25 --time 19 --moon --zoom  # near first quarter
./build/yard --date 2026-02-01 --time 21 --moon --zoom  # near full moon
./build/yard --date 2026-02-09 --time 5 --moon --zoom   # near last quarter
```

`make test` checks phase fractions against
[US Naval Observatory phase dates](https://aa.usno.navy.mil/calculated/moon/phases?year=2026),
a published position example, and calendar/DST boundaries, without a GUI.
`make smoke-test` captures 120 SVGA frames across new, quarter, full, and last-quarter
moon dates on Metal, then exits; it requires a graphical macOS session and checks
rendering execution, not visual correctness. `make clean` removes build output.

## Procedural geometry

The standalone C++ geometry engine generates indexed meshes with normals and UVs
from cubic Bézier curves, tapered profile sweeps, and planar polygons with holes.
Curve tolerances and profile segment counts control generated detail. Earcut
v2.2.4 is vendored under ISC for polygon triangulation; builds remain offline.
See [the library comparison, API, limitations, and measurements](design/GEOMETRY.md).
Tree generation is the next stage and is not implemented yet.

```sh
./build/yard --geometry-demo --date 2026-09-15 --time 12
make geometry-bench
```

The demo adds a curved sweep and holed panel with UV checkers. The default cube
also uses generated polygon meshes. Smoke mode includes all three; only the cube
has an analytic ground shadow. `make test` includes CPU geometry tests.

## Camera exposure

Manual controls follow the OV2640's line-based shutter and Espressif gain steps.
Use **comma/period** to decrease/increase shutter, and **minus/equals** to change
gain. The title shows effective milliseconds and gain multiplier. These controls
work while time is paused and affect the captured image before 8-bit conversion.

```sh
# Manual night starting point: about 33.3 ms at 16x gain
./build/yard --date 2026-01-21 --time 0 --aec-value 672 --agc-gain 15
# Physical-unit equivalent
./build/yard --date 2026-01-21 --time 0 --exposure-ms 33.333333 --gain 16
```

The default is about 10 ms at 1×. There is **no automatic exposure** yet; adjust
settings when moving between day and night. At nominal 30 fps shutter is capped
at one frame (33.3 ms). Hardware line timing and absolute sensitivity remain
uncalibrated; high gain currently adds no noise or motion blur.

`--camera-profile FILE` configures dimensions, frame rate, shutter timing/limits,
and gain steps for other cameras. The default stays OV2640 SVGA. See
[camera controls, sources, and profile format](design/CAMERA.md).

## Location-dependent night lighting

Night ambient uses David Lorenz's [2025 Light Pollution Atlas](https://djlorenz.github.io/astronomy/lp/).
The bundled sample at the default coordinates has artificial zenith brightness
**5.44× the natural sky** (6.44× total). Sky, ground, and cube share this light;
it fades out through astronomical twilight. The app remains offline.

To use another location, download a site profile once with Python 3, then load it:

```sh
python3 tools/fetch-skyglow.py --latitude 33.749 --longitude -84.388 --output /tmp/atlanta-site.txt
./build/yard --site /tmp/atlanta-site.txt --date 2026-01-21 --time 0
```

`--site` sets latitude, longitude, and skyglow together, including the location
used by the sun and moon. The title displays coordinates. Calendar input/display
still uses **America/New_York**, including for sites in other time zones.
The atlas covers 65° S through just below 75° N at about 1 km resolution;
profiles use the nearest grid cell. There is no network lookup during rendering.

Atlas values describe modeled clear-sky brightness at zenith, not Bortle class
or direct lamp illumination. Relative luminance is data-driven; absolute sensitivity,
light color, and the uniform-hemisphere ambient approximation remain uncalibrated.
Moonlight on surfaces, directional city light domes, clouds, and changes since the
atlas year are not modeled. See [data provenance and reproduction](data/README.md).

## Sky model

Sun and moon positions use a small, offline orbital model with lunar perturbations
from [Paul Schlyter's astronomical formulas](https://stjarnhimlen.se/comp/ppcomp.html).
The observer's position on an oblate Earth is subtracted to account for lunar
parallax. The moon's distance sets its angular size, and the Sun–Moon–observer
geometry determines its phase and terminator orientation in the local sky.
World axes are east +X, up +Y, north -Z, preserving a right-handed camera frame.
The disk is shaded as a sphere behind atmospheric extinction; it can appear in
daylight and is hidden by the ground when below the horizon.

This is a visual ephemeris, not an observatory-grade prediction: UTC approximates
UT1/TT; atmospheric refraction, eclipses, lunar libration, and terrain elevation
are not modeled. Moon surface variation is procedural, not a surveyed texture.
Its display brightness is artistic; it does not yet illuminate yard surfaces.

The sky integrates single Rayleigh and Mie scattering through a spherical,
exponentially thinning atmosphere, following the approach described in
[NVIDIA GPU Gems 2, chapter 16](https://developer.nvidia.com/gpugems/gpugems2/part-ii-shading-lighting-and-shadows/chapter-16-accurate-atmospheric-scattering).
The solar disk, atmospheric extinction, and directional light share one sun
position. Colors are lit in linear space, multiplied by camera shutter/gain, tone mapped, and
converted to sRGB. Ambient skylight and distant ground haze are approximations;
clouds, multiple scattering, stars, and photometric calibration are not implemented.
The ground shadow uses an analytic intersection with the stationary demo cube;
a general scene will need a scene shadow system. The sky integrates 16 view
samples with eight sun samples each per fragment; a cached sky lookup texture
is a possible optimization as the yard grows.

- `src/main.cpp`: application lifecycle, geometry demo, rendering, and input.
- `src/geometry.cpp`: curve sampling, profile sweeps, and polygon tessellation.
- `src/astronomy.cpp`: sun/moon ephemeris and local calendar conversion.
- `src/skyglow.cpp`: offline site profiles and location-dependent night lighting.
- `src/camera.cpp`: camera profiles, manual exposure, and gain response.
- `tools/fetch-skyglow.py`: explicit numeric atlas download and site sampling.
- `tests/astronomy_test.cpp`: astronomy reference and calendar regression checks.
- `src/sokol.m`: Sokol implementation compiled as Objective-C for macOS/Metal.
- `shaders/cube.glsl`: portable annotated GLSL, compiled by `sokol-shdc`.
- `build/generated/cube.glsl.h`: generated shader sources, uniform types, and
  binding declarations; do not edit or commit.
- `tools/setup-shdc.sh`: explicit compiler download pinned by commit and SHA-256.
- `vendor/earcut/`: unmodified Earcut v2.2.4 header, ISC license, and revision.
- `vendor/sokol/`: Sokol headers and upstream license, pinned to commit
  `c0db757ea10cbe40aa8398aa378b2b5aae0278b2` (also recorded in `REVISION`).

The application build currently targets macOS with Metal. Shader generation emits
Metal (macOS, iOS, simulator), HLSL5, GLSL430, GLSL300ES, and WGSL from the same
source. Other platforms still need application build configuration and runtime
verification. GLSL300ES supports these graphics shaders, not compute.

`make` automatically regenerates the shader header when its source changes;
`make shaders` generates it without building the app. Shader code uses depth
[0, 1] and `@glsl_options fixup_clipspace` for OpenGL's [-1, 1] convention.
The app consumes generated attribute/binding constants and uniform structs.

The shader compiler comes from `floooh/sokol-tools-bin` at revision
`11d0cf678105d614d675e6d9bd2aaf3eeff12f8c`. The bootstrap currently installs the
Apple Silicon macOS binary under ignored `tools/bin/`; `make clean` preserves it.
See [tools/README.md](tools/README.md) for the upgrade workflow.

Sokol and shader compiler upgrades should be explicit and checked together:
regenerate shaders, build, and run the app to catch API or binding changes.
