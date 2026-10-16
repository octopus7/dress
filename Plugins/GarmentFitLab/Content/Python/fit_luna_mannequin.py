"""REQ-028: fit limb proportions from the saved REQ-025 mannequin.

Run in background Blender with SKM_LunaMk2_Mannequin.blend loaded. Axial
deformation keeps limb girth and hand/foot size. New rest joints and skin use
the same segment maps; the original source, baseline and rig stay on disk.
"""
from pathlib import Path
import hashlib
import json
import bpy
import bmesh
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[4]
SOURCE = ROOT / 'Content/GarmentFitLab/Source'
NAME = 'SKM_LunaMk2_MannequinFit'
ARM_SCALE = 1.30
CALF_SCALE = .80
body = bpy.data.objects['SKM_LunaMk2_Mannequin']
rig = body.find_armature()
assert len(body.data.vertices) == 37507 and len(rig.data.bones) == 130
assert 'gfl_proportion_revision' not in body, 'Load the baseline, not the fitted output'
assert len(bpy.data.images) == 0
rig.data.pose_position = 'REST'
# Explicitly update both sides. Interactive X-mirror would overwrite the
# opposite side's imported axes while iterating the edit bones.
rig.data.use_mirror_x = False
for bone in rig.pose.bones:
    bone.matrix_basis = Matrix.Identity(4)
before = {b.name: {'head': b.head_local.copy(), 'tail': b.tail_local.copy(),
                   'parent': b.parent.name if b.parent else None,
                   'matrix': b.matrix_local.copy()} for b in rig.data.bones}

def chain_family(name):
    # IK targets live outside the deform hierarchy but must follow the wrists.
    if name in ('ik_hand_r', 'ik_hand_gun'):
        return 'r', 'hand'
    if name == 'ik_hand_l':
        return 'l', 'hand'
    ancestors = [name] + [b.name for b in rig.data.bones[name].parent_recursive]
    for side in ('l', 'r'):
        for joint, family in (('hand', 'hand'), ('lowerarm', 'forearm'), ('upperarm', 'upperarm')):
            if joint+'_'+side in ancestors:
                return side, family
    return None

families = {name: chain_family(name) for name in before}
ankle_z = sum(before['foot_'+s]['head'].z for s in ('l','r'))/2
knee_z = sum(before['calf_'+s]['head'].z for s in ('l','r'))/2
calf_height = knee_z-ankle_z
transition = .025

def compress_leg_z(z):
    # Integrate a tapered compression field: continuous first derivative at
    # ankle/knee, rigid feet below the ankle and rigid torso/thigh above knee.
    t = z-ankle_z
    if t <= 0:
        distance = 0
    elif t < transition:
        distance = t*t/(2*transition)
    elif t < calf_height-transition:
        distance = t-transition/2
    elif t < calf_height:
        distance = calf_height-transition-(calf_height-t)**2/(2*transition)
    else:
        distance = calf_height-transition
    return z-(1-CALF_SCALE)*calf_height*distance/(calf_height-transition)

def arm_delta(point, family):
    if family is None:
        return Vector((0,0,0))
    side, part = family
    shoulder = before['upperarm_'+side]['head']
    elbow = before['lowerarm_'+side]['head']
    wrist = before['hand_'+side]['head']
    upper = elbow-shoulder
    fore = wrist-elbow
    if part == 'hand':
        return (upper+fore)*(ARM_SCALE-1)
    if part == 'forearm':
        axis = fore.normalized()
        return (upper+axis*(point-elbow).dot(axis))*(ARM_SCALE-1)
    axis = upper.normalized()
    return axis*(point-shoulder).dot(axis)*(ARM_SCALE-1)

def weights_digest():
    return hashlib.sha256(repr([[(g.group,g.weight) for g in v.groups]
                                for v in body.data.vertices]).encode()).hexdigest()

weight_hash = weights_digest()
group_names = {g.index:g.name for g in body.vertex_groups}
for vertex in body.data.vertices:
    old = vertex.co.copy()
    weighted_delta = Vector((0,0,0))
    total = 0
    for group in vertex.groups:
        name = group_names[group.group]
        if name in before:
            total += group.weight
            weighted_delta += arm_delta(old, families[name])*group.weight
    assert total > 1e-6, f'Unweighted vertex {vertex.index}'
    vertex.co = old+weighted_delta/total
    vertex.co.z = compress_leg_z(vertex.co.z)
assert weights_digest() == weight_hash

bpy.ops.object.select_all(action='DESELECT')
rig.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.object.mode_set(mode='EDIT')
for bone in rig.data.edit_bones:
    old = before[bone.name]
    head = old['head']+arm_delta(old['head'], families[bone.name])
    head.z = compress_leg_z(head.z)
    # Imported tails are arbitrary display axes, not the next joint. Preserve
    # axes/roll; skeletal segment lengths are defined by joint head positions.
    transform = old['matrix'].copy()
    transform.translation = head
    bone.matrix = transform
    bone.length = (old['tail']-old['head']).length
bpy.ops.object.mode_set(mode='OBJECT')
bpy.context.view_layer.update()
after = {b.name:b.head_local.copy() for b in rig.data.bones}
max_rotation_error = 0
for name, old in before.items():
    current = rig.data.bones[name]
    assert (current.parent.name if current.parent else None) == old['parent']
    rotation_error = max(abs(current.matrix_local[row][col]-old['matrix'][row][col])
                         for row in range(3) for col in range(3))
    max_rotation_error = max(max_rotation_error, rotation_error)
    assert rotation_error < 5e-4, (name, rotation_error)
for side in ('l','r'):
    assert (after['hand_'+side]-after['ik_hand_'+side]).length < 1e-6
    assert (after['foot_'+side]-after['ik_foot_'+side]).length < 1e-6

def lengths(points):
    return {s:{label:(points[end+'_'+s]-points[start+'_'+s]).length*100
               for label,start,end in [('upperarm','upperarm','lowerarm'),
                   ('forearm','lowerarm','hand'),('thigh','thigh','calf'),('calf','calf','foot')]}
            for s in ('l','r')}

bm = bmesh.new()
bm.from_mesh(body.data)
nonmanifold = sum(not edge.is_manifold for edge in bm.edges)
degenerate = sum(face.calc_area() < 1e-12 for face in bm.faces)
assert nonmanifold == 0 and degenerate == 0
report = {'arm_scale':ARM_SCALE,'calf_scale':CALF_SCALE,
    'max_axis_component_error':max_rotation_error,
    'lengths_before_cm':lengths({n:b['head'] for n,b in before.items()}),
    'lengths_after_cm':lengths(after),
    'vertices':len(bm.verts),'triangles':len(bm.faces),
    'nonmanifold_edges':nonmanifold,'degenerate_faces':degenerate,
    'volume_m3':bm.calc_volume(),'skin_weights_sha256':weight_hash,
    'bounds_m':{'min':[min(v.co[i] for v in bm.verts) for i in range(3)],
                'max':[max(v.co[i] for v in bm.verts) for i in range(3)]},
    'bones':{n:{'parent':before[n]['parent'],'before_m':list(before[n]['head']),
                'after_m':list(p),'ue_cm':[-p.y*100,-p.x*100,p.z*100]}
             for n,p in after.items()}}
bm.free()
body.name = NAME
body.data.name = 'LunaMk2_MannequinFitSurface'
body['gfl_proportion_revision'] = 'REQ-028: arms 1.30, calf height 0.80'
body.data.materials.clear()
for material in list(bpy.data.materials):
    bpy.data.materials.remove(material,do_unlink=True)
body.select_set(True)
bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE/(NAME+'.blend')))
bpy.ops.export_scene.fbx(filepath=str(SOURCE/(NAME+'.fbx')),
    use_selection=True,object_types={'ARMATURE','MESH'},add_leaf_bones=False,
    bake_anim=False,use_armature_deform_only=False,axis_forward='-Y',axis_up='Z',
    apply_unit_scale=True,use_mesh_modifiers=True,mesh_smooth_type='FACE',
    path_mode='STRIP',embed_textures=False)
(ROOT/'Saved/GarmentFitLab/mannequin-fit-build.json').write_text(
    json.dumps(report,indent=2),encoding='utf-8')
print('GFL_MANNEQUIN_FIT_BUILT',json.dumps({k:v for k,v in report.items() if k != 'bones'}))
