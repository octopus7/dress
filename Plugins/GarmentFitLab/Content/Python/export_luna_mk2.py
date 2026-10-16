"""Run in Blender 4.5 background with the user-supplied blend loaded.

Only the Mk2 body and its armature are exported. The source blend is never saved.
"""
import bpy
import json
from pathlib import Path
from mathutils import Matrix

ROOT = Path(__file__).resolve().parents[4]
OUTPUT = ROOT / "Content/GarmentFitLab/Source/SKM_LunaMk2.fbx"
REPORT = ROOT / "Saved/GarmentFitLab/luna-source.json"
body = bpy.data.objects["SKM_LunaMk2"]
rig = body.find_armature()
assert body.type == "MESH" and rig is not None
report = {
    "source": bpy.data.filepath,
    "mesh": body.name,
    "armature": rig.name,
    "unit_system": bpy.context.scene.unit_settings.system,
    "meters_per_unit": bpy.context.scene.unit_settings.scale_length,
    "vertices": len(body.data.vertices),
    "polygons": len(body.data.polygons),
    "object_matrix": [list(row) for row in body.matrix_world],
    "armature_matrix": [list(row) for row in rig.matrix_world],
    "bones": [{"name": b.name, "parent": b.parent.name if b.parent else None,
               "head": list(rig.matrix_world @ b.head_local),
               "tail": list(rig.matrix_world @ b.tail_local)} for b in rig.data.bones],
    "excluded_meshes": [o.name for o in bpy.data.objects if o.type == "MESH" and o != body],
    "original_pose_position": rig.data.pose_position,
    "posed_bones": [b.name for b in rig.pose.bones
                    if max(abs(b.matrix_basis[i][j] - float(i == j))
                           for i in range(4) for j in range(4)) > 1e-5],
}
# Work in this disposable process only. Reference pose, no actions, no textures.
rig.animation_data_clear()
body.animation_data_clear()
rig.data.pose_position = "REST"
for bone in rig.pose.bones:
    bone.matrix_basis = Matrix.Identity(4)
body.data.materials.clear()
for poly in body.data.polygons:
    poly.material_index = 0
bpy.context.view_layer.update()
coords = [body.matrix_world @ v.co for v in body.data.vertices]
report["mesh_bounds_m"] = {"min": [min(v[i] for v in coords) for i in range(3)],
                           "max": [max(v[i] for v in coords) for i in range(3)]}
# Connected surface sizes document body separation without modifying topology.
neighbors = [set() for _ in body.data.vertices]
for edge in body.data.edges:
    a, b = edge.vertices
    neighbors[a].add(b)
    neighbors[b].add(a)
remaining = set(range(len(neighbors)))
components = []
while remaining:
    pending = [remaining.pop()]
    component = []
    while pending:
        v = pending.pop()
        component.append(v)
        for n in neighbors[v]:
            if n in remaining:
                remaining.remove(n)
                pending.append(n)
    components.append({"vertices": len(component),
                       "min": [min(coords[v][i] for v in component) for i in range(3)],
                       "max": [max(coords[v][i] for v in component) for i in range(3)]})
report["surface_components"] = sorted(components, key=lambda c: -c["vertices"])
bpy.ops.object.select_all(action="DESELECT")
body.select_set(True)
rig.select_set(True)
bpy.context.view_layer.objects.active = rig
OUTPUT.parent.mkdir(parents=True, exist_ok=True)
bpy.ops.export_scene.fbx(filepath=str(OUTPUT), use_selection=True,
    object_types={"ARMATURE", "MESH"}, add_leaf_bones=False,
    bake_anim=False, use_armature_deform_only=False,
    axis_forward="-Y", axis_up="Z", apply_unit_scale=True,
    use_mesh_modifiers=True, mesh_smooth_type="FACE", path_mode="STRIP",
    embed_textures=False)
REPORT.parent.mkdir(parents=True, exist_ok=True)
REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")
print("GFL_EXPORT_OK", OUTPUT)
