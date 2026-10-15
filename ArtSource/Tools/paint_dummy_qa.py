"""Build deterministic technical checker/left-right paint textures and source QA."""
import bpy,json,sys
import numpy as np
from pathlib import Path
from mathutils import Vector

root=Path(__file__).resolve().parents[2];out=root/'ArtSource/Dummy';size=2048
report=json.loads((out/'UVValidation.json').read_text())
if not report['passed']:raise RuntimeError('Refusing to approve paint on failed UV validation')
bpy.ops.wm.open_mainfile(filepath=str(out/'LaceLoftDummy.blend'))
body=bpy.data.objects['DummyBody'];scene=bpy.context.scene
triangles=json.loads((out/'UVTriangles.json').read_text())
yy,xx=np.mgrid[:size,:size]
cell=max(16,round(report['density_reference_px_per_cm']*.5))
checker=np.ones((size,size,4),dtype=np.float32)
checker[:,:,:3]=np.where(((xx//cell+yy//cell)%2)[...,None],.12,.8)
paint=np.zeros((size,size,4),dtype=np.float32);mask=np.zeros((size,size),dtype=bool)
glyphs={'L':['10000','10000','10000','10000','10000','10000','11111'],
        'R':['11110','10001','10001','11110','10100','10010','10001'],
        'F':['11111','10000','10000','11110','10000','10000','10000'],
        'B':['11110','10001','10001','11110','10001','10001','11110']}
letter_masks={}
for name,rows in glyphs.items():
    font=np.array([[c=='1' for c in row] for row in rows[::-1]])
    gx=(xx%96-28)//8;gy=(yy%96-20)//8
    good=(gx>=0)&(gx<5)&(gy>=0)&(gy<7)
    letter_masks[name]=good&font[gy.clip(0,6),gx.clip(0,4)]
for t in triangles:
    uv=np.asarray(t['uv'])*size
    minimum=np.maximum(0,np.floor(uv.min(axis=0)).astype(int));maximum=np.minimum(size-1,np.ceil(uv.max(axis=0)).astype(int))
    x0,y0=minimum;x1,y1=maximum
    y,x=np.mgrid[y0:y1+1,x0:x1+1];x=x+.5;y=y+.5
    a,b,c=uv;den=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
    w0=((b[1]-c[1])*(x-c[0])+(c[0]-b[0])*(y-c[1]))/den
    w1=((c[1]-a[1])*(x-c[0])+(a[0]-c[0])*(y-c[1]))/den
    inside=(w0>=-1e-6)&(w1>=-1e-6)&(w0+w1<=1+1e-6)
    center=np.mean(t['xyz'],axis=0)
    if t['region']=='head':
        letter='F' if center[1]<0 else 'B';color=(.16,.65,.26) if letter=='F' else (.86,.64,.1)
    else:
        letter='L' if center[0]>0 else 'R';color=(.78,.12,.09) if letter=='L' else (.09,.27,.82)
    patch=paint[y0:y1+1,x0:x1+1];patch[inside]=(*color,1)
    lettering=inside&letter_masks[letter][y0:y1+1,x0:x1+1];patch[lettering]=(.96,.96,.96,1)
    mask[y0:y1+1,x0:x1+1] |=inside
# Sixteen texels of color extension around UV coverage, measured at 2K.
for step in range(16):
    updated=mask.copy();colors=paint.copy()
    for dy,dx in ((0,1),(0,-1),(1,0),(-1,0),(1,1),(1,-1),(-1,1),(-1,-1)):
        source_mask=np.roll(mask,(dy,dx),(0,1));source=np.roll(paint,(dy,dx),(0,1))
        use=source_mask&~updated;colors[use]=source[use];updated|=use
    paint,mask=colors,updated
paint[~mask]=(.035,.035,.035,1)

def save_image(name,pixels):
    image=bpy.data.images.new(name,width=size,height=size,alpha=True)
    image.pixels.foreach_set(pixels.ravel());image.file_format='PNG';image.filepath_raw=str(out/(name+'.png'));image.save()
    return image

white=np.ones((size,size,4),dtype=np.float32);white[:,:,:3]=.8
white_image=save_image('BaseColor_White',white)
textures={}
for kind,pixels in [('Checker',checker),('Asymmetric',paint)]:
    for resolution in (2048,1024,512):
        image=save_image(kind+'_'+str(resolution),pixels)
        if resolution!=size:
            image.scale(resolution,resolution);image.save()
        textures[(kind,resolution)]=image
material=body.data.materials[0];nodes=material.node_tree.nodes
shader=nodes.get('Principled BSDF');texture=nodes.get('BaseColorTexture') or nodes.new('ShaderNodeTexImage')
texture.name='BaseColorTexture';texture.label='Replace this image to paint the unique UV0 atlas'
material.node_tree.links.new(texture.outputs['Color'],shader.inputs['Base Color'])
shader.inputs['Roughness'].default_value=.65
scene.cycles.samples=16
camera=scene.camera
for (kind,resolution),image in textures.items():
    texture.image=image
    for view,location in [('Front',(0,-32,7.5)),('Side',(32,0,7.5)),('Back',(0,32,7.5))]:
        camera.location=location;camera.rotation_euler=(Vector((0,0,7.5))-camera.location).to_track_quat('-Z','Y').to_euler()
        scene.render.filepath=str(out/'QA'/f'S3_{kind}_{resolution}_{view}.png')
        bpy.ops.render.render(write_still=True)
texture.image=white_image
for image in [white_image,*textures.values()]:
    image.use_fake_user=True
    image.pack()
bpy.ops.wm.save_as_mainfile(filepath=str(out/'LaceLoftDummy.blend'))
(out/'PaintValidation.json').write_text(json.dumps({'atlas':2048,'color_extension_pixels':16,
    'left':'red L','right':'blue R','head_front':'green F','head_back':'yellow B',
    'checker_nominal_cell_cm':.5,'checker_cell_pixels':cell,'source_render_resolutions':[2048,1024,512],
    'status':'source_images_generated; UE_filtering_and_visual_review_required'},indent=2))
print('SOURCE_PAINT_QA_COMPLETE',flush=True)
