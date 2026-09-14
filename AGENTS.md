# Working on Yard

## Project direction

Yard is an active, persistent yard simulation for robots to tend, with optional
photorealistic appearance. See `README.md` for the environment scope and
`design/PHYSICS_NOTES.md` for robotics and physics architecture discussions.

The user is experienced in game development and engine design. Explain robotics
concepts through that background without re-explaining basic engine concepts.

MuJoCo and physics LOD are candidates, not committed dependencies or implemented
features. Preserve that distinction when documenting or extending the project.
The proposed design separates persistent yard state, physical representation,
and visual/sensor representation, with one physics owner per dynamic object.

## Current scaffold

- C11 application code in `src/main.c`.
- Sokol implementation in `src/sokol.m`, compiled as Objective-C with ARC.
- macOS/Metal app target with portable shader source in `shaders/cube.glsl`.
- `sokol-shdc` generates `build/generated/cube.glsl.h`; never edit it by hand.
- Vendored Sokol headers in `vendor/sokol/`; the exact pin is in `REVISION` there.

Sokol changes its API frequently. Read the vendored headers before coding against
it; do not assume examples or remembered APIs match this revision. Keep upgrades
explicit, update the headers together, preserve the upstream license, and update
the revision record and README. Normal builds should not fetch dependencies.

The application build is currently Mac-specific, while shader generation targets
Metal, HLSL, GLSL, and WGSL. Use generated uniform types and binding constants.
Install the pinned compiler explicitly with `make setup-tools`; normal builds
regenerate shaders offline. See `tools/README.md` for compiler pinning.

## Build and verification

- `make setup-tools` installs the pinned shader compiler (network required once).
- `make shaders` generates the portable shader header.
- `make` builds the app with Apple's toolchain.
- `make run` launches the interactive cube; Space pauses and Escape quits.
- `make smoke-test` renders 120 frames on Metal and exits.
- `make clean` removes generated output under `build/`.

For rendering or build changes, build without warnings and run the smoke check
when a graphical macOS session is available. The smoke check exercises startup
and rendering but does not establish visual correctness or input behavior.

Sandboxed GUI launches may fail to connect to macOS services. If that happens,
use the available escalation mechanism to run in the desktop session. Report
what was actually verified; do not treat a successful build as a successful run.
Keep generated binaries and objects out of version control.

## Repository workflow

`dev` is the GitHub default branch and the development branch. `main` also exists;
merge and push when requested. The SSH origin is
`stocko-git:Stocko-2073/yard.git`, where `stocko-git` is a configured SSH host alias.
Preserve that alias rather than replacing it with a generic GitHub host URL.
