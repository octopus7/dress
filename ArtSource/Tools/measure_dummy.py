"""Dimensions from the authored surface, with anatomical joint references stated."""
import json,math
from pathlib import Path
import numpy as np
out=Path(__file__).resolve().parents[2]/'ArtSource/Dummy'
triangles=json.loads((out/'UVTriangles.json').read_text())
measure=json.loads((out/'Measurements.json').read_text())

def contour(z,regions):
    points=set()
    for t in triangles:
        if t['region'] not in regions:continue
        for i in range(3):
            a,b=np.asarray(t['xyz'][i]),np.asarray(t['xyz'][(i+1)%3])
            if (a[2]-z)*(b[2]-z)<0:
                p=a+(b-a)*(z-a[2])/(b[2]-a[2]);points.add(tuple(np.round(p[:2],7)))
    points=sorted(points)
    def cross(a,b,c):return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
    low=[];high=[]
    for p in points:
        while len(low)>1 and cross(low[-2],low[-1],p)<=0:low.pop()
        low.append(p)
    for p in points[::-1]:
        while len(high)>1 and cross(high[-2],high[-1],p)<=0:high.pop()
        high.append(p)
    hull=low[:-1]+high[:-1]
    return {'height_cm':z,'circumference_cm':sum(math.dist(hull[i],hull[(i+1)%len(hull)]) for i in range(len(hull))),
            'width_cm':max(p[0] for p in hull)-min(p[0] for p in hull),'contour_cm':hull}

sections={name:contour(z,{'torso_front','torso_back','neck'})
          for name,z in [('neck',9.7),('chest',8.6),('waist',7.4),('hip',6.4),('shoulder',9.15)]}
points=np.array([p for t in triangles for p in t['xyz']])
measure['height']=float(points[:,2].max()-points[:,2].min())
for name in ('neck','chest','waist','hip'):measure[name+'_circumference']=sections[name]['circumference_cm']
measure['shoulder_width']=sections['shoulder']['width_cm']
arm=np.array([p for t in triangles if t['region'] in ('arm_l','hand_l') for p in t['xyz']])
shoulder=np.array([1.48,0,9.15]);axis=np.array([4.58,-.12,5.5])-shoulder;axis/=np.linalg.norm(axis)
measure['arm_length']=float(((arm-shoulder)@axis).max())
measure['leg_length']=float(6.2-points[:,2].min())
measure['sections']=sections
measure['measurement_notes']={'circumferences':'Convex contour of actual triangle/plane intersections; arms excluded by anatomical face regions',
    'shoulder_width':'Actual torso surface width in z=9.15cm cross section',
    'arm_length':'Shoulder joint to furthest authored arm/hand surface projected along the A-pose arm axis',
    'leg_length':'Hip joint z=6.2cm to actual sole plane'}
(out/'Measurements.json').write_text(json.dumps(measure,indent=2))
print(json.dumps({k:v for k,v in measure.items() if isinstance(v,(float,int,str))},indent=2))
