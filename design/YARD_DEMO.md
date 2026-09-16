# Combined tree, grass and terrain preview

`--yard-demo` selects a framed tree/terrain scene. It defaults to a seeded
quaking aspen; `--tree NAME` selects any tree-gen preset. The viewer starts 24 m
from the trunk with the full default crown in view. Mouse look/WASD retain their
usual behavior, while eye height follows the density-derived surface at 1.6 m
(default; adjustable with `--eye-height`). G toggles grass; V toggles volume LOD;
C toggles frustum culling. All three controls work while time is paused.

The tree's world root is (0, terrain height − 0.02 m, −6). Wood, leaves, cube,
soil and grass share the camera, manual exposure, sunlight, ambient lighting,
HDR render target and depth buffer. There is no rigid-body collision, wind,
or growth. Objects cast sun shadows; terrain and grass receive them. Root embedding hides small extraction/interpolation
discrepancies; it does not deform terrain or remove grass under the trunk.
The half-metre cube remains as PR #7's occlusion reference. `--geometry-demo`
adds the curve and polygon examples to the same renderer.

![Daytime aspen with revised materials and sun shadows](yard-demo.png)

Paused at 2026-09-15 12:00 local time, with 8× SSAA and blade grass.
See [materials and shading](MATERIALS.md) for the palette and shadow model.

## Restored renderer from PR #7

The initial combined demo omitted the prototype's antialiasing/composite path;
that omission reintroduced grass shimmer. PR #7 is now merged into `dev`, with
its complete renderer ported to C++20 and shared by the tree demo. There is no
separate single-sample yard renderer.

Defaults are **8× spatial supersampling and 1× MSAA**: 2263×1698 internal pixels,
area-filtered to the fixed **800×600 RGBA8** sensor image. `--ssaa 1|8` and
`--msaa 1|4` independently control these settings. Each internal sample receives
exposure/tone mapping, then the area filter averages linear display light and
encodes sRGB. This restores the filtering from PR #7; it does not guarantee that
all foliage shimmer disappears. No temporal history, jitter or blending is used.
Projection/FOV, camera cadence, manual exposure, window letterboxing and preview
zoom remain independent of window size and Retina scaling.

Optional `--grass-volume` (V) uses PR #7's 5 cm shallow density layer, transitioning
from blades over 3–6 horizontal metres by default. `--lod-start` / `--lod-end`
configure the transition. Scene RGB remains linear HDR and alpha stores forward
camera depth in RGBA16F. The volume stops at the nearest opaque object, including
the tree's trunk and leaves, and composites before exposure/tone mapping. Near
blades and foreground volume can partially cover the trunk base. G/`--no-grass`
suppresses both representations. Resources remain resident for instant toggles.

The volume approximates blade coverage/lighting and may under-integrate long
grazing paths. Forward depth is half precision, and 4× MSAA averages depth at
mixed-coverage edges; default supersampling with MSAA off avoids that resolve.
The statistical volume has no single opaque depth surface and cannot participate
in transparent-object sorting. See the README for the full prototype limits.

## Terrain and resources

Terrain density, marching cubes, grass layout and culling come from PR #7's
`prototype/voxel-yard` branch. The imported algorithms and tests now use C++20.
Marching cubes retains the pinned BSD-3-Clause PyMCubes tables and license in
`vendor/marching_cubes`. The renderer uses the current pinned Sokol headers;
normal builds regenerate all shader targets offline.

The 63.61 m square has 1 cm density columns and a 64 cm vertical slab. Marching
cubes samples at 4 cm spacing, with interpolated crossings and normals. The seed
produces 5,557,104 terrain triangles and 40,462,321 possible grass blades. Each
blade is a 5 cm × 5 mm upright triangle with a deterministic randomized root and
azimuth. 625 static 2.56 m regions are culled independently for terrain and grass.
These are draw groups, not streamed simulation chunks. `--grass-stride N` is a
diagnostic density reduction; it also scales volume density.

The density volume uses 2,589,588,544 bytes (about 2.41 GiB), plus CPU heights,
GPU roots and meshes. CPU extraction meshes and packed roots are freed after
upload; density and navigation heights remain until shutdown. Initialization is
synchronous and takes several seconds. Supersampled HDR attachments and the volume
composite add GPU storage; no persistent simulation data is changed by rendering
quality or representation toggles.

## Run and verify

```sh
make run-yard
./build/yard --yard-demo --grass-volume --date 2026-09-15 --time 12
./build/yard --yard-demo --ssaa 1 --date 2026-09-15 --time 12  # unfiltered comparison
./build/yard --yard-demo --ssaa 8 --msaa 4 --grass-volume --eye-height 0.4
make yard-smoke-test
./build/yard --yard-smoke-test --grass-volume --msaa 4
```

The full CPU suite covers terrain topology/interpolation, culling/layout, tree
generation, geometry, astronomy and camera controls. Invalid rendering options are
rejected before GUI startup. PR #7's resolved C++ renderer passed a warning-free
build, the four-phase Metal smoke run and a tree/volume orbit with 4× MSAA. The
combined-demo checks exercise the framed tree orbit with the restored defaults
and optional volume path. Smoke runs do not establish absence of temporal artifacts.
