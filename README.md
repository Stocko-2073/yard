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

## Build and run

Requires an Apple Silicon Mac and Apple's command-line developer tools
(`xcode-select --install`) or Xcode. The app is C++20, with Sokol compiled as
Objective-C with ARC. `make setup-tools` downloads the pinned shader compiler
once; after that, builds are offline.

```sh
make setup-tools
make
make run        # sky, terrain, grass and the reference cube
make run-yard   # the same yard with a quaking aspen, paused at noon
```

`make shaders` regenerates only the shader header, and `make clean` removes
`build/` (the installed compiler in `tools/bin/` is kept).

## The scene

- **Sky:** single-scattering Rayleigh/Mie atmosphere, with the sun and moon placed
  by an ephemeris for the site and time. The moon's size, phase and terminator
  angle follow from its geometry. At night, the sky and all surfaces receive the
  site's light-pollution skyglow.
- **Terrain:** a 63.61 m square of rolling soil stored as a 1 cm density volume,
  64 cm deep (about 2.4 GiB, so startup takes several seconds). Marching cubes
  extracts the render mesh at 4 cm spacing, and the viewer walks on heights read
  from the density. A ground plane with a metre grid extends beyond the yard.
- **Grass:** one 5 cm × 5 mm blade per centimetre of surface, about 40 million in
  all, drawn as instances in 2.56 m frustum-culled regions. Optionally, a
  ray-marched 5 cm density layer replaces distant blades.
- **Objects:** a half-metre reference cube at the origin, plus an optional tree
  generated from any [tree-gen](https://github.com/friggog/tree-gen) preset, and
  optional geometry-engine specimens. Objects cast sun shadows onto everything;
  terrain and grass receive shadows but don't cast them.

There is no collision, physics, wind or growth.

## Camera

The camera models the OV2640 on a XIAO ESP32-S3 Sense: a fixed 800×600 image
captured at up to 30 fps with manual shutter and gain. The window only
letterboxes and magnifies that image (nearest-neighbor); resizing and Retina
scaling never change its resolution or projection. The stock lens is unmeasured,
so the 60° vertical FOV is a placeholder; pass a measured one with `--vfov`. A
flat target of height H that exactly fills the image at distance D gives
2·atan(H / 2D).

Scenes render at about 8× the pixel count (2263×1698 for SVGA), then each sample
is exposed and tone mapped before an area filter averages down to the 8-bit
image. Exposure multiplies scene radiance by (shutter ms / 10 ms) × gain; the
10 ms reference is arbitrary, not a measured sensitivity. Shutter is set in
sensor lines and never exceeds one frame. The default is 202 lines (about 10 ms)
at 1× gain; for night, try `--aec-value 672 --agc-gain 15` (33.3 ms at 16×).

Not modeled: automatic exposure, noise, motion blur, rolling shutter, lens
distortion, white balance, or the camera's JPEG/RGB565 output.

## Time and place

The clock runs in UTC and starts at the current time, advancing a simulated day
every four minutes. Dates and times are entered and shown in America/New_York,
whatever the site. Nonexistent spring-forward times are rejected, and repeated
fall-back times resolve to standard time.

The default site is 32.8908277° N, 84.3271342° W (Thomaston, Georgia). To use
another location, sample the light-pollution atlas once (network required), then
load the profile:

```sh
python3 tools/fetch-skyglow.py --latitude 33.749 --longitude -84.388 --output /tmp/atlanta.txt
./build/yard --site /tmp/atlanta.txt --date 2026-01-21 --time 0
```

See [data/README.md](data/README.md) for the atlas and profile format.

## Controls

| Input | Action |
| --- | --- |
| Click, mouse | Capture the mouse and look around |
| W A S D | Walk at 3 m/s; hold Shift for 9 m/s |
| Escape | Release the mouse; quit when already released |
| Space | Pause or resume the clock |
| Left / Right | Scrub time at two simulated hours per second |
| [ / ] | Step back or forward one day and pause |
| 1 2 3 4 | Jump to 07:00, 12:00, 19:00 or 00:00 local and pause |
| M | Track the moon |
| Z | Magnify the preview 8× |
| Comma / Period | Shutter down or up a third of a stop |
| Minus / Equals | Gain down or up one step |
| G | Toggle grass |
| V | Toggle the grass volume |
| C | Toggle frustum culling |
| H | Toggle sun shadows |
| R | Reset the view and return to the current time (camera settings stay) |

## Options

| Option | Effect |
| --- | --- |
| `--date YYYY-MM-DD`, `--time HOUR` | Start paused at a local date (1900–2100) and decimal hour; either defaults to now |
| `--paused` | Start with the clock paused |
| `--moon`, `--zoom` | Start tracking the moon, or magnified |
| `--site FILE` | Load a site profile for location and skyglow |
| `--eye-height M` | Viewer height above the ground, 0.1–10 m (default 1.6) |
| `--vfov DEGREES` | Vertical field of view, 1–170 (default 60) |
| `--tree SPECIES` | Plant a tree-gen preset, e.g. `quaking_aspen` or `hill_cherry` |
| `--yard-demo` | Frame a tree from 24 m away (quaking aspen unless `--tree` says otherwise) |
| `--geometry-demo` | Add a swept curve and a holed panel beside the cube |
| `--grass-volume` | Start with the grass volume on |
| `--lod-start M`, `--lod-end M` | Horizontal distance band where blades fade into the volume (default 6–18 m) |
| `--no-grass`, `--no-shadows`, `--no-culling` | Start with that feature off |
| `--grass-stride N` | Draw every Nth blade, 1–64, and thin the volume to match |
| `--ssaa 1\|8`, `--msaa 1\|4` | Supersampling (default 8) and MSAA (default 1) |
| `--camera-profile FILE` | Load another sensor, in the format of [data/cameras/ov2640-svga.txt](data/cameras/ov2640-svga.txt) |
| `--exposure-ms MS` or `--aec-value LINES` | Manual shutter |
| `--gain X` or `--agc-gain INDEX` | Manual gain |

## Sources

- Sun and moon: [Paul Schlyter's orbital formulas](https://stjarnhimlen.se/comp/ppcomp.html),
  with topocentric lunar parallax. Refraction, eclipses and libration are ignored,
  and UTC stands in for UT1/TT.
- Atmosphere: [GPU Gems 2, chapter 16](https://developer.nvidia.com/gpugems/gpugems2/part-ii-shading-lighting-and-shadows/chapter-16-accurate-atmospheric-scattering).
- Skyglow: David Lorenz's [Light Pollution Atlas](https://djlorenz.github.io/astronomy/lp/).
- Camera: the [OV2640 datasheet](https://files.waveshare.com/wiki/common/OV2640DS_en.pdf)
  and Espressif's [esp32-camera](https://github.com/espressif/esp32-camera) driver.

## Code and dependencies

`src/main.cpp` owns the app, rendering and input; the other files in `src/` are
the astronomy, camera, skyglow, terrain, marching-cubes, culling, geometry and
tree modules. Shaders live in `shaders/cube.glsl`, which `sokol-shdc` compiles to
`build/generated/cube.glsl.h` (generated, never edited or committed) for Metal,
HLSL5, GLSL430, GLSL300ES and WGSL. Only the macOS/Metal app build exists.

Vendored dependencies are pinned, with their licenses and revisions alongside:
Sokol (`vendor/sokol`, zlib), Earcut (`vendor/earcut`, ISC), the PyMCubes
marching-cubes tables (`vendor/marching_cubes`, BSD-3-Clause) and tree-gen
(`vendor/tree-gen`, GPLv3). The shader compiler's revision and checksum are
pinned in `tools/setup-shdc.sh`. Upgrade Sokol and the compiler together: the
generated shader reflection must match the Sokol API. Sokol's API changes often,
so read the vendored headers rather than relying on upstream examples.
`tools/import-tree-presets.py` regenerates `src/tree_*.inc` from tree-gen.

The tree generator is a C++ port of tree-gen, so the combined executable is
GPLv3; see [NOTICE](NOTICE) and [COPYING](COPYING).

`tools/camera-reference/` holds firmware and a collector for capturing reference
images from the real camera; see its [README](tools/camera-reference/README.md).
