# Audio DSP Research — Math & Techniques for Pickle Power

> A fact-checked, cited reference compiled to raise the algorithmic quality of the
> **Pickle Power** plugin. Each section gives the load-bearing math, concrete
> implementation guidance, authoritative sources, and a mapping to what we should
> do in this codebase.
>
> **Method.** The question was decomposed into focused angles and researched by
> parallel agents doing web search + source fetching, with claims cross-checked
> across ≥2 independent sources where possible and uncertain claims flagged.
> **Sourcing caveat:** during research, automated fetching of several primary PDFs
> (ITU-R, EBU, CCRMA/Julius Smith, some blogs) returned HTTP 403; those were
> verified via search extracts + corroborating implementations (FFmpeg/libebur128,
> librosa, JUCE source). Byte-exact coefficient tables should be confirmed against
> the official PDFs before shipping.

---

## 1. Anti-aliasing of nonlinear waveshaping — ADAA vs oversampling

Every memoryless shaper in Pickle Power (Brine, Fermentation, Snap, Pickle Juice,
Multiband) generates harmonics above Nyquist that fold back as **aliasing**. Two
complementary cures: **oversampling** (§2) and **Antiderivative Anti-Aliasing (ADAA)**.

**1st-order ADAA.** For a memoryless shaper `f` with first antiderivative `F1`
(`F1' = f`):
```
        F1(x[n]) − F1(x[n−1])
y[n] = ───────────────────────         if |x[n] − x[n−1]| > EPS
           x[n] − x[n−1]

y[n] = f( (x[n] + x[n−1]) / 2 )         otherwise   (ill-conditioned fallback)
```
The fallback is the analytic limit (L'Hôpital) at the **midpoint** — required
because the divided difference suffers catastrophic cancellation when consecutive
samples are nearly equal (numerator and denominator both → 0). Faust's `aanl.lib`
uses `EPS = 0.001`. Group delay ≈ **0.5 sample**. *(Formula verified verbatim from
Faust `aanl.lib`; matches the canonical references.)*

**2nd-order ADAA** uses the second antiderivative `F2` and **two** past samples
(`x[n−1]`, `x[n−2]`) — better SNR / steeper alias roll-off, group delay ≈ **1.0
sample**, and **two** fallback branches (two divisions → two near-zero risks). Faust
`T1 + T2` form (verified):
```
T1 = (x[n]·(F1(x[n])−F1(x[n−1])) − (F2(x[n])−F2(x[n−1]))) / (x[n]−x[n−1])²
T2 = (x[n−2]·(F1(x[n−2])−F1(x[n−1])) − (F2(x[n−2])−F2(x[n−1]))) / (x[n−2]−x[n−1])²
y[n] = T1 + T2          (each branch falls back to ½·f((·+2·x[n−1])/3) when ill-conditioned)
```

**Attribution (corrected during verification):** the seminal paper is
**J. D. Parker, V. Zavalishin & E. Le Bivic, "Reducing the Aliasing of Nonlinear
Waveshaping Using Continuous-Time Convolution," DAFx-16.** Higher-order ADAA is
**Bilbao, Esqueda, Parker & Välimäki, IEEE SP Letters, 2017** (this is where
*Esqueda* appears); **Holters (2019/2020)** extends ADAA to **stateful** systems.

**ADAA vs oversampling:**

| | ADAA | Oversampling |
|---|---|---|
| CPU | Cheap (extra antiderivative evals + branches) | Scales with factor (filters + N× the nonlinearity) |
| Strength | Best at suppressing **low-frequency (audible)** aliases | Uniformly pushes images up; quality = filter steepness |
| Weakness | Less effective at **HF**; acts as a mild in-band low-pass | Brute force; expensive at high factors |
| Latency | p/2 samples (0.5 / 1.0) | Filter-dependent |

**Consensus: combine them.** ADAA steepens the alias roll-off so even modest
oversampling becomes very effective — practical sweet spots: **1st-order ADAA + ~4×
OS**, or **2nd-order ADAA + 2×**.

**Gotchas:** use **double precision** for the divided differences (Faust calls this
"vital"); the EPS branch is mandatory; carry the `F1/F2` state across blocks (resetting
input but not `F` state clicks); ADAA imparts a slight HF roll-off + p/2 delay.
**Shaper suitability:** **hard-clip** has trivial polynomial antiderivatives → easiest
and benefits most (aliasing is severe there); **`tanh`** has `F1 = log(cosh x)` (cheap)
but `F2` needs the **dilogarithm Li₂** (expensive) — so `tanh` is usually done at
**1st-order only or via a lookup table**. Ready-made: **chowdsp** (`chowdsp_utils`) has
`ADAAHardClipper`, `ADAATanhClipper` (2nd-order, +1 sample), `ADAASoftClipper<deg>`.

---

## 2. Oversampling filter design

**Half-band filter** = the core 2× building block: cutoff at `fs/4` (half of Nyquist).
In a symmetric FIR half-band roughly every other tap is zero, so a polyphase
implementation makes one branch a pure delay → ~50% fewer multiplies. Multi-stage
factors (2×→4×→8×) cascade half-band stages; requirements relax above the first
stage so higher stages are cheaper.

**Polyphase IIR vs FIR (the central trade-off):**

| | FIR equiripple half-band | Polyphase IIR (allpass cascade) |
|---|---|---|
| Phase | **Linear** | **Minimum** (phase compromised near Nyquist) |
| Latency | High (= half the FIR length) | Very low (non-integer), bottoms out ~5 samples |
| Cost | Higher (~5× more multiplies) | Very low (~2.5 mult/sample) |
| Note | always stable | stable, but no stable IIR is truly linear-phase |

**JUCE `juce::dsp::Oversampling`** (what we use) — factors 2/4/8/16 via cascaded
stages. Verbatim trade-off from JUCE: *"With FIR filters the phase is linear but
the latency is maximised. With IIR filtering the phase is compromised around the
Nyquist frequency but the latency is minimised."*
- `filterHalfBandFIREquiripple` — linear-phase FIR (Zahradnik non-iterative design).
- `filterHalfBandPolyphaseIIR` — minimum-phase IIR allpass cascade; exact latency query.
- `getLatencyInSamples()` returns a **float** (IIR latency is non-integer);
  `setUsingIntegerLatency(true)` adds a fractional delay so total latency is an
  integer the host can compensate. Plugins can only report integer latency, so
  round or enable integer latency, then `setLatencySamples(round(...))`.

**Guidance:** for nonlinear processors, **2×–4×** is the saturation sweet spot, 8×
for aggressive clipping. Inserts/live (drums) → IIR (min phase, low latency);
mastering/linear-phase need → FIR (accept latency).

---

## 3. True-peak (inter-sample peak) metering & limiting — ITU-R BS.1770-4

**dBTP** = max of the *continuous-time* (reconstructed) waveform, relative to full
scale; `0 dBTP` ≡ a full-scale sine asynchronous to `fs`. Sample-peak can
underestimate the true peak by **≥3 dB** (inter-sample overshoots).

**BS.1770 true-peak algorithm (Annex 2):**
`attenuate −12.04 dB → ≥4× oversample → low-pass (polyphase FIR) → abs() → 20·log10(·) → +12.04 dB`.
- `−12.04 dB` (= 20·log10(4), a 2-bit shift) is headroom so processing can't clip;
  it's added back afterward.
- **≥4× oversampling is the minimum** (i.e. ≥192 kHz from 48 kHz); the interpolator
  is commonly cited as an order-48, 4-phase FIR (12 taps/phase). *(Exact coefficient
  table lives only in the ITU PDF Annex — verify there.)*

**Ceiling:** EBU R128 / AES TD1008 → **−1 dBTP** maximum true peak; some use
**−2 dBTP** for very loud (> −14 LUFS) or heavily lossy-encoded delivery, because
DAC reconstruction and MP3/AAC transcoding can push peaks above the mastered ceiling.

**Lookahead brick-wall limiter — architecture (Rudrich / Signalsmith):**
1. Compute required gain reduction in the **log (dB) domain**, max across channels
   (for true-peak: detect on the oversampled signal).
2. Delay **both audio and the gain signal by the lookahead** (same delay).
3. **Smooth the gain-reduction envelope** — the critical step. Two documented methods:
   - **Rudrich:** process the GR buffer **time-reversed**, fading gain in along a
     **linear dB-domain slope** so protection is never reduced at the peak.
   - **Signalsmith:** **moving-maximum (peak-hold)** over the lookahead window, then
     smooth with a **cascade of box filters** (non-negative impulse → stays ≥ the raw
     peak envelope, never under-protects); a triangular/Hann-like kernel has no overshoot.
4. Apply the smoothed gain to the delayed audio, then a **final hard clip** as an
   ISP safety net.

> **Key principle (both authors):** *smooth the gain signal, not the audio.* The
> lookahead window length **is** the attack time.

**Concrete params:** lookahead ~**5 ms** (range 0.5–20 ms); release ~**50–100 ms**
(or program-dependent); ceiling **−1 dBTP**; ≥4× oversample the detection/GR path.

---

## 4. Transient designer (attack / sustain shaping)

**Origin — SPL "Differential Envelope Technique"** (Ruben Tilgner): two parallel
envelope followers with **different attack times but the same release**; a
differential amplifier subtracts them. When the fast envelope exceeds the slow one,
that positive difference *is* the attack transient and drives a VCA. **No threshold
needed** — scaling the input scales both envelopes equally, so the gain trajectory
is **level-independent**. SPL uses a second pair for the release/sustain region.
Control ranges: **Attack ≈ ±15 dB, Sustain ≈ ±24 dB**.

**DSP realization:**
```
attack region:   Δ_att = E_fastAttack − E_slowAttack         (>0 at onsets)
sustain region:  Δ_sus = E_slowRelease − E_fastRelease       (>0 on the decay tail)
gain_dB = K_att · shaped(Δ_att) + K_sus · shaped(Δ_sus)      (K from the knobs)
y[n] = dbToGain(gain_dB) · x[n]
```
One-pole follower (musicdsp canonical): `coeff = exp(-1/(fs·time))`, with separate
attack/release coefficients chosen by `in > env`. Representative times (one impl):
fast attack ≈ 1 ms, slow attack ≈ 15 ms, shared release ≈ 20 ms. Detection is
usually mono-linked; multiband designs run the whole detector per band.

> **This is exactly what our `CrunchDesigner` does** (dual attack pair + dual
> release pair, `tanh`-shaped difference → dB gain, mono-linked). The research
> confirms the architecture is canonical; our ±15 dB attack / ±15 dB sustain could
> be widened toward SPL's ±24 dB sustain.

---

## 5. Dynamic EQ topologies

**A band = a peaking/shelf filter whose gain `G(n)` is modulated by a level
detector keyed on that band** (usually via a band-pass sidechain at the same
freq/Q): `sidechain BP → detector (env follower) → gain computer
(threshold/ratio/range/attack/release) → set band gain`.

- **Downward** (common): exceed threshold → **cut** the band (tame resonances when
  loud). **Upward**: boost-on-loud (expansion) or boost-on-quiet (upward
  compression). FabFilter encodes direction with the **sign of Range**.
- Typical params: threshold 0…−60 dB; ratio 1:1…10:1; **range = max gain change**
  (≈ ±18–30 dB) and direction; attack/release (often program-dependent/auto);
  optional knee; bell/shelf/notch/HP-LP shapes.

**The "peak-from-bandpass" identity** (cheap dynamic gain — no per-sample biquad
recompute):
```
H_peak(z) = 1 + (G − 1)·H_BP(z),    G = 10^(gain_dB/20)
```
The band-pass `BP(x)` is computed once with fixed coefficients; a changing `G(n)`
is a single multiply-add. This is the analytic basis of the **RBJ cookbook**
peaking filter (constant-skirt BPF numerator `s/(s²+s/Q+1)` added to unity).

> **This is exactly our `DynamicEQ`** (`peak = x + (G−1)·BP`, RBJ constant-0 dB
> bandpass, level-driven `G`). **Two caveats the research surfaced:**
> 1. The bandpass Q must be **corrected** vs the desired peaking Q (≈4× higher, and
>    the correction depends on `G`) — our fixed `Q = 1.4` is an approximation.
> 2. The simple `1+(G−1)·BP` is exact for **boost**; for **cut (G<1)** it does not
>    match a true peaking-cut (which needs `α/A` in the denominator). For accurate
>    cuts we'd switch to a real peaking biquad or, better, a **TPT/SVF**.
>
> **Recommended upgrade:** move filters to a **Cytomic SVF (TPT)** topology —
> better behaved under modulation and with single-precision than direct-form
> biquads (see Appendix).

---

## 6. STFT / overlap-add magnitude processing

**COLA (Constant Overlap-Add):** `Σ_m w[n − mR] = c` (constant). If the shifted
windows don't sum to a constant, unmodified OLA amplitude-modulates at the frame
rate. The constant is `c = (Σ_n w[n]) / R`. **NOLA** (`Σ_m w²[n−mR] > 0 ∀n`) is the
weaker necessary-and-sufficient condition for ISTFT invertibility.

**WOLA (window applied twice — analysis + synthesis, required for effects):**
perfect reconstruction needs the **product** window to be COLA; with
analysis = synthesis = `w`, the **squared** window must be COLA. **Unity-gain scale:**
```
scale = R / Σ_n w[n]²          (R = hop)
```
**Verified.** Robust libraries (librosa) divide the overlap-added output per-sample
by the actual squared-window envelope `Σ_m w²[n−mR]`, which reduces to the scalar
above when that envelope is constant.

**Hann numbers:** periodic Hann at 50% overlap sums to **1.0**; at 75% the single
window sums to **2.0** (OLA) while the **squared** window sums to **1.5** (WOLA).
Use the **periodic/"DFT-even" Hann** (symmetric Hann is *not* COLA).

**Params:** N = 1024–2048; **75% overlap** (R = N/4) for effects; latency = window
size (buffering a full frame). Real FFT → `N/2+1` bins, bin `k` center `= k·fs/N`.

**Magnitude-domain effects:** `Y = g(|X|)·e^{j∠X}` (saturate the magnitude, keep
phase). Pitfalls: circular convolution/time-aliasing (zero-pad), transient
smearing, and **phasiness** (broken vertical phase coherence — fixed with
peak phase-locking in pitch/time effects, not needed for magnitude-only).

> **Our `SpectralSaturator` is validated:** FFT 512, Hann, 75% overlap, double
> Hann (WOLA), `scale = hop / Σw²` — exactly the verified unity-gain formula, and
> it reconstructs the input at amount 0 (test: RMS 0.352 → 0.352). Possible upgrade:
> N = 1024 for finer bins on tonal material (at +latency).

---

## 7. Loudness — LUFS (ITU-R BS.1770) vs RMS

**K-weighting** = two cascaded biquads per channel: (1) a **head high-shelf**
(~+4 dB) and (2) an **RLB high-pass** (~−3 dB at ~38 Hz, Q≈0.5). Canonical 48 kHz
coefficients (confirmed via FFmpeg/libebur128):

```
Stage 1 (shelf):  b = [ 1.53512485958697, -2.69169618940638,  1.19839281085285]
                  a = [ 1.0,              -1.69065929318241,  0.73248077421585]
Stage 2 (HP/RLB): b = [ 1.0, -2.0, 1.0]
                  a = [ 1.0,              -1.99004745483398,  0.99007225036621]
```
**Loudness:** `L_K = −0.691 + 10·log10( Σ_i G_i · z_i )` where `z_i` = mean square
of the K-weighted channel, channel weights `G = 1.0` (L/R/C), `1.41` (surrounds),
**LFE excluded**. The `−0.691` calibrates a 0 dBFS 1 kHz sine to −3.01 LKFS.
**Gating** (integrated): 400 ms blocks @ 75% overlap, absolute gate −70 LKFS, then
relative gate −10 LU below the ungated mean. **Momentary** = 400 ms, **short-term**
= 3 s. **EBU R128:** target −23 LUFS, ceiling −1 dBTP.

**LUFS vs RMS — practical takeaway for us:** flat RMS weights 40 Hz and 2 kHz
equally; LUFS is essentially a *perceptually-weighted, gated RMS*. **But** for an
**internal A/B / auto-gain match** between a dry and processed signal of *similar
spectrum*, K-weighting affects both nearly equally, so **RMS gives almost the same
makeup offset as LUFS** — RMS is "good enough" and far cheaper. Prefer LUFS
(or momentary K-weighted) only when the two signals differ substantially in spectrum
(heavy EQ, de-essing, sub enhancement).

> **Our `LevelMatcher` (and Auto-Gain) use windowed RMS — which the research says
> is the correct, cheap choice for same-spectrum makeup.** A future "K-weighted /
> LUFS auto-gain" mode would help when a stage changes the spectrum a lot.

---

## How this maps to Pickle Power — prioritized next steps

1. **ADAA on the waveshaping stages** (Brine — especially the Spicy hard-clip blend —
   and Fermentation). Hard-clip antiderivatives are trivial, so **1st-order ADAA**
   there is cheap and high-impact; pair `tanh` stages with 1st-order ADAA or a LUT.
   Combining ADAA with our existing 2×–4× oversampling either cuts aliasing further
   **or** lets us drop the default OS factor for equal quality (less CPU/latency).
   `chowdsp_utils` provides drop-in ADAA clippers. Use double precision for the
   divided differences and keep the EPS midpoint fallback.
2. **Cytomic SVF (TPT) filters** for Dynamic EQ (and the Snap crossover) — better
   modulation behavior & single-precision accuracy than RBJ direct-form biquads;
   also fixes the dynamic-EQ **cut** inaccuracy and the Q-correction issue.
3. **Proper ITU-R BS.1770 true-peak path** in the limiter — detect on the
   oversampled signal and smooth the gain (Rudrich fade-in or Signalsmith
   peak-hold + box-stack) instead of our current one-pole + hard clamp; expose a
   `−1 dBTP` ceiling.
4. **Widen the transient Sustain range** toward SPL's ±24 dB; consider a per-band
   (multiband) transient option.
5. **Optional LUFS/K-weighted Auto-Gain mode** for spectrum-changing chains;
   keep RMS as the default cheap match.
6. **Spectral**: optional 1024-pt FFT for finer resolution on tonal sources.
7. **Perf** (Appendix): SVF over direct-form biquad for single precision, keep the
   denormal guard, prefer block ops, reciprocal-multiply over divide.

---

## Appendix A — Curated resources (from *awesome-audio-dsp*) & optimization tips

**Filters / cookbooks**
- **RBJ Audio-EQ-Cookbook** — canonical biquad coefficients. https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html
- **Cytomic technical papers** (SvfLinearTrapOptimised2) — SVF/TPT filters, *better than biquads* for modulation & single precision. https://cytomic.com/technical-papers/
- **EarLevel Engineering** — practical biquad/filter articles. https://www.earlevel.com/main/

**Anti-aliasing / saturation**
- **Jatin Chowdhury** blog + **chowdsp** — ADAA implementations & analog modeling. https://jatinchowdhury18.medium.com/
- **Nick Donaldson — Intro to Oversampling for Alias Reduction.** https://www.nickwritesablog.com/introduction-to-oversampling-for-alias-reduction/

**Limiting / metering / spectral**
- **Signalsmith Audio** — limiter design, box-stack smoothing, peak-hold, reverbs. https://signalsmith-audio.co.uk/writing/
- **Daniel Rudrich — SimpleCompressor** (lookahead limiter doc). https://github.com/DanielRudrich/SimpleCompressor
- **Julius O. Smith — Spectral Audio Signal Processing** (COLA/WOLA/phase vocoder). https://www.dsprelated.com/freebooks/sasp/
- **Bernsee — Pitch Shifting Using the FT** (classic STFT). http://blogs.zynaptiq.com/bernsee/pitch-shifting-using-the-ft/

**General references / community**
- **DAFx** papers https://www.dafx.de/ · **CCRMA** https://ccrma.stanford.edu/papers ·
  **musicdsp.org** https://www.musicdsp.org · **The Audio Programmer / ADC talks** ·
  **deip.pdf** sub-sample interpolators · **dspguru resampling** https://dspguru.com/dsp/faqs/multirate/resampling/

**Textbooks:** Zölzer *DAFX – Digital Audio Effects*; Pirkle *Designing Audio Effect
Plugins in C++*; Reiss & McPherson *Audio Effects: Theory, Implementation and
Application*; Giannoulis, Massberg & Reiss — *Digital Dynamic Range Compressor
Design (A Tutorial and Analysis)*, JAES.

**Audio optimization tips (BillyDM):** profile, don't guess; no locks/malloc/IO on
the audio thread (use atomics / SPSC ring buffers); **guard denormals**; prefer
**reciprocal-multiply over divide**; **block-based** processing; cache-friendly
buffer sizes; lookup tables / SIMD where measured; **single precision + SVF instead
of direct-form biquad**; enable LTO / CPU-feature flags.

Source list: BillyDM, *Awesome Audio DSP* — https://github.com/BillyDM/awesome-audio-dsp

---

## References (consolidated, with credibility)

**Oversampling & true-peak:** JUCE `dsp::Oversampling` docs/source (primary);
Wang & Reiss, AES 132 "Decimation Filters" (peer-reviewed); MathWorks halfband
docs; ITU-R BS.1770-4/-5 (primary standard); AES2 loudness/peak-metering; Thomas
Lund "BS.1770 Revisited" (AES); Essentia TruePeakDetector; EBU R128; AES TD1008;
Daniel Rudrich SimpleCompressor; Signalsmith Audio limiter writings.

**Transient & dynamic EQ:** elysia/SPL Transient Designer (inventor account +
manual); Sound on Sound; M. Brucher Audio Toolkit; musicdsp.org envelope followers;
RBJ Audio-EQ-Cookbook; comp.dsp peak-from-bandpass thread; FabFilter Pro-Q/Pro-MB
help; iZotope; Reiss & McPherson *Audio Effects*; McNally JAES 1984;
Giannoulis/Massberg/Reiss compressor tutorial.

**STFT:** Julius O. Smith *SASP* (COLA/WOLA/Poisson); SciPy `check_COLA`/`check_NOLA`;
MATLAB `iscola`/`dsp.ISTFT`; librosa source (`window_sumsquare`); TensorFlow #16465
& PyTorch-audio #452 (Hann overlap gains); audiodev.blog FFT-in-JUCE; Bernsee;
Laroche & Dolson 1999/1997; Allen & Rabiner 1977.

**Loudness:** ITU-R BS.1770-4/-5 (primary); EBU R128 / Tech 3341 / Tech 3342;
FFmpeg `f_ebur128.c` / libebur128 (coefficients); pyloudnorm (shelf-frequency
dispute); AES loudness basics; Telestream app note.

*Flagged / verify-directly:* exact ITU true-peak FIR coefficients (ITU PDF Annex 2);
the ~4× dynamic-EQ Q-correction factor (approximate, gain-dependent); precise SPL
internal time-constants & gain mapping (manufacturer-internal); K-weighting shelf
nominal frequency 1681.97 Hz vs 1500 Hz (disputed; 48 kHz coefficients are not).
