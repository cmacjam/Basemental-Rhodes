// Basemental Rhodes - effects: phaser, chorus, amp (drive + EQ), tremolo/auto-pan,
// spring reverb, speaker cabinet
#pragma once
#include "DSPUtils.h"

namespace brh
{

//------------------------------------------------------------------------------
// 4-stage script-style phaser (mono in/out)
struct Phaser
{
    double sr = 48000, phase = 0;
    float ap[4] {}; // allpass states
    float fb = 0;

    void prepare (double s) { sr = s; reset(); }
    void reset() { for (auto& z : ap) z = 0; fb = 0; phase = 0; }

    // returns wet signal mixed 50/50 (classic notch phaser)
    inline float process (float x, float rateHz, float depth, float feedback)
    {
        phase += rateHz / sr; if (phase >= 1) phase -= 1;
        const float tri = (float) (phase < 0.5 ? phase * 2 : 2 - phase * 2); // triangle 0..1
        const float lfo = tri * tri * (3 - 2 * tri);                        // smoothed
        const double f = 180.0 * std::pow (2.0, (0.4 + 3.6 * depth) * lfo);
        const double t = std::tan (kPi * std::min (f, sr * 0.45) / sr);
        const float a = (float) ((t - 1) / (t + 1));

        float s = x + fb * feedback;
        for (auto& z : ap)
        {
            // first-order allpass: y = a*s + z ; z = s - a*y
            const float y = a * s + z;
            z = s - a * y;
            s = y;
        }
        fb = s;
        return 0.5f * (x + s);
    }
};

//------------------------------------------------------------------------------
// Stereo chorus from a mono-ish input
struct Chorus
{
    double sr = 48000, phase = 0;
    DelayLine dl, dr;

    void prepare (double s)
    {
        sr = s;
        dl.prepare ((int) (0.04 * sr)); dr.prepare ((int) (0.04 * sr));
        phase = 0;
    }
    void reset() { dl.reset(); dr.reset(); phase = 0; }

    inline void process (float inL, float inR, float& outL, float& outR, float rateHz, float depth, float mix)
    {
        phase += rateHz / sr; if (phase >= 1) phase -= 1;
        const float sl = (float) std::sin (2 * kPi * phase);
        const float sr2 = (float) std::sin (2 * kPi * (phase + 0.25));
        const float base = (float) (0.0075 * sr), mod = (float) ((0.0005 + 0.0035 * depth) * sr);
        dl.push (inL); dr.push (inR);
        const float wl = dl.read (base + mod * sl);
        const float wr = dr.read (base + mod * sr2);
        outL = inL * (1 - 0.5f * mix) + wl * mix * 0.7f;
        outR = inR * (1 - 0.5f * mix) + wr * mix * 0.7f;
    }
};

//------------------------------------------------------------------------------
// Antialiased (ADAA-1) asymmetric tube-ish clipper
struct ADAATanh
{
    double x1 = 0, F1 = 0;
    static inline double F (double x) // antiderivative of tanh = log(cosh(x))
    {
        const double ax = std::abs (x);
        return ax + std::log1p (std::exp (-2 * ax)) - 0.6931471805599453;
    }
    void reset() { x1 = 0; F1 = F (0); }
    inline double process (double x)
    {
        const double Fx = F (x);
        const double d = x - x1;
        double y = (std::abs (d) < 1e-5) ? std::tanh (0.5 * (x + x1)) : (Fx - F1) / d;
        x1 = x; F1 = Fx;
        return y;
    }
};

struct AmpSection
{
    double sr = 48000;
    Biquad bassL, bassR, trebL, trebR, preHPL, preHPR;
    ADAATanh clipL, clipR;
    float lastBass = 999, lastTreble = 999;
    OnePoleLP dcL, dcR;

    void prepare (double s)
    {
        sr = s;
        preHPL.highpass (sr, 30, 0.707); preHPR = preHPL;
        dcL.set (sr, 8.0); dcR.set (sr, 8.0);
        reset();
    }
    void reset()
    {
        bassL.reset(); bassR.reset(); trebL.reset(); trebR.reset(); preHPL.reset(); preHPR.reset();
        clipL.reset(); clipR.reset(); dcL.reset(); dcR.reset();
    }
    void setEQ (float bassDb, float trebleDb)
    {
        if (std::abs (bassDb - lastBass) > 1.0e-4f)
        {
            bassL.lowShelf (sr, 160, bassDb, 0.7); bassR.b0 = bassL.b0; bassR.b1 = bassL.b1; bassR.b2 = bassL.b2;
            bassR.a1 = bassL.a1; bassR.a2 = bassL.a2; lastBass = bassDb;
        }
        if (std::abs (trebleDb - lastTreble) > 1.0e-4f)
        {
            trebL.highShelf (sr, 2800, trebleDb, 0.7); trebR.b0 = trebL.b0; trebR.b1 = trebL.b1; trebR.b2 = trebL.b2;
            trebR.a1 = trebL.a1; trebR.a2 = trebL.a2; lastTreble = trebleDb;
        }
    }
    // drive 0..1
    inline void process (float& l, float& r, float drive)
    {
        l = trebL.process (bassL.process (preHPL.process (l)));
        r = trebR.process (bassR.process (preHPR.process (r)));
        if (drive > 0.001f)
        {
            const double g = 0.6 + 14.0 * drive * drive;   // pre-gain
            const double bias = 0.18 * drive;              // asymmetry -> even harmonics
            const double comp = 1.0 / std::tanh (std::max (0.35, g * 0.5)) * 0.5;
            const double wetL = (clipL.process (g * l + bias) - std::tanh (bias)) * comp;
            const double wetR = (clipR.process (g * r + bias) - std::tanh (bias)) * comp;
            const float mix = std::min (1.0f, drive * 4.0f);
            l = (float) (l + mix * (wetL - l));
            r = (float) (r + mix * (wetR - r));
            // remove DC created by the asymmetry
            l -= dcL.process (l); r -= dcR.process (r);
        }
    }
};

//------------------------------------------------------------------------------
// Tremolo: Suitcase = stereo auto-pan (the "vibrato" on a Suitcase), Stage = mono amp tremolo
struct Tremolo
{
    double sr = 48000, phase = 0;
    void prepare (double s) { sr = s; phase = 0; }
    void reset() { phase = 0; }
    inline void process (float& l, float& r, float rateHz, float depth, bool stereoPan)
    {
        phase += rateHz / sr; if (phase >= 1) phase -= 1;
        const float s = (float) std::sin (2 * kPi * phase);
        if (stereoPan)
        {
            // equal-power-ish pan between speakers
            const float p = 0.5f + 0.5f * s * depth;   // 0..1
            const float gl = std::sqrt (1.0f - p) * 1.41421356f;
            const float gr = std::sqrt (p) * 1.41421356f;
            const float m = 0.5f * (l + r), sd = 0.5f * (l - r);
            l = m * gl + sd; r = m * gr - sd;
        }
        else
        {
            const float g = 1.0f - depth * (0.5f + 0.5f * s) * 0.9f;
            l *= g; r *= g;
        }
    }
};

//------------------------------------------------------------------------------
// Spring reverb: delay loops with stretched allpass chains for the dispersive "drip"
struct Spring
{
    static constexpr int kStages = 24;
    DelayLine loop;
    DelayLine apDelay[kStages];
    float apA = 0.6f;
    int stretch = 3;
    int loopLen = 1000;
    Biquad lp;
    float fbState = 0;

    void prepare (double sr, double loopSec, int stretchK, float a)
    {
        loopLen = (int) (loopSec * sr);
        loop.prepare (loopLen + 4);
        stretch = stretchK; apA = a;
        for (auto& d : apDelay) d.prepare (stretch + 2);
        lp.lowpass (sr, 4200, 0.6);
        reset();
    }
    void reset()
    {
        loop.reset(); for (auto& d : apDelay) d.reset(); lp.reset(); fbState = 0;
    }
    inline float process (float x, float feedback)
    {
        float s = x + feedback * fbState;
        // stretched allpass: H(z) = (a + z^-K) / (1 + a z^-K)
        for (auto& d : apDelay)
        {
            const float zK = d.readInt (stretch);   // v[n-K]
            const float v = s - apA * zK;
            const float y = apA * v + zK;
            d.push (v);
            s = y;
        }
        loop.push (s);
        const float out = loop.readInt (loopLen);
        fbState = lp.process (out);
        return out;
    }
};

struct SpringReverb
{
    double sr = 48000;
    Spring s1, s2, s3, s4;
    Biquad inHP, inLP, outHP_L, outHP_R;
    void prepare (double s)
    {
        sr = s;
        const int K = std::max (1, (int) std::round (3.0 * sr / 48000.0));
        s1.prepare (sr, 0.0331, K, 0.62f);
        s2.prepare (sr, 0.0397, K, 0.58f);
        s3.prepare (sr, 0.0359, K, 0.64f);
        s4.prepare (sr, 0.0427, K, 0.60f);
        inHP.highpass (sr, 180, 0.7); inLP.lowpass (sr, 5000, 0.7);
        outHP_L.highpass (sr, 120, 0.7); outHP_R = outHP_L;
    }
    void reset() { s1.reset(); s2.reset(); s3.reset(); s4.reset(); inHP.reset(); inLP.reset(); outHP_L.reset(); outHP_R.reset(); }

    // adds reverb into l/r
    inline void process (float& l, float& r, float mix, float decay)
    {
        const float in = inLP.process (inHP.process (0.5f * (l + r))) * 0.5f;
        const float fb = 0.62f + 0.31f * decay;
        const float a = s1.process (in, fb), b = s2.process (in, fb);
        const float c = s3.process (in, fb), d = s4.process (in, fb);
        const float wl = outHP_L.process (a - 0.7f * c + 0.3f * b);
        const float wr = outHP_R.process (b - 0.7f * d + 0.3f * a);
        l += wl * mix * 2.2f;
        r += wr * mix * 2.2f;
    }
};

//------------------------------------------------------------------------------
// Speaker cabinet voicing. Suitcase: 4x12 in a closed suitcase cab; Stage: 2x12 open-back combo
struct Cabinet
{
    double sr = 48000;
    Biquad hp[2], p1[2], p2[2], lp[2];
    OnePoleLP lp1[2];
    bool suitcase = false; bool first = true;

    void prepare (double s, bool isSuitcase) { sr = s; first = true; configure (isSuitcase); }
    void configure (bool isSuitcase)
    {
        if (! first && isSuitcase == suitcase) return;
        first = false; suitcase = isSuitcase;
        for (int c = 0; c < 2; ++c)
        {
            if (suitcase)
            {
                hp[c].highpass (sr, 70, 0.8);
                p1[c].peak (sr, 125, 1.1, 3.0);
                p2[c].peak (sr, 1800, 0.9, 1.5);
                lp[c].lowpass (sr, 4600, 0.75);
                lp1[c].set (sr, 9000);
            }
            else
            {
                hp[c].highpass (sr, 55, 0.7);
                p1[c].peak (sr, 100, 0.9, 1.5);
                p2[c].peak (sr, 2600, 1.0, 2.0);
                lp[c].lowpass (sr, 6200, 0.7);
                lp1[c].set (sr, 12000);
            }
        }
    }
    void reset() { for (int c = 0; c < 2; ++c) { hp[c].reset(); p1[c].reset(); p2[c].reset(); lp[c].reset(); lp1[c].reset(); } }
    inline float process (int c, float x) { return lp1[c].process (lp[c].process (p2[c].process (p1[c].process (hp[c].process (x))))); }
};

} // namespace brh
