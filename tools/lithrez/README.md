# lithrez and LithRez Studio

Cross-platform tools for LithTech resource files (`.rez`), in C++17 with CMake;
they build with clang, gcc or MSVC on Linux, macOS and Windows.

- **`lithrez`**: the command line tool. Its classic commands behave and
  print like Monolith's LITHREZ (1.00 to 1.10), and the archives it creates
  are byte-identical to theirs. Extra commands cover debugging and build
  scripts.
- **LithRez Studio** (`lithrez-studio`, Qt 6): a successor to WinRez LT Studio. It
  has an Explorer-style rez editor, a DTX texture viewer with thumbnails
  (PCX, TGA and BMP images preview and thumbnail too), animated sprites,
  lip-sync curves played with their voice lines, texture effects run as
  the engine runs them, a
  model preview for `.abc` (NOLF, AvP2) and `.ltb` (NOLF2, Contract
  J.A.C.K., Tron 2.0, and NOLF on the PS2) models, a sound player, and a string table editor for
  `CRes.dll`.
- **`core/`**: the shared library (`lithtech::rez`, `lithtech::dtx`,
  `lithtech::pcx`, `lithtech::tga`, `lithtech::bmp`, `lithtech::abc`, `lithtech::ltb`, `lithtech::ps2ltb`, `lithtech::sound`,
  `lithtech::pe`, `lithtech::spr`, `lithtech::lip`, `lithtech::texscript`).

`docs/REZ.md` describes the format and how these tools were checked against
the originals. Both tools also read the PS2 port's archive (`LITH_PS2.REZ`,
straight out of a disc image or on its own): see [PS2 archives](#ps2-archives)
and `docs/PS2REZ.md`.

## Building

```
cmake -S tools/lithrez -B build -G Ninja          # -DLITHREZ_BUILD_GUI=OFF without Qt
cmake --build build
ctest --test-dir build
```

LithRez Studio needs Qt 6.2 or later (Widgets, OpenGL, OpenGLWidgets,
Multimedia) and OpenGL 3.3. Without Qt only `lithrez` and the library are
built. MP3 decoding uses the vendored minimp3 (`third_party/minimp3`, CC0,
with one fix described there).

`docker/Dockerfile` is the Linux build environment used here (clang, CMake,
Ninja, Qt 6, and PulseAudio and FFmpeg for the sound tests):

```
docker build -t lithtools-rez-dev tools/lithrez/docker
docker run --rm -u $(id -u):$(id -g) -v $PWD:/src -w /src lithtools-rez-dev \
    sh -c 'cmake -S tools/lithrez -B out/lithrez-build -G Ninja && cmake --build out/lithrez-build'
```

`.github/workflows/tools.yml` builds, tests and packs on all three platforms: a release for every `v*` tag and a nightly for every push to master.

### Checks against the shipped games

These read the game archives (never committed), so they aren't part of
`ctest`:

- `tests/roundtrip_shipped.py`: every shipped `.rez` reads and writes back.
- `tests/ps2_verify.py <lithrez> <work dir> <reference> <disc images>...`:
  the PS2 discs extract byte-identical to a reference extraction, rebuild
  byte-identical from their tables, and take edits (docs/PS2REZ.md).
- `spr_verify <rez>...`: every sprite of a game's archives, its frames
  looked up among their textures (docs/SPR.md has the results; the only
  failures are five empty files). `spr_test` (in ctest) checks sprites made
  in the test.
- `lip_verify <rez>...`: every lip-sync file, against its voice line
  (docs/LIP.md; all 3,671 of NOLF2, Contract J.A.C.K. and Tron 2.0 read).
  `lip_test` (in ctest) checks files made in the test and the rotation
  maths against LipCompiler's.
- `texfx_verify [--dump] <rez>...`: every texture script and effect
  group of a game's archives, the scripts checked (and with `--dump`
  decompiled), the groups' scripts looked up (docs/TEXFX.md; all of
  NOLF2's, Contract J.A.C.K.'s and Tron 2.0's read). `texscript_test`
  (in ctest) checks scripts and groups made in the test.
- `dtx_decode_all`, `abc_verify`, `ltb_verify`, `ps2ltb_verify`: textures
  and models (`ps2ltb_verify --dump` against plugins/blender's
  `tests/ps2ltb_dump.py` compares the C++ and Python PS2 readers).
  `dtx_decode_all --ps2-alpha <folder>` decodes loose files, such as the
  PS2 discs' 8,930 textures, reading palette alpha as the PS2 does.
  `dtx_decode_all` also reads the old versions: Blood 2's `-2` (the demo's
  `B2Demo.rez`: 519 textures, 246 plain and 273 with alpha masks) and KISS
  Psycho Circus's `-3` (`PSYCHO.REZ` 1.11: 2,929, 1,941 plain and 988
  masked), strictly, every level, each file exactly the size its layout
  says; all decode. `dtx_old_test` (in ctest) checks hand-made `-2` and
  `-3` files pixel by pixel.
- `pcx_test` and `image_test` (in ctest): PCX, TGA and BMP images made in
  the tests, one per layout. `image_decode_all [--dump <dir>] <rez | image
  | folder>...` decodes every `.pcx`, `.tga` and `.bmp`, and
  `tests/compare_images.py <dir>` checks the dump against ImageMagick: the
  916 PCX of AvP2, NOLF GOTY and NOLF Mac's archives (752 8-bit, 163
  24-bit, one 16-colour EGA), NOLF.REZ's TGA, and the 1,825 distinct
  loose TGA and BMP files of the games' and tools' installs, all
  identical (the one allowed difference, a 32-bit BI_RGB bitmap's unused
  fourth byte, is in the script).
- `sound_decode_all <rez>...`: decodes every `.wav` (80,845 in the shipped
  games: 59,794 MP3, 17,867 PCM, 3,177 IMA ADPCM, one float, six empty).
- The same tool over folders of loose files decodes the PS2 discs' sound
  banks (416 `.lps` files, 58,403 sounds, 2,258 loops that agree with
  their records) and stream music, and `tests/compare_lps.py <build> <dir
  of ps2sound.py> <work dir> <folders>...` finds every bank sound's samples
  equal to the nolfsrc ps2 branch's `build/ps2/ps2sound.py`.
- `tests/compare_sound.py <build> <work dir> <rez>...`: a sample of every
  codec, channel count and rate against FFmpeg. PCM, float, A-law and mu-law
  match exactly and MP3 within FFmpeg's rounding. IMA ADPCM must equal the
  IMA recommendation's bitwise reference; FFmpeg uses a multiplying formula
  that rounds differently, and the script checks that is the only
  difference.
- `sound_play_test <rez> <path>` (or `<bank.lps> <index>`): drives the sound preview on a real output
  device: playback at real-time speed, pause and resume, seeking while
  paused and while playing, looping (the sound's loop region if it has
  one), and stopping at the end. In docker, use a PulseAudio null sink:

  ```
  pulseaudio -n --daemonize=yes --exit-idle-time=-1 \
      --load=module-null-sink --load=module-native-protocol-unix
  QT_QPA_PLATFORM=offscreen out/lithrez-build/tests/sound_play_test GAME.REZ CHARS/SND/X.WAV
  ```

## lithrez

The classic commands work as before, with options as letters in the command:

```
lithrez cv game.rez dir             create (verbose) from a directory tree
lithrez c game.rez dir "*.dtx;*.ltb" only these extensions (LithRez 1.10)
lithrez v game.rez                  view
lithrez x game.rez outdir           extract
lithrez f game.rez dir              freshen: add or replace files
lithrez s game.rez                  sort/compact (keeps game.BAK)
lithrez i game.rez                  is-sorted information
```

Options are `v` (verbose), `z` (warn about zero-length files) and `l` (keep
the case of names).

Extra commands: `list` (`--long`, `--json`), `header`, `dump`, `verify`,
`cat`, `get`, `add`, `rm`, `mkdir`, `diff`, `roundtrip`. Run `lithrez help`
for details.

### In CI

The original tools stamp the clock on every entry, so two builds never
match. Give a fixed time to get the same bytes every time:

```
lithrez c build/mymod.rez mod --time 1700000000
SOURCE_DATE_EPOCH=$(git log -1 --format=%ct) lithrez c build/mymod.rez mod
lithrez verify build/mymod.rez
```

`--compat 1.10` matches the later LithRez builds (NOLF2, Contract
J.A.C.K., Tron 2.0), which write 0 for every resource ID. The default,
1.00, matches the NOLF and AvP2 tools, which number them. `--title ""`
leaves the header's second line blank, as Shogo and Blood 2 files have it.

On Linux and macOS the directory is listed in Windows' (NTFS) order, so the
archive is the same as one built on Windows.

## PS2 archives

The Operative: No One Lives Forever on the PS2 keeps its data in
`LITH_PS2.REZ`, a different format (2048-byte sectors, 506 tables, files
known only by a hash of their name). lithrez and LithRez Studio open it, or
a PS2 disc image (`.iso`) holding it, as they open a `.rez`:

```
lithrez list "NOLF (USA).iso"                 every file, named
lithrez x "NOLF (USA).iso" out/               extract everything
lithrez cat LITH_PS2.REZ worlds/m01s01.ltb > m01s01.ltb
lithrez header | verify [--data] | tables "NOLF (USA).iso"
lithrez roundtrip "NOLF (USA).iso" LITH_PS2.REZ   the archive alone, rebuilt from its tables
```

- **Names**: 6040 of the US disc's 6354 files are named from a list
  recovered from the PC and PS2 data (`core/data/ps2-names.txt`, made by
  `tests/ps2_names.py`); the rest are `_UNNAMED/<hash>.<ext>`, and
  `--names <file>` adds more.
- **Compiled attribute files** (`.but`, the PS2's binary attributes and
  asset lists) show as text: `lithrez cat --text`, and Studio's preview.
- **Editing** (`add`, `rm`, `f`, and Studio's edits) is experimental: it
  follows the IOP module's code but hasn't been run on a PS2 or an
  emulator. A replaced file replaces table 0's version and its identical
  copies (other tables' variants, such as PAL layouts, stay); data that
  doesn't fit goes at the end (in a disc image, after the volume, which
  grows); up to 46 files can be added. Names lithrez adds are kept in the
  archive as `LITHTOOLS/NAMES.TXT`.

## LithRez Studio

- **Browse:** folder tree, location bar with history, Back/Forward/Up, and
  large icon, small icon, list and details views. Textures get thumbnails,
  and sprites their first frame's.
- **Edit:** insert files or folders (also by dragging them in), insert empty
  folders, rename, delete, and cut/copy/paste. Paste moves, as in WinRez: a
  rez file never holds the same data twice.
- **Extract:** the selection or everything, or drag items out to your file
  manager.
- **Make:** a new empty rez, a "Rez Project" from a folder (the same bytes
  as `lithrez c`), the Rez Wizard with WinRez's per-game folder layouts,
  Import (another rez's contents) and Merge (several rez files into one).
- **Preview pane:** textures and images (`.dtx`, `.pcx`, `.tga`, `.bmp`,
  every layout of each format, including the 8-bit `.dtx` of Blood 2
  (version -2) and KISS Psycho Circus (-3) with their 4-bit alpha masks; "Magenta transparent" shows the colour key
  the games' interface art uses), sprites (`.spr`, `docs/SPR.md`: the
  frames animated at the sprite's rate as the engine picks them, with
  play/pause, frame stepping, the header and the frame list; a frame the
  archive doesn't hold shows as missing), lip-sync (`.lip`, `docs/LIP.md`:
  the face nodes' curves over time, played with the voice line of the same
  name), texture effects (`.tfs` scripts decompiled, disassembled and
  run, showing the texture matrix they make; `.tfg` groups' stages with
  their scripts linked and their values; `docs/TEXFX.md`), models (`.abc`, `.ltb` and the PS2's `.ltb`)
  with their skins and animations, sounds, text (PS2 `.but` attribute files
  decoded), and hex for everything else. For NOLF2, Contract
  J.A.C.K. and Tron 2.0, skins come from the archive's own
  `ATTRIBUTES/MODELBUTES.TXT` when it has one. Double-click opens the texture viewer, the
  model viewer, the sound player, or (for a DLL) the string table editor;
  other files open with their system program.
- **Sounds:** `.wav` files in every codec the games use (PCM, IMA ADPCM,
  MP3, float) play in the preview, with play/pause (Space), stop, a
  waveform to click or drag through, loop, auto-play and volume. Beside it:
  codec and MPEG details (version, stereo mode, constant or variable bit
  rate), ADPCM block layout, duration, peak and RMS levels, compression,
  INFO tags (title, software, copyright...), and LithTech's own data: the
  `lith` chunk (buffer flags, random pitch range) and the loop the engine
  plays (a cue point and its length), which is shaded on the waveform and
  what Loop repeats. DirectMusic files (`.sgt`, `.sty`, `.dls`) don't play
  yet.
- **PS2 sound banks:** a `.lps` (the PS2 port's level and voice banks,
  `docs/LPS.md`) lists its sounds (name, group, rate, length, loop,
  lip-sync) and plays the selected one in the same player, its loop as the
  PS2's sound chip repeats it. Save as WAV writes the selected sounds, or
  the whole bank in folders by group, with their loops.
- **Settings:** case handling, compatibility (resource IDs), and the header
  title for new files. File > Rez File Information shows the header and lets
  you edit its title.
- **Tools menu:** the texture viewer, model viewer, sound player and string
  editor, which also work on loose files.

- **PS2:** File > Open (or dropping it on the window, or the command line)
  takes a PS2 `LITH_PS2.REZ` or a PS2 disc image: browse it named, preview
  and extract its files. Edits save back into the same kind of file (a disc
  image stays a disc image) after a warning: editing them is experimental,
  and the status bar says so while one is open.

Command line, with WinRez LT's switches:

```
lithrez-studio [file.rez | ps2 disc.iso | file.dtx | file.dll]
lithrez-studio name.rez /C <files or folders>...   create name.rez from them
lithrez-studio /A <files or folders>...            add them, then ask for a name
lithrez-studio /X name.rez <folder>                extract everything
lithrez-studio /E name.rez                         extract, asking where
```

WinRez's Explorer integration (shell menus and DTX thumbnails in Explorer)
is left to the desktop: point a file association or a context-menu entry at
these switches.
