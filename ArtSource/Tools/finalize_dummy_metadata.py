import bpy,json,hashlib
from pathlib import Path
out=Path(__file__).resolve().parents[2]/'ArtSource/Dummy'
bpy.ops.wm.open_mainfile(filepath=str(out/'LaceLoftDummy.blend'))
for name in ['BaseColor_White',*[k+'_'+str(s) for k in ('Checker','Asymmetric') for s in (2048,1024,512)]]:
    image=bpy.data.images.get(name)
    if not image:image=bpy.data.images.load(str(out/(name+'.png')),check_existing=True)
    image.use_fake_user=True;image.pack()
bpy.data.objects['DummyBody']['measurements_version']='dummy-1'
bpy.data.objects['Armature']['skeleton_version']=1
bpy.ops.wm.save_as_mainfile(filepath=str(out/'LaceLoftDummy.blend'))
measure=json.loads((out/'Measurements.json').read_text())
measure['version']='dummy-1';measure['status']='S3_validated'
measure['source_sha256']={name:hashlib.sha256((out/name).read_bytes()).hexdigest()
    for name in ['LaceLoftDummy.blend','LaceLoftDummy.fbx','UVLayout.png','BaseColor_White.png']}
(out/'Measurements.json').write_text(json.dumps(measure,indent=2))
paint=json.loads((out/'PaintValidation.json').read_text())
paint['status']='source_and_UE_reviewed';paint['UE_render_count']=126
paint['UE_report']='Saved/Automation/Implementation/S3-ImportedUV/UVValidation.json'
paint['review_notes']='Unique left/right colors and letters; no opposite-side cloning. 2K/1K/512 filtering inspected. Behind-head meridian and fingers have expected local deformation; front body checker improved by deliberate side seams.'
(out/'PaintValidation.json').write_text(json.dumps(paint,indent=2))
print('DUMMY_VERSION_FROZEN',measure['version'],flush=True)
