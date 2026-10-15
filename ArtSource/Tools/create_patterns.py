"""Author mm pattern panels and their assembled positions from one indexed source.

The sim surface ends at the stitch line; 3mm cut allowances are folded inside
the finished garment, rather than becoming extra collision layers at doll scale.
No independent replacement mesh is used. UVs are packed from these same panels.
"""
import json
import math
from pathlib import Path
from collections import Counter, defaultdict

ROOT = Path(__file__).resolve().parents[2]
MEASUREMENTS = json.loads((ROOT/'ArtSource/Dummy/Measurements.json').read_text())


def lerp(a, b, t):
    return [x+(y-x)*t for x,y in zip(a,b)]


def length(points):
    return sum(math.dist(a,b) for a,b in zip(points,points[1:]))


def offset_polygon(points, amount):
    """Miter offset of a CCW boundary, clamped at sharp allowance corners."""
    area=sum(a[0]*b[1]-b[0]*a[1] for a,b in zip(points,points[1:]+points[:1]))
    if area<0:
        points=list(reversed(points))
    result=[]
    for i,p in enumerate(points):
        a,b=points[i-1],points[(i+1)%len(points)]
        d1=math.dist(a,p);d2=math.dist(p,b)
        n1=[(p[1]-a[1])/d1,-(p[0]-a[0])/d1]
        n2=[(b[1]-p[1])/d2,-(b[0]-p[0])/d2]
        denom=max(.2,1+sum(x*y for x,y in zip(n1,n2)))
        result.append([p[k]+amount*(n1[k]+n2[k])/denom for k in range(2)])
    return result+[result[0]]


def panel(pid, name, cols, rows, flat, assembled, layout):
    p={'PieceId':pid,'DisplayName':name,'LayoutMm':layout,'GrainDirection':[0,1],
       'SimPositionsMm':[],'AssembledPositionsMm':[],'TriangleIndices':[],
       'MaxDistanceMm':[],'AnchorBones':[],'Edges':[],'NotchesMm':[], 'FoldLines':[]}
    for r in range(rows+1):
        for c in range(cols+1):
            p['SimPositionsMm'].append(flat(c/cols,r/rows))
            p['AssembledPositionsMm'].append(assembled(c/cols,r/rows))
            p['MaxDistanceMm'].append(12.)
            p['AnchorBones'].append('None')
    for r in range(rows):
        for c in range(cols):
            a=r*(cols+1)+c;b=a+1;d=a+cols+1;e=d+1
            p['TriangleIndices'].extend([a,b,e,a,e,d])
    p['_cols']=cols;p['_rows']=rows
    perimeter=list(range(cols+1))+[r*(cols+1)+cols for r in range(1,rows+1)]
    perimeter+=list(range(rows*(cols+1)+cols-1,rows*(cols+1)-1,-1))
    perimeter+=[r*(cols+1) for r in range(rows-1,0,-1)]
    p['CutOutline']={'PathId':pid+'.cut','bClosed':True,
                     'PointsMm':offset_polygon([p['SimPositionsMm'][i] for i in perimeter],3.)}
    return p


def edge(p, eid, indices, opening=False, fold=False):
    points=[p['SimPositionsMm'][i] for i in indices]
    value={'EdgeId':eid,'Path':{'PathId':p['PieceId']+'.'+eid,'PointsMm':points,'bClosed':False},
           'bOpening':opening,'SewingDirection':1,'AllowanceMm':3.,'StitchVertices':indices}
    p['Edges'].append(value)
    p['NotchesMm'].append(points[len(points)//2])
    if fold:
        p['FoldLines'].append({'PathId':p['PieceId']+'.fold.'+eid,'PointsMm':points,'bClosed':False})
    return (p['PieceId'],eid)


def get_edge(pattern, ref):
    p=next(p for p in pattern['Pieces'] if p['PieceId']==ref[0])
    e=next(e for e in p['Edges'] if e['EdgeId']==ref[1])
    return p,e


def seam(pattern,sid,name,a,b=(),kind='Join',before=(),ratio=1.):
    def refs(side):
        return [{'PieceId':p,'EdgeId':e,'Direction':1} for p,e in side]
    pattern['Seams'].append({'SeamId':sid,'DisplayName':name,'Kind':kind,
        'SideA':refs(a),'SideB':refs(b),'EaseRatio':ratio,'Prerequisites':list(before)})


def base(pid,name,slot):
    return {'PatternId':pid,'PatternVersion':1,'DummyMeasurementsVersion':MEASUREMENTS['version'],
            'DisplayName':name,'Slot':slot,'Pieces':[],'Seams':[],
            'Authoring':{'Unit':'mm','CutAllowanceMm':3.,'SurfaceConvention':'folded allowance inside stitch line',
             'MeasurementsCm':{k:MEASUREMENTS[k] for k in ['chest_circumference','waist_circumference','hip_circumference']}}}


def chain(pattern, refs):
    """Retain one endpoint at each shared junction; closure retains last=first."""
    flat_lengths=[0.];assembled=[];mapping=[]
    for ref in refs:
        p,e=get_edge(pattern,ref)
        for j,idx in enumerate(e['StitchVertices']):
            pos=p['AssembledPositionsMm'][idx]
            if j==0 and assembled:
                assert math.dist(assembled[-1],pos)<1e-5,(ref,assembled[-1],pos)
                continue
            if j:
                flat_lengths.append(flat_lengths[-1]+math.dist(e['Path']['PointsMm'][j-1],e['Path']['PointsMm'][j]))
            assembled.append(pos);mapping.append((ref[0],idx))
    assert len(flat_lengths)==len(assembled)
    return flat_lengths,assembled,mapping


def band(pattern,pid,name,refs,width,layout,ratio=1.,offset=None,anchor=None):
    distances,positions,_=chain(pattern,refs)
    cols=len(positions)-1;rows=3
    def assembled(u,v):
        i=round(u*cols);p=positions[i]
        if offset:
            return offset(p,v)
        # A binding wraps over the outer surface toward its free edge.
        radius=math.hypot(p[0],p[1]);out=[p[0]/max(radius,.01),p[1]/max(radius,.01),0]
        return [p[k]+out[k]*width*v for k in range(3)]
    p=panel(pid,name,cols,rows,lambda u,v:[distances[round(u*cols)]*ratio,v*width],assembled,layout)
    edge(p,'attach',list(range(cols+1)))
    edge(p,'free',[rows*(cols+1)+c for c in range(cols+1)],True)
    edge(p,'end_start',[r*(cols+1) for r in range(rows+1)])
    edge(p,'end_finish',[r*(cols+1)+cols for r in range(rows+1)])
    if anchor:
        p['MaxDistanceMm']=[0.]*len(p['MaxDistanceMm']);p['AnchorBones']=[anchor]*len(p['AnchorBones'])
    pattern['Pieces'].append(p)
    return p


def tee():
    p=base('TShirt','반팔 티셔츠','Top');cols=24;rows=24;pit=13
    # Measured chest + 20mm ease. Half circumference is each body's flat width.
    half=(MEASUREMENTS['chest_circumference']*10+20)/2
    for face,sign,layout in [('front',1,[7,7]),('back',-1,[65,7])]:
        def flat(u,v):
            top_x=33*u if u<1/3 else 11+(u-1/3)*66 if u<=2/3 else 33+(u-2/3)*33
            x=(u*half)*(1-v)+top_x*v
            neck=max(0,1-abs(u-.5)*6)
            return [x,v*(30-(9 if sign==1 else 7)*math.sin(neck*math.pi/2))]
        def assembled(u,v):
            theta=math.pi*u
            low=[sign*13.5*math.sin(theta),-20*math.cos(theta),66]
            t=max(0,(v-pit/rows)/(1-pit/rows))
            mid=[sign*12*math.sin(theta),-19*math.cos(theta),81]
            neck=max(0,1-abs(u-.5)*6)
            top=[sign*7*math.sin(neck*math.pi/2),-19*math.cos(theta),95-(5 if sign==1 else 3)*math.sin(neck*math.pi/2)]
            if v<=pit/rows:return lerp(low,mid,v/(pit/rows))
            result=lerp(mid,top,t)
            result[0]+=sign*7*math.sin(math.pi*t)*abs(math.cos(theta))**8
            return result
        piece=panel(face,'앞판' if sign==1 else '뒤판',cols,rows,flat,assembled,layout)
        edge(piece,'hem',list(range(cols+1)),True,True)
        for side,c in [('l',0),('r',cols)]:
            edge(piece,'side_'+side,[r*(cols+1)+c for r in range(pit+1)])
            # Both halves run continuously around each sleeve opening.
            indices=[r*(cols+1)+c for r in range(pit,rows+1)]
            if sign<0:indices.reverse()
            edge(piece,'arm_'+side,indices)
        edge(piece,'shoulder_l',[rows*(cols+1)+c for c in range(9)])
        edge(piece,'shoulder_r',[rows*(cols+1)+c for c in range(16,25)])
        edge(piece,'neck',[rows*(cols+1)+c for c in (range(8,17) if sign>0 else range(16,7,-1))],True)
        # Smoothly release torso cloth below the neckline, while shoulders follow skin.
        for r in range(rows+1):
            for c in range(cols+1):
                i=r*(cols+1)+c
                if r==rows:
                    piece['MaxDistanceMm'][i]=0. if 8<=c<=16 else 3.
                    piece['AnchorBones'][i]='chest' if 8<=c<=16 else 'None'
                else:piece['MaxDistanceMm'][i]=min(12.,(rows-r)*1.2)
        p['Pieces'].append(piece)
    for side,y_sign in [('l',-1),('r',1)]:
        refs=[('front','arm_'+side),('back','arm_'+side)]
        dist,pos,_=chain(p,refs);n=len(pos)-1;sleeve_rows=10
        def sleeve3(u,v):
            phase=2*math.pi*u
            cuff=[8*math.sin(phase),y_sign*(30-5.0*math.cos(phase)),83-6.2*math.cos(phase)]
            return lerp(pos[round(u*n)],cuff,v)
        sleeve=panel('sleeve_'+side,'왼쪽 소매' if side=='l' else '오른쪽 소매',n,sleeve_rows,
                     lambda u,v:[dist[round(u*n)],v*16],sleeve3,[7 if side=='l' else 65,52])
        edge(sleeve,'cap',list(range(n+1)))
        edge(sleeve,'cuff',[sleeve_rows*(n+1)+c for c in range(n+1)],True,True)
        edge(sleeve,'under_front',[r*(n+1) for r in range(sleeve_rows+1)])
        edge(sleeve,'under_back',[r*(n+1)+n for r in range(sleeve_rows+1)])
        sleeve['MaxDistanceMm']=[min(8.,2.+i//(n+1)*.7) for i in range(len(sleeve['MaxDistanceMm']))]
        p['Pieces'].append(sleeve)
    band(p,'neck_rib','목 시보리',[('front','neck'),('back','neck')],2.5,[7,83],.9,
         offset=lambda q,v:[q[0]*(1-.10*v),q[1]*(1-.10*v),q[2]+2.5*v],anchor='chest')
    for side in ['l','r']:
        seam(p,'shoulder_'+side,'어깨 '+side,[('front','shoulder_'+side)],[('back','shoulder_'+side)])
        seam(p,'sleeve_'+side,'소매 부착 '+side,[('front','arm_'+side),('back','arm_'+side)],
             [('sleeve_'+side,'cap')],before=['shoulder_l','shoulder_r'])
        seam(p,'side_'+side,'옆선 '+side,[('front','side_'+side)],[('back','side_'+side)],before=['sleeve_l','sleeve_r'])
        seam(p,'under_'+side,'소매 아래 '+side,[('sleeve_'+side,'under_front')],[('sleeve_'+side,'under_back')],before=['sleeve_l','sleeve_r'])
    seam(p,'rib_close','시보리 고리',[('neck_rib','end_start')],[('neck_rib','end_finish')],before=['side_l','side_r','under_l','under_r'])
    seam(p,'neck','목 시보리 달기',[('front','neck'),('back','neck')],[('neck_rib','attach')],
         kind='ElasticBand',ratio=.9,before=['rib_close'])
    for pid,eid in [('front','hem'),('back','hem'),('sleeve_l','cuff'),('sleeve_r','cuff')]:
        seam(p,pid+'_hem','밑단 접어박기 '+pid,[(pid,eid)],kind='Hem',before=['neck'])
    return p


def shorts():
    p=base('DolphinShorts','돌핀쇼츠','Bottom');cols=16;rows=20;crotch=9;slit=5
    # Four quarter panels share a center rise above the crotch and form two tubes below.
    waist_y=(MEASUREMENTS['waist_circumference']*10+8)/(2*math.pi)*1.2
    waist_x=waist_y*.8
    for side,ys in [('l',-1),('r',1)]:
        for face,xs in [('front',1),('back',-1)]:
            pid=face+'_'+side
            def flat(u,v):
                # Identical seam lengths across all mirrored panels, curved dolphin hem.
                return [u*25+3*(1-v)*(1-u),v*28+4*u**5*(1-v)]
            def assembled(u,v):
                theta=math.pi*u
                # The outer slit lips diverge below their common apex.
                hemi_x=xs*9*math.sin(theta)
                hemi_y=ys*(10-8*math.cos(theta))
                hem_z=43+5*u**5
                hip=[xs*12*math.cos(math.pi*u/2),ys*17*math.sin(math.pi*u/2),61]
                center=[0,0,50]
                middle=lerp(center,hip,u)
                # Smooth crotch arch with a center front/back rise meeting at z54.
                middle=[xs*11*math.sin(math.pi*u),ys*19*u,50+11*u]
                top=[xs*waist_x*math.cos(math.pi*u/2),ys*waist_y*math.sin(math.pi*u/2),72]
                if v<=crotch/rows:
                    q=lerp([hemi_x,hemi_y,hem_z],middle,v/(crotch/rows))
                else:
                    t=(v-crotch/rows)/(1-crotch/rows)
                    q=lerp(middle,top,t)
                    q[0]=xs*(abs(middle[0])+(abs(top[0])-abs(middle[0]))*t**.3)
                if v<slit/rows:
                    q[0]+=xs*2.8*(1-v/(slit/rows))*u**10
                return q
            piece=panel(pid,('왼쪽 ' if ys<0 else '오른쪽 ')+('앞판' if xs>0 else '뒤판'),cols,rows,
                        flat,assembled,[7+(0 if xs>0 else 35),7+(0 if ys<0 else 45)])
            # center has inseam below the crotch and rise above it.
            edge(piece,'inseam',[r*(cols+1) for r in range(crotch+1)])
            edge(piece,'rise',[r*(cols+1) for r in range(crotch,rows+1)])
            edge(piece,'side',[r*(cols+1)+cols for r in range(slit,rows+1)])
            # Leg boundary: inseam -> curved hem -> slit apex on front, reverse on back.
            hem=list(range(cols+1))+[r*(cols+1)+cols for r in range(1,slit+1)]
            if xs<0:hem.reverse()
            edge(piece,'hem',hem,True)
            waist=[rows*(cols+1)+c for c in range(cols+1)]
            if xs<0:waist.reverse()
            # Full waist chain: front_l reversed, back_l forward, back_r reverse, front_r forward.
            if ys>0:waist.reverse()
            edge(piece,'waist',waist,True)
            for r in range(rows+1):
                for c in range(cols+1):
                    i=r*(cols+1)+c
                    piece['MaxDistanceMm'][i]=min(10.,(rows-r)*.8)
                    if r>=rows-1:piece['AnchorBones'][i]='pelvis'
            p['Pieces'].append(piece)
    waist_refs=[('front_l','waist'),('back_l','waist'),('back_r','waist'),('front_r','waist')]
    band(p,'waistband','허리밴드',waist_refs,4.,[85,7],.92,
         offset=lambda q,v:[q[0]*(1-.04*v),q[1]*(1-.04*v),q[2]+4*v],anchor='pelvis')
    for side in ['l','r']:
        # The free binding edge follows the opening, including both lips of the slit.
        band(p,'binding_'+side,'다리 바인딩 '+side,[(f'front_{side}','hem'),(f'back_{side}','hem')],1.5,
             [85,24 if side=='l' else 42],offset=lambda q,v:[q[0]*(1+.025*v),q[1],q[2]-1.5*v])
    for face in ['front','back']:
        seam(p,'rise_'+face,'밑위 '+face,[(face+'_l','rise')],[(face+'_r','rise')])
    for side in ['l','r']:
        seam(p,'inseam_'+side,'안쪽 다리 '+side,[('front_'+side,'inseam')],[('back_'+side,'inseam')],before=['rise_front','rise_back'])
        seam(p,'side_'+side,'옆선 (트임 제외) '+side,[('front_'+side,'side')],[('back_'+side,'side')],before=['inseam_l','inseam_r'])
    seam(p,'waist_close','허리밴드 고리',[('waistband','end_start')],[('waistband','end_finish')],before=['side_l','side_r'])
    seam(p,'waist','허리밴드 달기',waist_refs,[('waistband','attach')],kind='ElasticBand',ratio=.92,before=['waist_close'])
    for side in ['l','r']:
        seam(p,'binding_close_'+side,'바인딩 고리 '+side,[('binding_'+side,'end_start')],[('binding_'+side,'end_finish')],before=['waist'])
        seam(p,'binding_'+side,'곡선 밑단과 트임 '+side,[('front_'+side,'hem'),('back_'+side,'hem')],
             [('binding_'+side,'attach')],kind='Binding',before=['binding_close_'+side])
    return p


def fit_to_dummy(p):
    """Constrain assembled vertices outside the measured source mesh, preserving seam equality."""
    import bpy
    from mathutils import Vector
    from mathutils.bvhtree import BVHTree
    if 'DummyBody' not in bpy.data.objects:
        bpy.ops.wm.open_mainfile(filepath=str(ROOT/'ArtSource/Dummy/LaceLoftDummy.blend'))
    body=bpy.data.objects['DummyBody'];bvh=BVHTree.FromObject(body,bpy.context.evaluated_depsgraph_get())
    shell=p['Pieces']
    cache={};adjusted=0;maximum=0.
    for piece in shell:
        for i,point in enumerate(piece['AssembledPositionsMm']):
            key=tuple(round(x,5) for x in point)
            if key not in cache:
                original=Vector((-point[1]/10,-point[0]/10,point[2]/10));position=original.copy()
                for iteration in range(6):
                    nearest,normal,_,distance=bvh.find_nearest(position)
                    signed=(position-nearest).dot(normal)
                    if signed>=.12:break
                    position=nearest+normal*.125
                delta=(position-original).length
                adjusted+=delta>.001;maximum=max(maximum,delta)
                cache[key]=[-position.y*10,-position.x*10,position.z*10]
            piece['AssembledPositionsMm'][i]=cache[key]
    # Relax the assembled surface as a welded mesh, projecting after each pass.
    # A point-only push can flip narrow shoulder triangles around the body bulge.
    neighbors=defaultdict(set);edge_count=Counter();local_lookup={}
    for piece in shell:
        mapping=[]
        for q in piece['AssembledPositionsMm']:
            # fit outputs can coincide; the pre-fit shared source key is recovered
            # below using the fitted coordinates, with a deterministic canonical id.
            mapping.append(tuple(round(x,6) for x in q))
        local_lookup[piece['PieceId']]=mapping
    unique={}
    for mapping in local_lookup.values():
        for key in mapping:
            if key not in unique:unique[key]=len(unique)
    positions=[Vector((-q[1]/10,-q[0]/10,q[2]/10)) for q in unique]
    for piece in shell:
        mapping=[unique[key] for key in local_lookup[piece['PieceId']]]
        local_lookup[piece['PieceId']]=mapping;indices=piece['TriangleIndices']
        for k in range(0,len(indices),3):
            tri=[mapping[x] for x in indices[k:k+3]]
            for a,b in zip(tri,tri[1:]+tri[:1]):
                neighbors[a].add(b);neighbors[b].add(a);edge_count[tuple(sorted((a,b)))]+=1
    boundary={v for e,n in edge_count.items() if n==1 for v in e}
    for iteration in range(45 if p['PatternId']=='TShirt' else 0):
        updated=[]
        for i,position in enumerate(positions):
            mean=sum((positions[j] for j in neighbors[i]),Vector())/len(neighbors[i])
            q=position.lerp(mean,.08 if i in boundary else .35)
            nearest,normal,_,distance=bvh.find_nearest(q)
            if (q-nearest).dot(normal)<.12:q=nearest+normal*.125
            updated.append(q)
        positions=updated
    if p['PatternId']=='TShirt':
        # Concave underarms need localized relaxation; global passes unnecessarily
        # shrink the free hems. Preserve body clearance during every local pass.
        centers=[Vector((1.7,0,8.0))]
        for iteration in range(45):
            updated=[]
            for i,position in enumerate(positions):
                weight=max(0.,1-min((position-center).length for center in centers))
                mean=sum((positions[j] for j in neighbors[i]),Vector())/len(neighbors[i])
                q=position.lerp(mean,.35*weight)
                for retry in range(6):
                    nearest,normal,_,distance=bvh.find_nearest(q)
                    if (q-nearest).dot(normal)>=.12:break
                    q=nearest+normal*.13
                updated.append(q)
            positions=updated
    for piece in shell:
        piece['AssembledPositionsMm']=[[-positions[i].y*10,-positions[i].x*10,positions[i].z*10]
                                        for i in local_lookup[piece['PieceId']]]
    for piece in p['Pieces']:
        if not any(word in piece['PieceId'] for word in ['rib','band','binding']):continue
        operation=next(s for s in p['Seams'] if s['Kind'] in ['Binding','ElasticBand'] and s['SideB'][0]['PieceId']==piece['PieceId'])
        refs=[(r['PieceId'],r['EdgeId']) for r in operation['SideA']]
        _,attached,_=chain(p,refs);cols=piece['_cols'];rows=piece['_rows']
        for r in range(rows+1):
            v=r/rows
            for c,q in enumerate(attached):
                if piece['PieceId']=='neck_rib':
                    radius=math.hypot(q[0],q[1]);scale=1+2.3*v/radius
                    value=[q[0]*scale,q[1]*scale,q[2]+v]
                elif piece['PieceId']=='waistband':value=[q[0],q[1],q[2]+4*v]
                else:value=[q[0]*(1+.025*v),q[1],q[2]-1.5*v]
                piece['AssembledPositionsMm'][r*(cols+1)+c]=value
    p['Authoring']['SurfaceFit']={'Method':('shared seam coordinates; 45 welded and 45 local underarm relaxation/projection passes' if p['PatternId']=='TShirt' else 'shared seam coordinates; body surface projection; no global relaxation'),
        'ClearanceCm':.12,'AdjustedVertices':adjusted,'MaximumAdjustmentCm':maximum}


def finalize(p):
    """Weld only declared seam correspondence, then prove expected open topology."""
    offsets={};count=0
    for piece in p['Pieces']:
        offsets[piece['PieceId']]=count;count+=len(piece['SimPositionsMm'])
    parent=list(range(count))
    def find(i):
        while parent[i]!=i:parent[i]=parent[parent[i]];i=parent[i]
        return i
    stitches=[]
    for s in p['Seams']:
        if s['Kind']=='Hem':continue
        refs=lambda side:[(r['PieceId'],r['EdgeId']) for r in s[side]]
        _,a,am=chain(p,refs('SideA'));_,b,bm=chain(p,refs('SideB'))
        assert len(am)==len(bm),(s['SeamId'],len(am),len(bm))
        for av,bv,ap,bp in zip(am,bm,a,b):
            assert math.dist(ap,bp)<.001,(s['SeamId'],ap,bp)
            i=offsets[av[0]]+av[1];j=offsets[bv[0]]+bv[1]
            parent[find(j)]=find(i);stitches.append([i,j])
    edges=Counter();faces=[];face_source=[];edge_faces=defaultdict(list)
    for piece in p['Pieces']:
        indices=piece['TriangleIndices'];off=offsets[piece['PieceId']]
        for k in range(0,len(indices),3):
            f=[find(off+x) for x in indices[k:k+3]]
            assert len(set(f))==3,(piece['PieceId'],k)
            faces.append(f)
            face_source.append((piece,k))
            for i in range(3):
                a,b=f[i],f[(i+1)%3];key=tuple(sorted([a,b]))
                edges[key]+=1;edge_faces[key].append((len(faces)-1,1 if a<b else -1))
    assert max(edges.values())==2,Counter(edges.values())
    adjacency=defaultdict(list)
    for connected in edge_faces.values():
        if len(connected)==2:
            (a,da),(b,db)=connected
            adjacency[a].append((b,-da*db));adjacency[b].append((a,-da*db))
    orientation={0:1 if p['PatternId']=='TShirt' else -1};stack=[0]
    while stack:
        a=stack.pop()
        for b,delta in adjacency[a]:
            expected_sign=orientation[a]*delta
            if b in orientation:assert orientation[b]==expected_sign,'nonorientable seam'
            else:orientation[b]=expected_sign;stack.append(b)
    assert len(orientation)==len(faces),'disconnected cloth component'
    for fi,(piece,k) in enumerate(face_source):
        if orientation[fi]<0:
            indices=piece['TriangleIndices'];indices[k+1],indices[k+2]=indices[k+2],indices[k+1]
    from mathutils.bvhtree import BVHTree
    positions=[q for piece in p['Pieces'] for q in piece['AssembledPositionsMm']]
    surface=BVHTree.FromPolygons(positions,faces,all_triangles=True,epsilon=0)
    crossings=[(a,b) for a,b in surface.overlap(surface) if a<b and not set(faces[a])&set(faces[b])]
    assert not crossings,('nonadjacent assembled triangle intersections',len(crossings),
        [(face_source[a][0]['PieceId'],face_source[a][1]//3,face_source[b][0]['PieceId'],face_source[b][1]//3) for a,b in crossings[:12]])
    boundary=defaultdict(set)
    for (a,b),n in edges.items():
        if n==1:boundary[a].add(b);boundary[b].add(a)
    assert all(len(v)==2 for v in boundary.values()),'nonmanifold boundary'
    seen=set();loops=0
    for start in boundary:
        if start in seen:continue
        loops+=1;stack=[start]
        while stack:
            node=stack.pop()
            if node in seen:continue
            seen.add(node);stack.extend(boundary[node]-seen)
    vertices=len({find(i) for i in range(count)});chi=vertices-len(edges)+len(faces)
    expected=(4,-2) if p['PatternId']=='TShirt' else (3,-1)
    assert (loops,chi)==expected,(loops,chi,expected)
    p['Authoring']['Stitches2D']=stitches
    p['Authoring']['Topology']={'WeldedVertices':vertices,'Triangles':len(faces),'BoundaryLoops':loops,'EulerCharacteristic':chi,'ConnectedComponents':1,'Orientable':True,'NonadjacentTriangleIntersections':0}
    for piece in p['Pieces']:
        piece.pop('_cols');piece.pop('_rows')
    return p


if __name__=='__main__':
    folder=ROOT/'ArtSource/Patterns';folder.mkdir(parents=True,exist_ok=True)
    for pattern in [tee(),shorts()]:
        fit_to_dummy(pattern)
        finalize(pattern)
        path=folder/(pattern['PatternId']+'.json')
        path.write_text(json.dumps(pattern,ensure_ascii=False,separators=(',',':')),encoding='utf-8')
        print(pattern['PatternId'],pattern['Authoring']['Topology'])
