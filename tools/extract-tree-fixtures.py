#!/usr/bin/env python3
"""Extract compact independent golden samples from Blender reference JSON files."""
import json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
files=['blender-aspen.json','blender-small-pine.json','blender-fan_palm.json','blender-hill_cherry.json','blender-sphere_tree.json','blender-willow.json']
lines=['# Blender 5.2.2 LTS; pinned tree-gen; seed 123. Yard axes, metres.']
for name in files:
    data=json.loads((root/'build'/name).read_text())
    branches=[b for b in data['branches'] if len(b['points'])>1]
    tolerance=5 if data['species']=='sphere_tree' else 0
    lines.append(f"tree {data['species']} {len(branches)} {len(data['leaves'])} {tolerance}")
    for index in sorted({0,len(branches)-1,*range(0,len(branches),max(1,len(branches)//24))}):
        b=branches[index]
        for j in sorted({0,len(b['points'])//2,len(b['points'])-1}):
            lines.append('point '+str(index)+' '+str(j)+' '+' '.join(map(str,b['points'][j])))
    if not tolerance:
        for index in range(0,len(data['leaves']),max(1,len(data['leaves'])//24)):
            lines.append('leaf '+str(index)+' '+' '.join(map(str,data['leaves'][index])))
    for kind, vertices in data.get('foliage_samples', {}).items():
        for index, point in enumerate(vertices):
            lines.append('vertex '+kind+' '+str(index)+' '+' '.join(map(str,point)))
(root/'tests/data/tree-reference.txt').write_text('\n'.join(lines)+'\n')
