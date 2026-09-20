# A-ROGUE boot cinematic, authored for Higgsfield 3D Jutsu (Blender 5.2).
# Run in scene_builder_3d_run_python after inspecting revision guards.
import bpy, math
from mathutils import Vector
S = bpy.context.scene
if len(S.objects):
    raise RuntimeError('Create this cinematic only in an inspected, empty project.')
S.render.engine = 'BLENDER_EEVEE'
S.render.resolution_x = 960
S.render.resolution_y = 540
S.render.resolution_percentage = 100
S.render.fps = 30
S.eevee.taa_render_samples = 4
S.eevee.use_fast_gi = False
S.frame_start = 1
S.frame_end = 190
S.render.image_settings.media_type = 'IMAGE'
S.render.image_settings.file_format = 'PNG'
S.view_settings.view_transform = 'AgX'
S.world = bpy.data.worlds.new('Midnight ambient')
S.world.use_nodes = True
S.world.node_tree.nodes['Background'].inputs['Color'].default_value = (0.045, 0.065, 0.085, 1)
S.world.node_tree.nodes['Background'].inputs['Strength'].default_value = 0.2

def material(name, color, metal=0, rough=.4, emission=None, strength=0):
    m = bpy.data.materials.new(name); m.use_nodes = True
    bs = m.node_tree.nodes.get('Principled BSDF')
    bs.inputs['Base Color'].default_value = (*color, 1)
    bs.inputs['Metallic'].default_value = metal
    bs.inputs['Roughness'].default_value = rough
    if emission:
        bs.inputs['Emission Color'].default_value = (*emission, 1)
        bs.inputs['Emission Strength'].default_value = strength
    return m

ivory = material('Warm ceramic polymer', (.31,.32,.285), .08,.32)
edge = material('Shadowed polymer recess', (.047,.057,.055),.1,.42)
navy = material('Floppy midnight ABS', (.012,.028,.045),.12,.28)
metal = material('Brushed stainless shutter', (.33,.39,.43),.88,.25)
black = material('Rubber and slot interior', (.003,.006,.009),0,.5)
paper = material('Warm archival paper', (.74,.77,.70),0,.58)
ink = material('Label graphite', (.008,.029,.030),0,.55)
gold = material('Copper contact', (.48,.245,.075),.8,.27)
glass = material('Smoked convex CRT glass', (.005,.017,.014),.28,.16)
phosphor = material('CRT green phosphor', (.016,.11,.068),.05,.3,(.03,1,.52),0)
ledmat = material('Read indicator', (.01,.06,.018),.1,.3,(.10,1,.38),0)
deskmat = material('Graphite satin desk', (.020,.030,.038),.32,.30)
wallmat = material('Machine room midnight blue', (.006,.013,.022),.3,.48)
rimmat = material('Cool overhead strip', (.13,.31,.34),.15,.28,(.20,.75,.8),2)
amber = material('Standby amber',(.16,.06,.008),.2,.3,(1,.37,.045),.6)

def cube(name, loc, dims, mat, bevel=0, parent=None):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
    o=bpy.context.object; o.name=name; o.dimensions=dims
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if mat: o.data.materials.append(mat)
    if bevel:
        b=o.modifiers.new('Manufactured edge radius','BEVEL'); b.width=bevel; b.segments=3
        o.modifiers.new('Weighted corner normals','WEIGHTED_NORMAL')
    if parent:
        o.parent=parent; o.location=loc
    return o

def sphere(name,loc,scale,mat,parent=None):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=16, location=loc)
    o=bpy.context.object; o.name=name; o.scale=scale; o.data.materials.append(mat)
    for p in o.data.polygons: p.use_smooth=True
    if parent: o.parent=parent; o.location=loc
    return o

def text(name,body,loc,size,mat,parent=None,align='CENTER'):
    c=bpy.data.curves.new(name,'FONT'); c.body=body; c.size=size
    c.align_x=align; c.align_y='CENTER'; c.extrude=.00005
    o=bpy.data.objects.new(name,c); S.collection.objects.link(o)
    o.location=loc; o.rotation_euler=(math.pi/2,0,0); c.materials.append(mat)
    if parent: o.parent=parent; o.location=loc
    return o

def light(name,kind,loc,color,power,size=1,target=(0,0,.3)):
    d=bpy.data.lights.new(name,kind); d.energy=power; d.color=color
    d.use_shadow_jitter=False
    if kind=='AREA': d.shape='DISK'; d.size=size
    if kind=='POINT': d.shadow_soft_size=size
    o=bpy.data.objects.new(name,d); S.collection.objects.link(o); o.location=loc
    o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler()
    return o

def key(obj,prop,t,value):
    setattr(obj,prop,value); obj.keyframe_insert(data_path=prop,frame=1+30*t)

def input_key(mat,input_name,t,value):
    sock=mat.node_tree.nodes['Principled BSDF'].inputs[input_name]
    sock.default_value=value; sock.keyframe_insert('default_value',frame=1+30*t)

cube('Desk slab',(0,.05,-.027),(2.6,2,.05),deskmat,.012)
cube('Far wall',(0,.85,.65),(3,.08,1.45),wallmat,.008)
for i in range(-6,7):
    cube('Wall service rib %02d'%i,(i*.19,.79,.65),(.012,.022,1.4),edge,.003)
cube('Wall illuminated seam',(0,.745,.51),(2.5,.009,.003),rimmat,.001)
for side in [-1,1]:
    cube('Equipment column '+str(side),(side*.85,.42,.33),(.2,.36,.66),wallmat,.018)
    for j in range(9):
        cube('Vent %s %s'%(side,j),(side*.85,.225,.1+j*.044),(.13,.008,.005),edge,.001)
    for j in range(3):
        sphere('Rack status %s %s'%(side,j),(side*.85-.055+j*.025,.22,.56),(.003,.002,.003),amber)
cube('Computer base',(0,0,.061),(.60,.40,.114),ivory,.009)
cube('Front fascia',(0,-.204,.061),(.57,.014,.084),ivory,.003)
cube('Base lower seam',(0,-.212,.016),(.56,.002,.003),edge,.001)
for side in [-1,1]:
    for yy in [-.135,.12]:
        cube('Rubber foot',(side*.235,yy,.004),(.055,.05,.012),black,.005)
cube('Floppy drive recessed bay',(.17,-.214,.071),(.164,.008,.042),edge,.003)
cube('Floppy drive bezel',(.17,-.220,.071),(.153,.008,.034),ivory,.002)
cube('Disk slot black cavity',(.17,-.226,.075),(.115,.006,.010),black,.001)
cube('Slot upper metal rail',(.17,-.230,.081),(.12,.003,.0025),metal,.0006)
cube('Disk eject key',(.226,-.231,.061),(.018,.009,.006),edge,.001)
cube('Read LED',(.112,-.232,.061),(.011,.002,.003),ledmat,.0005)
for i in range(8):
    cube('Front louver %02d'%i,(-.114,-.214,.035+i*.006),(.19,.004,.0023),edge,.0005)
cube('Power button',(-.244,-.22,.061),(.033,.014,.031),ivory,.003)
text('Case maker mark','A :  /  RECOVERY SYSTEM',(-.116,-.217,.099),.008,ink)
cube('Monitor pedestal',(0,.01,.134),(.19,.16,.036),edge,.009)
cube('CRT chassis',(0,.018,.322),(.48,.31,.353),ivory,.021)
cube('CRT front recess',(0,-.148,.325),(.435,.02,.299),edge,.016)
cube('CRT inner bezel',(0,-.161,.325),(.412,.015,.274),ivory,.013)
cube('CRT shadow cavity',(0,-.170,.330),(.382,.016,.248),black,.012)
cube('CRT glass',(0,-.181,.330),(.365,.012,.233),glass,.014)
cube('Monitor lower accent',(0,-.151,.165),(.27,.004,.0014),edge,.0005)
text('Monitor model','A - 18   /   PHOSPHOR',(-.11,-.152,.178),.008,ink)
cube('Monitor power jewel',(.185,-.153,.177),(.006,.003,.006),amber,.002)
for i in range(9):
    cube('Monitor side cooling %02d'%i,(.24,.03+i*.018,.325),(.005,.008,.15),edge,.001)
# Readable screen artwork is authored geometry; no generated text flicker.
text('CRT signature','A:\\ROGUE',(0,-.191,.350),.043,phosphor)
text('CRT diagnostic','RECOVERY / ONLINE',(0,-.191,.391),.007,phosphor)
text('CRT footer','READING SYSTEM MEMORY',(0,-.191,.264),.0065,phosphor)
for i in range(18):
    m=material('Sector phosphor %02d'%i,(.007,.035,.019),.0,.4,(.08,1,.5),0)
    cube('Read sector %02d'%i,(-.144+i*.017,-.192,.286),(.013,.001,.003),m,.0003)
    tt=4.94+i*(45-i)/1000
    input_key(m,'Emission Strength',tt,0)
    input_key(m,'Emission Strength',tt+.028,2)
# A powerless CRT has no externally lit letters; reveal artwork with the tube.
for o in list(S.objects):
    if o.name in ['CRT signature','CRT diagnostic','CRT footer'] or o.name.startswith('Read sector '):
        key(o,'scale',0,(0,0,0))
        key(o,'scale',4.72,(0,0,0))
        key(o,'scale',4.90,(1,1,1))
# Soft glass tube ignition line: grows from a horizontal filament.
line=cube('CRT ignition filament',(0,-.193,.330),(.35,.001,.0015),phosphor,.0003)
key(line,'scale',0,(0,1,1)); key(line,'scale',4.49,(0,1,1))
key(line,'scale',4.64,(1,1,1)); key(line,'scale',5.12,(1,1,0))
input_key(phosphor,'Emission Strength',0,0)
input_key(phosphor,'Emission Strength',4.50,0)
input_key(phosphor,'Emission Strength',4.67,1.1)
input_key(phosphor,'Emission Strength',4.77,.55)
input_key(phosphor,'Emission Strength',4.94,1.8)
input_key(phosphor,'Emission Strength',5.30,1.1)
input_key(ledmat,'Emission Strength',0,0)
input_key(ledmat,'Emission Strength',4.20,0)
input_key(ledmat,'Emission Strength',4.26,3)
input_key(ledmat,'Emission Strength',4.38,1)
input_key(ledmat,'Emission Strength',4.5,2)

disk=bpy.data.objects.new('Floppy choreography',None); S.collection.objects.link(disk)
cube('Floppy ABS shell',(0,0,0),(.096,.008,.094),navy,.002,disk)
cube('Floppy inset label',(0,-.0043,-.020),(.083,.001,.041),paper,.0013,disk)
text('Disk title','A:\\ROGUE',(0,-.00505,-.013),.012,ink,disk)
text('Disk serial','SYSTEM / 18 SECTORS',(0,-.00505,-.025),.0038,ink,disk)
for i in range(3):
    cube('Label ruled line %d'%i,(-.003,-.005,-.032-i*.003),(.067-i*.008,.00015,.00035),ink,0,disk)
cube('Shutter recessed channel',(0,-.0043,.026),(.068,.0006,.040),black,.001,disk)
cube('Magnetic window',(-.009,-.0048,.026),(.021,.0004,.030),gold,.001,disk)
shutter=cube('Sliding steel shutter',(0,-.0052,.026),(.052,.0013,.040),metal,.001,disk)
cube('Shutter punched window',(-.006,-.00085,0),(.012,.0003,.027),black,.0004,shutter)
for i in range(6):
    cube('Shutter brushed grain %d'%i,(.012+i*.003,-.00083,0),(.00015,.0001,.033),edge,0,shutter)
for xx in [-.041,.041]:
    cube('Write protect aperture',(xx,-.0047,-.041),(.004,.001,.005),black,.0005,disk)
    for zz in [-.035,.005]:
        sphere('Mould fastening point',(xx,-.0048,zz),(.0009,.00045,.0009),metal,disk)
cube('Rear hub recess',(0,.0047,-.007),(.04,.001,.04),black,.002,disk)
sphere('Rear steel hub',(0,.0055,-.007),(.015,.001,.015),metal,disk)
for t,p,r in [
 (0,(0,-.7,.302),(.08,0,-.28)),
 (.78,(0,-.7,.348),(0,0,.04)),
 (1.2,(0,-.7,.35),(0,0,0)),
 (1.85,(0,-.7,.35),(.035,.055,-.075)),
 (2.2,(0,-.7,.35),(.035,.055,-.075)),
 (2.66,(.13,-.57,.34),(-.82,.14,-.12)),
 (3.2,(.17,-.42,.21),(-math.pi/2,0,0)),
 (3.63,(.17,-.30,.075),(-math.pi/2,0,0)),
 (3.9,(.17,-.325,.075),(-math.pi/2,0,0)),
 (4.2,(.17,-.13,.075),(-math.pi/2,0,0)),
 (6.3,(.17,-.13,.075),(-math.pi/2,0,0))]:
    key(disk,'location',t,p); key(disk,'rotation_euler',t,r)
for t,x in [(0,0),(1.25,0),(1.55,.030),(1.95,.030),(2.2,0)]:
    key(shutter,'location',t,(x,-.0052,.026))

light('Macro cool key','AREA',(-.32,-.65,.83),(.63,.84,1),34,.44,(0,-.55,.28))
light('Warm edge','AREA',(.58,.18,.7),(1,.68,.34),36,.45)
light('Front fill','AREA',(-.4,-1.0,.5),(.49,.75,.85),12,.8)
light('Top softbox','AREA',(.0,.12,1.25),(.64,.89,.94),40,.7)
light('Portable rim point','POINT',(.48,.18,.56),(.2,.62,1),4,.14)
crtlight=light('Screen bounce','AREA',(0,-.24,.34),(.11,1,.55),0,.32,(0,-.8,0))
for t,e in [(0,0),(4.5,0),(4.67,2),(4.77,1),(4.94,4),(5.3,2)]:
    key(crtlight.data,'energy',t,e)
camdata=bpy.data.cameras.new('Cinematic 50 mm'); cam=bpy.data.objects.new('Delivery camera',camdata)
S.collection.objects.link(cam); S.camera=cam; camdata.lens=55
camdata.clip_start=.007; camdata.clip_end=50
# Keyframed focus target keeps macro material detail crisp and the reveal readable.
focus=bpy.data.objects.new('Focus pull',None); S.collection.objects.link(focus)
camdata.dof.use_dof=True; camdata.dof.focus_object=focus; camdata.dof.aperture_fstop=5.6
for t,pos,target,lens in [
 (0,(.012,-1.015,.365),(0,-.7,.335),55),
 (1.2,(.0,-1.005,.362),(0,-.7,.35),55),
 (2.2,(-.015,-1.01,.37),(0,-.7,.35),55),
 (2.66,(.26,-1.24,.55),(.08,-.37,.30),48),
 (3.2,(.55,-1.32,.63),(.055,-.07,.255),48),
 (3.63,(.46,-.82,.31),(.17,-.25,.085),52),
 (4.08,(.37,-.64,.22),(.16,-.16,.076),52),
 (4.38,(.37,-.64,.22),(.16,-.16,.076),52),
 (5.12,(0,-.98,.36),(0,-.181,.33),55),
 (5.5,(0,-.98,.36),(0,-.181,.33),55),
 (5.66,(0,-1.015,.36),(0,-.181,.33),55),
 (6.30,(0,-.232,.33),(0,-.181,.33),55)]:
    cam.location=pos
    rot=(Vector(target)-cam.location).to_track_quat('-Z','Y').to_euler()
    key(cam,'location',t,pos); key(cam,'rotation_euler',t,rot)
    key(camdata,'lens',t,lens); key(focus,'location',t,target)
# Bezier AUTO_CLAMPED curves avoid overshoots at the slot and camera endpoints.
for action in bpy.data.actions:
    for layer in action.layers:
        for strip in layer.strips:
            if strip.type=='KEYFRAME':
                for slot in action.slots:
                    bag=strip.channelbag(slot)
                    if bag:
                        for fc in bag.fcurves:
                            for k in fc.keyframe_points:
                                k.interpolation='BEZIER'
                                k.handle_left_type='AUTO_CLAMPED'; k.handle_right_type='AUTO_CLAMPED'
S.frame_set(155)
target=artifacts.file(name='boot-lighting-preview.png',media_type='image/png')
S.render.filepath=target.path
bpy.ops.render.render(write_still=True)
published=target.publish()
result={'preview':published,'frame_count':190,'fps':30,'size':[960,540],
        'objects':len(S.objects),'timing':'latch 4.20; power 4.50; dive 5.50; native bridge 6.056'}
