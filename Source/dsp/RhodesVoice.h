// Basemental Rhodes - one key of the electro-mechanical piano
//
// Model per key:
//   hammer  -> tine (cantilever bending modes) + tonebar (coupled fundamental)
//   tine displacement x(t) -> electromagnetic pickup (nonlinear flux, offset voicing)
//   pickup voltage = d(flux)/dt   -> this is where the bell, growl and "bark" come from
//
// Runs at the engine's oversampled rate (2x host rate).
#pragma once
#include "DSPUtils.h"

namespace brh
{

struct VoiceParams
{
    float bell      = 0.5f;  // hammer hardness / tine bell amount 0..1
    float bark      = 0.5f;  // pickup voicing (offset from tine axis) 0..1
    float decay     = 1.0f;  // sustain length multiplier 0.25..2
    float release   = 0.3f;  // damper softness 0..1
    float dynamics  = 0.7f;  // velocity sensitivity 0..1
    float mechNoise = 0.3f;  // key/damper thump amount 0..1
    bool  suitcase  = false; // model voicing (Suitcase pickups sit a little closer)
};

class RhodesVoice
{
public:
    static constexpr int kModes = 4;

    int   note = -1;
    bool  active = false;
    bool  keyDown = false;
    bool  releasing = false;
    bool  stealing = false;
    uint64_t age = 0;

    void prepare (double osRate) { sr = osRate; }

    void start (int midiNote, float velocity01, float a4Hz, const VoiceParams& p, uint64_t stamp)
    {
        note = midiNote; active = true; keyDown = true; releasing = false; stealing = false; age = stamp;
        vp = p;

        // per-key "manufacturing" variation, fixed per note so the instrument is consistent
        Noise kr; kr.seed (0x9E3779B9u * (uint32_t) (midiNote + 17));
        const float rnd1 = kr.next(), rnd2 = kr.next(), rnd3 = kr.next();

        f0 = a4Hz * std::pow (2.0, (midiNote - 69) / 12.0);

        // ---- velocity --------------------------------------------------------
        const float v  = std::clamp (velocity01, 0.0f, 1.0f);
        const float dyn = 0.35f + 0.65f * vp.dynamics;
        const float vEff = 1.0f - dyn + dyn * v;               // compressed range when dynamics low
        const float vel = std::pow (vEff, 1.35f);

        // tine excursion relative to pickup geometry (drives the nonlinearity)
        const double keyBark = std::clamp (std::pow (261.6 / f0, 0.15), 0.7, 1.35); // bass tines swing further
        amp = (0.04 + 0.48 * std::pow (vel, 2.5f)) * keyBark;

        // pickup geometry
        q = 0.10 + 0.42 * vp.bark + (vp.suitcase ? 0.04 : 0.0);  // offset of tine axis from pickup centre
        h = 0.55;                                                 // pickup "height" (smoothness)
        const double d0 = q / std::pow (q * q + h * h, 1.5);      // small-signal slope dφ/dx at rest
        norm = 1.0 / d0;

        // ---- decay times -----------------------------------------------------
        const double nRel = (midiNote - 28) / 72.0; // 0 at E1, 1 at E7
        double t60Fund = 16.0 * std::pow (2.0, -3.6 * std::clamp (nRel, -0.3, 1.3)) * vp.decay;
        t60Fund = std::max (t60Fund, 0.6);

        // contact time of the neoprene hammer tip, in periods of the fundamental:
        // shorter when hit harder, relatively longer in the treble (purer, less bell up top)
        const double hardness = 0.35 + 0.65 * vp.bell;
        const double cp = (0.30 - 0.17 * vel * hardness) * std::clamp (std::pow (f0 / 261.6, 0.3), 0.5, 2.2);
        const double tc = cp / f0;
        contactSamples = std::max (8, (int) (tc * sr));

        // mode table: {ratio, amplitude, t60}
        const double bellRatio1 = 6.267 * (1.0 + 0.012 * rnd1); // first cantilever overtone
        const double bellRatio2 = 17.55 * (1.0 + 0.018 * rnd2);
        const double tonebarBeat = 1.0 + 0.0011 * (0.6 + 0.4 * rnd3);

        const double bellAmt = (0.12 + 0.55 * vp.bell) * (0.35 + 0.65 * vel);
        const double bellT60 = std::clamp (0.45 * std::pow (261.6 / f0, 0.35), 0.06, 1.1);

        struct M { double ratio, a, t60; };
        const M modes[kModes] = {
            { 1.0,          1.00,                  t60Fund },
            { tonebarBeat,  0.10,                  t60Fund * 0.55 },
            { bellRatio1,   bellAmt,               bellT60 },
            { bellRatio2,   bellAmt * 0.35 * vel,  bellT60 * 0.3 },
        };

        for (int m = 0; m < kModes; ++m)
        {
            const double f = f0 * modes[m].ratio;
            // hammer contact acts as a low-pass on what gets excited
            const double contactLP = (1.0 + cp * cp) / (1.0 + std::pow (modes[m].ratio * cp, 2.0));
            const bool ok = f < sr * 0.45;
            const double w = 2 * kPi * f / sr;
            cosw[m] = std::cos (w); sinw[m] = std::sin (w);
            re[m] = 1.0; im[m] = 0.0;             // phasor, sin component starts at 0 (struck from rest)
            gain[m] = ok ? modes[m].a * contactLP : 0.0;
            env[m] = 1.0;
            sustainCoef[m] = t60ToCoef (modes[m].t60, sr);
            coef[m] = sustainCoef[m];
        }
        // high bell modes inject less displacement (stiffer) - scale by 1/ratio so pickup slope stays sane
        gain[2] /= 2.2; gain[3] /= 5.0;

        sampleIdx = 0;
        prevFlux = flux (0.0);
        keyScale = (float) std::clamp (std::pow (f0 / 261.6, -0.12), 0.7, 1.25);
        outGain = 1.0f;

        // mechanical thump on key-down
        noise.seed (0xA5A5u + (uint32_t) midiNote * 7919u + (uint32_t) (v * 1000));
        thumpEnv = 0.020f * vp.mechNoise * (0.3f + 0.7f * v);
        thumpCoef = (float) t60ToCoef (0.025, sr);
        thumpLP.set (sr, 180.0 + 220.0 * v); thumpLP.reset();
        damperEnv = 0.0f;
        damperCoef = (float) t60ToCoef (0.06, sr);
        damperBP.bandpass (sr, 320.0, 0.9); damperBP.reset();
        velocity = v;
    }

    void stopKey (bool sustainHeld)
    {
        keyDown = false;
        if (! sustainHeld) damp();
    }

    void damp()
    {
        if (releasing) return;
        releasing = true;
        // felt damper: decay time depends on softness param, longer in the bass
        const double base = 0.07 + 1.1 * std::pow ((double) vp.release, 1.6);
        const double t60 = base * std::clamp (std::pow (220.0 / f0, 0.3), 0.6, 1.7);
        for (int m = 0; m < kModes; ++m)
            coef[m] = std::min (sustainCoef[m], t60ToCoef (m < 2 ? t60 : t60 * 0.5, sr));
        damperEnv = 0.010f * vp.mechNoise * (0.4f + 0.6f * velocity);
    }

    // fast fade when a voice is stolen or the key is re-struck
    void steal()
    {
        stealing = true; releasing = true;
        for (int m = 0; m < kModes; ++m) coef[m] = t60ToCoef (0.03, sr);
        thumpEnv = 0.0f; damperEnv = 0.0f;
    }

    // adds into out[0..n)
    void render (float* out, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            double x = 0.0;
            for (int m = 0; m < kModes; ++m)
            {
                const double r = re[m] * cosw[m] - im[m] * sinw[m];
                const double s = re[m] * sinw[m] + im[m] * cosw[m];
                re[m] = r; im[m] = s;
                x += gain[m] * env[m] * s;
                env[m] *= coef[m];
            }

            // hammer contact: displacement builds up over the contact time
            if (sampleIdx < contactSamples)
            {
                const double t = (double) sampleIdx / contactSamples;
                x *= 0.5 - 0.5 * std::cos (kPi * t);
            }
            ++sampleIdx;

            x *= amp;

            // electromagnetic pickup: voltage ~ d(flux)/dt
            const double fl = flux (x);
            double y = (fl - prevFlux) * norm * sr / (2 * kPi * f0);
            prevFlux = fl;

            float o = (float) y * keyScale * 0.3f;

            // mechanical noises (key thump and damper felt)
            if (thumpEnv > 1e-6f)
            {
                o += thumpLP.process (noise.next()) * thumpEnv * 6.0f;
                thumpEnv *= thumpCoef;
            }
            if (damperEnv > 1e-6f)
            {
                o += damperBP.process (noise.next()) * damperEnv;
                damperEnv *= damperCoef;
            }

            out[i] += o;
        }

        // voice finished?
        if (env[0] * amp < 2.0e-5 && env[1] * amp < 2.0e-5 && thumpEnv < 1e-6f && damperEnv < 1e-6f)
        {
            active = false; note = -1;
        }
    }

    double level() const { return env[0] * amp; }

private:
    inline double flux (double x) const
    {
        const double d = x - q;
        return 1.0 / std::sqrt (d * d + h * h);
    }

    double sr = 96000.0;
    VoiceParams vp;
    double f0 = 440.0, amp = 0.3, q = 0.3, h = 0.55, norm = 1.0;
    double cosw[kModes] {}, sinw[kModes] {}, re[kModes] {}, im[kModes] {};
    double gain[kModes] {}, env[kModes] {}, coef[kModes] {}, sustainCoef[kModes] {};
    double prevFlux = 0.0;
    int contactSamples = 100;
    int64_t sampleIdx = 0;
    float keyScale = 1.0f, outGain = 1.0f, velocity = 0.0f;

    Noise noise;
    float thumpEnv = 0, thumpCoef = 0, damperEnv = 0, damperCoef = 0;
    OnePoleLP thumpLP;
    Biquad damperBP;
};

} // namespace brh
