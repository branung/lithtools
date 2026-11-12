# Lip-sync files (.lip)

NOLF2, Contract J.A.C.K. and Tron 2.0 move their characters' faces to the
voice lines with `.lip` files: keyframes for a few face nodes (jaw, lips,
mouth corners, sometimes brows, cheeks and eyelids), one file per voice
line, beside it with the same name (`VOICE/20835.WAV` and
`VOICE/20835.LIP`). `lithtech::lip` (`core/include/lithtech/lip.hpp`)
reads them, and LithRez Studio previews them.

## Evidence

- The writer, LipCompiler: Build 69's
  `legacy/jupiter/src/Engine/tools/lipcompiler/main.cpp`
  (`CompileFile`), which compiles a text export (node count, length in
  ms, node names, then per keyframe a time and per node a translation and
  Maya Euler angles).
- The reader, the game code's
  `CNodeController::HandleNodeControlLipSync` and its player
  `UpdateLipSyncControl`, the same in NOLF2's and Tron 2.0's
  `Game/ClientShellDLL/ClientShellShared/NodeController.cpp` (nolf2src,
  tron20src). The engine has no part in it: the game opens the file with
  `ILTBase::OpenFile`.

## Layout

Little-endian:

| Size | Field | Meaning |
|---|---|---|
| 4 | file id | `LIPSYNC_FILE_ID`, `('L' << 24) \| ('I' << 16) \| ('P' << 8) \| 'S'` written as a `uint32`, so the bytes are `SPIL` |
| 4 | version | `LIPSYNC_FILE_VERSION`, 1 |
| 4 | node count | |
| 4 | keyframe count | the same for every node |
| | node names | per node: a `uint32` length, then the name (no terminator) |
| | keyframes | per node, all its keyframes in one run (the game reads each node's with one `Read`): `NCKEYFRAME`, 28 bytes |

`NCKEYFRAME` is an `LTVector` translation (3 floats) and an `LTRotation`
(4 floats: x, y, z, w).

LipCompiler converts from Maya's axes as it writes: it negates the
translation's x and the angles' y and z, then builds the rotation with
`Rotate(Up(), y)`, `Rotate(Right(), x)`, `Rotate(Forward(), z)`, each
about the axes the previous turn left (`LTRotation::Rotate` is
`*this = LTRotation(axis, angle) * *this`), which comes to
`q = Ry * Rx * Rz`. `lithtech::lip::euler_degrees` takes a rotation back to
those three angles (LithRez Studio plots them); `lip_test` checks it
against the compiler's steps done literally.

The keyframe count isn't the text's: LipCompiler assumes keyframes at 0,
33 and 66 ms of every 100 and writes `(ms / 100) * 3 + (ms % 100) / 33`.

## Playing

The game checks the id and version (anything else plays the line with the
old jaw flap, `HandleNodeControlLipFlap`), reads the counts, starts the
sound, then matches each name to a model node through the model's
skeleton in `ModelButeMgr` (a name it doesn't know is skipped, its
keyframes too). `UpdateLipSyncControl` advances a timer by the frame time
(starting at minus the `fudge` console variable), takes keyframe
`timer / (1/30 s)` and the next, blends the translations linearly and
the rotations with `LTRotation::Slerp`, and multiplies the node's
transform by the result. Past the last keyframe it stops changing the
node; when the sound ends, the control goes. So a `.lip` runs at 30
keyframes a second, and `lithtech::lip::sample` is that blend.

The name buffer is `char szBone[256]` and the length isn't checked, so a
node name of 256 bytes or more would overrun it (a retail bug in the game
code; no shipped name is longer than 17).

## In the shipped games

`tests/lip_verify <rez>...` reads every `.lip` and compares it with its
voice line:

| Game | Files | Read | Failed | Without a voice line |
|---|---|---|---|---|
| NOLF2 (EU) | 1,512 | 1,512 | 0 | 112 |
| Contract J.A.C.K. (EU) | 138 | 138 | 0 | 0 |
| Tron 2.0 1.030 (US) | 2,021 | 2,021 | 0 | 694 |

All in `SOUND.REZ`/`Sound.rez`'s `VOICE` folder; the ones without a voice
line are lines the games didn't ship. No file has bytes after its
keyframes, and every rotation is unit length. Node sets: Tron 2.0 mostly
the five mouth nodes (`Face_Jaw`, `Face_LowerLip`, `Face_LMouthCorner`,
`Face_RMouthCorner`, `Face_UpperLip`: 1,855), ten face nodes (36), nine
(2), and 128 files for `joint1` to `joint12`; NOLF2 five
(`Face_LCorner`/`Face_RCorner` for the corners: 1,195), seven with
`Face_LLower`/`Face_RLower` (226), the jaw alone (85), and a few larger
sets; Contract J.A.C.K. five (92), seven (17) and the jaw alone (29).

A `.lip` is usually a few keyframes shorter than its sound (the compiler's
count drops one in ten): of Tron 2.0's 1,327 pairs, 1,309 are shorter or
equal, most by 1 to 5 keyframes; NOLF2 1,330 of 1,400; Contract J.A.C.K.
125 of 138.

## LithRez Studio

The preview plots one channel of every node over time (the rotation's X,
Y or Z angle as above, or the translation's X, Y or Z; at first the
rotation axis that moves the most), with the node list (untick a node to
hide its curve), the header, and the voice line in the sound player
below. Play runs the voice line and the playhead together; clicking or
dragging on the plot seeks. Without a voice line the curves play on their
own clock.
