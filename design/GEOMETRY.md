# Procedural geometry

Yard's geometry module converts cubic Bézier paths, closed swept profiles, and
planar polygon surfaces into CPU-owned indexed triangle meshes. It has no tree,
Sokol, or physics dependency. Tree generation remains the second TODO stage;
branch skeletons should remain owned by that generator rather than reconstructed
from triangles. Physics LOD and MuJoCo remain design candidates.

## Reuse investigation and decision

Research and local measurements: 2026-09-15. These are rendering workloads, not
CAD accuracy or arbitrary damaged-input repair workloads.

| Candidate | Primitives and mesh quality | Tessellation/detail controls | Integration and license |
| --- | --- | --- | --- |
| Earcut v2.2.4 | Planar polygon triangulation including holes; reuses input vertices. No minimum-angle guarantee or invalid-input correctness guarantee. Curves, sweeps, normals and UVs need a wrapper. | Input contour sampling determines detail; this pin has no Delaunay refinement. | One C++ header; compiled with Apple Clang. ISC; vendored with license and exact revision. |
| libtess2 | Polygon contours with winding rules; can create intersection vertices. No curve/sweep generation or UV policy. Supports a constrained-Delaunay option. | Winding rules, polygon size and CDT option; boundary detail still comes from input. | Small C library callable from C++; compiled locally. SGI Free Software License B 2.0. Bucket allocation and custom allocator support. |
| Open CASCADE | Broad curve/surface, pipe/sweep and BRep modeling operations followed by meshing. Suitable if exact CAD surfaces and topology become requirements. | Linear/angular meshing deflection and additional mesher parameters. | C++ library/toolkit build rather than one header. LGPL 2.1 with exception; macOS is supported. Not integrated or locally benchmarked. |

Sources: [pinned Earcut README](https://github.com/mapbox/earcut.hpp/tree/v2.2.4),
[libtess2 README](https://github.com/memononen/libtess2),
[libtess2 public API](https://github.com/memononen/libtess2/blob/master/Include/tesselator.h),
[OCCT modeling](https://dev.opencascade.org/doc/overview/html/occt_user_guides__modeling_algos.html),
[OCCT meshing](https://dev.opencascade.org/doc/overview/html/occt_user_guides__mesh.html),
and [OCCT repository and licensing](https://github.com/Open-Cascade-SAS/OCCT).

Choose Earcut plus a small Yard curve/sweep layer. The integration is small and
our clean polygon benchmark favors it. A general CAD model and tessellation
pipeline adds capabilities we do not yet need; its time and memory costs are
**unmeasured**, not assumed slower. Keep libtess2 as an alternative if contour
intersections or winding-rule fills become necessary. Earcut's public types do
not escape `geometry.cpp`, so replacing it does not change the Yard API.

The pinned header is unmodified. `-isystem vendor/earcut` keeps an upstream
unused-variable warning out of application diagnostics without suppressing Yard
warnings. Normal builds are offline. Dependency updates must explicitly replace
the header/license/revision and rerun geometry checks.

## API and mesh conventions

See `src/geometry.h`. Positions and normals are float3, UVs float2 (32 bytes per
vertex), and indices are uint32. Triangle winding is counterclockwise when
viewed from the normal side. The renderer uploads this layout with explicit
stride/offsets and generated shader binding constants, adding its own color.
Geometry coordinates have no implicit up axis; the app uses metres and +Y up.

- `sample_curve`: a connected chain of cubic Bézier segments with matching
  endpoint radii. De Casteljau subdivision tests both inner control points
  against the endpoint chord segment and limits chord length. Radius interpolates
  linearly in each segment's parameter, not arc length. Returns positions and
  radii; it does not change the source curve.
- `sweep`: an open sampled path with positive radii, and a simple CCW profile
  without a repeated endpoint. A deterministic initial frame is transported by
  the minimal rotation between averaged path tangents. This avoids Frenet-frame
  flips at straight sections/inflections. Side normals accumulate triangle areas,
  including taper, and are smoothed across the duplicated UV seam. Caps have
  separate flat normals. Arbitrary profiles are supported, but their corners
  are smoothed; hard side creases are not yet represented.
- `polygon`: plane-local 2D rings plus origin and orthonormal axes. Outer ring
  first, then interior holes; either winding is accepted. Output normals point
  along `cross(axis_u, axis_v)`. Earcut indices are normalized to this winding.
  The wrapper checks triangle area against outer area minus hole areas.

Sweep U follows profile perimeter from 0 to 1; V follows sampled path distance
in world units. Caps use unscaled profile coordinates. Polygon UVs are plane
coordinates. Seams and cap boundaries intentionally duplicate positions for
normal/UV discontinuities; this is a render mesh, not welded collision topology.

Inputs must be finite, at a local yard scale. Coincident edges/path steps use a
1e-6 world-unit threshold; ring twice-area must exceed 1e-6 square units. Tiny
objects should be generated at a larger local scale then transformed. Simple
rings, contained disjoint holes, and sweeps without self-intersection are caller
contracts. The area check is a sanity check, **not** a topology validator. No
robust predicates, polygon repair, Boolean unions, NURBS, closed-path frame
closure, zero-radius tips, or branch-junction stitching are implemented.

Invalid numeric/degenerate input throws `std::invalid_argument`; detail or size
budgets throw `std::length_error`. Each operation permits at most one million
vertices/samples. Default subdivision depth is 16, maximum accepted depth 24;
exhaustion throws rather than silently violating detail. Allocation failure may
also propagate. Functions build local results and never partially mutate caller
meshes. The control-hull criterion bounds centerline approximation in ordinary
float precision, not the swept surface's screen-space error or curvature/radius
self-intersections.

## LOD and use

Regenerate from the same source primitives with different `Detail` values and
profile segment counts. For example:

```cpp
using namespace yard::geometry;
std::array<Bezier, 1> curve = {
    Bezier{{Vec3{0,0,0}, Vec3{1,2,0}, Vec3{-1,4,1}, Vec3{0,6,1}}, .3f, .02f}
};
auto coarse = sweep(sample_curve(curve, {.05f, .5f, 16}), circle_profile(6));
auto fine = sweep(sample_curve(curve, {.002f, .1f, 16}), circle_profile(16));
```

These controls generate separate meshes; there is no runtime LOD selection,
transition stitching or progressive simplifier. Polygon detail is its supplied
boundary; automatic polygon simplification is deferred.

`./build/yard --geometry-demo --date 2026-09-15 --time 12` shows a tapered curve
and a holed panel beside the cube. Checkers expose UVs and the shader consumes
generated normals. The existing analytic ground shadow only represents the cube;
the additional specimens do not cast ground shadows. The default cube itself is
now built from six polygon surfaces. Smoke mode includes both specimens.

## Measurements and reproduction

Apple M1 Max, macOS 26.6.2, Apple Clang 21.0.0, arm64, `-O3 -DNDEBUG`.
Single-threaded elapsed means including fresh allocation/destruction; no warm-up
or timing thresholds. Background load and allocator state affect these numbers.

`make geometry-bench` runs the full Yard path, including sampling, normals, UVs,
caps, and input checks. One observed run:

| Workload | Iterations | µs/mesh | Vertices | Triangles | Payload bytes | Vector capacity bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Six-metre Bézier, coarse controls above | 2,000 | 8.76 | 131 | 200 | 6,592 | 6,592 |
| Same Bézier, fine controls above | 2,000 | 28.35 | 1,239 | 2,268 | 66,864 | 66,864 |
| 128-point circular boundary, 32-point hole | 10,000 | 9.05 | 160 | 160 | 7,040 | 10,240 |

Peak process RSS for this run was 2,441,216 bytes. Payload excludes temporary
sampling/triangulation/frame allocations; capacity includes unused vector
storage but excludes allocator metadata. RSS includes runtime and all workloads,
so it is not per-mesh workspace memory.

A separate raw-triangulator comparison uses identical float contours, default
settings, 10,000 fresh invocations, and separate process runs:

| Triangulator | µs/polygon | Triangles | Peak process RSS bytes |
| --- | ---: | ---: | ---: |
| Earcut v2.2.4 | 8.64 | 160 | 1,490,944 |
| libtess2 `8dbd6483e920311a58c9af10a10beb278efebc36` | 19.75 | 160 | 1,933,312 |

This comparison excludes Yard normals/UVs and validation. Both preserve the
circular boundary and hole at the supplied resolution; no minimum triangle
angle is guaranteed. It does not rank robustness on malformed polygons or
performance on other contours. RSS is a coarse process measure, not an allocator
trace, and these small differences should not be generalized to whole scenes.

To reproduce the optional comparison, supply an external libtess2 checkout at
the revision above. It is not vendored or needed by normal builds:

```sh
git clone https://github.com/memononen/libtess2.git /tmp/yard-libtess2-geometry
git -C /tmp/yard-libtess2-geometry checkout 8dbd6483e920311a58c9af10a10beb278efebc36
mkdir -p build/libtess2
for source in /tmp/yard-libtess2-geometry/Source/*.c; do
    xcrun clang -O3 -DNDEBUG -I/tmp/yard-libtess2-geometry/Include \
        -c "$source" -o "build/libtess2/$(basename "$source" .c).o"
done
xcrun clang++ -std=c++20 -O3 -DNDEBUG -isystem vendor/earcut \
    -I/tmp/yard-libtess2-geometry/Include tools/triangulation-bench.cpp \
    build/libtess2/*.o -o build/triangulation-bench
./build/triangulation-bench earcut
./build/triangulation-bench libtess2
```

`make test` covers area/holes, winding, finite unit normals, taper, cap/UV seams,
inflections, LOD, deterministic indices, 32-bit indices and rejected inputs.
AddressSanitizer/UndefinedBehaviorSanitizer geometry checks passed. The app built
without warnings and Metal smoke mode rendered 120 fixed 800×600 frames. This
checks execution, not visual correctness, interactive behavior or Blender parity.
