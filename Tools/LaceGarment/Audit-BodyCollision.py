"""Independent reference-pose QA of UE-exported body and cooked collision.

Three triangle vertices in one convex collider prove coverage of the whole face.
Faces crossing collider joins are reported as unproven, not falsely called holes.
For those faces, extra surface samples can prove a gap but cannot prove coverage.
This is a development check, not a product dependency or moving-pose acceptance.
"""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np


def distances(points, data):
    result = []
    for c in data['capsules']:
        a, b = np.asarray(c['aCm']), np.asarray(c['bCm'])
        axis = b-a
        q = a+np.clip((points-a)@axis/max(axis@axis, 1e-20),0,1)[:,None]*axis
        result.append(np.linalg.norm(points-q,axis=1)-c['radiusCm'])
    for c in data['convexes']:
        planes = np.asarray(c['planesCm'])
        # Exact membership. Outside this is a lower bound on Euclidean distance.
        result.append(np.max(points@planes[:,:3].T-planes[:,3],axis=1))
    return np.asarray(result)


def audit(data):
    points=np.asarray(data['positionsCm']);faces=np.asarray(data['triangles'],dtype=int)
    signed=distances(points,data)
    inside=signed<=2e-4
    face_covered=np.any(np.all(inside[:,faces],axis=2),axis=0)
    names={'pelvis','spine_01','chest','neck','clavicle_l','clavicle_r','upperarm_l','upperarm_r','forearm_l','forearm_r','thigh_l','thigh_r'}
    relevant_points={tuple(p) for bone in data['boneSurface'] if bone['name'] in names for p in bone['dominantPointsCm']}
    relevant_vertices=np.array([tuple(p) in relevant_points for p in points])
    relevant=np.all(relevant_vertices[faces],axis=1)
    unproven=np.flatnonzero(relevant & ~face_covered)
    maximum=0.0;outside_count=0;examples=[]
    for start in range(0,len(unproven),2048):
        ids=unproven[start:start+2048];tri=points[faces[ids]]
        samples=np.stack([tri.mean(axis=1),(tri[:,0]+tri[:,1])/2,(tri[:,1]+tri[:,2])/2,(tri[:,2]+tri[:,0])/2],axis=1)
        bounds=distances(samples.reshape(-1,3),data).min(axis=0).reshape(-1,4)
        maximum=max(maximum,float(bounds.max()))
        outside_count+=int(np.count_nonzero(bounds>2e-4))
        for idx in np.flatnonzero(np.any(bounds>2e-4,axis=1))[:20]:
            if len(examples)<20:examples.append({'face':int(ids[idx]),'outsideLowerBoundCm':float(bounds[idx].max())})
    return {'scope':'Reference-pose surface containment; single-collider proof and join sampling, not pose or garment acceptance',
            'coverageProven':bool(np.all(face_covered[relevant])),
            'vertices':len(points),'triangles':len(faces),
            'uncoveredVertices':int(np.count_nonzero(~inside.any(axis=0))),
            'relevantContactFaces':int(relevant.sum()),'contactFacesProvenInsideOneCollider':int(np.count_nonzero(relevant & face_covered)),
            'unprovenContactFaces':len(unproven),'joinSamplesOutsideTolerance':outside_count,
            'maxJoinSampleOutsideLowerBoundCm':maximum,'gapExamples':examples}


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('input',type=Path);parser.add_argument('output',type=Path);args=parser.parse_args()
    if args.output.exists():parser.error('Refusing to overwrite an existing audit')
    raw=args.input.read_bytes();result=audit(json.loads(raw.decode('utf-8-sig')));result['bodyReportSha256']=hashlib.sha256(raw).hexdigest()
    args.output.write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in result.items() if k!='gapExamples'},indent=2))
    raise SystemExit(0 if result['coverageProven'] else 1)
