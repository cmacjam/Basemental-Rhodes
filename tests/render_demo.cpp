// Offline test harness: renders single notes and a demo song to WAV for listening/analysis.
#include "../Source/dsp/RhodesEngine.h"
#include <cstdio>
#include <string>
#include <vector>
#include <functional>

using namespace brh;

static void writeWav (const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    const int n = (int) L.size();
    const int dataBytes = n * 2 * 4;
    auto w32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + dataBytes); std::fwrite ("WAVE", 1, 4, f);
    std::fwrite ("fmt ", 1, 4, f); w32 (16); w16 (3); w16 (2); w32 (sr); w32 (sr * 8); w16 (8); w16 (32);
    std::fwrite ("data", 1, 4, f); w32 (dataBytes);
    for (int i = 0; i < n; ++i) { std::fwrite (&L[i], 4, 1, f); std::fwrite (&R[i], 4, 1, f); }
    std::fclose (f);
}

struct Ev { double t; int type; int note; float vel; }; // type 0=on 1=off 2=sus down 3=sus up

static void render (RhodesEngine& e, std::vector<Ev> evs, double seconds, int sr, std::vector<float>& L, std::vector<float>& R)
{
    std::sort (evs.begin(), evs.end(), [] (auto& a, auto& b) { return a.t < b.t; });
    const int total = (int) (seconds * sr);
    L.assign (total, 0); R.assign (total, 0);
    size_t ei = 0; int pos = 0;
    while (pos < total)
    {
        int next = total;
        if (ei < evs.size()) next = std::min (total, (int) (evs[ei].t * sr));
        if (next > pos) { e.process (L.data() + pos, R.data() + pos, next - pos); pos = next; }
        while (ei < evs.size() && (int) (evs[ei].t * sr) <= pos)
        {
            auto& ev = evs[ei++];
            if (ev.type == 0) e.noteOn (ev.note, ev.vel);
            else if (ev.type == 1) e.noteOff (ev.note);
            else e.setSustain (ev.type == 2);
        }
        if (ei >= evs.size() && pos < total) { e.process (L.data() + pos, R.data() + pos, total - pos); pos = total; }
    }
}

static void chord (std::vector<Ev>& ev, double t, double dur, std::vector<int> notes, float vel, double strum = 0.012)
{
    for (size_t i = 0; i < notes.size(); ++i)
    {
        ev.push_back ({ t + i * strum, 0, notes[i], vel * (0.92f + 0.08f * (float) ((i * 7) % 3) / 2.0f) });
        ev.push_back ({ t + dur, 1, notes[i], 0 });
    }
}

int main (int argc, char** argv)
{
    const int sr = argc > 1 ? std::atoi (argv[1]) : 48000;
    std::vector<float> L, R;

    // --- single notes for analysis: notes x velocities, dry stage, no cab -----------
    {
        RhodesEngine e; EngineParams p; p.cabinet = false; p.drive = 0.0f; e.setParams (p); e.prepare (sr, 512);
        for (int note : { 40, 52, 64, 76, 88 })
            for (float v : { 0.3f, 0.7f, 1.0f })
            {
                e.reset();
                std::vector<Ev> ev { { 0.05, 0, note, v }, { 3.0, 1, note, 0 } };
                render (e, ev, 3.6, sr, L, R);
                char name[128]; std::snprintf (name, sizeof name, "out/note_%d_v%02d.wav", note, (int) (v * 10));
                writeWav (name, L, R, sr);
            }
    }

    // --- demo song --------------------------------------------------------------
    auto song = [] () {
        std::vector<Ev> ev;
        // ii-V-I-vi in F, rootless voicings + bass
        const double bar = 2.4;
        double t = 0.2;
        struct C { int bass; std::vector<int> v; };
        std::vector<C> prog = {
            { 43, { 58, 62, 65, 69 } },   // Gm9
            { 36, { 58, 62, 64, 69 } },   // C13
            { 41, { 57, 60, 64, 67 } },   // Fmaj9
            { 38, { 57, 60, 65, 69 } },   // Dm9
        };
        for (int rep = 0; rep < 2; ++rep)
            for (size_t i = 0; i < prog.size(); ++i)
            {
                const float vel = rep == 0 ? 0.55f : 0.8f;
                ev.push_back ({ t, 0, prog[i].bass, vel + 0.1f }); ev.push_back ({ t + bar * 0.95, 1, prog[i].bass, 0 });
                chord (ev, t + 0.02, bar * 0.45, prog[i].v, vel);
                chord (ev, t + bar * 0.5 + 0.3, bar * 0.4, prog[i].v, vel * 0.85f);
                // little melody on top
                int mel[4] = { 74, 72, 69, 72 };
                ev.push_back ({ t + bar * 0.25, 0, mel[i] + (rep ? 12 : 0), 0.75f });
                ev.push_back ({ t + bar * 0.45, 1, mel[i] + (rep ? 12 : 0), 0 });
                t += bar;
            }
        // final hard "bark" stabs
        for (int k = 0; k < 3; ++k) chord (ev, t + k * 0.6, 0.35, { 41, 53, 57, 60, 64 }, 1.0f, 0.004);
        t += 1.8;
        chord (ev, t, 3.5, { 29, 41, 57, 60, 64, 67, 72 }, 0.65f, 0.03);
        return ev;
    };
    const double songLen = 0.2 + 2.4 * 8 + 1.8 + 5.5;

    struct Variant { const char* name; std::function<void (EngineParams&)> set; };
    std::vector<Variant> vars = {
        { "out/demo_stage_dry.wav",        [] (EngineParams& p) { p.suitcase = false; p.drive = 0.2f; } },
        { "out/demo_suitcase_vibrato.wav", [] (EngineParams& p) { p.suitcase = true; p.tremOn = true; p.tremRate = 4.0f; p.tremDepth = 0.7f; p.bassDb = 3.0f; } },
        { "out/demo_stage_phaser_spring.wav", [] (EngineParams& p) { p.suitcase = false; p.phaserOn = true; p.reverbOn = true; p.reverbMix = 0.3f; p.drive = 0.3f; } },
        { "out/demo_stage_chorus_drive.wav",  [] (EngineParams& p) { p.suitcase = false; p.chorusOn = true; p.drive = 0.6f; p.voice.bark = 0.8f; p.reverbOn = true; p.reverbMix = 0.15f; } },
    };
    for (auto& v : vars)
    {
        RhodesEngine e; EngineParams p; v.set (p); e.setParams (p); e.prepare (sr, 512);
        render (e, song(), songLen, sr, L, R);
        float pk = 0; bool bad = false;
        for (size_t i = 0; i < L.size(); ++i) { pk = std::max (pk, std::max (std::abs (L[i]), std::abs (R[i]))); if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) bad = true; }
        std::printf ("%-40s peak %.3f (%.1f dBFS) %s\n", v.name, pk, 20 * std::log10 (pk + 1e-9), bad ? "NaN/INF!" : "ok");
        writeWav (v.name, L, R, sr);
    }
    return 0;
}
