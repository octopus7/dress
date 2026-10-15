"""Save inspectable Blender panels/UVs from the exact gameplay pattern source."""
import bpy,json
from pathlib import Path
from mathutils import Vector

root=Path(__file__).resolve().parents[2]
out=root/'ArtSource/Garments';out.mkdir(parents=True,exist_ok=True)
(out/'QA').mkdir(exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(root/'ArtSource/Dummy/LaceLoftDummy.blend'))
scene=bpy.context.scene;scene.cycles.samples=16
colors={'TShirt':(.18,.42,.35,1),'DolphinShorts':(.58,.19,.12,1),'Trim':(.87,.8,.63,1)}
materials={}
for name,color in colors.items():
    mat=bpy.data.materials.new(name);mat.use_nodes=True
    shader=mat.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value=color;shader.inputs['Roughness'].default_value=.7
    materials[name]=mat
for pid in ['TShirt','DolphinShorts']:
    source=json.loads((root/'ArtSource/Patterns'/f'{pid}.json').read_text())
    collection=bpy.data.collections.new(pid);scene.collection.children.link(collection)
    for piece in source['Pieces']:
        mesh=bpy.data.meshes.new(pid+'_'+piece['PieceId'])
        vertices=[(-y/10,-x/10,z/10) for x,y,z in piece['AssembledPositionsMm']]
        indices=piece['TriangleIndices'];faces=[indices[i:i+3] for i in range(0,len(indices),3)]
        mesh.from_pydata(vertices,[],faces);mesh.update()
        obj=bpy.data.objects.new(mesh.name,mesh);collection.objects.link(obj)
        obj['PieceId']=piece['PieceId'];obj['PatternId']=pid;obj['PatternVersion']=source['PatternVersion']
        obj['SourceFile']='../Patterns/'+pid+'.json';obj['Units']='cm (mm source / 10)'
        uv=mesh.uv_layers.new(name='UV0')
        for poly in mesh.polygons:
            poly.use_smooth=True
            for li in poly.loop_indices:
                vi=mesh.loops[li].vertex_index
                uv.data[li].uv=[(piece['SimPositionsMm'][vi][k]+piece['LayoutMm'][k])/200 for k in range(2)]
        name=piece['PieceId'];trim='rib' in name or 'binding' in name or name=='waistband'
        mesh.materials.append(materials['Trim' if trim else pid])
        thickness=obj.modifiers.new('Render fabric thickness (not simulation topology)','SOLIDIFY');thickness.thickness=.02
        thickness.offset=0
bpy.ops.wm.save_as_mainfile(filepath=str(out/'LaceLoftGarments.blend'))
camera=scene.camera;camera.data.ortho_scale=12
for name,location in [('Front',(0,-32,7.7)),('Side',(32,0,7.7)),('Back',(0,32,7.7))]:
    camera.location=location;camera.rotation_euler=(Vector((0,0,7.7))-camera.location).to_track_quat('-Z','Y').to_euler()
    scene.render.filepath=str(out/'QA'/f'S4_Source_{name}.png');bpy.ops.render.render(write_still=True)
print('GARMENT_SOURCE_PREVIEW_COMPLETE',flush=True)
