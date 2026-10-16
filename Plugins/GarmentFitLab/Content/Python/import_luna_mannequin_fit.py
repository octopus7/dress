"""Import REQ-028 with its own rest skeleton, preserving earlier inputs."""
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[4]
DEST = '/Game/GarmentFitLab/Characters/LunaMk2'
NAME = 'SKM_LunaMk2_MannequinFit'
assets = unreal.EditorAssetLibrary
assert not assets.does_asset_exist(DEST+'/'+NAME), 'Fitted input exists; use explicit reimport for revisions'
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
options = unreal.FbxImportUI()
for prop,value in {'automated_import_should_detect_type':False,
    'mesh_type_to_import':unreal.FBXImportType.FBXIT_SKELETAL_MESH,
    'import_as_skeletal':True,'import_mesh':True,'import_materials':False,
    'import_textures':False,'import_animations':False,'create_physics_asset':False,
    'skeleton':None}.items():
    options.set_editor_property(prop,value)
data = options.get_editor_property('skeletal_mesh_import_data')
for prop,value in {'convert_scene':True,'convert_scene_unit':True,'force_front_x_axis':True,
    'use_t0_as_ref_pose':False,'update_skeleton_reference_pose':False,
    'import_morph_targets':False,'normal_import_method':unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS}.items():
    data.set_editor_property(prop,value)
task = unreal.AssetImportTask()
for prop,value in {'filename':str(ROOT/'Content/GarmentFitLab/Source'/(NAME+'.fbx')),
    'destination_path':DEST,'destination_name':NAME,'automated':True,
    'save':False,'replace_existing':False,'factory':unreal.FbxFactory(),'options':options}.items():
    task.set_editor_property(prop,value)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
mesh = assets.load_asset(DEST+'/'+NAME)
assert isinstance(mesh,unreal.SkeletalMesh)
skeleton = mesh.get_editor_property('skeleton')
assert isinstance(skeleton,unreal.Skeleton)
assert skeleton.get_path_name() == DEST+'/'+NAME+'_Skeleton.'+NAME+'_Skeleton'
mat = assets.load_asset('/Game/GarmentFitLab/Materials/M_LunaMk2_Matte')
slots = mesh.get_editor_property('materials')
for i,slot in enumerate(slots):
    slot.set_editor_property('material_interface',mat)
    slots[i] = slot
mesh.set_editor_property('materials',slots)
assert assets.save_loaded_asset(skeleton,only_if_is_dirty=False)
assert assets.save_loaded_asset(mesh,only_if_is_dirty=False)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level('/Game/Maps/L_GarmentFitLab')
actor = next(a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
             if isinstance(a,unreal.GarmentFitPreviewActor))
actor.set_editor_property('input_mesh',mesh)
actor.refresh_input()
assert actor.get_input_state() == unreal.GarmentFitInputState.READY
assert levels.save_current_level()
unreal.log('GFL_MANNEQUIN_FIT_IMPORT_OK')
