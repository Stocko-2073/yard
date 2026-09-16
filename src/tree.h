// C++ adaptation of tree-gen by Charlie Hewitt and Sami Pflibsen-Jones.
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "geometry.h"
#include <cstddef>
#include <string_view>

namespace yard::tree {
using geometry::Vec3;
struct Parameters {
#include "tree_params.inc"
};
struct Preset { std::string_view name; Parameters parameters; };
std::span<const Preset> species();
Parameters preset(std::string_view name); // throws for unknown names
inline constexpr std::size_t no_parent = static_cast<std::size_t>(-1);
struct Point {
    Vec3 position, handle_left, handle_right;
    float radius;
};
struct Branch {
    std::size_t parent = no_parent; // attachment parent, including same-level splits
    unsigned depth = 0;
    bool split = false;
    double offset = 0; // metres along attachment parent's original stem
    double length = 0, radius = 0;
    std::vector<Point> points;
};
struct Leaf {
    std::size_t branch;
    Vec3 position, direction, right;
    bool blossom = false;
};
struct Skeleton {
    Parameters parameters;
    double foliage_scale = 1;
    std::vector<Branch> branches;
    std::vector<Leaf> leaves;
};
struct Limits {
    std::size_t branches = 250000, leaves = 4000000, points = 4000000;
};
// Deterministic, including seed zero. Coordinates: +Y up, -Z north (metres).
// Generation has no render/physics state. Budgets throw rather than truncate.
Skeleton generate(const Parameters&, std::uint32_t seed = 1, Limits = {});
struct MeshDetail {
    geometry::Detail curve;
    unsigned radial_segments = 8;
    bool foliage = true;
    std::size_t max_vertices = 16000000;
};
struct Meshes { geometry::Mesh wood, leaves, blossoms; };
// Tessellates an existing skeleton without rerunning branch placement.
// Independent capped sweeps overlap at junctions; they are not a manifold union.
Meshes mesh(const Skeleton&, MeshDetail = {});
} // namespace yard::tree
