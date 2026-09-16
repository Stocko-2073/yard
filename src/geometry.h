#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace yard::geometry {
struct Vec2 { float x, y; };
struct Vec3 { float x, y, z; };
struct Vertex { Vec3 position, normal; Vec2 uv; };
struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};
struct Bezier {
    std::array<Vec3, 4> points;
    float start_radius = 1, end_radius = 1;
};
struct Detail {
    float chord_error = 0.01f;       // world units; control hull to chord
    float max_segment_length = 0.25f;
    unsigned max_depth = 16;        // exceeding this throws, never silently degrades
};
struct Sample { Vec3 position; float radius; };
using Ring = std::vector<Vec2>;

// All functions return owned CPU data; no renderer, tree, or physics dependency.
// Invalid numeric/degenerate input throws invalid_argument; budgets throw length_error.
// Simple, nonintersecting rings and non-self-intersecting sweeps are caller contracts.
std::vector<Sample> sample_curve(std::span<const Bezier> curve, Detail detail = {});
Ring circle_profile(unsigned segments = 12);
// Open path, closed CCW profile (no repeated final point), positive radii.
// Smooth side normals, split UV seam, optional flat caps. No branch junction union.
Mesh sweep(std::span<const Sample> path, std::span<const Vec2> profile, bool caps = true);
// First ring is the outer boundary; remaining rings are disjoint interior holes.
// Plane axes must be orthonormal. Output faces toward cross(axis_u, axis_v).
// UVs are input plane coordinates (one texture repeat per world unit).
Mesh polygon(std::span<const Ring> rings, Vec3 origin = {0,0,0},
             Vec3 axis_u = {1,0,0}, Vec3 axis_v = {0,1,0});
} // namespace yard::geometry
