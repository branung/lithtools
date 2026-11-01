## Download

| File | What it is |
|---|---|
| `lithtools-{{NAME}}-windows-x64.zip` | `lithrez.exe` and LithRez Studio for Windows 10 and later (64-bit) |
| `lithtools-{{NAME}}-linux-x86_64.tar.gz` | `lithrez` and LithRez Studio (an AppImage) for 64-bit Linux |
| `lithtools-{{NAME}}-macos-universal.zip` | `lithrez` and LithRez Studio for macOS 12 and later (Apple silicon and Intel) |
| `lithtools-{{NAME}}-blender.zip` | the Blender extension: LithTech models and textures in Blender 5.2 and later |

No game data is included.

## The tools

- **`lithrez`**: LithTech's resource compiler, rebuilt for Windows, Linux and macOS. The classic commands (`c`, `v`, `x`, `f`, `s`, `i`) work as LITHREZ 1.00 to 1.10 do, and the archives it creates are byte-identical to theirs. There are extra commands for debugging and build scripts, and `--time` (or `SOURCE_DATE_EPOCH`) gives reproducible archives. Run `lithrez help`.
- **LithRez Studio**: browse, edit and create `.rez` files, with a texture viewer, a model preview, a sound player and a `CRes.dll` string table editor. It succeeds WinRez LT Studio.
- **Blender extension**: import and export of LithTech models, both NOLF and AvP2 `.abc` and NOLF2, Contract J.A.C.K. and Tron 2.0 `.ltb` (with skeletons, weights, LODs, sockets and animations), and of `.dtx` textures.

The guide is [`tools/lithrez/README.md`](https://github.com/{{REPO}}/blob/{{TAG}}/tools/lithrez/README.md). The Blender extension's guide is [`plugins/blender/docs/BLENDER.md`](https://github.com/{{REPO}}/blob/{{TAG}}/plugins/blender/docs/BLENDER.md).

## Install

### Windows

Unpack the zip anywhere and run `lithrez-studio.exe`. `lithrez.exe` is the command line tool.

### Linux

```sh
tar xzf lithtools-{{NAME}}-linux-x86_64.tar.gz
cd lithtools-{{NAME}}-linux-x86_64
./LithRezStudio-x86_64.AppImage
./lithrez help
```

The AppImage needs FUSE 2 (`libfuse2`). Without it, run it with `--appimage-extract-and-run`.

### macOS

Unpack the zip and move `LithRez Studio.app` to Applications. The app isn't notarised, so the first time, right-click it and choose **Open** (or run `xattr -dr com.apple.quarantine "LithRez Studio.app"`). The same applies to `lithrez`.

### Blender

In Blender, open **Edit > Preferences > Get Extensions**, then choose **Install from Disk...** from the menu at the top right and pick `lithtools-{{NAME}}-blender.zip`. The importers and exporters appear under **File > Import** and **File > Export**.
