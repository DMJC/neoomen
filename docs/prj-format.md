# PRJ implementation notes

Primary reference: `../../doreverse/reversing/reverse_engineered_functions.md`, sections “.PRJ format,” “PrjInstanceRecord,” and “TERR + ATTR chunks decoded.” This file records local implementation evidence rather than changing the upstream reverse-engineering database.

The reader checks the tags and walks **BASE → WATR → FURN → INST → TERR → ATTR**, then retains the rest of the file verbatim. It does not trust FURN/INST/TERR sizes as generic seek distances. It retains the original banner rather than requiring a particular version string, matching the documented loader's behavior. All integers are little-endian.

| Structure | Implemented layout |
| --- | --- |
| Banner | 32 uninterpreted bytes |
| BASE / WATR | Tag, u32 byte count, filename bytes (no separate chunk size) |
| FURN | Tag, BSIZE, count, count × (u32 length + filename bytes) |
| INST | Tag, BSIZE, count, record size, records; size ≥152 accepted and extensions preserved |
| TERR | Tag, BSIZE, width, height, delta-block count, patch count, Layer1 byte count, two Layer1 arrays, Layer2 byte count, Layer2 array |
| ATTR | Tag, BSIZE, width, height, packed nibbles; any extra BSIZE payload bytes retained |
| Trailer | Everything after ATTR retained, including the observed 64 extra bytes and subsequent chunks |
| TRAC | Trailer camera-track block: marker `2`, cadence `6`, then a variable-length payload up to `EDIT` |

## Additional local observations

The following were directly checked against **all 41 PRJs** under the installed game's GameData directory. They are high-confidence structural observations about that corpus, not claims about every possible game/editor version.

1. TERR has a **u32 byte count before each array allocation**. After the six-dword TERR header, the first is `16 × patch_count` (both Layer1 copies). After those records, the second is `64 × delta_block_count`. Thus Layer1 starts at TERR offset 28, and Layer2 starts at `32 + 16 × patch_count`. The upstream overview omitted these two prefixes from its pseudostructs.
2. `TERR.BSIZE = 24 + 8 × patch_count + 64 × delta_block_count`. It omits one Layer1 copy despite both being in the file. When this convention matches the input, dictionary growth updates BSIZE by 64 per new block. A different nonzero marker is retained as-is because the documented game loader tests only presence.
3. Several maps have negative Layer1 base heights encoded as signed 32-bit integers (including B1_04, B1_05, B2_01, B2_08 and B4_01). Height decoding uses signed base plus unsigned delta ×128. Byte counts and dictionary offsets remain unsigned.
4. The reader reconstructs every installed file byte for byte. Tests edit one cell, check all other heights and the other layer, and check that the modified file can be parsed again.

The B1_01 offsets used as an initial check are: BASE 32, WATR 49, FURN 69, INST 245, TERR 5885, ATTR 45389, EXCL 63869, MUSC 63945, TRAC 63969 and EDIT 67197. These offsets are test observations, **not hardcoded parser offsets**. `TRAC` has a non-size marker field (`2` in the installed corpus), so its end is located at the following `EDIT` tag. The reader exposes its `6` cadence and preserves the variable-length payload exactly; the camera-transform record layout remains unverified.

## Editing guarantees

A height patch is reconstructed exactly, one cell is changed, then its minimum becomes the new signed base. Every delta must be in 0–255 and every height difference divisible by 128. When another patch or layer references the same delta block, the editor appends a private block before changing it. Unshared blocks are overwritten in place. A no-op edit leaves the bytes untouched. Dictionary blocks are not garbage-collected, so repeated edits may grow a project.

FURN size is count bytes plus filename bytes, omitting each filename length prefix. INST size is count × record size, omitting the two metadata dwords. Existing values remain untouched until a structural edit requires updating them. New instances have zeroed records; duplicates retain unknown fields but clear known editor links/selection and runtime mesh pointers.

MUSC is found by scanning the opaque trailer, matching the documented game's strategy. The first complete `MUSC` tag plus 20 bytes is used. This inherits the format's ambiguity if opaque preceding data happens to contain that byte sequence.

## Remaining uncertainty

- Layer A/B gameplay roles, terrain-to-world scale, and height-to-world Y conversion are not established by the source notes.
- The nibble order is not specified by those notes. Low-first is an explicit default, with a high-first toggle; neither interpretation changes data merely by viewing it.
- The source's water and impassable flag semantics are community-derived and not independently traced here.
- Unknown trailers and companion assets are retained or referenced, not regenerated. Blank drafts omit undocumented chunks and have not been proven game-loadable.

For bounded resource use, input files are limited to 256 MiB, grid axes to 4096 cells, catalogs/instances to 100000, and delta dictionaries to 1000000 blocks. Terrain patch counts must match ceiling-divided 8×8 dimensions; byte lengths and offsets must be consistent. ATTR width must be even, matching the documented width/2 row stride. Projects without TERR data are rejected explicitly. Padding outside the logical edge of a partial terrain patch is preserved during recompression.
