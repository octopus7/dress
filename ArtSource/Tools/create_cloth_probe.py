"""Small-scale development probe: sewn front/back tee torso panels, not final art."""
import json, math
from pathlib import Path

root=Path(__file__).resolve().parents[2]
out=root/'ArtSource/Development'
out.mkdir(parents=True,exist_ok=True)
cols,rows=24,18
panels=[]
for side in (1,-1):
    p2,p3,tri=[],[],[]
    for r in range(rows+1):
        v=r/rows
        for c in range(cols+1):
            t=math.pi*c/cols
            top=9.65-(.5 if side==1 else .3)*math.sin(t)**4
            z=6.65+(top-6.65)*v
            # Slightly flared hem leaves room for gravity settling against the body.
            radius_y=1.8+.12*(1-v)
            radius_x=1.13+.2*(1-v)
            x=side*radius_x*math.sin(t)
            if r==rows and abs(math.cos(t))>.65:x=0
            p2.append([round((c/cols)*5.0+(0 if side==1 else 6),6),round(v*(top-6.65),6)])
            p3.append([round(x,6),round(-radius_y*math.cos(t),6),round(z,6)])
    for r in range(rows):
        for c in range(cols):
            a=r*(cols+1)+c;b=a+1;d=a+cols+1;e=d+1
            tri.extend([[a,b,e],[a,e,d]] if side==1 else [[a,e,b],[a,d,e]])
    panels.append({'id':'front' if side==1 else 'back','positions2d_cm':p2,'positions3d_cm':p3,'triangles':tri})
count=(cols+1)*(rows+1)
stitches=[]
for r in range(rows+1):
    if r<=10:
        for c in (0,cols):stitches.append([r*(cols+1)+c,count+r*(cols+1)+c])
for c in range(cols+1):
    if abs(math.cos(math.pi*c/cols))>.65:
        i=rows*(cols+1)+c;stitches.append([i,count+i])
collar_pins=[panels[side]['positions3d_cm'][rows*(cols+1)+c]
    for side in (0,1) for c in range(cols+1) if abs(math.cos(math.pi*c/cols))<.65]
source={'version':2,'purpose':'S2 actual-size cloth feasibility; provisional torso panels only',
    'unit':'cm','dummy_measurements':'blockout-1','panels':panels,'stitches':stitches,
    'collar_pins_cm':collar_pins,'collision_thickness_cm':.05,'minimum_particle_mass_kg':.0000001}
(out/'ClothProbe.json').write_text(json.dumps(source,separators=(',',':')),encoding='utf-8')
print(f'Probe source: {len(panels)} panels, {count*2} unsewn vertices, {len(stitches)} stitches')
