"""Reproducible S1 sculpted blockout; final UV/art pass follows cloth gate.
Run in a clean background Blender. Dimensions are centimetres throughout.
"""
import bpy, math, json, sys
from pathlib import Path
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[2]
FINAL = '--final' in sys.argv
sys.path.insert(0,str(Path(__file__).resolve().parent))
OUT = ROOT / 'ArtSource/Dummy'
OUT.mkdir(parents=True, exist_ok=True)
(OUT / 'QA').mkdir(exist_ok=True)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 0.01
parts = []

def curved_profile(points):
    if not FINAL:return points
    result=[]
    for i in range(len(points)-1):
        a,b=points[i],points[i+1]
        left=points[max(0,i-1)];right=points[min(len(points)-1,i+2)]
        span=b[0]-a[0]
        for step in range(6):
            t=step/6;values=[a[0]+span*t]
            for k in range(1,len(a)):
                m0=(b[k]-left[k])/(b[0]-left[0])
                m1=(right[k]-a[k])/(right[0]-a[0])
                value=(2*t**3-3*t*t+1)*a[k]+(t**3-2*t*t+t)*span*m0+(-2*t**3+3*t*t)*b[k]+(t**3-t*t)*span*m1
                values.append(value)
            result.append(tuple(values))
    return result+[points[-1]]

def loft(name, rings, segments=32):
    # z, centre x/y, width/depth radii; shaped cross sections, not stock primitives.
    rings=curved_profile(rings)
    verts, faces = [], []
    for z, x, y, rx, ry in rings:
        for j in range(segments):
            a = 2 * math.pi * j / segments
            verts.append((x + rx * math.cos(a), y + ry * math.sin(a), z))
    for i in range(len(rings)-1):
        for j in range(segments):
            a = i*segments+j; b = i*segments+(j+1)%segments
            faces.append((a,b,b+segments,a+segments))
    faces.append(tuple(reversed(range(segments))))
    faces.append(tuple((len(rings)-1)*segments+j for j in range(segments)))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces); mesh.update()
    obj = bpy.data.objects.new(name, mesh); scene.collection.objects.link(obj)
    parts.append(obj)
    return obj

loft('Torso', [(5.5,0,0,.75,.6),(5.8,0,0,1.32,.84),(6.35,0,0,1.57,1.0),
    (6.85,0,0,1.5,.98),(7.4,0,0,1.27,.81),(8,0,-.01,1.29,.84),
    (8.6,0,-.02,1.52,.96),(9.05,0,0,1.67,.87),(9.35,0,.02,1.5,.74),
    (9.65,0,0,.72,.59),(10.1,0,0,.62,.57)])
loft('Head', [(9.85,0,0,.42,.4),(10.03,0,-.05,.95,.83),(10.35,0,-.08,1.55,1.4),
    (10.8,0,-.05,1.98,1.78),(11.35,0,0,2.29,2.01),(12.0,0,.06,2.48,2.18),
    (12.7,0,.11,2.52,2.22),(13.4,0,.15,2.38,2.11),(14,0,.17,2.08,1.9),
    (14.5,0,.18,1.6,1.48),(14.83,0,.18,.95,.88),(15,0,.18,.08,.08)],48)

def limb(name, start, end, profile):
    profile=curved_profile(profile)
    start, end = Vector(start), Vector(end)
    direction = (end-start).normalized()
    u = Vector((0,1,0)); v = direction.cross(u).normalized()
    verts, faces = [], []
    n = 24
    for t,r1,r2 in profile:
        c=start+(end-start)*t
        for j in range(n):
            a=j*2*math.pi/n
            verts.append(c+u*(math.cos(a)*r1)+v*(math.sin(a)*r2))
    for i in range(len(profile)-1):
        for j in range(n):
            a=i*n+j;b=i*n+(j+1)%n;faces.append((a,b,b+n,a+n))
    faces += [tuple(reversed(range(n))),tuple((len(profile)-1)*n+j for j in range(n))]
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(verts,[],faces);mesh.update()
    obj=bpy.data.objects.new(name,mesh);scene.collection.objects.link(obj);parts.append(obj)

bones=[('root',(0,0,0),(0,0,1),None),('pelvis',(0,0,5.9),(0,0,7),'root'),
       ('spine_01',(0,0,7),(0,0,8.3),'pelvis'),('chest',(0,0,8.3),(0,0,9.3),'spine_01'),
       ('neck',(0,0,9.3),(0,0,10.5),'chest'),('head',(0,0,10.5),(0,0,14.2),'neck')]
for s,side in [(1,'l'),(-1,'r')]:
    shoulder=(s*1.48,0,9.15); elbow=(s*2.88,0,7.51); wrist=(s*4.07,-.04,6.1)
    hand=(s*4.58,-.12,5.5)
    limb('Arm_'+side,shoulder,wrist,[(-.08,.3,.3),(0,.57,.6),(.18,.58,.59),
        (.4,.45,.45),(.52,.43,.43),(.65,.48,.46),(.85,.36,.34),(1,.28,.28),(1.03,.25,.25)])
    limb('Palm_'+side,wrist,hand,[(-.12,.24,.24),(.18,.28,.4),(.5,.29,.43),(.8,.24,.36),(1,.12,.15)])
    limb('Thumb_'+side,(s*4.03,-.15,5.98),(s*4.02,-.27,5.42),[(0,.2,.21),(.4,.2,.2),(.85,.16,.16),(1,.06,.06)])
    hip=(s*.85,0,6.2);knee=(s*.96,-.07,3.48);ankle=(s*.98,.02,.9)
    loft('Leg_'+side,[(.6,s*.98,.05,.34,.36),(.95,s*.98,.04,.37,.39),
        (1.5,s*.98,.05,.48,.49),(2.2,s*.98,.06,.61,.6),(2.9,s*.97,-.02,.55,.56),
        (3.5,s*.96,-.07,.48,.51),(4.0,s*.94,0,.63,.65),(4.8,s*.91,0,.78,.81),
        (5.6,s*.87,0,.81,.84),(6.2,s*.85,0,.69,.75),(6.5,s*.84,0,.44,.5)])
    loft('Foot_'+side,[(0,s*.98,-.43,.4,.82),(.12,s*.98,-.43,.53,.98),
        (.38,s*.98,-.46,.56,1.03),(.63,s*.98,-.32,.51,.92),(.87,s*.98,-.12,.37,.58),
        (1.0,s*.98,.02,.29,.36)])
    bones += [(f'clavicle_{side}',(0,0,9.2),shoulder,'chest'),(f'upperarm_{side}',shoulder,elbow,f'clavicle_{side}'),
              (f'forearm_{side}',elbow,wrist,f'upperarm_{side}'),(f'hand_{side}',wrist,hand,f'forearm_{side}'),
              (f'thigh_{side}',hip,knee,'pelvis'),(f'calf_{side}',knee,ankle,f'thigh_{side}'),
              (f'foot_{side}',ankle,(s*.98,-1.2,.35),f'calf_{side}')]

# Union creates a continuous body at shoulder/hip/neck intersections.
bpy.ops.object.select_all(action='DESELECT')
for o in parts:o.select_set(True)
bpy.context.view_layer.objects.active=parts[0]
bpy.ops.object.join();body=bpy.context.object;body.name='DummyBody'
remesh=body.modifiers.new('Continuous sculpt surface','REMESH');remesh.mode='VOXEL';remesh.voxel_size=.085
bpy.ops.object.modifier_apply(modifier=remesh.name)
smooth=body.modifiers.new('Surface relaxation','SMOOTH');smooth.factor=.65;smooth.iterations=4
bpy.ops.object.modifier_apply(modifier=smooth.name)
# Normalize geometry to precisely 15 cm and floor after surface processing.
zmin=min(v.co.z for v in body.data.vertices);zmax=max(v.co.z for v in body.data.vertices)
for v in body.data.vertices:v.co.z=(v.co.z-zmin)*15/(zmax-zmin)
for p in body.data.polygons:p.use_smooth=True
if not FINAL:
    # Temporary S1 UVs; final mode uses deliberate anatomical seams below.
    bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=1.15,island_margin=.02)
    bpy.ops.object.mode_set(mode='OBJECT')
mat=bpy.data.materials.new('M_Dummy_Source');mat.use_nodes=True
bsdf=mat.node_tree.nodes.get('Principled BSDF');bsdf.inputs['Base Color'].default_value=(.86,.86,.86,1)
bsdf.inputs['Roughness'].default_value=.58
body.data.materials.append(mat)

armdata=bpy.data.armatures.new('SKEL_Dummy');arm=bpy.data.objects.new('Armature',armdata);scene.collection.objects.link(arm)
bpy.context.view_layer.objects.active=arm;body.select_set(False);arm.select_set(True)
bpy.ops.object.mode_set(mode='EDIT')
for name,h,t,parent in bones:
    b=armdata.edit_bones.new(name);b.head=h;b.tail=t
    if parent:b.parent=armdata.edit_bones[parent]
bpy.ops.object.mode_set(mode='OBJECT')
body.select_set(True)
bpy.ops.object.parent_set(type='ARMATURE_AUTO')
bpy.context.view_layer.objects.active=body;arm.select_set(False)
bpy.ops.object.vertex_group_limit_total(limit=4)
bpy.ops.object.vertex_group_normalize_all(lock_active=False)
unweighted=[v.index for v in body.data.vertices if sum(g.weight for g in v.groups)<.99]
assert not unweighted, f'Unweighted vertices: {len(unweighted)}'
if FINAL:
    print('S3_WEIGHTED_BODY_READY',len(body.data.vertices),flush=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'Candidate_PreUV.blend'))
    from dummy_uv import author_uv
    author_uv(body,OUT)

def section_perimeter(z):
    # Intersect actual mesh edges with horizontal plane and measure exterior convex contour.
    pts=[]
    for e in body.data.edges:
        a,b=[body.data.vertices[i].co for i in e.vertices]
        if (a.z-z)*(b.z-z)<0:
            p=a+(b-a)*((z-a.z)/(b.z-a.z))
            # For torso measurement exclude the hanging arms.
            if abs(p.x)<1.8:pts.append((p.x,p.y))
    pts=sorted(set(pts))
    def cross(o,a,b):return (a[0]-o[0])*(b[1]-o[1])-(a[1]-o[1])*(b[0]-o[0])
    lower=[];upper=[]
    for p in pts:
        while len(lower)>=2 and cross(lower[-2],lower[-1],p)<=0:lower.pop()
        lower.append(p)
    for p in reversed(pts):
        while len(upper)>=2 and cross(upper[-2],upper[-1],p)<=0:upper.pop()
        upper.append(p)
    hull=lower[:-1]+upper[:-1]
    return sum(math.dist(hull[i],hull[(i+1)%len(hull)]) for i in range(len(hull)))

measure={'version':'candidate-2' if FINAL else 'blockout-1','status':'S3_validation_pending' if FINAL else 'provisional_until_S3','unit':'cm','height':15.0,
    'source':'actual mesh section convex contour, arm regions excluded',
    'chest_circumference':section_perimeter(8.6),'waist_circumference':section_perimeter(7.4),
    'hip_circumference':section_perimeter(6.4),'vertices':len(body.data.vertices),
    'skeleton_version':1,'unweighted_vertices':0,'maximum_influences':4,
    'bones':[{ 'name':n,'head':h,'tail':t,'parent':p} for n,h,t,p in bones]}
(OUT/'Measurements.json').write_text(json.dumps(measure,indent=2),encoding='utf-8')

# Author source pose clips; no procedural UI or engine content replacement.
scene.render.fps=30
for name,rotations in [('A',{}),('ElbowKnee90',{'forearm_l':(0,0,-math.pi/2),'calf_l':(math.pi/2,0,0)}),
    ('HeadTurn',{'head':(0,math.pi/4,0)}),('ArmsRaised',{'upperarm_l':(0,0,-1.0),'upperarm_r':(0,0,1.0)})]:
    if arm.animation_data:arm.animation_data.action=None
    for pb in arm.pose.bones:
        pb.rotation_mode='XYZ';pb.rotation_euler=(0,0,0)
    for n,r in rotations.items():arm.pose.bones[n].rotation_euler=r
    for f in [1,30]:
        for pb in arm.pose.bones:pb.keyframe_insert(data_path='rotation_euler',frame=f,group=pb.name)
    arm.animation_data.action.name='Dummy_'+name
    arm.animation_data.action.use_fake_user=True
arm.animation_data.action=None
for pb in arm.pose.bones:pb.rotation_euler=(0,0,0)
scene.frame_set(1)
bpy.ops.object.select_all(action='DESELECT');body.select_set(True);arm.select_set(True)
bpy.context.view_layer.objects.active=arm
bpy.ops.export_scene.fbx(filepath=str(OUT/'LaceLoftDummy.fbx'),use_selection=True,object_types={'MESH','ARMATURE'},
    add_leaf_bones=False,axis_forward='-Y',axis_up='Z',apply_unit_scale=True,apply_scale_options='FBX_SCALE_ALL',
    use_armature_deform_only=True,bake_anim=False,mesh_smooth_type='FACE')

# Studio views are source QA; separate UE renders required for import acceptance.
bpy.ops.object.camera_add(location=(0,-32,8))
camera=bpy.context.object;camera.name='QA_Camera';scene.camera=camera;camera.data.type='ORTHO';camera.data.ortho_scale=18
scene.render.engine='CYCLES';scene.cycles.samples=24
scene.render.resolution_x=800;scene.render.resolution_y=900;scene.render.resolution_percentage=100
scene.world.color=(.25,.25,.25)
for loc,power,size in [((6,-12,20),1600,10),((-10,-4,10),1000,8),((0,7,18),1800,8)]:
    bpy.ops.object.light_add(type='AREA',location=loc);lamp=bpy.context.object;lamp.data.energy=power;lamp.data.shape='DISK';lamp.data.size=size
    lamp.rotation_euler=(Vector((0,0,8))-lamp.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'LaceLoftDummy.blend'))
for name,loc in [('Front',(0,-32,7.5)),('Side',(32,0,7.5)),('Back',(0,32,7.5))]:
    camera.location=loc;camera.rotation_euler=(Vector((0,0,7.5))-camera.location).to_track_quat('-Z','Y').to_euler()
    stage='S3' if FINAL else 'S1'
    scene.render.filepath=str(OUT/'QA'/f'{stage}_Source_{name}.png');bpy.ops.render.render(write_still=True)
print('LACELOFT_DUMMY_BLOCKOUT_COMPLETE',json.dumps(measure))
