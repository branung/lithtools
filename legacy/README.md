# legacy: the original LithTech development tools, archived

Monolith's own editing tools for the LithTech games, kept here for
preservation and research: how the tools worked, the exact file formats they
read and wrote, and the bugs they shipped with. They are not the tools to use
for new work. For that, see the cross-platform tools at the top of the
repository: [`tools/lithrez/`](../tools/lithrez/README.md) and
[`plugins/blender/`](../plugins/blender/README.md).

| Directory | Tools | How |
|---|---|---|
| [`talon/`](talon/README.md) | The "Talon" tools of No One Lives Forever and Aliens versus Predator 2 (2001-2002): DEdit, ModelEdit, the world processors, LithRez, and the 3ds Max and Maya plug-ins | **Decompiled.** Reconstructed as C++ that compiles, function by function, to the binaries that shipped, with the compilers that built them. Its README has the progress table |
| [`jupiter/`](jupiter/README.md) | The Jupiter tools of No One Lives Forever 2, Contract J.A.C.K. and Tron 2.0: DEdit, ModelEdit, the packers, FxED, the plug-ins | **Built from source:** Touchdown Entertainment's "Jupiter System Enterprise Edition, Build 69" (2006), built with Visual C++ .NET 2003 under Wine |

## How the reconstruction is checked

- **Byte for byte.** `talon/progress/` records every function that compiles
  to exactly the shipped bytes. The build fails if one stops doing so
  (`make progress`, and in CI).
- **By behaviour.** `talon/tests/` runs the shipped tools and the
  reconstructed ones on the same inputs (the sample worlds that shipped with
  the tools) and compares what they write, byte for byte.
- **Retail bugs** found along the way are kept as they shipped, and listed
  in [`talon/RETAIL_BUGS.md`](talon/RETAIL_BUGS.md), for fixing when the
  tools are re-implemented.

The period toolchains (Visual C++ 5.0 and 6.0, Intel C/C++ 4.0, the SDKs of
the time) come from a separate repository, the submodule `ext/toolchains`, and run in
Docker under Wine. The shipped tools that the reconstruction is compared
against are extracted at build time from the original game discs, fetched
from archive.org (`ext/rez`). No shipped binary is in this repository.

## Terms

The tools are Monolith Productions' and LithTech, Inc.'s (later Touchdown
Entertainment's). The reconstructed source is derived from their binaries,
and Build 69's source is theirs. Both are kept here for preservation and
research. Work out the terms before distributing anything built from them
(see [`jupiter/README.md`](jupiter/README.md#terms)). Third-party code keeps
its own terms.
