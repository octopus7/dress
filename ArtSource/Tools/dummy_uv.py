"""Deliberate anatomical UV seams for the LaceLoft doll; keeps left/right separate."""
import bpy,json,numpy as np
from collections import defaultdict
from uv_validation import validate

def author_uv(body,out):
    print('S3_UV_BEGIN',flush=True)
    mesh=body.data
    bpy.context.view_layer.objects.active=body
    bpy.ops.object.select_all(action='DESELECT');body.select_set(True)
    group_names={g.index:g.name for g in body.vertex_groups}
    def region(face):
        c=face.center
        if c.z>9.9:return 'head'
        if c.z>9.5 and abs(c.x)<.9:return 'neck'
        if abs(c.x)>3.98:return 'hand_l' if c.x>0 else 'hand_r'
        weights=defaultdict(float)
        for vi in face.vertices:
            for g in mesh.vertices[vi].groups:weights[group_names[g.group]]+=g.weight
        for side in ('l','r'):
            arm=sum(weights[n+'_'+side] for n in ('upperarm','forearm','hand'))
            if arm>len(face.vertices)*.52:return 'arm_'+side
        if c.z<5.7 and (c.z<5.2 or abs(c.x)>.15):
            suffix='_l' if c.x>0 else '_r'
            if c.z<.22:return 'sole'+suffix
            if c.z<.9:return 'foot'+suffix
            return 'leg'+suffix
        return 'torso_front' if c.y<0 else 'torso_back'
    mesh.update()
    regions=[region(p) for p in mesh.polygons]
    face_edges=defaultdict(list)
    for p in mesh.polygons:
        for key in p.edge_keys:face_edges[tuple(sorted(key))].append(p.index)
    for edge in mesh.edges:
        faces=face_edges[tuple(sorted(edge.vertices))]
        edge.use_seam=False
        if len(faces)!=2:continue
        a,b=[mesh.polygons[i].center for i in faces]
        ra,rb=[regions[i] for i in faces]
        if ra!=rb:edge.use_seam=True;continue
        c=(a+b)*.5
        if ra in ('head','neck'):
            edge.use_seam=(a.x<0)!=(b.x<0) and c.y>0
        elif ra.startswith(('arm','hand')):
            axis_x=1.48+(9.15-c.z)*(4.07-1.48)/(9.15-6.1)
            edge.use_seam=(a.y<0)!=(b.y<0) and abs(c.x)<axis_x
        elif ra.startswith('leg'):
            edge.use_seam=(a.y<0)!=(b.y<0) and abs(c.x)<.97
        elif ra.startswith('foot'):
            center=.98 if c.x>0 else -.98
            edge.use_seam=(a.x<center)!=(b.x<center) and c.y>0
    uv=mesh.uv_layers.new(name='UV0',do_init=False)
    mesh.uv_layers.active_index=0;uv.active_render=True
    bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT')
    print('S3_UV_UNWRAP',flush=True)
    bpy.ops.uv.unwrap(method='ANGLE_BASED',fill_holes=True,correct_aspect=True)
    bpy.ops.uv.minimize_stretch(fill_holes=True,blend=0.0,iterations=100)
    print('S3_UV_SCALE_PACK',flush=True)
    bpy.ops.uv.average_islands_scale(scale_uv=False,shear=False)
    bpy.ops.uv.pack_islands(rotate=True,rotate_method='CARDINAL',scale=True,merge_overlap=False,
        margin_method='FRACTION',margin=34/2016,shape_method='CONCAVE')
    bpy.ops.object.mode_set(mode='OBJECT')
    print('S3_UV_VALIDATE',flush=True)
    # Edit-mode flush replaces UV custom-data storage; reacquire its RNA wrapper.
    uv=mesh.uv_layers.active
    coordinates=np.empty(len(mesh.loops)*2,dtype=np.float32)
    uv.uv.foreach_get('vector',coordinates)
    coordinates=coordinates*(2016/2048)+(16/2048)
    uv.uv.foreach_set('vector',coordinates)
    coordinates=coordinates.reshape((-1,2))
    mesh.calc_loop_triangles()
    triangles=[{'vertices':list(t.vertices),'xyz':[list(mesh.vertices[v].co) for v in t.vertices],
                'uv':[coordinates[l].tolist() for l in t.loops],
                'region':regions[t.polygon_index]} for t in mesh.loop_triangles]
    (out/'UVTriangles.json').write_text(json.dumps(triangles,separators=(',',':')))
    report=validate(triangles)
    report['seam_edges']=sum(e.use_seam for e in mesh.edges)
    report['anatomical_regions']=dict((r,regions.count(r)) for r in sorted(set(regions)))
    (out/'UVValidation.json').write_text(json.dumps(report,indent=2))
    # Blender's built-in PNG layout exporter requires a GPU context in 5.2.
    # Technical line rasterization keeps the background authoring path CPU-only.
    layout=np.ones((2048,2048,4),dtype=np.float32)
    for t in triangles:
        points=np.asarray(t['uv'])*2047
        for i in range(3):
            a,b=points[i],points[(i+1)%3]
            steps=max(2,int(np.linalg.norm(b-a)*1.5)+1)
            pixels=np.rint(np.linspace(a,b,steps)).astype(int).clip(0,2047)
            layout[pixels[:,1],pixels[:,0],:3]=(.08,.17,.24)
    image=bpy.data.images.new('UVLayout',width=2048,height=2048,alpha=True)
    image.pixels.foreach_set(layout.ravel());image.filepath_raw=str(out/'UVLayout.png');image.file_format='PNG';image.save()
    print('UV_VALIDATION',json.dumps({k:v for k,v in report.items() if k!='islands'}),flush=True)
