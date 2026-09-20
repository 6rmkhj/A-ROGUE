# New-game cinematic

Authored for this game with Higgsfield 3D Jutsu. No externally supplied footage,
models or textures are used; the machine, floppy and screen artwork are editable
Blender geometry and materials. Typography uses Blender's bundled default font.

- Project: https://higgsfield.ai/3d-jutsu/25761aee-abd1-4d6a-80a9-156eee0a5b9c
- Committed scene revision: 3 (Blender 5.2)
- Authoring recipe: `tools/higgsfield_boot_scene.py`
- Playback master: 960 × 540, 30 fps, 190 frames; native exit at 6.32 seconds
- Runtime asset: `assets/boot_intro.arvf`, embedded as RCDATA 201
- Packed size: 7,279,697 bytes
- SHA-256: `a84ff7e8f594981d4404206f7a33f99c51f4c849a0cd5d9e995e59c3e7028faf`

## Rebuild

Export revision 3's `.blend` from Higgsfield into
`build/higgsfield/boot_intro.blend`, then run Blender (5.1.2 also verified):

```text
blender -b build/higgsfield/boot_intro.blend -P tools/render_boot_film.py -- --start 1 --end 190 --samples 16
ffmpeg -framerate 30 -start_number 1 -i build/higgsfield/frames/%04d.png -frames:v 190 -vf hqdn3d=2:1.5:3:2.25 -c:v libx264 -crf 14 -pix_fmt yuv420p build/higgsfield/boot_intro.mp4
python tools/pack_boot_film.py build/higgsfield/boot_intro.mp4
tools\check-boot-film.bat
tools\check-fx.bat --render
```

The local renderer raises Eevee sampling to 16, with mild spatial/temporal
denoising in the video encode; camera, choreography, lighting and materials
remain those of the committed scene. It also exports the projected
label centres used by the native title-to-disk handoff. These must be regenerated
if the first 0.8 seconds of disk/camera animation change.

Blender 5.1's loader does not migrate the 5.2 Principled emission socket's indexed
animation path. The renderer rebinds those existing curves by socket name (no
retiming) and asserts that CRT power is animated before rendering.

Audio, skip handling, title snapshot and final arrival bridge remain native and
share `BOOT_INTRO_MS`. The video contains no audio or hard-coded user interface.
The ARVF and its runtime require neither Blender nor FFmpeg on players' systems.

## Native composition preview

`tools\render-boot-preview.bat` renders 240 deterministic BMPs using the actual
title snapshot, film, arrival shutter and first story screen. It runs offscreen
and does not change player saves or start audio. Convert the frames into an
eight-second silent review clip:

```text
ffmpeg -framerate 30 -start_number 0 -i build/boot-preview/frame_%04d.bmp -frames:v 240 -c:v libx264 -crf 17 -pix_fmt yuv420p -movflags +faststart build/boot-preview/new-game-seamless.mp4
```

The BMPs are disposable build output (approximately 986 MB); the MP4 is a review
artifact, not a runtime dependency. The game retains its synchronized effects.
