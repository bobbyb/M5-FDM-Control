# M5 FDM Control
![image](AnkerStudio/resources/icons/AnkerStudio.png)

## Overview

A **printer control application** for AnkerMake M5 and M5C printers, native to Apple
Silicon. It monitors your printers, streams their cameras, drives the gantry by hand, and
sends pre-sliced G-code jobs.

**It does not slice.** The slicing engine, the 3D view, the object list and the entire
preset system have been removed. Slice elsewhere — eufyMake Studio, OrcaSlicer, PrusaSlicer
— and use this to send the result and to control the machine.

It began as a size-trimmed fork of
[M5 FDM Studio](https://github.com/bobbyb/eufyMake-PrusaSlicer-Release-ARM), the native
Apple Silicon build of eufyMake Studio, and has since had the slicer taken out of it.

## What it does

- **Device tab** — status, temperatures, print progress, Z-offset, extrude/retract, and
  live camera streaming over the local network or remotely.
- **Device Details tab** — manual control:
  - X/Y/Z jog with a selectable step (1 / 10 / 20 / 50 mm) and per-axis homing
  - Auto-Level (`G29`), behind a confirmation that names the printer it will tie up
  - a raw G-code box, unfiltered, with a per-printer history of what you sent
- **Start Printing** — `File > Start Printing` opens a G-code file and sends it to a
  printer. It reads print time, filament use and thumbnails from the file; see
  [docs/GCODE_FLAVOURS.md](docs/GCODE_FLAVOURS.md) for which slicers' output is understood
  (eufyMake and PrusaSlicer fully, OrcaSlicer partially).
- **No proprietary network plugin.** The closed-source `libAnkerNet` has been replaced with
  a native implementation, so there is no plugin download prompt. The MQTT broker's
  certificate is pinned rather than verification being skipped.

Runs on **macOS 27**, which drops Intel app support — fully native arm64, no Rosetta.

See [RELEASE_NOTES.md](RELEASE_NOTES.md) for what works and what is known not to.

## What was removed, and why

The slicing subsystem is gone from the build entirely — not merely hidden. That is roughly
240 source entries: `Plater`, the OpenGL stack, all 20 gizmos, the object list/bar/
manipulation panels, the preset tabs and configuration wizard, the G-code preview,
calibration, and the background slicing process.

The binary is **75 MB**, down from 90 MB before the removal. Earlier size trimming (Intel
dylibs, Windows runtime, unused font weights, gettext `.po` sources) had already taken the
source tree from 254 MB to 98 MB.

Deliberately kept:

- **All 21 language catalogs.** Only the compiled `.mo` files are read at runtime, so
  dropping the `.po` sources removed no language.
- **`AnkerTaskPanel`, `AnkerSliceCommentDialog`, `Utils/WxFontUtils`** — these read as
  slicing code but are not; the first two are Device-tab code and the third is used by the
  dialogs that remain.

Everything removed is still in the parent repo at tag `m5-fdm-studio-v0.1.0`.

## Installing alongside M5 FDM Studio

M5 FDM Control has its own app name and bundle identifier
(`com.bobbyb.m5-fdm-control`), so it installs *beside* its parent rather than over it.

**It shares the parent's data directory.** `SLIC3R_APP_KEY` is still `eufyMake Studio`, so
configuration and the login cache carry over from an existing eufyMake Studio or M5 FDM
Studio install. That key is also the basename of the translation catalogs
(`eufyMake Studio.mo`), so changing it would orphan settings *and* silently disable every
translation.

The practical consequence: **running either app writes settings the other reads**, window
geometry included. If you use both, expect them to tread on each other.

## How to compile

macOS on Apple Silicon — [Compile Guide](AnkerStudio/doc/MacOs_build.md).

Apple Silicon only. For Windows or Intel builds, use the
[upstream project](https://github.com/eufymake/eufyMake-PrusaSlicer-Release).

Two things that will bite you:

- The app loads **two** build products: `Contents/MacOS/eufyStudio` and
  `Contents/Frameworks/libAnkerNet.dylib`. Everything under `src/slic3r/GUI/AnkerNetModule/`
  compiles into the dylib. Use `make_app.sh` to bundle; copying just the executable leaves
  the app running stale networking code with no error.
- In a Release build the loader does not use the bundled dylib directly. It loads a cached
  copy from `~/Library/Application Support/eufyMake Studio Profile/OnlineAnkerNet/Current/`,
  and only refreshes it when the cached one fails to load. Changing net code and seeing no
  effect almost always means that cache is stale.
- `.gitignore` is inherited verbatim and contains `*.txt` and `*.ttf`, which match every
  `CMakeLists.txt` and bundled font. They are tracked only because the initial commit
  force-added them; a plain `git add` will **not** pick up a new `CMakeLists.txt`.

## Status

Early. The jog controls, Auto-Level, the G-code box and the print path have been exercised
on real hardware, but this is a young fork and the slicing removal touched a great deal.

Known rough edges:

- The Device Details settings rows below Auto-Level (Accessories, Wi-Fi, AI Settings,
  Share Printer, Timelapses, About Device) are **placeholders** and are greyed out.
- Debug tracing is still compiled in and prints to stderr on every button press.
- Extrude/retract over the vendor G-code channel are believed correct but have not been
  confirmed on a machine.

## Lineage

This is an unofficial community fork. It is not affiliated with, endorsed by, or supported
by Anker or eufyMake.

Fork of [M5 FDM Studio](https://github.com/bobbyb/eufyMake-PrusaSlicer-Release-ARM), which
forks [eufyMake Studio](https://github.com/eufymake/eufyMake-PrusaSlicer-Release) (tracking
upstream v1.5.26), which is based on
[PrusaSlicer](https://github.com/prusa3d/PrusaSlicer) by Prusa Research, which is from
[Slic3r](https://github.com/Slic3r/Slic3r) by Alessandro Ranellucci and the RepRap
community.

Protocol details for the AnkerMake MQTT and P2P layers were informed by
[Ankermgmt/ankermake-m5-protocol](https://github.com/Ankermgmt/ankermake-m5-protocol).

## License

M5 FDM Control is licensed under the GNU Affero General Public License, version 3 — see
[LICENSE](LICENSE). It is based on eufyMake Studio, which is based on PrusaSlicer by Prusa
Research.

PrusaSlicer is licensed under the GNU Affero General Public License, version 3. PrusaSlicer is owned by Prusa Research. PrusaSlicer is originally based on Slic3r by Alessandro Ranellucci.

Slic3r is licensed under the GNU Affero General Public License, version 3. Slic3r was created by Alessandro Ranellucci with the help of many other contributors.

The GNU Affero General Public License, version 3 ensures that if you use any part of this software in any way (even behind a web server), your software must be released under the same license.
