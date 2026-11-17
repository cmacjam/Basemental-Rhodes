// Basemental Rhodes - complete instrument: voices (2x oversampled) + effects chain
//
// Signal flow (like a real rig):
//   keys -> tines -> pickups (2x OS, decimated) -> Phaser -> Chorus -> Amp (EQ + drive)
//        -> Tremolo (Suitcase: stereo auto-pan / Stage: mono) -> Spring reverb -> Cabinet -> Volume
#pragma once
#include "RhodesVoice.h"
#include "RhodesFX.h"
#include <array>

namespace brh
{

struct EngineParams
{
    // instrument
    bool  suitcase   = false;
    float volumeDb   = 0.0f;
    float tuneA4     = 440.0f;
    VoiceParams voice;

    // amp
    float drive      = 0.15f;
    float bassDb     = 0.0f;
    float trebleDb   = 0.0f;
    bool  cabinet    = true;

    // tremolo / vibrato
    bool  tremOn     = false;
    float tremRate   = 4.5f;
    float tremDepth  = 0.6f;

    // phaser
    bool  phaserOn   = false;
    float phaserRate = 0.4f;
    float phaserDepth= 0.7f;
    float phaserFb   = 0.3f;

    // chorus
    bool  chorusOn   = false;
    float chorusRate = 0.6f;
    float chorusDepth= 0.5f;
    float chorusMix  = 0.5f;

    // spring reverb
    bool  reverbOn   = false;
    float reverbMix  = 0.25f;
    float reverbDecay= 0.5f;
};

class RhodesEngine
{
public:
    static constexpr int kMaxVoices = 48;
    static constexpr int kMaxBlock = 2048;

    void prepare (double hostRate, int /*maxBlock*/)
    {
        sr = hostRate; osr = hostRate * 2.0;
        for (auto& v : voices) { v.prepare (osr); v.active = false; v.note = -1; }
        osBuf.assign (kMaxBlock * 2, 0.0f);
        dec.reset();
        phaser.prepare (sr); chorus.prepare (sr); amp.prepare (sr); trem.prepare (sr);
        reverb.prepare (sr); cab.prepare (sr, params.suitcase);
        outHP.highpass (osr, 20.0, 0.707); outHP.reset();

        sVol.prepare (sr, 30); sDrive.prepare (sr, 30); sTremDepth.prepare (sr, 30);
        sPhDepth.prepare (sr, 50); sChMix.prepare (sr, 30); sRevMix.prepare (sr, 30);
        sTremOn.prepare (sr, 15); sPhOn.prepare (sr, 15); sChOn.prepare (sr, 15); sRevOn.prepare (sr, 15); sCabOn.prepare (sr, 15);
        snapSmoothers();
        sustain = false;
    }

    void setParams (const EngineParams& p) { params = p; }
    const EngineParams& getParams() const { return params; }

    void noteOn (int note, float velocity01)
    {
        if (velocity01 <= 0.0f) { noteOff (note); return; }
        // re-striking a ringing key: fade the old strike quickly, start a new one
        for (auto& v : voices)
            if (v.active && v.note == note && ! v.stealing) v.steal();

        RhodesVoice* target = nullptr;
        for (auto& v : voices) if (! v.active) { target = &v; break; }
        if (target == nullptr)
        {
            // steal: prefer released voices, then the quietest/oldest
            double best = 1e30;
            for (auto& v : voices)
            {
                double score = v.level() * (v.keyDown ? 4.0 : 1.0) + (double) v.age * 1e-12;
                if (score < best) { best = score; target = &v; }
            }
        }
        VoiceParams vp = params.voice; vp.suitcase = params.suitcase;
        target->start (note, velocity01, params.tuneA4, vp, ++stamp);
    }

    void noteOff (int note)
    {
        for (auto& v : voices)
            if (v.active && v.note == note && v.keyDown && ! v.stealing)
                v.stopKey (sustain);
    }

    void setSustain (bool down)
    {
        sustain = down;
        if (! down)
            for (auto& v : voices)
                if (v.active && ! v.keyDown) v.damp();
    }

    void allNotesOff()
    {
        sustain = false;
        for (auto& v : voices) if (v.active) v.damp();
    }

    void reset()
    {
        for (auto& v : voices) { v.active = false; v.note = -1; }
        dec.reset(); phaser.reset(); chorus.reset(); amp.reset(); trem.reset(); reverb.reset(); cab.reset(); outHP.reset();
    }

    int activeVoices() const { int n = 0; for (auto& v : voices) n += v.active ? 1 : 0; return n; }

    // render n host-rate samples into L/R (overwrites)
    void process (float* L, float* R, int n)
    {
        int done = 0;
        while (done < n)
        {
            const int chunk = std::min (kMaxBlock, n - done);
            processChunk (L + done, R + done, chunk);
            done += chunk;
        }
    }

private:
    void snapSmoothers()
    {
        sVol.reset (dbToGain (params.volumeDb)); sDrive.reset (params.drive); sTremDepth.reset (params.tremDepth);
        sPhDepth.reset (params.phaserDepth); sChMix.reset (params.chorusMix); sRevMix.reset (params.reverbMix);
        sTremOn.reset (params.tremOn ? 1.f : 0.f); sPhOn.reset (params.phaserOn ? 1.f : 0.f);
        sChOn.reset (params.chorusOn ? 1.f : 0.f); sRevOn.reset (params.reverbOn ? 1.f : 0.f);
        sCabOn.reset (params.cabinet ? 1.f : 0.f);
    }

    void processChunk (float* L, float* R, int n)
    {
        const int on = n * 2;
        std::fill (osBuf.begin(), osBuf.begin() + on, 0.0f);
        for (auto& v : voices)
            if (v.active) v.render (osBuf.data(), on);

        // decimate the pickup bus back to host rate
        for (int i = 0; i < n; ++i)
        {
            dec.push (outHP.process (osBuf[(size_t) (2 * i)]));
            dec.push (outHP.process (osBuf[(size_t) (2 * i + 1)]));
            L[i] = dec.output();
        }

        const auto& p = params;
        sVol.set (dbToGain (p.volumeDb)); sDrive.set (p.drive); sTremDepth.set (p.tremDepth);
        sPhDepth.set (p.phaserDepth); sChMix.set (p.chorusMix); sRevMix.set (p.reverbMix);
        sTremOn.set (p.tremOn ? 1.f : 0.f); sPhOn.set (p.phaserOn ? 1.f : 0.f); sChOn.set (p.chorusOn ? 1.f : 0.f);
        sRevOn.set (p.reverbOn ? 1.f : 0.f); sCabOn.set (p.cabinet ? 1.f : 0.f);
        amp.setEQ (p.bassDb, p.trebleDb);
        cab.configure (p.suitcase);

        // Suitcase preamp has a touch more level and a little built-in warmth
        const float modelGain = p.suitcase ? 0.9f : 1.0f;

        for (int i = 0; i < n; ++i)
        {
            float x = L[i] * modelGain;

            // phaser (mono pedal)
            const float phOn = sPhOn.next();
            const float phDepth = sPhDepth.next();
            if (phOn > 1e-4f) { const float w = phaser.process (x, p.phaserRate, phDepth, p.phaserFb); x += phOn * (w - x); }

            // chorus (mono -> stereo)
            float l = x, r = x;
            const float chOn = sChOn.next();
            const float chMix = sChMix.next();
            if (chOn > 1e-4f)
            {
                float cl, cr; chorus.process (x, x, cl, cr, p.chorusRate, p.chorusDepth, chMix);
                l += chOn * (cl - l); r += chOn * (cr - r);
            }

            // amp: EQ + drive
            amp.process (l, r, sDrive.next());

            // tremolo / suitcase vibrato
            const float trOn = sTremOn.next();
            const float trDepth = sTremDepth.next() * trOn;
            if (trDepth > 1e-4f) trem.process (l, r, p.tremRate, trDepth, p.suitcase);

            // spring reverb (inside the amp, before the speaker)
            const float rvOn = sRevOn.next();
            const float rvMix = sRevMix.next() * rvOn;
            if (rvOn > 1e-4f) reverb.process (l, r, rvMix, p.reverbDecay);

            // speaker
            const float cbOn = sCabOn.next();
            if (cbOn > 1e-4f)
            {
                const float cl = cab.process (0, l), cr = cab.process (1, r);
                l += cbOn * (cl - l); r += cbOn * (cr - r);
            }

            const float g = sVol.next();
            L[i] = l * g; R[i] = r * g;
        }
    }

    double sr = 48000.0, osr = 96000.0;
    EngineParams params;
    std::array<RhodesVoice, kMaxVoices> voices;
    std::vector<float> osBuf;
    HalfbandDecimator dec;
    Biquad outHP;
    bool sustain = false;
    uint64_t stamp = 0;

    Phaser phaser; Chorus chorus; AmpSection amp; Tremolo trem; SpringReverb reverb; Cabinet cab;
    Smoothed sVol, sDrive, sTremDepth, sPhDepth, sChMix, sRevMix, sTremOn, sPhOn, sChOn, sRevOn, sCabOn;
};

} // namespace brh
