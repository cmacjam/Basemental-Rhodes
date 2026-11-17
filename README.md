# Basemental Rhodes

A modeled electro-mechanical tine piano (Stage 73 and Suitcase 73) for Logic Pro, any AU/VST3 host and as a standalone app. It uses no samples: every note is synthesized from a physical model of the instrument.

## How the sound is made

| Real instrument | In the plug-in |
|---|---|
| Neoprene-tipped hammer hits the tine | Velocity sets how far the tine swings and how short the hammer contact is. A harder, shorter hit brings out more bell. |
| Tine + tonebar (a tuning fork) | Modal synthesis: the fundamental, a tonebar partial that beats slowly against it, and the tine's inharmonic bending modes (×6.27 and ×17.5) that make the bell attack. |
| Electromagnetic pickup | The tine displacement passes through a nonlinear magnetic-flux curve, and the output is the voltage it induces. The **Bark** knob moves the tine off the pickup centre. This is what makes the growl and the hard "bark" when you dig in. Runs 2x oversampled. |
| Felt dampers, key thump | Release sets how soft the dampers are. **Mech** controls the key and damper noise. The sustain pedal (CC64) holds the dampers off. |
| Stage vs Suitcase | Stage is dry, voiced like a 2x12 combo. Suitcase is the Peterson preamp with a closed 4x12 cabinet, and its vibrato becomes the stereo auto-pan between speakers. |

Effects, in the order of a real rig: **Phaser → Chorus → Amp (Bass/Treble/Drive) → Vibrato/Tremolo → Spring reverb → Cabinet → Volume**.

Factory presets: Stage 73 Classic, Suitcase Vibrato, Headhunter Phase, Bark & Drive, Ballad Chorus, Twin Spring Room, Soft Neo-Soul, Bright Bell Tines.

## Building

### GitHub Actions (recommended)
Push this folder to a GitHub repo. The workflow in `.github/workflows/build.yml` builds:
- **macOS**: a universal (Apple Silicon + Intel) AU, VST3 and Standalone app. It checks the AU with `auval` and packs all three into one `.pkg` installer.
- **Windows**: VST3 and Standalone, as a `.zip`.

Download the builds from the run's **Artifacts**. If you push a tag like `v1.0.0`, the workflow also attaches them to a GitHub Release.

### Locally (macOS)
```
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release -j
```

### Pro Tools (AAX)
AAX needs Avid's AAX SDK, which you download from your Avid developer account. Configure with `-DAAX_SDK_PATH=/path/to/aax-sdk` and AAX is added to the build and the installer. Pro Tools only loads AAX plug-ins that are signed through Avid's PACE/Eden program.

### Signing for sale
CI signs ad hoc. Before selling, sign with your Apple Developer ID and notarize, otherwise Gatekeeper will warn your customers.

## Offline sound test (no JUCE needed)
```
cd tests && g++ -O2 -std=c++17 render_demo.cpp -o render_demo && mkdir -p out && ./render_demo
python3 analyze.py   # harmonic levels, decay and release per note/velocity
```

## Code map
- `Source/dsp/RhodesVoice.h`: one key (hammer, tine modes, pickup, dampers)
- `Source/dsp/RhodesFX.h`: phaser, chorus, amp, tremolo, spring reverb, cabinet
- `Source/dsp/RhodesEngine.h`: 48-voice engine, 2x oversampling, signal chain
- `Source/PluginProcessor.*`, `Source/PluginEditor.*`: the JUCE plug-in and UI
- `Source/Presets.h`: factory presets
