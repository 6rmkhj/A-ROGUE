"""Render the committed Higgsfield .blend with local Blender GPU acceleration.

blender -b build/higgsfield/boot_intro.blend -P tools/render_boot_film.py -- --start 1 --end 190
The .blend must be exported from the documented Higgsfield project revision.
"""
import argparse
import json
from pathlib import Path
import sys
import time

import bpy
from bpy_extras.object_utils import world_to_camera_view
from mathutils import Vector

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--start", type=int, default=1)
parser.add_argument("--end", type=int, default=190)
parser.add_argument("--samples", type=int, default=16)
parser.add_argument("--output", type=Path, default=Path("build/higgsfield/frames"))
args = parser.parse_args(sys.argv[sys.argv.index("--") + 1:])
args.output.mkdir(parents=True, exist_ok=True)
s = bpy.context.scene
# Blender 5.2 inserts a Principled input before emission. The older loader
# restores named sockets, but not indexed FCurve paths: strength (29) would
# animate Thin Film Thickness in 5.1. Rebind the authored emission-only tracks
# by socket name, preserving every original key, handle and interpolation.
if bpy.app.version < (5, 2, 0):
    rebound = 0
    for material in bpy.data.materials:
        tree = material.node_tree
        if not tree or not tree.animation_data or not tree.animation_data.action:
            continue
        bsdf = tree.nodes.get("Principled BSDF")
        if not bsdf:
            continue
        action = tree.animation_data.action
        for layer in action.layers:
            for strip in layer.strips:
                if strip.type != "KEYFRAME":
                    continue
                for slot in action.slots:
                    bag = strip.channelbag(slot)
                    if bag:
                        for curve in bag.fcurves:
                            if curve.data_path == 'nodes["Principled BSDF"].inputs[29].default_value':
                                curve.data_path = bsdf.inputs["Emission Strength"].path_from_id("default_value")
                                rebound += 1
    print("Rebound emission animation tracks:", rebound)
s.frame_set(166)
strength = bpy.data.materials["CRT green phosphor"].node_tree.nodes["Principled BSDF"].inputs["Emission Strength"].default_value
if strength < 1:
    raise RuntimeError("CRT power animation did not survive scene loading")
s.render.engine = "BLENDER_EEVEE"
s.eevee.taa_render_samples = args.samples
s.render.resolution_x, s.render.resolution_y = 960, 540
s.render.resolution_percentage = 100
s.render.fps = 30
s.frame_start, s.frame_end, s.frame_step = args.start, args.end, 1
if hasattr(s.render.image_settings, "media_type"):
    s.render.image_settings.media_type = "IMAGE"
s.render.image_settings.file_format = "PNG"
s.render.image_settings.color_mode = "RGB"
s.render.image_settings.compression = 15
s.render.filepath = str(args.output.resolve()) + "/"

anchors = []
disk = bpy.data.objects["Floppy choreography"]
for frame in range(1, 26):
    s.frame_set(frame)
    p = world_to_camera_view(s, s.camera, disk.matrix_world @ Vector((0, -.005, -.020)))
    anchors.append([round(p.x * 10000), round((1 - p.y) * 10000)])
(args.output.parent / "label_anchors.json").write_text(json.dumps(anchors), encoding="utf-8")
tick = time.monotonic()
bpy.ops.render.render(animation=True)
print("BOOT_RENDER", json.dumps({"frames": [args.start, args.end],
    "samples": args.samples, "seconds": round(time.monotonic() - tick, 3), "anchors": anchors}))
