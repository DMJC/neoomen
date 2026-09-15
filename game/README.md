# neoomen

A native Linux reconstruction of Warhammer: Dark Omen using C++17, SDL2, OpenGL 4.3 core, and SDL2 audio. This is an initial runnable implementation, **not a complete recreation of the original game**. The original campaign script now drives branching progression, and a CTL interpreter drives supported mission operations. Compatibility remains partial; see the limits below.

The `neoomen-editor` project remains at the workspace root. This separate CMake project shares its PRJ/M3D loaders and math code, with no GTK dependency in the game build. No original assets are copied or bundled.

## Build

Dependencies: C++17, CMake, pkg-config, SDL2 development headers, libepoxy and FFmpeg development headers, and a driver supporting OpenGL 4.3 core. Debian package names: `libsdl2-dev libepoxy-dev libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libswresample-dev`.

From the workspace root:

```sh
cmake -S game -B build-game -DCMAKE_BUILD_TYPE=Release
cmake --build build-game -j
ctest --test-dir build-game --output-on-failure
./build-game/neoomen --demo
```

The workspace's existing `cmake -S . -B build` also builds the game as `build/neoomen` alongside `build/neoomen-editor`, unless `NEOOMEN_BUILD_GAME=OFF`.

## Startup movies and main menu

```sh
./build-game/neoomen --data '/path/to/Warhammer Dark Omen'
./build-game/neoomen --data '/path/to/Warhammer Dark Omen' --skip-intro
```

Normal startup plays `Movies/ENG.TGQ`, then `Movies/INTRO.TGQ`, and opens the main menu on the original `MAINMENU.BMP` background. Escape, Space or Enter skips a startup movie directly to the menu, matching the documented English startup flow. `--skip-intro` goes straight to the menu. Explicit `--campaign`, `--resume`, `--mission` and `--demo` remain direct launch shortcuts.

The player uses FFmpeg libraries inside the SDL/OpenGL window: streamed demux/decode, RGBA video conversion, timestamp-based presentation, SDL stereo PCM audio, aspect-preserving letterboxing and resize handling. It does not launch an external player or preconvert files. M mutes movie audio while playback continues. Missing/unreadable startup movies fall back to the menu; an unavailable campaign movie leaves its prompt available to continue manually. The installed ENG movie has a malformed audio packet that is logged and discarded, as in FFmpeg's command-line decoder.

Mouse or Up/Down + Enter selects menu items. New Campaign starts the existing campaign interpreter; Load Campaign uses the native checkpoint selected by `--save` and is disabled when no file exists. Options provides sound on/off and intro replay. Escape during gameplay opens this menu, pauses simulation, and exposes Continue. Quit closes the game. Multiplayer and Tutorial are visibly disabled; those modes are not implemented. Menu labels use the original `F_MENBG` and selected `F_MENBGR` bitmap fonts over the original background. The original cursor theme loads all 21 documented `Graphics/Cursors` resources, including ANI frame timing and CUR hotspots.

## Original assets

```sh
./build-game/neoomen --data '/path/to/Warhammer Dark Omen' --mission B1_01
# Or point directly at a battle project:
./build-game/neoomen --mission '/path/to/B1_01/B1_01.PRJ'
```

Asset lookup handles Windows path separators and case differences. A mission forces its PRJ terrain/water references to M3X, combines every mesh group, and loads literal furniture M3D catalog entries. Each furniture instance uses its complete PRJ transform plus both its primary and destroyed mesh slots. Filename render flags select translucency, transparency, colour-key cutouts and animated UVs. The BTB header supplies roster filenames; neighboring `*MRC.ARM` and `*NME.ARM` files are a fallback. ARM files supply names, identities, counts and attributes. BTB game-object nodes supply initial positions for matching unit IDs, with documented 1/8 coordinate conversion. Explicit `--player FILE.ARM` and `--enemy FILE.ARM` override them. If a roster is unavailable, a demo roster is used and reported on stdout.

**Deployment and gameplay are provisional:** original SPR units now use palette banks, signed anchors, eight camera-relative directions and idle/walk/attack visual frame runs from the executable. Formations remain generated. BTB positions are used where IDs match; unmatched regiments use generated positions. CTL initialization runs before deployment, with script waits resumed by Enter. Deployment orders validate formation footprints against original BTB player regions. Movement follows direct orders without obstacle avoidance. The sandbox uses a deterministic 30 Hz simulation, simplified hit/wound/save rolls, casualty-based morale/routing, and victory/defeat when one side has no active regiments. Ranged regiments and war machines acquire targets within provisional 70- and 120-unit ranges, release travelling `BOLT.SPR`/`CANNON.SPR` volleys, then apply a ballistic-skill, strength/toughness and temporary armour-save resolution at impact. These mechanics are not verified reproductions of Dark Omen's combat tables, timing, AI, mission objectives or army stats. Traders and other special units are not distinguished by the sandbox rules.

Controls:

| Input | Action |
|---|---|
| Left click / Shift+left click | Select / add a regiment |
| Right click ground / enemy | Move / attack with selected friendly regiments |
| Enter / 1 / 2 | Begin battle, continue campaign, choose a branch |
| F5 / F9 | Save / restore campaign checkpoint |
| Left click terrain | Move a selected unit; click an enemy to target it |
| Left drag at 33–81 world units | Turn selected unit to face the cursor |
| Shift + left drag | Turn selected unit to face the cursor at any distance |
| Right click | Recenter camera on terrain or a unit's destination |
| Middle drag | Orbit camera |
| Mouse wheel | Zoom |
| Arrow keys / F | Pan / fit map |
| Space / R | Pause / reset sandbox |
| M / Escape | Mute / open main menu (simulation pauses) |

During deployment, ground orders reposition regiments immediately. Army names, troop counts, morale and battle state appear in the HUD.

## Campaign and CTL

```sh
./build-game/neoomen --data '/path/to/Warhammer Dark Omen' --campaign
./build-game/neoomen --data '/path/to/Warhammer Dark Omen' --resume --save neoomen.save
```

The campaign interpreter reads the original embedded script from `PRG_ENG/DarkOmen.exe`; it never executes native Windows code. Choices select original branches, missions load the active roster, and battle results carry casualties into subsequent script execution. Gold, roster activation, objectives, script stacks and choices survive NeoOmen checkpoints. F5 saves to `neoomen.save` (override with `--save`); F9 restores. A checkpoint taken during a battle **restarts that mission from its campaign roster**. `--resume --save SaveGame/DarkOmen.000` also imports an original fixed-size `DarkOmen.###` save: its ARM roster, gold, shared magic inventory and saved chapter are shown in the army book. Original campaign VM state contains relocated Windows-process pointers, so imported saves cannot yet continue the original script or preserve battle positions and CTL state.

Campaign movie cues now play their original TGQ files in the game window. Meetings, army-book stops and dialogue cues still appear as text prompts. Spoken dialogue, recruitment purchases, equipment UI and full campaign presentation remain unimplemented. The executable layout currently targets the installed English version and rejects incompatible PE layouts.

CTL execution is enabled for original missions. The VM supports function lookup, calls, loops, conditionals, registers, timers, flags, events, node movement, target searches and selected mission operations. Its combat host maps the documented engage, charge, retreat, hold, turn, approach and attack/shoot search actions into deterministic formation orders; hidden-unit sighting events and close-combat state are also represented. It uses direct paths and formation-level state, so original collision, cover, member-level movement and combat timing are still approximate. A 3,000-tick idle sweep passes B1_01, B1_04, B1_07, B2_01 and B3_06; other tested missions stop on unsupported operations such as spell casting, special-unit actions or game-status tests. These are explicit on-screen faults with function/unit context. This is **not yet a fully playable original campaign**. `--no-ctl` explicitly selects sandbox AI if you want to explore another map.

Animations use the initial visual frame runs for idle, walk and attack, including repeated-frame timing. The full animation instruction VM, death sequences, animation-triggered gameplay effects and original formation logic remain outstanding.

## Audio and asset inspection

MAD mono and SAD stereo decode to signed PCM using the documented IMA ADPCM framing, low-first nibbles, stereo four-byte channel groups, and raw PCM tails, including continuation across read-buffer boundaries. SDL2 converts source audio to the output format and queues it to the device. Audio failure leaves the game usable without sound.

```sh
./build-game/neoomen --demo --audio '/path/to/Sound/Music/02btl001.sad' --audio-rate 22050
./build-game/neoomen --inspect '/path/to/B101NME.ARM'
./build-game/neoomen --inspect '/path/to/B1_01.BTB'
./build-game/neoomen --inspect '/path/to/supported-sheet.SPR'
```

The sample rate is not encoded in MAD/SAD block headers. The current 22050 Hz default is provisional and can be overridden. Automatic playback now mixes original WAV effects, MAD dialogue and SAD music at 44.1 kHz stereo. Music reads the original FSM sample aliases, sequences and state tables, choosing sequence/table variants with a deterministic random generator. BATTLE1 follows deployment, battle and end states; EERIE9 accompanies menus and campaign screens. All four supplied FSM scripts are supported by the sequencer. Each music clip is decoded as needed, with a short SDL output queue; effects are cached and limited to 24 simultaneous voices. Speech lowers music volume and stopping dialogue preserves music. Movies suspend the game mixer and retain their own soundtrack.

Original button, page-turn, horn and melee effects are connected to UI and battle events. These mappings, music context selection, and FSM variation probabilities are provisional; positional audio, regiment-specific acknowledgements and complete animation/CTL sound triggers remain outstanding. Use Options to toggle all sound, M during battle, or `--mute`. Muted game audio advances silently rather than replaying stale queued speech when re-enabled. `--audio` remains a one-shot asset audition.

`--inspect` runs without SDL initialization or a display. It supports PRJ, M3D/M3X, ARM, BTB, SPR, MAD and SAD. SPR decoding handles raw indexed frame types 0–4, palette banks, signed anchors and type-5 placeholders. Unsupported encodings fail explicitly. The unit renderer reads the original sprite registry and visual animation runs from the installed English PE32 executable.

## Verification

```sh
./build-game/neoomen --demo --frames 3 --hidden --mute --screenshot /tmp/neoomen.bmp
./build-game/neoomen --data '/path/to/Warhammer Dark Omen' --mission B1_01 --frames 3 --hidden --mute
```

A hidden graphics smoke run still requires a display and working OpenGL driver. `--frames` bounds rendered frames, not simulation ticks. The tests cover malformed assets, ARM offsets, BTB framing and unknown chunk preservation, SPR palette/transparency, ADPCM golden vectors, PCM escape blocks, deployment, movement, deterministic combat and completion.

See [local validation results](docs/validation.md) and [implementation status and source references](docs/implementation.md) for compatibility gaps and next steps.

Optional original-data integration tests (assets stay outside the source tree):

```sh
cmake -S game -B build-game -DNEOOMEN_GAME_DATA='/path/to/Warhammer Dark Omen'
cmake --build build-game -j
ctest --test-dir build-game --output-on-failure
```

These replay two campaign routes with simulated victories and checkpoint roundtrips, verify casualty carryover and malformed-save rejection, validate original player sprite frames, and execute B1_01 CTL for 3,000 ticks. They do not establish combat or full mission parity.

Movie tests decode both startup files to completion without a display. To run the additional interactive-rendering checks on a machine with OpenGL:

```sh
./build-game/neoomen_frontend_tests '/path/to/Warhammer Dark Omen'
```

Between missions, B opens the army book, M opens the map, Left/Right changes regiment and S switches the equipment view. Original talking heads accompany MAD dialogue; facial motion is approximate and equipment is read-only. Campaign checkpoints preserve screen presentation. Deployment validates the whole formation against BTB boundaries. Select banners in the world or tray, then use the Start Battle button and Halt/Shoot/Break/Charge controls (H/T/B/C). Strength and magic indicators display live battle state; magic and command rules remain provisional.

During battle, the right-hand Combat Controls panel provides Halt, Shoot, Break and Charge. Buttons show disabled, active and hover states; hover text explains each order. Shoot remains unavailable to units without a missile weapon. The buttons and H/T/B/C shortcuts issue the same selected-regiment command.

Cursor themes use SDL native colour cursors, with original artwork enabled by default. In the main menu's Options screen, press **C** or click the cursor setting to switch between **Original Colour** and the **System** pointer. Start with `--cursor-theme system` or `--cursor-theme original` to choose explicitly; the menu setting lasts for the current session.

Original cursors animate from the four-frame BMP sheets using ANI timing/sequence data and CUR/ANI hotspots. Selection, friendly-unit hover, movement, attack, invalid deployment and middle-drag rotation change the pointer. Missing assets fall back to the system pointer. Context mappings are provisional; spell and equipment actions will gain their own cursors as those actions are implemented.

During deployment, hold the left mouse button on a friendly banner in the battlefield or deployment tray, drag onto the terrain and release to place that regiment. A green formation preview marks a valid drop; red indicates an invalid position. The entire formation must fit inside the deployment area. Invalid drops leave the regiment in place; Escape or right-click cancels the drag. Other selected regiments are not moved by a banner drag.

Sprite palette RGB `000000` is transparent, and `00ffff` becomes a half-opacity black shadow. World sprite shadows blend without depth writes, followed by opaque sprite pixels. Palette transparency indices remain supported.

Mission `.SHD` files load automatically beside the PRJ. The decoder reconstructs the original tiled occluder-height grid, which the shader uses for directional terrain/furniture shadowing. Missing SHD files leave ordinary lighting enabled. The current light direction, 96-unit tracing range, bias and shadow strength are provisional; this does not implement the full original LIT lighting system.

Water meshes named by the PRJ WATR section combine all M3X groups as static layers and scroll texture coordinates when the documented animated-UV flag is present. Animation runs during deployment and battle and freezes while paused. The original scroll speed has not been recovered.

In battle, CTL `play_self` and `play_other` requests now queue original speech clips and show the speaker's animated head in the lower-left HUD. The deployment banner tray disappears and becomes inactive when battle starts; battlefield banners remain selectable. The portrait closes when speech finishes, and mission retries clear old dialogue. CTL's sound-playing condition tracks queued and active speech.

Portrait models and speech come from the original installation. Mouth motion follows audio amplitude with subtle head movement; the original SEQ/KEY facial animation interpreter is not yet implemented. Generic acknowledgement variants currently choose the first registered clip, and exact priority/expiry rules remain provisional.
