# blender -b --python colormonkey.py -- in.obj out.obj
# gives every vertex a colour from its position (x->r, y->g, z->b, 0..1
# over the bounding box) and exports the OBJ with the "v x y z r g b"
# colour extension
import bpy, sys
argv = sys.argv[sys.argv.index("--")+1:]
src, dst = argv
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.wm.obj_import(filepath=src)
for ob in bpy.context.scene.objects:
    if ob.type != 'MESH':
        continue
    me = ob.data
    lo = [min(v.co[i] for v in me.vertices) for i in range(3)]
    hi = [max(v.co[i] for v in me.vertices) for i in range(3)]
    col = me.color_attributes.new(name="Col", type='FLOAT_COLOR', domain='POINT')
    for v in me.vertices:
        c = [(v.co[i]-lo[i])/(hi[i]-lo[i]) for i in range(3)]
        col.data[v.index].color = (c[0], c[1], c[2], 1.0)
    me.color_attributes.active_color = col
bpy.ops.wm.obj_export(filepath=dst, export_colors=True, export_materials=False,
                      export_normals=True, export_uv=True, apply_modifiers=True)
