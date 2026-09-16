"""Run with Blender --background --factory-startup --python this.py -- species output.json.
Uses the unmodified pinned algorithm, without registering its UI.
"""
import importlib
import json
from pathlib import Path
import random
import sys
import time
import types
import bpy
root = Path(__file__).resolve().parents[1]
package = types.ModuleType('yard_treegen_reference')
package.__path__ = [str(root / 'vendor/tree-gen')]
sys.modules[package.__name__] = package
gen = importlib.import_module(package.__name__ + '.parametric.gen')
gen.update_log = lambda *args: None
name, output = sys.argv[sys.argv.index('--') + 1:]
params = importlib.import_module(package.__name__ + '.parametric.tree_params.' + name).params
random.seed(123)
tree = gen.Tree(gen.TreeParam(params))
begin = time.perf_counter()
tree.create_branches()
generated = time.perf_counter()
branches = []
for depth, curve in enumerate(tree.branch_curves):
    for spline in curve.splines:
        branches.append({'depth': depth, 'points': [[p.co.x, p.co.z, -p.co.y, p.radius] for p in spline.bezier_points]})
leaves = [[l.position.x, l.position.z, -l.position.y] for l in tree.leaves_array]
# Mesh construction is timed separately from skeleton generation.
tree.create_leaf_mesh()
bpy.context.view_layer.update()
depsgraph = bpy.context.evaluated_depsgraph_get()
verts = tris = 0
foliage_samples = {}
for obj in bpy.context.scene.objects:
    if obj.type not in ('CURVE', 'MESH') or obj.name in ('Cube',):
        continue
    if obj.name in ('Leaves', 'Blossom'):
        flattened = [obj.data.vertices[i].co for poly in obj.data.polygons for i in poly.vertices]
        foliage_samples[obj.name] = [[v.x, v.z, -v.y] for v in flattened[:64]]
    evaluated = obj.evaluated_get(depsgraph)
    mesh = evaluated.to_mesh()
    mesh.calc_loop_triangles()
    verts += len(mesh.vertices)
    tris += len(mesh.loop_triangles)
    evaluated.to_mesh_clear()
end = time.perf_counter()
result = dict(species=name, seed=123, blender=bpy.app.version_string, branches=branches, leaves=leaves,
              generation_ms=(generated-begin)*1000, mesh_ms=(end-generated)*1000, vertices=verts, triangles=tris, foliage_samples=foliage_samples)
Path(output).write_text(json.dumps(result))
print(json.dumps({k:v for k,v in result.items() if k not in ('branches','leaves','foliage_samples')}))
print('branches', len(branches), 'leaves', len(leaves))
