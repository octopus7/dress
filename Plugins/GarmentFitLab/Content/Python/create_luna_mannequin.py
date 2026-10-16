"""Author the REQ-025 mannequin derivative in a disposable Blender process.

Load the supplied SKM_LunaMk2.blend first. Never save over that source.
"""
from pathlib import Path
import bpy
import bmesh
import json
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[4]
SOURCE = ROOT / 'Content/GarmentFitLab/Source'
REPORT = ROOT / 'Saved/GarmentFitLab/mannequin-build.json'
body = bpy.data.objects['SKM_LunaMk2']
rig = body.find_armature()
body.shape_key_clear()
body.animation_data_clear()
rig.animation_data_clear()
rig.data.pose_position = 'REST'
for bone in rig.pose.bones:
    bone.matrix_basis = Matrix.Identity(4)

# Keep the main surface; discard separate collar/fastener/pad islands and loose
# vertices. Sleeves are connected to this surface and need reshaping below.
bm = bmesh.new()
bm.from_mesh(body.data)
remaining = set(bm.verts)
parts = []
while remaining:
    pending = [remaining.pop()]
    part = []
    while pending:
        vertex = pending.pop()
        part.append(vertex)
        for edge in vertex.link_edges:
            other = edge.other_vert(vertex)
            if other in remaining:
                remaining.remove(other)
                pending.append(other)
    parts.append(part)
parts.sort(key=len, reverse=True)
assert len(parts[0]) == 2760, 'Unexpected source topology; review before editing'
bmesh.ops.delete(bm, geom=[v for p in parts[1:] for v in p], context='VERTS')
bm.to_mesh(body.data)
bm.free()

def smoothstep(a, b, value):
    t = max(0.0, min(1.0, (value-a)/(b-a)))
    return t*t*(3.0-2.0*t)

# Remove the inflated sleeve and cuff silhouette without changing the skeleton
# or moving elbows/wrists. Fade the radial correction into the chest and forearm.
adjusted = 0
for side in ('l', 'r'):
    shoulder = rig.data.bones['upperarm_'+side].head_local.copy()
    elbow = rig.data.bones['lowerarm_'+side].head_local.copy()
    axis = elbow-shoulder
    length = axis.length
    axis.normalize()
    sign = 1.0 if side == 'l' else -1.0
    for v in body.data.vertices:
        if v.co.x*sign < 0.06 or not 0.975 < v.co.z < 1.18:
            continue
        delta = v.co-shoulder
        t = delta.dot(axis)/length
        radial = delta-axis*(t*length)
        if not -0.25 < t < 1.15 or radial.length < 1e-8:
            continue
        radius = 0.038*(1-max(0,min(1,t))) + 0.024*max(0,min(1,t))
        if radial.length <= radius:
            continue
        blend = smoothstep(.065,.103,v.co.x*sign)
        blend *= 1-smoothstep(.9,1.15,t)
        target = shoulder+axis*(t*length)+radial.normalized()*radius
        v.co = v.co.lerp(target,blend)
        adjusted += 1

# Preserve source skin weights as the nearest-face transfer reference.
weight_source = body.copy()
weight_source.data = body.data.copy()
weight_source.name = 'MannequinWeightReference'
bpy.context.collection.objects.link(weight_source)
weight_source.hide_render = True
body.modifiers.clear()
bpy.ops.object.select_all(action='DESELECT')
body.select_set(True)
bpy.context.view_layer.objects.active = body
remesh = body.modifiers.new('ContinuousMannequinSurface','REMESH')
remesh.mode = 'VOXEL'
remesh.voxel_size = .0025
remesh.use_smooth_shade = True
bpy.ops.object.modifier_apply(modifier=remesh.name)

# A sleeve has both inner and outer cuff rings. Project the rebuilt surface to
# one continuous upper-arm envelope so their old crease cannot remain visible.
for side in ('l','r'):
    shoulder = rig.data.bones['upperarm_'+side].head_local.copy()
    axis = rig.data.bones['lowerarm_'+side].head_local-shoulder
    length = axis.length
    axis.normalize()
    sign = 1 if side == 'l' else -1
    for v in body.data.vertices:
        t = (v.co-shoulder).dot(axis)/length
        if not -.05 < t < 1.05 or v.co.x*sign < .07:
            continue
        center = shoulder+axis*(t*length)
        radial = v.co-center
        if not 1e-6 < radial.length < .08:
            continue
        radius = .034*(1-max(0,min(1,t)))+.024*max(0,min(1,t))
        blend = smoothstep(.07,.11,v.co.x*sign)*smoothstep(-.05,.18,t)*(1-smoothstep(.82,1.05,t))
        v.co = v.co.lerp(center+radial.normalized()*radius,blend)

region = body.vertex_groups.new(name='MannequinSurfaceFairing')
for v in body.data.vertices:
    weight = smoothstep(.91,1.02,v.co.z)*(1-smoothstep(1.16,1.21,v.co.z))
    if weight > 0:
        region.add([v.index],weight,'REPLACE')
smooth = body.modifiers.new('FairShouldersAndCuffs','SMOOTH')
smooth.factor = 1.0
smooth.iterations = 45
smooth.vertex_group = region.name
bpy.ops.object.modifier_apply(modifier=smooth.name)
body.vertex_groups.remove(body.vertex_groups['MannequinSurfaceFairing'])

region = body.vertex_groups.new(name='BodyFinish')
for v in body.data.vertices:
    weight = (1-smoothstep(1.19,1.22,v.co.z))
    if .84 < v.co.z < 1.03:
        weight *= 1-smoothstep(.21,.245,abs(v.co.x))
    if weight > 0:
        region.add([v.index],weight,'REPLACE')
smooth = body.modifiers.new('SoftenOriginalFacets','SMOOTH')
smooth.factor = .7
smooth.iterations = 12
smooth.vertex_group = region.name
bpy.ops.object.modifier_apply(modifier=smooth.name)
body.vertex_groups.remove(body.vertex_groups['BodyFinish'])
simplify = body.modifiers.new('MannequinPreviewDensity','DECIMATE')
simplify.ratio = .2
simplify.use_collapse_triangulate = True
bpy.ops.object.modifier_apply(modifier=simplify.name)

transfer = body.modifiers.new('PreserveMk2SkinWeights','DATA_TRANSFER')
transfer.object = weight_source
transfer.use_vert_data = True
transfer.data_types_verts = {'VGROUP_WEIGHTS'}
transfer.vert_mapping = 'POLYINTERP_NEAREST'
bpy.ops.object.datalayout_transfer(modifier=transfer.name)
bpy.ops.object.modifier_apply(modifier=transfer.name)
# All output vertices must retain weights, including the repaired cuff surface.
unweighted = sum(not any(g.weight > 1e-6 for g in v.groups) for v in body.data.vertices)
assert unweighted == 0, f'{unweighted} vertices lost skin weights'
armature = body.modifiers.new('Armature','ARMATURE')
armature.object = rig
body.parent = rig
body.name = 'SKM_LunaMk2_Mannequin'
body.data.name = 'LunaMk2_MannequinSurface'
body.data.materials.clear()
for p in body.data.polygons:
    p.material_index = 0
    p.use_smooth = True

check = bmesh.new()
check.from_mesh(body.data)
bad_edges = sum(not e.is_manifold for e in check.edges)
assert bad_edges == 0, f'{bad_edges} non-manifold edges remain'
report = {'adjusted_source_vertices':adjusted,'vertices':len(check.verts),
          'faces':len(check.faces),'nonmanifold_edges':bad_edges,'unweighted_vertices':unweighted,
          'bounds_m':{'min':[min(v.co[i] for v in check.verts) for i in range(3)],
                      'max':[max(v.co[i] for v in check.verts) for i in range(3)]}}
check.free()

# The editable derivative contains only its surface and the required armature.
for other in list(bpy.data.objects):
    if other not in (body,rig):
        bpy.data.objects.remove(other,do_unlink=True)
for material in list(bpy.data.materials):
    bpy.data.materials.remove(material,do_unlink=True)
for texture_image in list(bpy.data.images):
    bpy.data.images.remove(texture_image,do_unlink=True)
bpy.data.orphans_purge(do_recursive=True)
SOURCE.mkdir(parents=True,exist_ok=True)
bpy.ops.object.select_all(action='DESELECT')
body.select_set(True)
rig.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE/'SKM_LunaMk2_Mannequin.blend'))
bpy.ops.export_scene.fbx(filepath=str(SOURCE/'SKM_LunaMk2_Mannequin.fbx'),
    use_selection=True,object_types={'ARMATURE','MESH'},add_leaf_bones=False,
    bake_anim=False,use_armature_deform_only=False,axis_forward='-Y',axis_up='Z',
    apply_unit_scale=True,use_mesh_modifiers=True,mesh_smooth_type='FACE',
    path_mode='STRIP',embed_textures=False)
REPORT.write_text(json.dumps(report,indent=2),encoding='utf-8')
print('GFL_MANNEQUIN_BUILT',json.dumps(report))
