"""UE editor authoring of the explicitly supplied stage-1 input; no textures.

Run after export_luna_mk2.py. Reuse existing character/material assets, update
material assignments and the dedicated map's input and observation position.
"""
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[4]
DEST = "/Game/GarmentFitLab/Characters/LunaMk2"
MESH = DEST + "/SKM_LunaMk2"
MATERIAL = "/Game/GarmentFitLab/Materials/M_LunaMk2_Matte"
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
existing = assets.load_asset(MESH) if assets.does_asset_exist(MESH) else None
if existing is None or existing.get_editor_property("skeleton") is None:
    # Explicit legacy factory makes these FBX options authoritative.
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
    options = unreal.FbxImportUI()
    options.set_editor_property("automated_import_should_detect_type", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    options.set_editor_property("import_as_skeletal", True)
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("import_animations", False)
    options.set_editor_property("create_physics_asset", False)
    data = options.get_editor_property("skeletal_mesh_import_data")
    data.set_editor_property("convert_scene", True)
    data.set_editor_property("convert_scene_unit", True)
    data.set_editor_property("force_front_x_axis", True)
    data.set_editor_property("use_t0_as_ref_pose", False)
    data.set_editor_property("import_morph_targets", False)
    data.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(ROOT / "Content/GarmentFitLab/Source/SKM_LunaMk2.fbx"))
    task.set_editor_property("destination_path", DEST)
    task.set_editor_property("destination_name", "SKM_LunaMk2")
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("replace_existing", existing is not None)
    task.set_editor_property("factory", unreal.FbxFactory())
    task.set_editor_property("options", options)
    tools.import_asset_tasks([task])
    unreal.log("GFL_IMPORTED " + str(task.get_editor_property("imported_object_paths")))
mesh = assets.load_asset(MESH)
assert isinstance(mesh, unreal.SkeletalMesh), "Expected only the requested skeletal mesh"
assert assets.save_loaded_asset(mesh.get_editor_property("skeleton"))

if not assets.does_asset_exist(MATERIAL):
    mat = tools.create_asset("M_LunaMk2_Matte", "/Game/GarmentFitLab/Materials",
                             unreal.Material, unreal.MaterialFactoryNew())
    edit = unreal.MaterialEditingLibrary
    color = edit.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -360, 0)
    # Linear RGB; approximately sRGB #BBC8C7. Subtle teal, light gray.
    color.set_editor_property("constant", unreal.LinearColor(0.50, 0.58, 0.57, 1))
    edit.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    for y, value, prop in [(120, 0.0, unreal.MaterialProperty.MP_METALLIC),
                            (240, 0.9, unreal.MaterialProperty.MP_ROUGHNESS),
                            (360, 0.25, unreal.MaterialProperty.MP_SPECULAR)]:
        node = edit.create_material_expression(mat, unreal.MaterialExpressionConstant, -360, y)
        node.set_editor_property("r", value)
        edit.connect_material_property(node, "", prop)
    mat.set_editor_property("used_with_skeletal_mesh", True)
    edit.recompile_material(mat)
    assert assets.save_loaded_asset(mat)
mat = assets.load_asset(MATERIAL)
slots = mesh.get_editor_property("materials")
assert slots, "Missing material slot"
for index, slot in enumerate(slots):
    slot.set_editor_property("material_interface", mat)
    slots[index] = slot
mesh.set_editor_property("materials", slots)
# Struct-array edits do not reliably dirty the mesh package in editor Python.
assert assets.save_loaded_asset(mesh, only_if_is_dirty=False)

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level("/Game/Maps/L_GarmentFitLab")
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
all_actors = actors.get_all_level_actors()
previews = [a for a in all_actors if isinstance(a, unreal.GarmentFitPreviewActor)]
assert len(previews) == 1
preview = previews[0]
preview.set_editor_property("input_mesh", mesh)
preview.refresh_input()
assert preview.get_input_state() == unreal.GarmentFitInputState.READY
position = unreal.Vector(220, -100, 95)
rotation = unreal.MathLibrary.find_look_at_rotation(position, unreal.Vector(0, 0, 73))
for a in all_actors:
    if isinstance(a, unreal.PlayerStart):
        a.set_actor_location(position, False, False)
        a.set_actor_rotation(rotation, False)
    if isinstance(a, unreal.DirectionalLight):
        a.set_actor_rotation(unreal.Rotator(pitch=-45, yaw=145, roll=0), False)
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(position, rotation)
actors.set_selected_level_actors([preview])
assert levels.save_current_level()
unreal.log("GFL_MK2_AUTHORING_OK")
