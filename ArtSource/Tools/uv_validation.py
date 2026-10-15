"""Numerical UV checks on exported triangles; no Blender/UI dependency.

Positions are cm. UV overlap uses positive-area polygon clipping, not pixels.
Boundary clearance is measured between segments, not island bounding boxes.
"""
import math
from collections import defaultdict
import numpy as np

EPS=1e-12

def cross(a,b): return a[0]*b[1]-a[1]*b[0]
def area(poly):
    return abs(sum(cross(poly[i],poly[(i+1)%len(poly)]) for i in range(len(poly))))*.5 if len(poly)>2 else 0.

def intersection_area(first,second):
    polygon=[(float(p[0]),float(p[1])) for p in first]
    second=[(float(p[0]),float(p[1])) for p in second]
    winding=1 if cross((second[1][0]-second[0][0],second[1][1]-second[0][1]),
                      (second[2][0]-second[0][0],second[2][1]-second[0][1]))>=0 else -1
    for i in range(3):
        a,b=second[i],second[(i+1)%3]
        ex,ey=b[0]-a[0],b[1]-a[1]
        output=[]
        for j,p in enumerate(polygon):
            q=polygon[(j+1)%len(polygon)]
            dp=(ex*(p[1]-a[1])-ey*(p[0]-a[0]))*winding
            dq=(ex*(q[1]-a[1])-ey*(q[0]-a[0]))*winding
            inside_p,inside_q=dp>=0,dq>=0
            if inside_p:output.append(p)
            if inside_p != inside_q:
                t=dp/(dp-dq)
                output.append((p[0]+t*(q[0]-p[0]),p[1]+t*(q[1]-p[1])))
        polygon=output
        if len(polygon)<3:return 0.
    return area(polygon)

def point_segment_distances(p,a,b):
    direction=b-a
    length=(direction*direction).sum(axis=-1)
    t=np.clip(((p-a)*direction).sum(axis=-1)/np.maximum(length,EPS),0,1)
    return np.linalg.norm(p-(a+t[...,None]*direction),axis=-1)

def segment_clearance(first,second):
    a,b=second[:,0],second[:,1]
    result=float('inf')
    for p,q in first:
        # Intersecting boundaries have already been detected by the overlap pass.
        d=min(float(point_segment_distances(p,a,b).min()),float(point_segment_distances(q,a,b).min()),
              float(point_segment_distances(a,p,q).min()),float(point_segment_distances(b,p,q).min()))
        result=min(result,d)
    return result

def validate(triangles,resolution=2048,gap_px=32,outer_px=16):
    uv=np.array([t['uv'] for t in triangles],dtype=float)
    xyz=np.array([t['xyz'] for t in triangles],dtype=float)
    ids=[t['vertices'] for t in triangles]
    count=len(triangles)
    result={'triangle_count':count,'resolution':resolution,'tolerance_uv_area':EPS,
            'required_gap_px':gap_px,'required_outer_px':outer_px,'errors':[]}
    errors=result['errors']
    if count==0 or not np.isfinite(uv).all() or not np.isfinite(xyz).all():
        errors.append('missing_or_nonfinite');result['passed']=False;return result
    auv=np.abs((uv[:,1,0]-uv[:,0,0])*(uv[:,2,1]-uv[:,0,1])-(uv[:,1,1]-uv[:,0,1])*(uv[:,2,0]-uv[:,0,0]))*.5
    a3=np.linalg.norm(np.cross(xyz[:,1]-xyz[:,0],xyz[:,2]-xyz[:,0]),axis=1)*.5
    if (auv<=EPS).any():errors.append('degenerate_uv')
    if (a3<=EPS).any():errors.append('degenerate_surface')
    if uv.min() < -1e-8 or uv.max()>1+1e-8:errors.append('outside_atlas')
    outer=float(min(uv.min(),1-uv.max())*resolution)
    result['minimum_outer_px']=outer
    if outer<outer_px-.02:errors.append('outer_margin')
    parent=list(range(count))
    def root(i):
        while parent[i]!=i:parent[i]=parent[parent[i]];i=parent[i]
        return i
    edge_faces=defaultdict(list)
    for i in range(count):
        for j in range(3):
            k=(j+1)%3
            endpoints=tuple(sorted(((ids[i][j],*np.round(uv[i,j],8)),(ids[i][k],*np.round(uv[i,k],8)))))
            edge_faces[endpoints].append((i,j,k))
    for entries in edge_faces.values():
        for e in entries[1:]:parent[root(e[0])]=root(entries[0][0])
    islands=defaultdict(list)
    for i in range(count):islands[root(i)].append(i)
    labels={r:n for n,r in enumerate(islands)}
    density_ref=resolution*math.sqrt(float(auv.sum()/max(a3.sum(),EPS)))
    summaries=[]
    for r,indices in islands.items():
        surface=float(a3[indices].sum());texture=float(auv[indices].sum())
        density=resolution*math.sqrt(texture/max(surface,EPS))
        ratio=density/max(density_ref,EPS)
        summaries.append({'id':labels[r],'triangles':len(indices),'area_cm2':surface,'area_uv':texture,
                          'density_px_per_cm':density,'relative_density':ratio})
        if not .9-1e-5<=ratio<=1.1+1e-5:errors.append('density') if 'density' not in errors else None
    result.update(island_count=len(islands),density_reference_px_per_cm=density_ref,islands=summaries)
    # Spatial buckets prune pairs; polygon clipping also checks adjacent faces for foldover.
    lower=uv.min(axis=1);upper=uv.max(axis=1);grid=defaultdict(list);overlaps=[]
    for i in range(count):
        lo=np.floor(lower[i]*128).astype(int);hi=np.floor(upper[i]*128).astype(int)
        cells=[(x,y) for x in range(lo[0],hi[0]+1) for y in range(lo[1],hi[1]+1)]
        candidates=set(j for cell in cells for j in grid[cell])
        for j in candidates:
            if upper[i,0]<=lower[j,0] or upper[i,1]<=lower[j,1] or upper[j,0]<=lower[i,0] or upper[j,1]<=lower[i,1]:continue
            if intersection_area(uv[i],uv[j])>EPS:
                if len(overlaps)<100:overlaps.append([j,i])
        for cell in cells:grid[cell].append(i)
    result['overlap_pairs']=overlaps
    if overlaps:errors.append('overlap')
    boundaries=defaultdict(list)
    for entries in edge_faces.values():
        if len(entries)==1:
            i,j,k=entries[0];boundaries[root(i)].append([uv[i,j],uv[i,k]])
    segments={r:np.asarray(s) for r,s in boundaries.items()}
    smallest=float('inf');nearest=None;keys=list(segments)
    for i,r in enumerate(keys):
        for s in keys[i+1:]:
            a,b=segments[r],segments[s]
            separation=np.maximum(0,np.maximum(a.min(axis=(0,1))-b.max(axis=(0,1)),b.min(axis=(0,1))-a.max(axis=(0,1))))
            if np.linalg.norm(separation)>=smallest:continue
            distance=segment_clearance(a,b)
            if distance<smallest:smallest=distance;nearest=[labels[r],labels[s]]
    result['minimum_island_gap_px']=smallest*resolution if math.isfinite(smallest) else None
    result['closest_islands']=nearest
    if smallest*resolution<gap_px-.02:errors.append('island_margin')
    valid=(a3>EPS)&(auv>EPS)
    u=xyz[valid,1]-xyz[valid,0];v=xyz[valid,2]-xyz[valid,0]
    length=np.linalg.norm(u,axis=1);projection=(u*v).sum(axis=1)/length
    height=np.sqrt(np.maximum(0,(v*v).sum(axis=1)-projection**2))
    base=np.zeros((len(length),2,2));base[:,0,0]=length;base[:,0,1]=projection;base[:,1,1]=height
    mapped=np.stack([uv[valid,1]-uv[valid,0],uv[valid,2]-uv[valid,0]],axis=2)
    if len(length):
        singular=np.linalg.svd(mapped@np.linalg.inv(base),compute_uv=False)
        stretch=singular[:,0]/singular[:,1]
        order=np.argsort(stretch);cumulative=np.cumsum(a3[valid][order]);cumulative/=cumulative[-1]
        result['stretch_anisotropy']={str(q):float(np.interp(q/100,cumulative,stretch[order])) for q in [50,95,99,100]}
    result['passed']=not errors
    return result

if __name__=='__main__':
    import argparse,json
    from pathlib import Path
    parser=argparse.ArgumentParser();parser.add_argument('triangles');parser.add_argument('report')
    args=parser.parse_args()
    result=validate(json.loads(Path(args.triangles).read_text(encoding='utf-8-sig')))
    Path(args.report).write_text(json.dumps(result,indent=2))
    print(json.dumps({k:v for k,v in result.items() if k not in ('islands','overlap_pairs')},indent=2))
    raise SystemExit(0 if result['passed'] else 1)
