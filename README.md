# M5 FDM Control
![image](AnkerStudio/resources/icons/AnkerStudio.png)

## Overview

A slimmed-down fork of [M5 FDM Studio](https://github.com/bobbyb/eufyMake-PrusaSlicer-Release-ARM),
the native Apple Silicon build of **eufyMake Studio** for AnkerMake M5 and M5C printers.

Forked from `m5-fdm-studio-v0.1.0`. **No functional changes** — the slicing engine,
profiles, GUI, and networking layers behave exactly as in the parent. The differences are
that files the macOS arm64 build never uses have been removed, and the app is renamed.

### Why this fork exists

Two reasons, in order:

1. **It is the base for a compact-UI variant.** That work has not started; no GUI code has
   been touched yet.
2. **It is much smaller.** The parent carries Intel dylibs, Windows runtime DLLs, unused
   font faces, and gettext `.po` sources — none of which reach a macOS arm64 build.

### Relationship to M5 FDM Studio

M5 FDM Control installs alongside its parent rather than over it: it has its own app name
and its own bundle identifier (`com.bobbyb.m5-fdm-control`).

It deliberately **shares the parent's data directory**. `SLIC3R_APP_KEY` is still
`eufyMake Studio`, so presets, configuration, and the login cache carry over from an
existing eufyMake Studio or M5 FDM Studio install. That key is also the name of the
translation catalogs (`eufyMake Studio.mo`), so changing it would orphan settings *and*
silently disable every translation.

### What was removed

| Removed | Size | Why it is safe |
|---|---|---|
| `AnkerStudio/deps/bin/mac/x86/` | 57 MB | 28 dylibs, all `Mach-O x86_64`. This fork builds `-DCMAKE_OSX_ARCHITECTURES=arm64` against `deps/deps_build/destdir`, never these. |
| `AnkerStudio/pack/windows/` | 22 MB | Windows runtime (`opengl32.dll`, WebView2, CRT). macOS-only fork. |
| `AnkerStudio/deps/bin/win/` | 1 MB | `PPCS_API.dll`, `paho-mqtt3cs.dll` — Windows builds of the P2P/MQTT libs. |
| 4 HarmonyOS Sans SC weights | 31 MB | Thin, Light, Medium, Black. Zero references in the source tree; only Bold and Regular are loaded, by `AnkerFont.cpp` and `Widgets/Label.cpp`. |
| `NotoSansCJK-Regular.ttc` | 19 MB | No code reference. macOS supplies system CJK coverage (PingFang, Hiragino), and HarmonyOS Sans SC covers Simplified Chinese. |
| `*.po` / `*.pot` in `localization/` | 27 MB | gettext **sources**. Only the compiled `.mo` catalogs are read at runtime, via `wxFileTranslationsLoader`. |

**Total: 157 MB removed, 109 files. Source tree 254 MB → 98 MB.**

### What was deliberately kept

- **All 21 language catalogs.** Dropping `.po` sources removes no language — every `.mo`
  is intact, so the app is exactly as localized as the parent.
- **`resources/calib/` (11 MB of STLs).** These look like dead weight but every one is
  loaded by `GUI/Calibration/FlowCalibration.cpp`. Removing them breaks the calibration
  features.
- **`HarmonyOS_Sans_SC_{Bold,Regular}.ttf`, `NotoSans-Regular.ttf`.** All three are loaded
  by path at runtime.

### Restoring anything

Every removed file is still in the parent repo at tag `m5-fdm-studio-v0.1.0`:

```bash
git --git-dir=../eufyMake-PrusaSlicer-Release-ARM/.git \
    show m5-fdm-studio-v0.1.0:AnkerStudio/resources/fonts/NotoSansCJK-Regular.ttc \
    > AnkerStudio/resources/fonts/NotoSansCJK-Regular.ttc
```

## Known wrinkles

- `.gitignore` is inherited verbatim and contains `*.txt` and `*.ttf`, which match every
  `CMakeLists.txt` and every bundled font. They are tracked here because the initial commit
  force-added them; a plain `git add` will **not** pick up a new `CMakeLists.txt`. Use
  `git add -f`, or narrow those patterns.
- Renaming the app changed the msgid of every user-facing string containing the app name,
  so those specific strings now fall back to English in translated builds. This is
  inherited, not new — the parent already broke the same strings when it renamed away from
  `eufyMake Studio`. Regenerating the catalogs would fix it, and needs the `.po` sources
  from the parent repo.

## Features

- **Runs on macOS 27**, which drops Intel app support — fully native arm64, no Rosetta;
- Basic slicing features & GCode viewer;
- Remote control & monitoring;
- Live camera streaming, on the local network and remotely;
- No proprietary network plugin, and no plugin download prompt;

See [RELEASE_NOTES.md](RELEASE_NOTES.md) for what works, and for the list of things that
are known not to work.

## How to compile

- macOS on Apple Silicon, [Compile Guide](AnkerStudio/doc/MacOs_build.md)

This fork targets Apple Silicon only. For Windows or Intel builds, use the
[upstream project](https://github.com/eufymake/eufyMake-PrusaSlicer-Release).

## Lineage

This is an unofficial community fork. It is not affiliated with, endorsed by, or supported
by Anker or eufyMake.

Compact fork of [M5 FDM Studio](https://github.com/bobbyb/eufyMake-PrusaSlicer-Release-ARM),
which forks [eufyMake Studio](https://github.com/eufymake/eufyMake-PrusaSlicer-Release)
(tracking upstream v1.5.26), which is based on
[PrusaSlicer](https://github.com/prusa3d/PrusaSlicer) by Prusa Research, which is from
[Slic3r](https://github.com/Slic3r/Slic3r) by Alessandro Ranellucci and the RepRap
community.

## License

M5 FDM Control is licensed under the GNU Affero General Public License, version 3. It is
based on eufyMake Studio, which is based on PrusaSlicer by PrusaResearch.

PrusaSlicer is licensed under the GNU Affero General Public License, version 3. PrusaSlicer is owned by Prusa Research. PrusaSlicer is originally based on Slic3r by Alessandro Ranellucci.

Slic3r is licensed under the GNU Affero General Public License, version 3. Slic3r was created by Alessandro Ranellucci with the help of many other contributors.

The GNU Affero General Public License, version 3 ensures that if you use any part of this software in any way (even behind a web server), your software must be released under the same license.
