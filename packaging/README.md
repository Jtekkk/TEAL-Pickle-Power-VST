# Packaging & installers

Build the plugin in **Release** first, from the repo root:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Artefacts land in `build/PicklePower_artefacts/Release/` (`VST3/`, `AU/` on macOS,
`Standalone/`).

## Windows — Inno Setup
Requires [Inno Setup 6](https://jrsoftware.org/isinfo.php).

```bat
ISCC.exe packaging\windows\PicklePower.iss
```

Produces `packaging\windows\Output\PicklePower-<ver>-Windows.exe`, which installs
the VST3 to `Program Files\Common Files\VST3` and the standalone app.

## macOS — .pkg
```bash
bash packaging/macos/build_pkg.sh
```
Produces `PicklePower-<ver>-macOS.pkg` installing the VST3 + AU to the system
plug-in folders and the standalone to `/Applications`. The package is **unsigned**;
for public distribution, codesign + notarize it.

## Linux — user install
```bash
bash packaging/linux/install.sh
```
Copies the VST3 to `~/.vst3`.

## CI

`.github/workflows/build.yml` builds + tests on Linux / macOS / Windows and
uploads, per OS:

- the raw plugin artefacts,
- a `.zip` / `.tar.gz` package,
- a native installer (Windows `.exe` via Inno Setup, macOS `.pkg`) — best-effort.

Native-installer steps are `continue-on-error`, so the core build/test stays green
even if an installer tool needs tweaking on a runner.
