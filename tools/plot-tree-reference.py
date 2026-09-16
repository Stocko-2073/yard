#!/usr/bin/env python3
"""Draw comparable front projections of Blender/C++ skeletons (requires Pillow).
This is a structural comparison, not a shaded mesh render.
"""
import json
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
root=Path(__file__).resolve().parents[1]
cases=[('aspen','aspen'),('small-pine','small-pine'),('fan_palm','fan-palm'),('hill_cherry','cherry'),('sphere_tree','sphere'),('willow','willow')]
image=Image.new('RGB',(1800,900),'#f5f4ef');draw=ImageDraw.Draw(image)
font=ImageFont.truetype('/System/Library/Fonts/Helvetica.ttc',18)
small=ImageFont.truetype('/System/Library/Fonts/Helvetica.ttc',14)
draw.text((24,12),'Pinned tree-gen / Blender 5.2.2 (top) and Yard C++ (bottom), seed 123',fill='#222222',font=font)
draw.text((24,40),'Same projection and scale per species. Branch polylines and sampled foliage anchors; not shaded meshes.',fill='#555555',font=small)
for col,(ref_name,cpp_name) in enumerate(cases):
    ref=json.loads((root/'build'/f'blender-{ref_name}.json').read_text())
    cpp={'branches':[],'leaves':[]}
    lines=iter((root/'build'/f'{cpp_name}.txt').read_text().splitlines())
    for line in lines:
        values=line.split()
        if values[0]=='b':cpp['branches'].append({'points':[list(map(float,next(lines).split())) for _ in range(int(values[2]))]})
        elif values[0]=='l':cpp['leaves'].append(list(map(float,values[1:])))
    points=[p for b in ref['branches'] if len(b['points'])>1 for p in b['points']]+ref['leaves']
    xmin,xmax=min(p[0] for p in points),max(p[0] for p in points)
    ymin,ymax=min(p[1] for p in points),max(p[1] for p in points)
    scale=min(260/(xmax-xmin),325/(ymax-ymin))
    for row,data in enumerate([ref,cpp]):
        ox=col*300+150-(xmin+xmax)*scale/2;oy=430+row*410+ymin*scale
        def project(p):return (ox+p[0]*scale,oy-p[1]*scale)
        draw.text((col*300+12,75+row*410),ref['species'],fill='#222222',font=small)
        draw.line((col*300+10,oy,col*300+290,oy),fill='#c6c6ba')
        for b in data['branches']:
            points=b['points']
            if len(points)<2:continue
            for a,b in zip(points,points[1:]):
                draw.line([project(a),project(b)],fill='#766249',width=max(1,round((a[3]+b[3])*scale)))
        step=max(1,len(data['leaves'])//2500)
        for p in data['leaves'][::step]:
            x,y=project(p);draw.ellipse((x-1,y-1,x+1,y+1),fill='#c87e98' if ref['species']=='hill_cherry' else '#648553')
image.save(root/'design/tree-reference/comparison.png')
