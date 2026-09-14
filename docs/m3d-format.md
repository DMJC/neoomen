# 3D mesh interpretation and renderer

Sources: `../../doreverse/reversing/reverse_engineered_functions.md` and a read-only Ghidra decompilation of `LoadM3DModel` at `004488b0`, plus the instance-rotation section of `LoadBattleMap` at `0042c9a0`. Existing `M3dFileHeader`, `M3dSubObjectHeader`, and `M3dTextureEntry` types were searched before filling the remaining geometry gaps.

Confidence **2** under the doreverse methodology: the following field interpretations are supported by the decompiled loader and real-file parsing. The new renderer has been inspected and tested, but has not been differentially compared against the game's renderer; this is not a claim of complete behavioral equivalence.

| Record | Layout used |
| --- | --- |
| File header | 24 bytes: `PD3M`, marker `0x36243600`, version 1, CRC and complement, u16 texture and group counts |
| Texture | 96 bytes: ignored 64-byte Windows build path, 32-byte texture filename |
| Group | 64 bytes: name at 0, pivot XYZ floats at 36/40/44, u16 vertex/face counts at 48/50, flags at 52 |
| Face | **Before the vertices**; 28 bytes: u16 indices at 0/2/4, signed material index at 6, normal at 8/12/16, two unused floats |
| Vertex | 44 bytes: XYZ floats at 0/4/8, normal at 12/16/20, unused field at 24, UV at 28/32, two unused fields |

The loader's face loop advances by seven floats, followed by its vertex loop advancing by eleven floats. This resolves the misleading vertex-before-face order in the older prose overview. Material `-1` falls back to the first texture in the normal load path. When group flags contain bit value 2, the loader adds the group's pivot to each vertex position; otherwise it uses the position directly. Applying all group pivots unconditionally would displace the terrain patches.

The parser validates geometry boundaries, indices, finite floats and complete file consumption. All **338 M3D/M3X files** in the installed GameData corpus parsed successfully, covering **110233 triangles**. This is a structural corpus check, not a game-renderer differential test.

Instance translation uses signed fixed-point coordinates divided by 1024. The rotation matrix corresponds to `Rz × Ry × Rx`, with the stored Y angle negated and angle units equal to 2π/4096, matching the inspected LoadBattleMap matrix construction. Positions and rotations remain in the game's coordinate system. Camera rendering uses a **left-handed world basis** (`right = up × forward`) with negative view Z for OpenGL clipping. Picking rays, panning, and WASD movement use the same basis. This corrects horizontal reflection without changing model data or saved coordinates.

Render-flag naming follows the source's `_` plus base-36 character convention. The existing research suggests translucency and color-key flags; this preview combines group and model filename flags, uses BMP palette entry zero for indexed foliage cutouts, and draws water/translucent batches at 65% opacity after opaque geometry, sorted by batch-center distance. **Confidence 1** for exact game equivalence of these rendering choices; opacity and simple directional shading are preview choices. BMP row order is supplied by GdkPixbuf and UV V is used as stored.

Deliberate limits: no M3D writing, water animation, SHD shadows, LIT lighting reproduction, or destroyed-mesh state simulation. Mesh references and instance edits remain supported in PRJ. Grid height edits do not rewrite terrain mesh geometry. Alpha picking intersects geometry; transparent texels are not rejected by the CPU picking ray.
