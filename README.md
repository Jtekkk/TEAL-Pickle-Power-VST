# Pickle Power 🥒⚡

A characterful saturation / "fermentation" effect plugin built with **JUCE 8**
(C++20). Brine it, crunch it, snap it, ferment it — and watch the pickle dance.

> **Phase 1 — Foundation.** This is a complete, compilable v0.1 with the full
> signal chain, parameter system, custom neon UI and the audio-reactive dancing
> pickle. Later phases extend the DSP (multiband, M/S, dynamic EQ, spectral
> saturation) and the GUI.

---

## Formats

- **VST3**
- **AU** (macOS)
- **Standalone** (Windows / macOS / Linux)

## Signal flow

```
Input
  └─ Oversampling  (Off / 2× / 4× / 8×)
       ├─ Brine Saturation     (Dill · Kosher · Garlic · Spicy)
       ├─ Crunch Designer      (transient shaper, −100 … +100)
       ├─ Snap Exciter         (high-frequency "air")
       ├─ Fermentation Engine  (stateful, ages over time)
       └─ Pickle Juice         (one-knob compress + saturate + makeup)
  └─ Stereo Width  (M/S, 0 … 200 %)
  └─ Mix           (dry / wet)
  └─ Output        (−24 … +24 dB)
  └─ True-Peak Limiter
Output
```

> Note: the Output trim is placed *before* the final true-peak limiter so the
> plugin output stays peak-safe regardless of the trim setting.

## Parameters

| Parameter      | Range            | Notes                                    |
|----------------|------------------|------------------------------------------|
| Brine          | 0 – 100 %        | Saturation drive                         |
| Brine Type     | Dill/Kosher/Garlic/Spicy | Harmonic flavour                 |
| Crunch         | −100 – +100 %    | − softens transients, + adds snap        |
| Snap           | 0 – 100 %        | High-frequency exciter                    |
| Fermentation   | 0 – 100 %        | Stateful nonlinear saturation             |
| Age            | 1 day – 10 years | Maturity: ramp time + aggression ceiling  |
| Pickle Juice   | 0 – 100 %        | One-knob glue + harmonics                  |
| Width          | 0 – 200 %        | Stereo width (mono → wide)                |
| Mix            | 0 – 100 %        | Dry / wet                                  |
| Output         | −24 – +24 dB     | Output trim (pre-limiter)                 |
| Oversampling   | Off / 2× / 4× / 8× | Anti-aliasing for the nonlinear core    |

### The Fermentation engine 🧪

Rather than a memoryless `out = tanh(in)`, the engine keeps a slow leaky
integrator of the signal's energy — a *harmonic age*. The longer the plugin is
driven, the more it fills, and the more aggressive the saturation becomes; it
relaxes again when the input goes quiet. The curve is further shaped by RMS,
crest factor and the **Age** control (which sets the ramp time and the
aggression ceiling). The sound literally matures in the jar.

### Nuclear Pickle ☢️

Push Brine, Crunch, Snap, Fermentation **and** Pickle Juice to (near) maximum
and the pickle goes Nuclear — shades on, glowing, spinning.

---

## Building

Requires CMake ≥ 3.22 and a C++20 compiler. JUCE is fetched automatically
(pinned to a release tag), or point CMake at a local copy with `-DJUCE_DIR=...`.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Use a local JUCE checkout instead of downloading:

```bash
cmake -B build -DJUCE_DIR=/path/to/JUCE -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Linux build dependencies

The JUCE GUI / audio modules need a few system packages, e.g. on Debian/Ubuntu:

```bash
sudo apt-get install libasound2-dev libx11-dev libxext-dev libxinerama-dev \
  libxrandr-dev libxcursor-dev libxcomposite-dev libfreetype6-dev \
  libfontconfig1-dev libgl1-mesa-dev libcurl4-openssl-dev libwebkit2gtk-4.1-dev
```

(VST3 + Standalone build on Linux; AU is macOS-only.)

Build artifacts land under `build/PicklePower_artefacts/`.

---

## Project layout

```
PicklePower/
├── CMakeLists.txt
├── assets/                 # design references (UI is drawn procedurally)
└── Source/
    ├── PluginProcessor.{h,cpp}
    ├── PluginEditor.{h,cpp}
    ├── DSP/                # Oversampler, BrineSaturator, CrunchDesigner,
    │                       # SnapExciter, FermentationEngine, PickleJuice,
    │                       # TruePeakLimiter
    ├── GUI/                # NeonLookAndFeel, DancingPickleComponent,
    │                       # PickleJarComponent, BubbleSystem, PickleMeter
    └── Utils/              # Constants, Theme
```

## Roadmap

- **Phase 2** — Brine flavour refinement, full transient designer (attack /
  sustain / mix), three-band Snap.
- **Phase 3** — Deeper Fermentation modelling (spectral density tracking).
- **Phase 5/6** — Pickle Juice macro routing UI, preset system.
- **Pro** — Mid/Side, multiband, dynamic EQ, spectral saturation, auto-gain.

## Licence

The plugin source here is yours to build on. **JUCE** is a separate dependency
under its own licence (GPLv3 or a commercial JUCE licence) — a commercial,
closed-source release requires an appropriate JUCE licence, and the splash
screen must stay enabled until you hold one.
