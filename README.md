# lithtools

Tools for the LithTech engine's games and their files: No One Lives Forever,
Aliens versus Predator 2, No One Lives Forever 2, Contract J.A.C.K., Tron 2.0
and others of that era. Part of a game preservation effort: modern,
cross-platform tools for the files these games ship, and the original
development tools archived and reconstructed from their binaries for
research.

<p align="center">
  <img src="screenshots/lithrez_studio.png" alt="LithRez Studio: No One Lives Forever 2's GAME.REZ open, previewing Cate Archer's HERO_CASUAL.LTB model with its pieces, sockets and skins" width="49%">
  <img src="screenshots/blender.png" alt="Blender: No One Lives Forever 2's mime model imported with its LODs, and the LithTech Animations panel" width="49%">
</p>

## What's here

| Directory | What it is |
|---|---|
| [`tools/lithrez/`](tools/lithrez/README.md) | **lithrez** (command line) and **LithRez Studio** (Qt 6): cross-platform tools for `.rez` archives, with texture, model and sound previews; and `core/`, the C++17 library they share for the games' file formats (`.rez`, `.dtx`, `.abc`, `.ltb`, sounds, attribute files) |
| [`plugins/blender/`](plugins/blender/README.md) | a Blender extension that imports and exports LithTech models (`.abc`, `.ltb`, the PS2's `.ltb`) and DTX textures |
| [`legacy/`](legacy/README.md) | the original LithTech development tools, archived and decompiled for research: the "Talon" tools of NOLF and AvP2, rebuilt as source that compiles to the shipped binaries, and the Jupiter tools of NOLF2 and its successors, built from Build 69's source |
| `scripts/` | survey scripts: the inventory of every tools release (`inventory.py`), the format versions in each game's assets (`fmtversions.py`), and installer unpackers |
| [`INVENTORY.md`](INVENTORY.md) | every public LithTech tools release for these games, and what built each binary (`inventory.tsv` has one row per binary) |
| [`FORMATS.md`](FORMATS.md) | which format versions each game ships and each tool build reads and writes |

`downloads/`, `extracted/` and `out/` are local working directories and are
not in the repository.

## Building

- **lithrez and LithRez Studio:** CMake, with clang, gcc or MSVC on Linux,
  macOS and Windows; LithRez Studio needs Qt 6.2 or later. See
  [`tools/lithrez/README.md`](tools/lithrez/README.md).
- **The Blender extension:** packaged by `plugins/blender/package.py`; see
  its README.
- **The legacy tools:** built in Docker with the period compilers (Visual
  C++ 5 and 6, Intel C/C++ 4.0, Visual C++ .NET 2003) under Wine; see each
  project's README under [`legacy/`](legacy/README.md).

## Continuous integration

- [`tools.yml`](.github/workflows/tools.yml): builds and tests lithrez and
  LithRez Studio on Windows, Linux and macOS, packs the Blender extension,
  and publishes release builds.
- [`talon.yml`](.github/workflows/talon.yml): builds the Talon
  reconstruction from the original game discs (fetched from archive.org),
  checks every function recorded as byte-identical still is, and runs the
  behaviour tests against the shipped tools.

## Releases

The tools workflow's builds (Windows, Linux AppImage, macOS universal, and
the Blender extension) are attached to each tagged release, and to a
nightly build of the latest commit.

## Credits

This project builds on the work of others:

- **Monolith Productions**, for _No One Lives Forever_, _Aliens versus
  Predator 2_, _No One Lives Forever 2_ and the other LithTech games, and
  for the editing tools they released to their players, which `legacy/`
  archives.
- [StenApp](https://github.com/StenApp) for generously sharing his invaluable,
  in-depth knowledge of all things NOLF and LithTech.
