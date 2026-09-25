# CRT filter

Shows the game the way a CRT television drew it: scanlines from a gaussian beam whose width follows
brightness, an NTSC signal simulation for the horizontal blend, glow, bloom and CRT gamma. It runs
RetroArch shader presets ([libretro/slang-shaders](https://github.com/libretro/slang-shaders), the
`crt-guest-advanced` family and friends) on the finished game image.

In game: **Settings > Graphics > CRT Filter**.

## What it does while enabled

- The game renders at the simulated TV's line count, **240 lines, 4:3 (320x240)** by default. The
  internal resolution, advanced resolution and N64 mode settings are overridden while the filter is on
  and apply again when it is off.
- The filtered image is displayed at an **integer multiple of the line count**, centred, so every
  scanline is equally thick: 3x on a 720p screen (handheld Switch), 4x at 1080p (docked, with a
  border), 6x at 1440p. Turn *Integer Scaling* off to fill the screen instead.
- Menus are never filtered.
- **Keep MSAA on.** It stands in for the edge anti-aliasing the N64 did in hardware before the signal
  reached the TV; without it the 240p image is harsher than the real thing ever was.
- Every parameter the loaded preset exposes can be edited live under *Shader Parameters*; changes
  persist per parameter.

## Presets

| Preset | What it is |
|---|---|
| 2Ship: Composite, soft beam | guest-advanced-ntsc with a wide horizontal beam and sharpening off |
| 2Ship: S-Video, clean signal (recommended) | the same without composite artifacts |
| 2Ship: RGB monitor, sharpest | no signal simulation, broadcast-monitor look |
| 2Ship: Living room TV | S-Video on a curved tube, Trinitron colours |
| 2Ship: Light scanlines | wider beams, less of the screen dark, for SDR displays |
| Guest Advanced NTSC / Guest Advanced / Fast | the stock shaders; *Fast* is much lighter on the GPU |
| CRT Royale, Geom, Lottes, Easymode, Hyllian, Aperture, Consumer | other well known CRT shaders |

The stock guest-advanced values are tuned for crisp 2D pixel art. A 3D N64 image through a TV was
much softer, which is what the 2Ship presets restore (`SIGMA_HOR 1.6`, `S_SHARP 0`, `HSHARPNESS 3.0`).

## How it runs on each platform

| Platform | Renderer | Shader runtime |
|---|---|---|
| Nintendo Switch | OpenGL | libultraship's built-in slang runtime, shaders bundled in the NRO's romfs |
| Windows | DirectX 11 | [librashader](https://github.com/SnowflakePowered/librashader) (`librashader.dll` next to `2ship.exe`), with HDR output |
| Windows, Linux | OpenGL | libultraship's built-in slang runtime |
| macOS | Metal | not yet (the menu reports the filter as unavailable) |

The built-in runtime (`libultraship/src/fast/backends/gfx_opengl_slang.cpp`) needs no Rust, no
glslang and no SPIRV-Cross, which is what makes it possible on the Switch: it reads the `.slangp`
preset, rewrites each `.slang` pass from Vulkan flavoured GLSL 450 into plain GLSL (uniform blocks
become std140 uniform buffers, descriptor set and binding qualifiers are dropped, samplers and
uniforms are matched by name) and lets the driver compile it. Pass scaling, filtering and wrap
modes, float and sRGB framebuffers, `#pragma format`, mipmapped inputs, LUT textures, aliases,
`PassOutput#`, `PassFeedback#` and `OriginalHistory#` are all supported, so the presets behave as they
do in RetroArch and under librashader.

### Switch notes

- The shaders are inside the NRO (`romfs:/shaders`). To try presets that are not bundled, put a
  `shaders` folder next to `2ship.nro` on the SD card; files there take precedence over the romfs.
- Expect the heavy presets (CRT Royale, Guest Advanced NTSC at 1080p) to drop below 60 FPS docked.
  The 2Ship presets at 240 lines and *Guest Advanced Fast* are the lightest. The *Switch performance
  mode* setting helps as well.
- HDR output is a Windows feature and does not appear in the Switch menu.

## Building

The shaders are fetched at configure time by `CMake/crt-shaders.cmake`: a sparse, blob-less clone of
libretro/slang-shaders pinned to `CRT_SHADERS_GIT_TAG`. Only the files listed in
`mm/crt-filter/shader-files.txt` (about 1.5 MB) are staged, next to the executable on desktop and
into `build/romfs/shaders` for the NRO. To build without network access point `CRT_SHADER_SOURCE_DIR`
at an existing checkout, or drop one into `../crt-deps/slang-shaders`. `-DCRT_FILTER_SHADERS=OFF`
leaves the shaders out.

For the DirectX 11 path on Windows, put `librashader.dll` from a
[librashader release](https://github.com/SnowflakePowered/librashader/releases) into
`../crt-deps/librashader/`; it is copied next to the executable. Without it DirectX 11 reports the
filter as unavailable and the OpenGL renderer still works.

To regenerate `shader-files.txt` after adding presets, walk the `shader#`, `textures` and `#include`
references of every preset the menu lists (`mm/2s2h/BenGui/CrtFilter.cpp`) from the slang-shaders
root and list the files relative to it.

## Troubleshooting

| Symptom | Likely cause / fix |
|---|---|
| Menu says *Could not load the shader preset* | The message carries the reason. *Preset not found*: the `shaders` folder is missing next to the executable (or in the romfs on Switch). |
| *... failed to compile* | The GL driver rejected the translated pass; the log (`logs/`) has the compiler output and the numbered GLSL. Report it with the preset name. |
| Image is black with the filter on | A pass produced no output. Check the log for *framebuffer is incomplete* (the driver lacks a float or sRGB colour format). |
| Scanlines uneven | Turn *Integer Scaling* on, and pick a line count that divides the screen height. |
| Too slow | Use *Guest Advanced Fast* or the 2Ship presets at 240 lines, or lower the line count. |
