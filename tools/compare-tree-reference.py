#!/usr/bin/env python3
"""Compare tree-bench's skeleton dump with an independently generated Blender reference."""
import argparse
import json
import math
from pathlib import Path
parser = argparse.ArgumentParser()
parser.add_argument('reference', type=Path)
parser.add_argument('actual', type=Path)
parser.add_argument('--leaf-count-tolerance', type=int, default=0)
parser.add_argument('--tolerance', type=float, default=0.002)
a = parser.parse_args()
reference = json.loads(a.reference.read_text())
# Blender allocates a placeholder spline before radius/pruning rejection.
reference['branches'] = [b for b in reference['branches'] if len(b['points']) > 1]
branches, leaves = [], []
vertices = {}
lines = iter(a.actual.read_text().splitlines())
for line in lines:
    values = line.split()
    if values[0] == 'b':
        branches.append(dict(depth=int(values[1]), points=[list(map(float, next(lines).split())) for _ in range(int(values[2]))]))
    elif values[0] == 'v':
        vertices.setdefault(values[1], []).append(list(map(float, values[2:])))
    else:
        leaves.append(list(map(float, values[1:])))
branches.sort(key=lambda b:b['depth'])
assert len(branches) == len(reference['branches']), (len(branches), len(reference['branches']))
assert abs(len(leaves) - len(reference['leaves'])) <= a.leaf_count_tolerance, (len(leaves), len(reference['leaves']))
point_error = radius_error = leaf_error = 0
for actual, expected in zip(branches, reference['branches']):
    assert actual['depth'] == expected['depth']
    assert len(actual['points']) == len(expected['points'])
    for p, q in zip(actual['points'], expected['points']):
        point_error = max(point_error, math.dist(p[:3], q[:3]))
        radius_error = max(radius_error, abs(p[3]-q[3]))
for p,q in zip(leaves,reference['leaves']) if len(leaves)==len(reference['leaves']) else []:
    leaf_error = max(leaf_error, math.dist(p,q))
vertex_error = 0
for kind, expected in reference.get('foliage_samples', {}).items():
    assert len(vertices.get(kind, [])) == len(expected)
    for p,q in zip(vertices[kind], expected):
        vertex_error = max(vertex_error, math.dist(p,q))
print(f"{reference['species']}: branches={len(branches)} leaves={len(leaves)} max_point_m={point_error:.8g} max_radius_m={radius_error:.8g} max_leaf_m={leaf_error:.8g} leaf_count_delta={len(leaves)-len(reference['leaves'])} max_foliage_vertex_m={vertex_error:.8g}")
assert max(point_error,radius_error,leaf_error,vertex_error) <= a.tolerance
