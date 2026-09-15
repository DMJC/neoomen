# neoomen-editor

The separate [neoomen game project](game/README.md) reconstructs Dark Omen for Linux using C++17, SDL2, OpenGL 4.3 and SDL2 audio. The default workspace build now produces both `neoomen-editor` and `neoomen`; use `-DNEOOMEN_BUILD_GAME=OFF` for an editor-only build.

A C++17 / **gtkmm-3.0** 3D viewer and level editor for Warhammer: Dark Omen `.PRJ` battle projects. The binary implementation is based on `../doreverse/reversing/reverse_engineered_functions.md`, with additional structural checks against 41 locally installed game projects.

## Build and launch

Requires a C++17 compiler, CMake, pkg-config, gtkmm-3.0 and libepoxy development files (Debian/Ubuntu: `libgtkmm-3.0-dev libepoxy-dev`; Fedora: `gtkmm30-devel libepoxy-devel`). The default combined build also requires SDL2 development files (`libsdl2-dev` on Debian/Ubuntu; `SDL2-devel` on Fedora). The editor's 3D view requires desktop OpenGL 3.3; the 2D editor remains available if a GL context cannot be created. The game requires OpenGL 4.3.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/neoomen-editor
./build/neoomen-editor /path/to/B1_01.PRJ
```

No game files are bundled. The editor opens PRJs directly, without a configured game installation.

## 3D scene and camera

Opening a PRJ displays its textured **3D scene**. The viewer forces terrain and water references to `.M3X`, loads every group as a combined mesh, and loads furniture from the literal `.M3D` catalog. It applies the complete PRJ instance transform and both the primary and destroyed mesh slots. BMP textures resolve from `TEXTURE`, `LTEXTURE`, or the mission folder, with case-insensitive path lookup. It supports filename render flags for translucency, transparency, colour-key cutouts and animated UVs; water uses UV scrolling rather than vertex animation.

- **Left drag:** orbit around the camera target.
- **Middle/right drag:** pan in the camera plane.
- **Wheel:** zoom; **F** or **Fit:** frame the terrain.
- **WASD:** move the camera target; **arrow keys:** rotate the view.
- **Click:** select furniture. Its mesh is highlighted and the Furniture properties panel edits it.
- With **Place furniture** active, click the terrain to place the chosen mesh at the actual 3D surface height. Left dragging still orbits.

The renderer converts Direct3D's left-handed world to OpenGL view/depth coordinates. Picking and panning share the same handedness; saved PRJ coordinates are not reflected.

The **2D terrain editor** tab retains painting and marker dragging. Terrain-grid painting is performed there. Instance property changes, additions, deletion and undo/redo also update the 3D scene.

The asset folder defaults to the opened PRJ's directory. **Project → Choose asset folder** can point at a separate mission asset folder. **Reload 3D assets** reloads meshes/textures after external changes. Save As retains the current asset folder for viewing; reopening the saved PRJ resolves assets beside that saved file. Missing assets are reported under the scene and in stderr; missing textures use a neutral fallback.

## Editing a level

For a project intended for the game, copy an existing mission directory, open its PRJ, and use **Save As** in the copied directory. This preserves the original camera/editor/exclusion data and gives you the companion assets to work with.

- **Terrain:** choose Layer A or B, inspect raw heights and attribute values under the pointer, then paint attributes or raise/lower heights. Each drag is one undoable edit. The radius controls the circular brush. Height changes use multiples of 128 and preserve the existing base-height remainder. An 8×8 patch can span at most 32640 raw height units; an unrepresentable stroke is rolled back with an explanation.
- **Attributes:** painting replaces the entire four-bit value. `2` marks blocked ground, `8` water, and `10` blocked water. Values `1` and `4` are undocumented flags; all combinations are preserved. Unknown flags are highlighted in yellow. Low-nibble-first is the default; the notes do not establish nibble order conclusively, so an alternate view/edit interpretation is available.
- **Furniture:** add a mesh filename to the catalog, select **Place furniture**, and click the map. New furniture starts at Y=0 with zeroed properties; set its height, local bounds and gameplay properties for the asset. Select and drag a marker to move it in X/Z. Locked instances can be selected but cannot be dragged. Duplicate and Delete operate on the selection. The property panel exposes transforms, mesh slots, bounds, damage properties, exclusions, effects and lights; press **Apply furniture properties** to commit its fields. Mesh slots are 1-based, with 0 meaning none. Rotation Y displays the stored angle, which the game negates.
- **Project:** edit terrain/water mesh references and the music cue, then press **Apply project properties**. Grid origin and world-units-per-cell control marker display and placement; these are session-only settings. The default is one world unit per cell at origin zero. Verify alignment against your assets.
- **2D navigation:** wheel zooms around the cursor; middle/right drag pans; **Fit** frames the map. Markers show position and yaw, with local X/Z bounds for the selection.

Keyboard shortcuts: Ctrl+N / Ctrl+O / Ctrl+S, Ctrl+Shift+S (Save As), Ctrl+Z, Ctrl+Shift+Z or Ctrl+Y. Undo/redo retains up to 32 document snapshots, with a 64 MiB target limit per history stack. Changed documents prompt before closing or replacing them. Saves use a same-directory temporary file and rename to avoid partial destination files.

**New** creates a blank, editable terrain-data draft with dimensions that are multiples of eight. It does not manufacture the undocumented `EXCL`, `TRAC` or `EDIT` structures. Use an existing PRJ as a template when those structures are needed.

## Scope and format limits

This editor renders existing M3D/M3X meshes and BMP textures, but does not generate or modify those assets. It does not implement SHD/LIT lighting, BTB battle boundaries, armies or CTL mission scripts. Water has the documented animated-UV flag, although the original scroll speed was not recovered. BTB-driven animated furniture subparts, such as windmill rotors, belong to the game-object system and are not represented by PRJ instances. Editing PRJ heights changes the terrain grids, **not the visible terrain mesh**. A PRJ alone is not a complete playable mission; game-side validation of saved edits has not been performed.

The two height layers are independently editable. The source notes do not establish which is movement versus line of sight, or confirm conversion between raw terrain heights and furniture world Y. The 2D view consequently does not snap furniture Y to terrain-grid heights. Placement in the 3D view instead intersects the actual mesh surface.

Unedited projects serialize byte for byte. Unknown instance fields, extended record bytes, the 32-byte banner, attribute padding, and all trailing data (including EXCL/TRAC/EDIT) are preserved. Music edits change only the scanned 20-byte MUSC field. See [format implementation notes](docs/prj-format.md) for observed chunk-size conventions and validation limits.

## Tests

```sh
ctest --test-dir build --output-on-failure
# Optional: checks every PRJ recursively, without modifying game files.
./build/prj_tests '/path/to/Dark Omen/GameData'
./build/m3d_tests '/path/to/Dark Omen/GameData'
# Requires a graphical display; opens and closes a test window.
./build/editor_smoke
./build/scene_smoke /path/to/B1_01.PRJ
```

The tests cover exact serialization, signed heights, shared-dictionary isolation, rejected edits, nibble packing, instance fields, opaque trailers, malformed/truncated input and atomic saving. The GTK smoke test exercises painting, height editing, undo/redo, furniture placement, duplication and deletion through real controls/events.

Mesh tests cover field offsets, conditional pivots, indices, truncation, instance transforms, handedness and picking math; the local corpus check passed for 338 meshes. The 3D smoke test checks a textured framebuffer, orbiting, terrain-surface placement, mesh picking, transform refresh and asset reload. Its optional second argument saves a scene screenshot.
