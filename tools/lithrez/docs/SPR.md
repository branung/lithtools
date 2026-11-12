# Sprites (.spr)

A sprite is an animation of `.dtx` textures named by path: explosions,
flares, smoke, interface animations. NOLF, AvP2, NOLF2, Contract J.A.C.K.
and Tron 2.0 all ship them in the same format. `lithtech::spr`
(`core/include/lithtech/spr.hpp`) reads it, and LithRez Studio previews it.

## Evidence

- The engine's reader, `spr_Create` in Build 69's
  `legacy/jupiter/src/Engine/runtime/client/src/sprite.cpp` (the Jupiter
  engine of NOLF2, Contract J.A.C.K. and Tron 2.0), called by `LoadSprite`
  in `runtime/client/src/setupobject.cpp`.
- AvP2 1.0.9.6's `lithtech.exe`, decompiled in avp2src
  (`src/engine/lithtech/sprite.cpp`: `spr_LoadQ474` at 0x00483a60, and
  the tracker `spr_UpdateTrackerQ414` at 0x00483d00): the same reads in the
  same order, and the same frame choice. NOLF's engine (nolfsrc,
  `cmodelload.cpp`) is the same code.
- The writer, DEdit's `CSpriteFile::Save` in Build 69's
  `Engine/tools/shared/world/SpriteFile.cpp`, which writes the fields in
  this order (and reads them back in `Load`).

## Layout

Little-endian:

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 4 | frame count | `nFrames` |
| 4 | 4 | frame rate | frames per second (stored in `SpriteAnim::m_MsFrameRate`, which is per second despite the name) |
| 8 | 4 | keyed | `bTransparent`; the engine keeps the low byte (`m_bKeyed`) |
| 12 | 4 | translucent | `bTranslucent`; the low byte (`m_bTranslucent`) |
| 16 | 4 | colour key | `m_ColourKey` |
| 20 | | frames | per frame: a `uint16` name length, then the name (no terminator) |

A name longer than 1000 bytes makes `spr_Create` give up (its buffer is
1024). Each name is looked up as a client file (`FILE_CLIENTFILE`) with
`AddSharedTexture`: a path such as `SpriteTextures\SFX\Explo\firebla.dtx`
from the root of the game's resources, in whichever of the game's archives
holds it. There is one animation per file ("Sprites only get one animation
currently...", the reader's comment); it is named "Untitled".

## Playing

`spr_Create` sets the animation's length to `(1000 / rate) * frames`
milliseconds, in integers (so 30 fps makes 33 ms frames). The tracker
(`spr_UpdateTracker`) adds the elapsed time, wraps it at that length when
looping (sprites start playing and looping, `spr_InitTracker`) and shows
frame `(time / (1000 / rate)) % frames`. A length of 0 (no frames, or a
rate over 1000) shows no frame. `lithtech::spr::Sprite::length_ms()` and
`frame_at()` are these two calculations, and LithRez Studio animates with
them.

## In the shipped games

`tests/spr_verify <rez>...` reads every sprite of the archives given (a
game's archives together) and looks every frame up among their textures:

| Game | Sprites | Read | Failed | Frames | Frames not in the archives |
|---|---|---|---|---|---|
| NOLF GOTY (EU) | 173 | 173 | 0 | 1,106 | 1 (in 1 sprite) |
| AvP2 1.0.9.6 (EU) | 477 | 477 | 0 | 3,791 | 925 (in 38 sprites) |
| NOLF2 (EU) | 164 | 163 | 1 | 1,035 | 131 (in 8 sprites) |
| Contract J.A.C.K. (EU) | 187 | 186 | 1 | 1,140 | 118 (in 7 sprites) |
| Tron 2.0 1.030 (US) | 387 | 384 | 3 | 2,396 | 49 (in 10 sprites) |

No file has bytes after its last frame. Frame rates run from 1 to 30.
NOLF has 4 sprites keyed and translucent (1 and 1); every other sprite of
every game has 0 in both.

The failures are empty files (0 bytes), not sprites:
`FX/GUNS/TAZER/PVFLARE.SPR` in NOLF2's `GAME.REZ` and Contract J.A.C.K.'s
`Game.rez`, and `FX/SPR/FAKE.SPR`, `FX/SPR/JETREZIN.SPR` and
`INTERFACE/MENU/SPR/LOADING.SPR` in Tron 2.0's `Game.rez`. Nothing in the
archives' text names them. The engine could not load them either: its rez
streams (`RezFileStream::Read`, Build 69's
`runtime/kernel/io/src/sys/win/de_file.cpp`) zero what they can't read,
so `spr_Create` would get no frames at a rate of 0 and divide by that
rate before it checks the stream's error status (a retail bug, latent as
long as nothing loads them). LithRez Studio says the file is empty.

The missing frames are textures the games didn't ship, in sprites they
don't use: AvP2's `Sprites/` folder's lightning, storm and EEG sprites
(their textures under `WorldTextures\Details\` and similar), NOLF2's and
Contract J.A.C.K.'s `FX/TEST` and `TEXFX/NORMAL/ISLAND2.SPR`, Tron 2.0's
`FX/TRON_OLD/TRON.SPR` and a few interface sprites. LithRez Studio shows
such a frame as missing.
