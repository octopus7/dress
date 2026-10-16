"""Reopen the fitted .blend in a disposable Blender process and inspect FK bends.

Reports and renders go to Saved; no verification poses/materials are saved.
"""
from pathlib import Path
import hashlib
import json
import math
import bpy
import bmesh
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[4]
OUT = ROOT/'Saved/GarmentFitLab'
body = bpy.data.objects['SKM_LunaMk2_MannequinFit']
rig = body.find_armature()
assert len(bpy.data.objects) == 2 and len(bpy.data.images) == 0 and len(bpy.data.materials) == 0
build = json.loads((OUT/'mannequin-fit-build.json').read_text(encoding='utf-8'))
digest = hashlib.sha256(repr([[(g.group,g.weight) for g in v.groups]
                             for v in body.data.vertices]).encode()).hexdigest()
assert digest == build['skin_weights_sha256']
for bone in rig.data.bones:
    assert (bone.head_local-Vector(build['bones'][bone.name]['after_m'])).length < 1e-6
rig.data.pose_position = 'POSE'
for bone in rig.pose.bones:
    bone.matrix_basis = Matrix.Identity(4)
bpy.context.view_layer.update()
evaluated = body.evaluated_get(bpy.context.evaluated_depsgraph_get())
rest_error = max((a.co-b.co).length for a,b in zip(body.data.vertices,evaluated.data.vertices))
assert rest_error < 1e-5, f'Rest/bind mismatch {rest_error}'

def rotate_world(name, degrees):
    bone = rig.pose.bones[name]
    pivot = bone.head.copy()
    bone.matrix = Matrix.Translation(pivot) @ Matrix.Rotation(math.radians(degrees),4,'X') @ Matrix.Translation(-pivot) @ bone.matrix
    bpy.context.view_layer.update()

for name, angle in [('lowerarm_l',-80),('lowerarm_r',-65),('thigh_l',-20),('calf_l',65),('calf_r',35)]:
    rotate_world(name,angle)
evaluated = body.evaluated_get(bpy.context.evaluated_depsgraph_get())
bm = bmesh.new()
bm.from_mesh(evaluated.data)
degenerate = sum(face.calc_area() < 1e-12 for face in bm.faces)
nonmanifold = sum(not e.is_manifold for e in bm.edges)
assert degenerate == 0 and nonmanifold == 0
max_edge_ratio = max((evaluated.data.vertices[e.vertices[0]].co-evaluated.data.vertices[e.vertices[1]].co).length/
    (body.data.vertices[e.vertices[0]].co-body.data.vertices[e.vertices[1]].co).length for e in body.data.edges)
report = {'rest_bind_max_error_m':rest_error,'weights_unchanged':True,
          'nonmanifold_edges_bent':nonmanifold,'degenerate_faces_bent':degenerate,
          'max_bent_to_rest_edge_ratio':max_edge_ratio,'bent_volume_m3':bm.calc_volume()}
bm.free()
(OUT/'mannequin-fit-reopen.json').write_text(json.dumps(report,indent=2),encoding='utf-8')

mat = bpy.data.materials.new('VerificationOnlyMatte')
mat.use_nodes = True
bsdf = mat.node_tree.nodes.get('Principled BSDF')
bsdf.inputs['Base Color'].default_value = (.50,.58,.57,1)
bsdf.inputs['Roughness'].default_value = .9
body.data.materials.append(mat)
scene = bpy.context.scene
scene.render.engine = 'CYCLES'
scene.cycles.samples = 32
scene.render.resolution_x = 900
scene.render.resolution_y = 1050
scene.render.resolution_percentage = 100
scene.world.color = (.2,.2,.2)
camdata = bpy.data.cameras.new('VerificationCamera')
cam = bpy.data.objects.new('VerificationCamera',camdata)
scene.collection.objects.link(cam)
scene.camera = cam
camdata.type = 'ORTHO'
camdata.ortho_scale = 1.6
for name,loc,power,size in [('Key',(2,-3,3),350,3),('Fill',(-2,-1,2),180,3),('Back',(0,2,2),250,2)]:
    data = bpy.data.lights.new(name,'AREA')
    data.energy = power
    data.size = size
    light = bpy.data.objects.new(name,data)
    scene.collection.objects.link(light)
    light.location = loc
    light.rotation_euler = (Vector((0,0,.8))-light.location).to_track_quat('-Z','Y').to_euler()
for view,location in [('bend-front',(0,-4,.65)),('bend-side',(4,0,.65))]:
    cam.location = location
    cam.rotation_euler = (Vector((0,0,.65))-cam.location).to_track_quat('-Z','Y').to_euler()
    scene.render.filepath = str(OUT/('mannequin-fit-'+view+'.png'))
    bpy.ops.render.render(write_still=True)
print('GFL_MANNEQUIN_FIT_REOPEN_OK',json.dumps(report))
