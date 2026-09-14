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

Requires Apple's command-line developer tools (`xcode-select --install`) or
Xcode, and a Mac with Metal support. No package manager or network access is
needed to build: the Sokol headers are vendored.

```sh
make
make run
```

The app opens a resizable window with a rotating, colored cube, depth testing,
4x MSAA, and Retina support. Press **Space** to pause or resume and **Escape** to
quit. `make smoke-test` runs 120 frames and exits; it requires a graphical macOS
session. `make clean` removes build output.

- `src/main.c`: application lifecycle, cube geometry, rendering, and input.
- `src/sokol.m`: Sokol implementation compiled as Objective-C for macOS/Metal.
- `src/cube_shader.h`: native Metal shader source for the initial Mac target.
- `vendor/sokol/`: Sokol headers and upstream license, pinned to commit
  `c0db757ea10cbe40aa8398aa378b2b5aae0278b2` (also recorded in `REVISION`).

This scaffold targets macOS with Metal. Adding other graphics backends will
require corresponding shaders and platform build configuration; `sokol-shdc`
is a candidate for generating portable shaders and their binding declarations.
Sokol upgrades should replace the headers together at an explicit revision and
include a build and runtime check for API changes.
