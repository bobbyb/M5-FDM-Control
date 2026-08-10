# Building M5 FDM Studio on macOS

This fork builds for **Apple Silicon (arm64) only** — `gen_xcode_proj.sh` passes
`-DCMAKE_OSX_ARCHITECTURES=arm64`. There is no Intel or universal build.

## 0. Prerequisites

- An Apple Silicon Mac (M1 or later), macOS 11 or newer
- Xcode from the App Store, plus the command line tools (`xcode-select --install`)
- Homebrew packages:

```
brew install cmake git gettext openssl@3 xz zstd jansson dylibbundler
```

`cmake`, `git`, and `gettext` are needed to build. `openssl@3`, `xz`, `zstd`, and
`jansson` are linked by the application. `dylibbundler` is only needed if you want to
package a distributable `.app` (see step 4).

FFmpeg is **not** required. The camera stream is decoded with VideoToolbox on macOS;
FFmpeg is only used by the Windows build.

## 1. Download sources

Clone the repository. Use a directory relatively close to the drive root, so the path is
not too long. Avoid spaces and non-ASCII characters:

```
mkdir src
cd src

git clone https://github.com/bobbyb/eufyMake-PrusaSlicer-Release-ARM.git
```

## 2. Compile the dependencies

```
cd eufyMake-PrusaSlicer-Release-ARM/AnkerStudio/deps
mkdir deps_build
cd deps_build
cmake ..
make -jN
```

`N` is the number of CPU cores, so for example `make -j8` on an 8-core machine. This takes
a while — it builds Boost, wxWidgets, and the other bundled libraries.

The result lands in `AnkerStudio/deps/deps_build/destdir`, which is where the next step
expects to find it.

## 3. Generate the Xcode project

```
cd ../..          # back to AnkerStudio/
./gen_xcode_proj.sh
```

The script resolves every path relative to its own location, so there is nothing to edit
before running it — a fresh clone in any directory works as-is. It creates
`build_xcode/`, runs CMake with the Xcode generator, and copies the pinned certificates
from `resources/crt/` into the build output directory.

## 4. Build and run

Open `AnkerStudio/build_xcode/eufyStudio.xcodeproj` in Xcode, select the **eufyStudio**
scheme, and press Run. Debug and Release both work.

To build from the command line instead:

```
xcodebuild -project build_xcode/eufyStudio.xcodeproj -target eufyStudio -configuration Debug -jobs 8
```

The binary lands in `build_xcode/src/Debug/` (or `Release/`).

### Packaging a distributable app

Running the binary directly out of the build directory works, but it is not an application
bundle, so macOS gives it no `Info.plist` and no menu bar. To assemble a real `.app`, use
`make_app.sh`:

```
CONFIG=Debug DO_NOTARIZE=0 DO_DMG=0 DEV_ID="" ./make_app.sh   # quick local bundle
./make_app.sh                                                 # full: Release, signed, notarized, DMG
```

The full pipeline needs a "Developer ID Application" certificate and notarization
credentials stored once with `xcrun notarytool store-credentials`. See the comments at the
top of `make_app.sh`. Pass `DEV_ID` in the environment rather than editing the script, so
your signing identity is not committed.

## Network plugin

**No plugin download is needed, on any architecture.**

Upstream ships the network layer — login, device discovery, telemetry, printing — as a
closed-source `libAnkerNet.dylib` that the app downloads on first run. That library is
x86_64 only, so it cannot load on Apple Silicon.

This fork builds a native replacement from source at
`src/slic3r/GUI/AnkerNetModule/AnkerNetNative.*`. It is compiled as `libAnkerNet.dylib`
alongside the executable and loaded through the same `boost::dll` contract, so nothing
needs to be copied into place and the app never prompts to install a plugin. It is built
automatically; `OPEN_SOURCE` is set `ON` in `CMakeLists.txt` and is not a user-facing
option.

Both Debug and Release work. Upstream's requirement to use Release mode for the network
plugin does not apply here.
