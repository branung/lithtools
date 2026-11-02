# tests: behaviour, against the shipped tools

`make progress` checks that the reconstructed code is the shipped code,
function by function. These tests check what the tools *do*: each suite
runs a shipped tool and ours on the same inputs and compares what they
write, byte for byte. A near function that compiles differently can still
behave the same, and one that looks almost right can compute something
else; a test like these is how the second kind shows (two did, in the
AvP2 processor: `docs/PLAN.md`).

```sh
make reference      # the shipped tools and their sample data, into out/reference/
make                # ours
make test           # every suite
make test TESTS=processor-avp2
```

The shipped tool is the reference, so no expected output is stored here:
both run in the same container, under the same Wine prefix and virtual
display (`lib.sh`), so nothing depends on another machine's Wine behaving
the same. Work files go to `out/tests/<suite>/`, the shipped side's under
`ship/`, ours under `ours/`, so a failure can be diffed afterwards (the Wine
prefix is `out/tests-wine/`).

Where a retail bug makes the shipped tool's own output vary, the
comparison is normalised over exactly the bytes the bug makes vary, and
says so: the processor's physics BSP vertex colours (`datcmp.py`), every
other byte compared raw. A pass that needed it says "equal but for N ...
bytes" rather than "identical". The exception is tested against the
shipped tool itself: a control run of the shipped processor from another
path, whose garbage differs, must pass the same comparison with the
shipped run (Sample_DM always, every world with `CONTROL=1`). That control
is also why the exception is no narrower: garbage can be 255, so a poly
can be all 255 in one retail run and not in another.

`pending.txt`'s suites are failing ones, not covered ones: the default
`make test` covers the AvP2 processor only, not NOLF's processor, the
editors, the packer or the Max and Maya plug-ins.

| Suite | What it runs | Inputs |
|---|---|---|
| `processor-avp2` | `Processor.exe <world.ed> -skipdialog` (default options), shipped (AvP2 v1.0) and ours | the seven sample worlds of the AvP2 Editing Tools v1.0 |
| `processor-nolf` (pending) | `Processor.exe <world>` with `processor.opt` (`-skipdialog`): DEdit's default options, and with lighting and full visibility; shipped (NOLF Tools 001) and ours | the six sample worlds of the NOLF Tools 001 (from NOLFTOOLS001.REZ) |

A suite listed in `pending.txt` is one ours doesn't pass yet: `make test` skips it
and `make test TESTS=<suite>` runs it. It leaves the list when it passes.

## Adding a suite

A suite is `tests/<name>.sh`, sourced by `run.sh` with `lib.sh` loaded; it
defines `suite_<name>` (dashes as underscores) and reports each case with
`pass` or `fail`. Use `need` for its inputs, `run_tool` to run a Windows
tool to completion in a working directory of its own, and `winpath` for the
paths it is given. Keep the two sides' conditions identical: the same
inputs, fresh working directories (several tools read settings from theirs,
as the processor reads `processor.opt`), and paths of the same length (the
processor's output follows the heap: `RETAIL_BUGS.md`). Add the suite to the
table above.

Good cases are the ones a tool's users run: the sample data the tools
shipped with, every option that changes the output, and anything that once
caught a difference.
