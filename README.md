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

The application and native tests use C++20; Sokol uses Objective-C with ARC.
Requires Apple's command-line developer tools (`xcode-select --install`) or
Xcode, and an Apple Silicon Mac with Metal support. Install the pinned shader
compiler once with `make setup-tools` (requires network). Subsequent builds work
offline; the Sokol headers are vendored.

```sh
make setup-tools
make
make run
```

The app opens a resizable Metal window with an atmospheric sky, date-driven sun
and moon, and a static, mildly lumpy voxel yard. A ground plane with a metre
grid extends beyond the yard boundary. The default camera produces a fixed **800×600, 4:3** image at up to **30 fps**,
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
rolling shutter, Bayer sampling, noise, and JPEG/RGB565 output are not simulated.
The camera defaults to **8× spatial supersampling** (about eight times the
pixel count), downsampled into the same fixed RGBA8 image. `--ssaa 1` selects
native-resolution rendering. `--msaa 1|4` controls MSAA independently; it defaults
to 1 for the supersampling experiment. Sun and moon retain their physical angular sizes.

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
  The viewer follows the voxel surface at a default 1.6 m eye height, without
  collision handling. Set `--eye-height METRES` (0.1–10) for other viewpoints.
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

## Static marching-cubes yard prototype

The yard occupies **63.61 × 63.61 m** (4,046.23 m², just under one acre), with
**6,361 × 6,361 × 64 one-centimeter cells** and one byte per cell. The
2,589,588,544-byte dense array remains resident. Each byte now represents density,
with soil above 127.5 and air below it. Density is sampled at cell centers and
encodes the vertical distance to the surface at 16 units/cm, clamped to 0–255.
This retains sub-centimeter surface position within one byte; it is not a material
ID or a Euclidean signed-distance field.

Seeded smooth value noise at 4 m, 1.5 m and 0.55 m scales produces irregular
humps and dips, with heights bounded to 4–60 cm above the slab's y=0 base.
The vertical volume is now 64 cm deep to allow greater relief than the original
32 cm prototype; voxel spacing remains 1 cm. Heights are not rounded to whole
centimeters. Classical **marching cubes** interpolates the 127.5 isosurface from
the density array. The preview samples on a **uniform 4 cm extraction grid**
(`YARD_TERRAIN_MESH_STEP`), with shortened final intervals at volume boundaries.
This keeps the whole-acre mesh manageable; the source volume still has 1 cm
spacing. This is neither a full-resolution 1 cm mesh nor distance-based LOD.

Adjacent cells share indexed vertices through a rolling edge cache. Normals
come from the density gradient over the extraction spacing, interpolated across
triangles for smooth lighting. The soil uses uniform **`#56341B`** sRGB albedo, converted to linear before
lighting and exposure. The previous procedural color patches are absent.
Brightness varies with surface orientation under directional sunlight and ambient
sky lighting; terrain-cast shadows are not yet implemented. The
mesh has 2,781,892 vertices and 5,557,104 triangles (127.3 MiB of GPU geometry)
with static draw regions for frustum culling. There is no editing or physics.
Temporary CPU mesh data is freed after upload. A roughly 154 MiB floating-point
height cache remains for navigation. Startup generation and upload take several
seconds.

### Grass experiment

Each surface voxel column carries **one upright triangle, 5 cm tall and 5 mm
wide**, for **40,462,321 blades** over the acre. Roots are randomly placed within
each centimeter cell's horizontal footprint (up to ±5 mm from its center in X/Z).
Placement is deterministic, with an independent stable hash selecting azimuth.
Root height still uses the cell-center density-derived surface height; both this
and the coarser render mesh approximate the surface at the offset position. The blades are green
and two-sided, with no wind, textures, alpha blending, or crossed billboards.
Both sides receive sunlight in this simple thin-leaf shading model; grass does
not cast shadows on the soil or other blades yet.

Instanced draws reuse a single triangle. The existing navigation heights
supply a 154.4 MiB immutable GPU root buffer; the vertex shader reconstructs X/Z
and orientation from the region and instance ID. Visible regions retain full
grass density, with no distance LOD. Very distant blades are
subpixel in the 800×600 camera and can shimmer during movement.
Randomized roots break up the regular rows of the original cell-centered placement. The initial
full-acre moving-view check measured 17.1 captures/sec on M1 Max/32 GB, down from
29.5 for bare terrain; the 30 fps cap is unchanged.

### Cube baseline and optional grass volume LOD

A light-gray **50 cm cube** sits at the yard center, with its base embedded 8 cm
below the center's soil height. It uses the same sunlight/exposure and opaque depth
testing as the terrain. It casts a sun shadow, but has no collision.

Full triangle grass remains the default. **V** toggles the experimental shallow
volume LOD without moving the camera; the title shows `blades` or `volume LOD`.
`--grass-volume` enables it at startup. The existing **C** culling toggle still works.

```sh
./build/yard --date 2026-09-14 --time 9 --eye-height 0.4
./build/yard --date 2026-09-14 --time 9 --eye-height 0.4 --grass-volume
# Bring the volume close to inspect where it meets the cube:
./build/yard --date 2026-09-14 --time 9 --eye-height 0.4 --grass-volume --lod-start 0.5 --lod-end 1
```

The default transition is **6–18 m horizontal distance** from the camera. Within
that band, blade widths smoothly narrow as the volume contribution increases.
Draw regions entirely beyond the band stop submitting blades; nearby intersecting
regions can still submit degenerate distant blades. `--lod-start` and `--lod-end`
accept finite distances from 0 to 100 m, with end strictly greater than start.
This first experiment uses distance thresholds, not automatic projected-size LOD.

The replacement is a **5 cm density layer above a bilinear 4 cm height field**,
ray-marched at the internal rendering resolution. Extinction derives from 10,000
roots/m², 5 mm blade base width, a triangular height profile, and an approximate
uniform azimuth distribution. Beer–Lambert transmittance combines the layer with
the scene; leaf lighting averages 16 azimuths weighted by projected area under
the current sun and ambient illumination. `--grass-stride` scales volume density,
and `--no-grass` suppresses both representations.

The scene stores linear HDR RGB and forward camera depth in RGBA16F. The volume
stops at the nearest rendered terrain, cube or blade surface, so solid objects
block grass behind them and grass in front can partially cover their lower edges.
Compositing precedes exposure/tone mapping. The volume has no single opaque depth
surface and does not participate in collision, shadow casting, or transparent-object sorting.
It receives object sun shadows during integration.
With 4× MSAA, resolved depth is averaged at mixed-coverage edges; volume occlusion
there is approximate. Default 8× SSAA with MSAA off avoids that depth resolve.

The height texture uses about 9.7 MiB and the additional 2263×1698 RGBA16F target
about 29.3 MiB. These resources stay allocated for instant toggling. Persistent
voxels, grass roots and the existing GPU blade data remain unchanged.

**Limits:** this is a smooth statistical layer, not filtered captures of actual
blade arrangements. It loses individual distant silhouettes and fine density
variation. The height field approximates the original roots and render mesh;
transition coverage/lighting matching is approximate. Empty-space skipping uses
a conservative height-slope bound, followed by <=1 cm integration steps in the
layer. Marches stop at 1,024 steps or 0.2% transmission, so unusually long grazing
paths may under-integrate. There is no wind or grass shadow casting; objects cast shadows onto the layer.

The final 120-capture orbit at 40 cm eye height on M1 Max/32 GB with 8× SSAA
measured **28.4 captures/sec for the cube + blades baseline** and **29.5 with volume
LOD**. Average submitted blades fell from **9.13 million to 0.66 million**;
terrain submission stayed at 1.25 million triangles. These are end-to-end rates
with a **30 fps cap**, not uncapped GPU timings.

### Offscreen geometry culling

Camera-frustum culling is enabled by default. **C** toggles it during a walk;
`--no-culling` disables it at startup. The title shows the current setting.

Terrain indices and grass heights are grouped into **625 static draw regions**,
up to **2.56 m square** (256×256 columns). Each capture tests their axis-aligned
bounds against the fixed sensor's six frustum planes and submits only intersecting
regions. Terrain bounds include complete triangles that cross region boundaries;
grass bounds include random root offsets, blade width and the full 5 cm height.
The root hash still uses the original global voxel ID, preserving every blade's
placement and orientation. Shared terrain vertices remain unchanged.

This adds draw grouping, not streaming or persistent voxel chunks. Dense yard
state and GPU geometry remain resident. It does not reject geometry hidden behind
hills or blades, and intersecting regions may include some offscreen triangles.
The packed height buffer has the same GPU size; its temporary CPU copy is freed
after upload. Sensor FOV and preview zoom remain independent of culling.

On M1 Max/32 GB, the 120-capture full-density orbit at 40 cm eye height with
8× SSAA measured **27.4 captures/sec with culling**, versus **16.6 disabled**.
Average submitted geometry fell from 40.46 to **9.13 million blades** and 5.56 to
**1.25 million terrain triangles** (about 77% fewer). The disabled comparison
retains the new grass draw grouping, so it includes its submission overhead.
These are end-to-end measurements with a 30 fps cap, not GPU stage timings.

```sh
./build/yard --terrain-smoke-test --eye-height 0.4
./build/yard --terrain-smoke-test --eye-height 0.4 --no-culling
```

### Spatial supersampling and rendering diagnostics

The default **`--ssaa 8 --msaa 1`** renders at **2263×1698** internally for an
800×600 camera, then area-filters into the final fixed **800×600 RGBA8** image.
8× refers to approximately eight times the pixel count, not eight times each
dimension. Each dimension is `ceil(sensor_dimension × sqrt(8))`; integer rounding
accounts for the small difference from exactly 8×. The projection keeps the
sensor's nominal aspect and FOV. Window resizing, Retina and preview zoom do not
change either resolution.

The area filter computes source-pixel overlap for each output pixel, including
fractional footprints and image boundaries. Each source sample receives exposure/tone mapping, then is averaged in linear
display light and encoded to sRGB. Scene radiance remains linear HDR RGBA16F
through the optional grass volume composite. The
scene is instantaneous: there is no jitter, frame history, or temporal blending.
Filtering happens after tone mapping, so this is not a calibrated optical/sensor
model. Small grass can still alias, although spatial sampling is denser.

For comparisons:

```sh
./build/yard --terrain-smoke-test --eye-height 0.4 --no-culling --ssaa 8 --msaa 1
./build/yard --terrain-smoke-test --eye-height 0.4 --no-culling --ssaa 1 --msaa 4
./build/yard --terrain-smoke-test --eye-height 0.4 --no-culling --ssaa 1 --msaa 1
```

`--ssaa 1` uses the native camera size. `--msaa 4` can independently add four
coverage samples at the internal rendering resolution, with a hardware resolve
before downsampling. The primary 8× SSAA comparison disables MSAA to isolate
spatial supersampling. The removed TAA experiment's T key and CLI switches are
no longer active.

`--no-grass` skips its draw while leaving data resident. `--grass-stride N` draws
every Nth blade within each draw region (1–64, default 1), retaining the selected root and
orientation. These are diagnostic density reductions, not automatic culling or
LOD. Use the same orbit and avoid concurrent Yard instances for comparisons.
The 30 fps capture cap remains, so fast configurations cannot report uncapped
throughput. Startup/allocation time is excluded from the reported capture rate.

Historical full-density grass measurements before culling/draw grouping on
M1 Max/32 GB using the same 120-capture terrain orbit, excluding startup:

| Rendering | Internal size | Captures/sec |
|---|---|---:|
| Native, no AA | 800×600 | 17.3 |
| Native, 4× MSAA | 800×600 | 17.2 |
| 8× SSAA, no MSAA | 2263×1698 | 17.3 |

These runs show no meaningful throughput difference. The result is consistent
with the earlier geometry bottleneck: supersampling increases pixel work, while
the submitted 40.5 million grass triangles remain unchanged. These are end-to-end
capture rates, not GPU stage timings or a guarantee for other scenes.

For a daylight walk, or the low camera view used for the visual comparison:

```sh
./build/yard --date 2026-09-14 --time 9
./build/yard --date 2026-09-14 --time 9 --eye-height 0.4
```

Click to capture the mouse, use WASD to walk, Shift to move faster, and Escape
to release. The clock starts paused when `--time` is supplied. The viewer follows
bilinearly interpolated heights recovered from the density field, without the
previous 1 cm vertical jumps. The 4 cm render mesh approximates that field, so
navigation is not an exact triangle collision query. Outside the square, the
viewer follows the y=0 ground plane.

The surface spans the outer cell centers (63.60 m); the perimeter and bottom
are uncapped. There are no caves in this seed, terrain self-shadows,
or photorealistic materials. Classic marching cubes does not guarantee correct
topology for arbitrary ambiguous density configurations; this prototype is a
smooth height field. Camera resolution, FOV, manual exposure, 800×600 output, and preview scaling
retain their existing behavior.

`./build/yard --terrain-smoke-test --eye-height 0.4` renders 120 frames along a
scripted daytime orbit and exits. It reports observed capture cadence excluding
startup; this is an execution/cadence check, not a GPU timing benchmark or a test
of physical mouse/keyboard input. `make smoke-test` retains the lunar-phase check.
`make test` covers interpolated plane accuracy and coverage, closed sphere edge
connectivity, outward winding, normals, indices, density and navigation heights.
The lookup tables are vendored from [PyMCubes](vendor/marching_cubes/README.md)
under its accompanying BSD license; normal builds remain offline.

## Generated objects

The application now builds as C++20 alongside the merged geometry engine and
tree-gen port. `--tree SPECIES` places a generated tree at the terrain surface;
`--geometry-demo` adds the sweep and holed-panel examples. Trees, cube, terrain and
grass all use the same HDR/depth, supersampling and optional volume-composite path.

```sh
./build/yard --tree quaking_aspen --date 2026-09-15 --time 12
make tree-bench
make geometry-bench
```

See [geometry](design/GEOMETRY.md) and [tree generation](design/TREES.md).
The tree port is GPLv3; [NOTICE](NOTICE) and [COPYING](COPYING) preserve attribution
and licensing for the combined application. Other vendored components retain
their original licenses.

## Trees in the grassy yard

```sh
make run-yard
# Or select a species and a fixed local date/time:
./build/yard --yard-demo --tree quaking_aspen --date 2026-09-15 --time 12
make yard-smoke-test
```

`--yard-demo` combines the tree port with the existing voxel-terrain and grass
prototype. It starts with an aspen on a 63.61 m square of rolling ground, with
one 5 cm grass blade per centimetre surface column. **G** toggles grass to inspect
the soil and trunk contact; `--no-grass` starts with soil exposed. WASD and mouse
look work as usual, and the camera follows the surface at 1.6 m eye height.
The tree is planted at the terrain height, with its base embedded 2 cm.

This is a static preview, with no collision or growth. Trees and the cube now cast
sun shadows onto themselves, terrain, blades and the grass volume. **H** toggles
shadows; `--no-shadows` disables them at startup. `--paused` holds the initial
clock for comparisons (Space resumes). See [materials and shading](design/MATERIALS.md)
for the revised palette, thin-leaf transmission, and shadow limitations.
The dense 1 cm × 64 cm terrain volume alone uses about 2.4 GiB, so startup takes
several seconds. Frustum culling submits visible 2.56 m draw regions. This mode uses the same restored 8× supersampling, configurable MSAA, and
optional grass-volume LOD as the terrain renderer. **V** toggles volume LOD and
**C** toggles culling; `--ssaa 1` provides the unfiltered comparison. The final
camera image remains 800×600. See [integration notes](design/YARD_DEMO.md).

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
**5.44× the natural sky** (6.44× total). Sky and terrain share this light;
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
Terrain uses directional sunlight and ambient sky lighting, with no terrain
self-shadowing or ambient occlusion yet. The sky integrates 16 view
samples with eight sun samples each per fragment; a cached sky lookup texture
is a possible optimization as the yard grows.

- `src/main.cpp`: application lifecycle, rendering, and input.
- `src/terrain.cpp`: dense centimeter density generation and height queries.
- `src/marching_cubes.cpp`: interpolated surface extraction, shared vertices and gradient normals.
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
