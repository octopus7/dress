"""Compare actual UE posed body and simulated cloth snapshots in centimeters."""
import json,sys,unittest
from pathlib import Path
from mathutils import Vector
from mathutils.bvhtree import BVHTree


RAY_DIRECTIONS=[Vector(v).normalized() for v in
    [(1,.371,.217),(.193,1,.413),(.327,.173,1),(-1,.231,.527),(.479,-1,.183),(.157,.439,-1),(-.613,-.283,1)]]


def ray_parity(body,point,direction):
    origin=point.copy();hits=0
    for _ in range(256):
        location,_,_,_=body.ray_cast(origin,direction)
        if location is None:return hits%2
        hits+=1;origin=location+direction*1e-5
    raise RuntimeError('Body ray traversal did not terminate')


def penetration_report(body_vertices,body_faces,cloth_vertices,cloth_faces,tolerance=.02):
    body=BVHTree.FromPolygons(body_vertices,body_faces,all_triangles=False,epsilon=0)
    vertices=[Vector(v) for v in cloth_vertices];samples=[(v,'vertex',i) for i,v in enumerate(vertices)]
    for index,face in enumerate(cloth_faces):
        a,b,c=[vertices[i] for i in face]
        # Interior and edge points catch body poking through a coarse cloth face
        # even when all of that face's vertices are outside.
        for i in range(4):
            for j in range(4-i):
                k=3-i-j
                if 3 in (i,j,k):continue
                samples.append(((a*i+b*j+c*k)/3,'triangle',index))
    inside=[];maximum=0.;ambiguous=0
    for point,kind,index in samples:
        nearest,normal,_,distance=body.find_nearest(point)
        if distance<=tolerance:continue
        # Face winding/tangent normals may disagree after skeletal deformation.
        # Classify the closed surface by crossings, independent of those normals.
        votes=[ray_parity(body,point,d) for d in RAY_DIRECTIONS[:3]]
        if len(set(votes))>1:
            ambiguous+=1;votes += [ray_parity(body,point,d) for d in RAY_DIRECTIONS[3:]]
        if sum(votes)>len(votes)//2:
            maximum=max(maximum,distance)
            inside.append({'kind':kind,'index':index,'depth_cm':distance,'position_cm':list(point)})
    inside.sort(key=lambda s:s['depth_cm'],reverse=True)
    return {'passed':not inside,'maximum_penetration_cm':maximum,'inside_samples':len(inside),
            'sample_count':len(samples),'tolerance_cm':tolerance,'worst_samples':inside[:12],
            'classification':'3-ray parity, 7 rays on disagreement; unsigned nearest distance',
            'extended_vote_samples':ambiguous}


def read_obj(path):
    vertices=[];faces=[]
    for line in path.read_text(encoding='utf-8-sig').splitlines():
        fields=line.split()
        if fields and fields[0]=='v':vertices.append(tuple(map(float,fields[1:4])))
        elif fields and fields[0]=='f':faces.append(tuple(int(i.split('/')[0])-1 for i in fields[1:]))
    return vertices,faces


class SurfaceTests(unittest.TestCase):
    def test_face_orientation_does_not_turn_outside_into_inside(self):
        vertices=[(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]
        faces=[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]
        faces=[tuple(reversed(f)) for f in faces]
        self.assertTrue(penetration_report(vertices,faces,[(2,0,0)],[])['passed'])
        self.assertFalse(penetration_report(vertices,faces,[(0,0,0)],[])['passed'])

    def test_inside_vertex_is_detected(self):
        vertices=[(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]
        faces=[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]
        self.assertFalse(penetration_report(vertices,faces,[(0,0,0)],[])['passed'])
        self.assertTrue(penetration_report(vertices,faces,[(2,0,0)],[])['passed'])
        self.assertFalse(penetration_report(vertices,faces,[(2,0,0),(-2,0,0),(0,2,0)],[(0,1,2)])['passed'])


if __name__=='__main__':
    args=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
    if not args:
        result=unittest.TextTestRunner().run(unittest.defaultTestLoader.loadTestsFromTestCase(SurfaceTests))
        if not result.wasSuccessful():raise RuntimeError('Surface validator tests failed')
    else:
        folder=Path(args[0]);reports=[]
        for pose in range(4):
            body=read_obj(folder/f'Pose{pose}_Body.obj');cloth=read_obj(folder/f'Pose{pose}_Cloth.obj')
            report=penetration_report(*body,*cloth);report['pose']=pose;reports.append(report)
            print('POSE',pose,'PASS',report['passed'],'DEPTH_CM',report['maximum_penetration_cm'],'INSIDE',report['inside_samples'],flush=True)
        report={'passed':all(r['passed'] for r in reports),'poses':reports}
        (folder/'surface-validation.json').write_text(json.dumps(report,indent=2))
        if not report['passed']:raise RuntimeError('Simulated cloth intersects actual posed body surface')
