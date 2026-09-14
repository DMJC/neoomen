# Validation — 2026-09-14

Tested locally with GCC 16.2, SDL2 2.32.70, libepoxy 1.5.10 and Mesa 26.1.6 on an AMD Radeon RX 9070 XT. The application requested an OpenGL 4.3 core context; the driver supplied OpenGL 4.6 core.

- Standalone game build succeeded.
- Combined workspace build succeeded; PRJ, M3D and new game test suites all passed.
- Game core tests passed under AddressSanitizer, UndefinedBehaviorSanitizer and LeakSanitizer. LeakSanitizer required running outside the process-traced sandbox.
- An auto-started demo advanced 8 simulation ticks while rendering 180 frames.
- SDL/OpenGL smoke runs rendered the asset-free demo and installed B1_01 mission without reported GL errors. The mission loaded 1739 unique mesh triangles, 37 furniture instances and 11 BTB positions joined to its player/enemy army records. Screenshots were visually inspected.
- A 90-frame mission run decoded `02btl003.sad`, converted and queued it through SDL audio with output muted. This verifies the playback path, not audible quality or source sample rate.

Read-only corpus validation through `neoomen --inspect`:

| Asset set | Passed | Unsupported |
|---|---:|---:|
| GameData/1pbat PRJ | 41 | 0 |
| GameData/1pbat ARM | 82 | 0 |
| GameData/1pbat BTB, including header and node decoding | 54 | 0 |
| Graphics/Books SPR | 20 | 10 |
| Sound/SP_ENG MAD | 929 | 0 |
| Sound/Music SAD | 271 | 0 |

Unsupported SPR sheets: `BARS`, `BOOKBUT`, `GNUMBERS`, `HSHIELD`, `NUMBERS`, `RED2GREY`, `RED2ZERO`, `SCROLL`, `ZERO2RED`, and `menubut`. Nine contain frame types outside the implemented 4/5 subset; HSHIELD uses multiple palette banks. Unsupported cases produce an explicit diagnostic.

These checks establish structural decoding and initial rendering/runtime behavior. They do not establish original mission, campaign, combat, animation, audio or savegame parity.

## Campaign / CTL / unit animation update — 2026-09-14

- Root Release build: all three existing/editor/core CTest tests pass.
- Standalone Debug build: core and optional original-data runtime tests pass.
- AddressSanitizer + UndefinedBehaviorSanitizer: both tests pass, including original-data route replay.
- Campaign first-choice route: 25 mission results to credits; second-choice route: 20, with different mission sets. Results are simulated victories, not played battles. Checkpoints are saved/restored at every script stop and continuation is compared. A loss test checks casualty retention. Truncated saves are rejected without mutation.
- Original player roster: 22 unique sprite IDs, 2,224 frames decoded. Idle/walk/attack frame selection checked across 120 ticks and all eight directions against sheet bounds. Synthetic tests verify signed anchors, nonzero palette banks and transparency.
- B1_01: 3,000 idle simulation ticks, 43,103 CTL instructions without unsupported-opcode faults. Synthetic VM regression covers deployment barrier, assignment, timer suspension/resumption, arithmetic, yield, termination and malformed lookup.
- Broader 31-script idle sweep: B1_01, B1_04, B1_07, B2_01 and B3_06 pass 3,000 ticks. Other scripts expose remaining operations; examples include 0x50 special-unit handling, 0x55 teleport effects, 0x99 spell casting, 0xaf other-unit animation, 0xc3 game-status testing and 0x89 host action. Passing an idle trace does not imply full mission coverage.
- SDL/OpenGL smoke: campaign prompt screen renders; B1_01 original-unit rendering runs 100 frames / 19 simulation ticks without GL errors on Mesa / RX 9070 XT. Screenshots are local `/tmp/neoomen-campaign.bmp` and `/tmp/neoomen-animation-final.bmp`, not bundled assets.

Remaining compatibility limits are stated in the game README. In particular, campaign presentation is textual, saves are checkpoints rather than mid-battle snapshots, combat is provisional, CTL coverage is incomplete, and animation supports initial visual runs rather than the full animation instruction machine.

## Intro movies and main menu — 2026-09-14

- FFmpeg full-stream tests decode INTRO.TGQ (1,104 video frames, 3,242,848 stereo PCM sample frames, approximately 73.6 seconds) and ENG.TGQ (158 video frames, 464,464 stereo PCM sample frames, approximately 10.5 seconds). Missing-file construction is rejected. ENG has one malformed audio packet also reported by the FFmpeg CLI; playback logs and skips it.
- Standalone CTest: all four tests pass, including both complete movie decodes. ASan/UBSan runs of all four tests pass. Root editor/game build and its three tests pass.
- Explicit OpenGL frontend test passes: Continue, disabled Load, keyboard selection, Options sound toggle, scaled mouse activation, startup skip to menu, campaign-movie skip back to game, Quit and GL error checks.
- Live SDL/OpenGL playback with audio naturally transitions from ENG.TGQ to INTRO.TGQ. A screenshot captured the intro cinematic at `/tmp/neoomen-intro.bmp`; the menu capture is `/tmp/neoomen-menu.bmp`. No original image/video files are bundled in the repository.
- Direct campaign startup plays INFO_ENG.TGQ through the shared movie player. Battle simulation remains at zero ticks while frontend movies/menu are active.

Menu background, layout and F_MENBG/F_MENBGR font rendering use original assets. Frontend OpenGL checks cover menu drawing with the decoded palette font, an active original-sprite ranged volley, the `PANELS.SPR` Combat Controls backing and the Halt command. Multiplayer and Tutorial remain disabled. These frontend tests do not expand CTL or combat compatibility.

## Sound/music integration (2026-09-14)

Standalone Debug CTest passes all six tests, including deterministic speech/mute/cancellation checks and original-data playback of all four FSM scripts across deployment, normal and end states. Original BUTTON01 WAV conversion and preservation of music when speech stops are exercised. The root Release build and its four tests also pass. A 180-frame B1_01 OpenGL deployment run completed with SDL audio enabled and no playback errors. These checks establish signal generation and device submission, not listening-based fidelity or original FSM probabilities.

The preceding campaign UI work also passed original-data OpenGL tests covering talking heads, book/equipment/map rendering, banner selection, Start Battle and Halt. Core tests cover deployment polygon/formation rejection, command behavior, CTL charge and search-and-shoot orders, magic recharge and campaign presentation checkpoint replay. Equipment remains read-only and facial motion/combat/magic rules are approximate.

The ASan/UBSan build also passes all six tests, including original audio sequencing (14.50 seconds total).

## Colour cursors (2026-09-14)

Native SDL/OpenGL integration checks pass for original SELECT, HAND, HAND3, MOVE, SWORD, ROTATE, SELL, STAFF, POINT and HGLASS assets. Tests verify animated frame advancement, missing-asset fallback, restoration of the system theme and the Options keyboard toggle. CUR/ANI hotspots are read from the original resources; legacy ANI RIFF sizes that include the eight-byte header are accepted while child chunks remain bounds checked. Debug and root Release builds succeed; all six standalone CTest checks pass.

Deployment banner drag/drop integration tests pass with original B1_01 terrain: valid terrain drop, no position changes during preview, preservation of other regiments, invalid-drop rejection, Escape cancellation and OpenGL preview rendering. Both builds and all six standalone tests pass.

## Sprite transparency and SHD (2026-09-14)

All 41 installed SHD files decode successfully. Core tests verify signed base/detail reconstruction, shared dictionary addressing, partial edge tiles, bad offsets and truncated data. Sprite tests verify black transparency and cyan conversion to translucent black. The original-data OpenGL test loads B1_01's 184x200 SHD grid and renders the terrain, sprites and UI without GL errors. Both builds and all six standalone tests pass. Shadow light direction and strength remain provisional.

Water animation: original B1_01 OpenGL frame comparisons with fixed units/camera detect changing pixels while running and identical output across paused frames. Both game builds succeed. The deformation is confined to WATR batches; its wave/UV parameters remain provisional.

## In-mission talking heads (2026-09-15)

Core tests cover CTL play_self/play_other queueing, sound-playing conditions, duplicate suppression and reset cleanup. Original-data OpenGL checks play S_A001 on a Bernhardt portrait, verify that the deployment tray cannot select units after battle starts, and ensure retrying into deployment stops old dialogue. Both builds succeed. Facial motion uses the existing audio-amplitude approximation rather than the original SEQ/KEY interpreter.
