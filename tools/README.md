# Shader compiler

Run `make setup-tools` once on Apple Silicon macOS. The script downloads the
prebuilt compiler from the official [sokol-tools-bin repository](https://github.com/floooh/sokol-tools-bin)
at commit `11d0cf678105d614d675e6d9bd2aaf3eeff12f8c` and verifies SHA-256
`92db37975ad7ff3c3c9bc27cba1503287377cb287ebabf60d1c6b597abfa3244`.
The installed executable is ignored by Git and survives `make clean`.
Normal builds never download tools; a missing compiler produces setup instructions.

The pin and checksum live in `setup-shdc.sh`. To upgrade, select an explicit
upstream revision, obtain and verify the corresponding binary, update the pin
and checksum together, and update this document and the root README. Run
`make setup-tools`, `make clean`, `make`, and `make smoke-test`. Confirm generated
reflection matches the vendored Sokol API. Do not silently track upstream HEAD.

Edit `shaders/cube.glsl`; `make` generates backend shader source and C reflection
under `build/generated/`. Source output is retained for graphics debugging.
The Makefile generates Metal macOS/iOS/simulator, HLSL5, GLSL430, GLSL300ES, and
WGSL. This verifies translation, not execution on every backend. Future compute
shaders must use compatible targets (for example GLSL310ES instead of GLSL300ES).

Authoring reference: [sokol-shdc documentation](https://github.com/floooh/sokol-tools/blob/master/docs/sokol-shdc.md).

## Worktree maintenance

`cleanup_worktrees.py` previews or removes clean worktrees with confirmed merged
PRs. See [CLEANUP_WORKTREES.md](CLEANUP_WORKTREES.md) for usage and retention rules.
