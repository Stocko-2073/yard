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
Xcode, and an Apple Silicon Mac with Metal support. Install the pinned shader
compiler once with `make setup-tools` (requires network). Subsequent builds work
offline; the Sokol headers are vendored.

```sh
make setup-tools
make
make run
```

The app opens a resizable window with a rotating, colored cube, depth testing,
4x MSAA, and Retina support. Press **Space** to pause or resume and **Escape** to
quit. `make smoke-test` runs 120 frames and exits; it requires a graphical macOS
session. `make clean` removes build output.

- `src/main.c`: application lifecycle, cube geometry, rendering, and input.
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
verification. GLSL300ES supports this cube's graphics shaders, not compute.

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
