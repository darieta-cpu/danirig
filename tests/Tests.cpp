// Test DSP di DaniRig (nessuna scheda audio necessaria). Esegui: ./DaniRigTests [percorso/modello.nam]
#include "dsp/NoiseGate.h"
#include "dsp/Pedals.h"
#include "dsp/NamAmp.h"
#include "dsp/CabIR.h"
#include "dsp/Delay.h"
#include "dsp/Reverb.h"
#include "dsp/Resampler.h"
#include "dsp/MultiMod.h"
#include "dsp/DynaComp.h"
#include "dsp/Taper.h"
#include "dsp/IrAnalysis.h"
#include "FileLibrary.h"
#include "RigEngine.h"
#include <juce_events/juce_events.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <cstdio>
#include <random>

using namespace dr;

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { std::printf ("  FAIL: %s\n", msg); ++failures; } else std::printf ("  ok:   %s\n", msg); } while (0)

static float rms (const std::vector<float>& v, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b; ++i) s += (double) v[i] * v[i]; return (float) std::sqrt (s / (double) (b - a)); }
static bool finite (const std::vector<float>& v) { for (float x : v) if (! std::isfinite (x)) return false; return true; }
static float peakOf (const std::vector<float>& v) { float p = 0; for (float x : v) p = std::max (p, std::abs (x)); return p; }

template <class M> static void runMono (M& m, std::vector<float>& x, int block)
{
    for (size_t i = 0; i < x.size(); i += (size_t) block)
        m.run (x.data() + i, (int) std::min<size_t> ((size_t) block, x.size() - i));
}
static std::vector<float> sine (double sr, double f, double amp, size_t n)
{ std::vector<float> v (n); for (size_t i = 0; i < n; ++i) v[i] = (float) (amp * std::sin (2 * kPi * f * (double) i / sr)); return v; }

int main (int argc, char** argv)
{
    juce::MessageManager::getInstance();
    const double sr = 48000; const int block = 128;

    std::printf ("[Noise gate]\n");
    {
        NoiseGate g; g.prepare (sr, block);
        std::vector<float> x = sine (sr, 220, 0.0005, 48000);       // rumore molto basso (-66 dB)
        auto loud = sine (sr, 220, 0.3, 24000);
        x.insert (x.end(), loud.begin(), loud.end());
        runMono (g, x, block);
        CHECK (rms (x, 20000, 40000) < 0.0003f, "chiude sotto la soglia");
        CHECK (rms (x, 60000, 70000) > 0.15f, "apre con il segnale forte");
        CHECK (finite (x), "nessun NaN");
    }

    std::printf ("[Tube Screamer / DS-1]\n");
    for (int which = 0; which < 2; ++which)
    {
        TubeScreamer ts; Ds1 ds;
        MonoModule& m = which == 0 ? (MonoModule&) ts : (MonoModule&) ds;
        m.prepare (sr, block);
        bool ok = true; float lastPk = 0; bool mono = true;
        for (double amp : { 0.001, 0.01, 0.05, 0.2, 0.5, 1.0 })
        {
            m.reset();
            auto x = sine (sr, 220, amp, 24000);
            runMono (m, x, block);
            ok &= finite (x);
            const float pk = peakOf (x);
            if (amp > 0.01 && pk + 1e-4f < lastPk * 0.5f) mono = false;
            lastPk = pk;
            std::printf ("       %s amp %.3f -> picco %.3f\n", which == 0 ? "TS " : "DS1", amp, pk);
        }
        CHECK (ok, which == 0 ? "TS: stabile da -60 a 0 dBFS" : "DS-1: stabile da -60 a 0 dBFS");
        CHECK (peakOf (std::vector<float> { lastPk }) < 8.0f, "uscita limitata (clipping)");
        CHECK (mono, "livello coerente al crescere dell'ingresso");
        // aliasing: una sinusoide a 5 kHz distorta non deve produrre componenti "sporche" forti sotto la fondamentale
        m.reset();
        auto x = sine (sr, 5000, 0.3, 48000);
        runMono (m, x, block);
        CHECK (finite (x), "5 kHz distorto senza NaN");
        // cambio on/off senza click: differenza massima tra campioni consecutivi limitata
        m.enabled->set (0.0f);
        auto y = sine (sr, 220, 0.2, 9600);
        runMono (m, y, block);
        m.enabled->set (1.0f);
        auto z = sine (sr, 220, 0.2, 9600);
        runMono (m, z, block);
        float maxStep = 0; for (size_t i = 1; i < z.size(); ++i) maxStep = std::max (maxStep, std::abs (z[i] - z[i - 1]));
        CHECK (maxStep < 1.0f, "bypass on/off con crossfade (niente salti)");
    }

    std::printf ("[Resampler]\n");
    {
        for (double devRate : { 44100.0, 96000.0, 88200.0 })
        {
            RateBridge br; br.prepare (devRate, 48000.0, 128);
            std::vector<float> x = sine (devRate, 1000, 0.5, (size_t) devRate);
            for (size_t i = 0; i < x.size(); i += 128)
                br.process (x.data() + i, (int) std::min<size_t> (128, x.size() - i), [] (float*, int) {});
            { char m2[64]; std::snprintf (m2, sizeof m2, "ponte %.0f: nessun underrun", devRate); CHECK (br.underruns == 0, m2); }
            const float r = rms (x, x.size() / 2, x.size() - 1000);
            char msg[96]; std::snprintf (msg, sizeof msg, "ponte %.0f<->48000: sinusoide 1 kHz con guadagno ~1 (rms %.3f)", devRate, r);
            CHECK (std::abs (r - 0.3536f) < 0.01f, msg);
            std::snprintf (msg, sizeof msg, "ponte %.0f: latenza stimata %d campioni", devRate, br.latencyDeviceSamples (devRate, 48000.0));
            std::printf ("       %s\n", msg);
        }
    }

    std::printf ("[Delay]\n");
    {
        DelayPedal d; d.prepare (sr, block);
        d.enabled->set (1.0f); d.find ("mix")->set (100); d.find ("repeats")->set (0); d.find ("time")->set (100); d.find ("mod")->set (0);
        d.reset();
        std::vector<float> l (24064, 0.0f), r (24064, 0.0f); l[0] = r[0] = 1.0f;
        for (size_t i = 0; i + block <= l.size(); i += block) d.run (l.data() + i, r.data() + i, block);
        size_t peakIdx = 1; for (size_t i = 100; i < l.size(); ++i) if (std::abs (l[i]) > std::abs (l[peakIdx])) peakIdx = i;
        std::printf ("       eco a %zu campioni (atteso 4800)\n", peakIdx);
        CHECK (std::abs ((int) peakIdx - 4800) <= 4, "tempo del delay corretto (100 ms)");
        CHECK (std::abs (l[peakIdx]) > 0.5f, "ampiezza dell'eco");
        // tap tempo
        DelayPedal t; t.prepare (sr, block);
        t.tap (); juce::Thread::sleep (500); t.tap (); juce::Thread::sleep (500); t.tap ();
        std::printf ("       tap ~500 ms -> time = %.0f ms\n", t.find ("time")->get());
        CHECK (std::abs (t.find ("time")->get() - 500.0f) < 40.0f, "tap tempo ~500 ms");
    }

    std::printf ("[Reverb]\n");
    {
        ReverbModule rv; rv.prepare (sr, block);
        rv.enabled->set (1.0f); rv.find ("mix")->set (100); rv.find ("decay")->set (2.0f); rv.reset();
        std::vector<float> l (48000 * 6, 0.0f), r (48000 * 6, 0.0f); l[0] = r[0] = 1.0f;
        for (size_t i = 0; i < l.size(); i += block) rv.run (l.data() + i, r.data() + i, block);
        const float early = rms (l, 2000, 14000), mid = rms (l, 48000, 60000), late = rms (l, 48000 * 5, 48000 * 6);
        std::printf ("       rms: 0.04-0.3 s %.5f   1-1.25 s %.5f   5-6 s %.6f\n", early, mid, late);
        CHECK (early > 0.0f && finite (l) && finite (r), "coda presente, nessun NaN");
        CHECK (mid < early && late < mid * 0.2f, "la coda decade nel tempo");
        // rumore: guadagno del wet
        ReverbModule rn; rn.prepare (sr, block); rn.enabled->set (1.0f); rn.find ("mix")->set (100); rn.reset();
        std::mt19937 g (1); std::normal_distribution<float> nd (0.0f, 0.1f);
        std::vector<float> a (96000), b (96000), dry (96000); for (auto& v : a) v = nd (g); dry = a; b = a;
        for (size_t i = 0; i < a.size(); i += block) rn.run (a.data() + i, b.data() + i, block);
        std::vector<float> wet (a.size()); for (size_t i = 0; i < a.size(); ++i) wet[i] = a[i] - dry[i];
        std::printf ("       rumore: rms dry %.4f  wet %.4f\n", rms (dry, 48000, 96000), rms (wet, 48000, 96000));
    }

    std::printf ("[Catena completa + NAM]\n");
    {
        NamAmp amp;
        const juce::File model = argc > 1 ? juce::File (argv[1]) : juce::File();
        if (model.existsAsFile())
        {
            for (double devRate : { 48000.0, 44100.0 })
            {
                NamAmp a; a.loadModelAsync (model); a.prepare (devRate, block);
                for (int i = 0; i < 200 && ! a.getInfo().loaded && a.getInfo().error.isEmpty(); ++i) juce::Thread::sleep (50);
                auto info = a.getInfo();
                std::printf ("       modello: %s  loaded=%d slimmable=%d rate=%.0f err='%s'\n", info.name.toRawUTF8(), info.loaded, info.slimmable, info.modelRate, info.error.toRawUTF8());
                CHECK (info.loaded, "modello caricato in background");
                auto x = sine (devRate, 220, 0.1, (size_t) devRate);
                for (size_t i = 0; i < x.size(); i += block) a.run (x.data() + i, block <= (int) (x.size() - i) ? block : (int) (x.size() - i));
                std::printf ("       %.0f Hz: rms out %.4f  picco %.3f  latenza ponte %d campioni\n", devRate, rms (x, x.size() / 2, x.size() - 500), peakOf (x), a.getLatencySamples());
                CHECK (finite (x) && rms (x, x.size() / 2, x.size() - 500) > 1e-4f, "uscita non nulla e finita");
                a.collectGarbage();
            }
            // cambio di modello a caldo (crossfade) + Lite
            NamAmp a; a.prepare (48000.0, block); a.loadModelAsync (model);
            for (int i = 0; i < 200 && ! a.getInfo().loaded; ++i) juce::Thread::sleep (50);
            auto x = sine (48000, 220, 0.1, 48000);
            for (size_t i = 0; i < x.size(); i += block)
            {
                if (i == 12800) { a.quality->set (1.0f); a.reloadCurrent(); }
                a.run (x.data() + i, block);
                if (i > 12800 && i < 38400) juce::Thread::sleep (1);
            }
            CHECK (finite (x), "swap Full->Lite a caldo senza NaN");
            a.collectGarbage();
        }
        else std::printf ("  (nessun modello passato: salto)\n");
    }

    std::printf ("[Cabinet IR]\n");
    {
        // IR passa-basso (media mobile su 8 campioni): le alte frequenze devono scendere di molto DOPO il caricamento
        const juce::File ir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("danirig_test_ir.wav");
        ir.deleteFile();
        {
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::FileOutputStream> os (ir.createOutputStream());
            std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), 48000.0, 1, 24, {}, 0));
            if (w) { os.release(); juce::AudioBuffer<float> b (1, 64); b.clear(); for (int i = 0; i < 8; ++i) b.setSample (0, i, 0.125f); w->writeFromAudioSampleBuffer (b, 0, 64); }
        }
        CHECK (ir.existsAsFile() && ir.getSize() > 0, "file IR di prova creato");
        auto gainAt = [&] (CabIR& c, double f)
        {
            auto x = sine (sr, f, 0.3, 19200);
            for (size_t i = 0; i + block <= x.size(); i += block) c.run (x.data() + i, block);
            return rms (x, 9600, 19000) / 0.2121f;
        };
        for (int variant = 0; variant < 2; ++variant)      // 0: IR caricato dopo prepare, 1: prima di prepare (come in un preset)
        {
            CabIR c;
            c.find ("lowcut")->set (20.0f); c.find ("highcut")->set (20000.0f);
            if (variant == 1) c.loadIR (ir);
            c.prepare (sr, block);
            if (variant == 0) c.loadIR (ir);
            float hf = 1.0f;
            for (int attempt = 0; attempt < 600; ++attempt)  // il caricamento è asincrono: si processa finché lo scambio avviene
            {
                hf = gainAt (c, 10000.0);
                if (hf < 0.6f) break;
                juce::Thread::sleep (20);
            }
            const float lf = gainAt (c, 200.0);
            char msg[128]; std::snprintf (msg, sizeof msg, "IR %s prepare: 10 kHz %.2f, 200 Hz %.2f", variant ? "prima di" : "dopo", hf, lf);
            std::printf ("       %s\n", msg);
            CHECK (hf < 0.4f && lf > 0.05f && hf / lf < 0.35f, "l'IR viene caricato e filtra il segnale");
        }
        CabIR off; off.prepare (sr, block);
        CHECK (std::abs (gainAt (off, 10000.0) - 1.0f) < 0.15f, "senza IR il cab lascia passare il segnale");
        ir.deleteFile();
    }

    std::printf ("[Multi Mod: Phase / Chorus / Tremolo]\n");
    {
        MultiMod m; m.prepare (sr, block); m.enabled->set (1.0f);
        auto run = [&] (std::vector<float>& x) { for (size_t i = 0; i + block <= x.size(); i += block) m.run (x.data() + i, block); };
        // Tremolo 4 Hz, depth 10: l'inviluppo deve oscillare tra ~1 e ~0
        m.find ("mode")->set (2.0f); m.find ("rate")->set (4.0f); m.find ("depth")->set (10.0f); m.reset();
        auto x = sine (sr, 1000, 0.5, 48128); run (x);
        float mn = 1e9f, mx = 0; for (size_t i = 12000; i + 240 <= 48000; i += 240) { const float r = rms (x, i, i + 240); mn = std::min (mn, r); mx = std::max (mx, r); }
        std::printf ("       tremolo: inviluppo min %.3f max %.3f (ingresso rms 0.354)\n", mn, mx);
        CHECK (mx > 0.30f && mn < 0.06f, "tremolo: modulazione profonda");
        // Tremolo depth 0 = trasparente
        m.find ("depth")->set (0.0f); m.reset(); auto t0 = sine (sr, 1000, 0.5, 24064); run (t0);
        CHECK (std::abs (rms (t0, 12000, 24000) - 0.3536f) < 0.02f, "tremolo a depth 0: segnale invariato");
        // Phase e Chorus: stabili e di livello sensato su rumore
        for (int mode = 0; mode < 2; ++mode)
        {
            m.find ("mode")->set ((float) mode); m.find ("rate")->set (1.0f); m.find ("depth")->set (6.0f); m.reset();
            std::mt19937 g (3); std::normal_distribution<float> nd (0.0f, 0.1f);
            std::vector<float> n (96128); for (auto& v : n) v = nd (g);
            auto dry = n; run (n);
            const float r = rms (n, 48000, 96000) / rms (dry, 48000, 96000);
            char msg[96]; std::snprintf (msg, sizeof msg, "%s: finito, guadagno su rumore %.2f", mode ? "chorus" : "phase", r);
            CHECK (finite (n) && r > 0.4f && r < 1.3f, msg);
            float diff = 0; for (size_t i = 48000; i < 96000; ++i) diff += std::abs (n[i] - dry[i]); 
            CHECK (diff / 48000.0f > 0.005f, mode ? "chorus: modifica il segnale" : "phase: modifica il segnale");
        }
        // Tap tempo: 500 ms a 1/4 -> 2 Hz ; a 1/8 -> 4 Hz
        MultiMod t; t.prepare (sr, block);
        t.find ("div")->set (3.0f);
        t.tap(); juce::Thread::sleep (500); t.tap(); juce::Thread::sleep (500); t.tap();
        std::printf ("       tap 500 ms con 1/8 -> rate = %.2f Hz\n", t.find ("rate")->get());
        CHECK (std::abs (t.find ("rate")->get() - 4.0f) < 0.4f, "tap tempo con suddivisione");
    }

    std::printf ("[Motore: process() condiviso da standalone e plugin]\n");
    {
        RigEngine e;
        e.prepare (sr, block);
        e.master->set (0.0f);
        e.mod.enabled->set (1.0f); e.mod.find ("mode")->set (2.0f);
        std::vector<float> in (block * 40), l (in.size()), r (in.size());
        for (size_t i = 0; i < in.size(); ++i) in[i] = (float) (0.2 * std::sin (2 * kPi * 440.0 * (double) i / sr));
        for (size_t i = 0; i + block <= in.size(); i += block)
        {
            const float* ins[1] = { in.data() + i }; float* outs[2] = { l.data() + i, r.data() + i };
            e.process (ins, 1, outs, 2, block);
        }
        CHECK (finite (l) && finite (r) && rms (l, 2560, 5000) > 0.01f, "mono in -> stereo out con la catena di default");
        // stesso buffer in/out (come nel plugin)
        std::vector<float> a (in), b (in.size(), 0.0f);
        for (size_t i = 0; i + block <= a.size(); i += block)
        {
            const float* ins[2] = { a.data() + i, b.data() + i }; float* outs[2] = { a.data() + i, b.data() + i };
            e.process (ins, 2, outs, 2, block);
        }
        CHECK (finite (a) && rms (a, 2560, 5000) > 0.01f, "elaborazione in-place (plugin)");
    }

    std::printf ("[Phase 90: notch e tremolo piu' profondo]\n");
    {
        MultiMod m; m.prepare (sr, block); m.enabled->set (1.0f);
        m.find ("mode")->set (0.0f); m.find ("rate")->set (0.1f); m.find ("depth")->set (0.0f); m.reset();
        float gmin = 10.0f, gmax = 0.0f, fmin = 0;
        for (double f = 100.0; f < 6000.0; f *= 1.07)
        {
            auto x = sine (sr, f, 0.3, 12032);
            for (size_t i = 0; i + block <= x.size(); i += block) m.run (x.data() + i, block);
            const float g = rms (x, 8000, 12000) / 0.2121f;
            if (g < gmin) { gmin = g; fmin = (float) f; }
            gmax = std::max (gmax, g);
        }
        std::printf ("       phaser: guadagno min %.3f (a %.0f Hz) max %.3f\n", gmin, fmin, gmax);
        CHECK (gmin < 0.35f && gmax > 0.8f, "phaser: notch profondi (almeno -9 dB) e passa-tutto fuori dai notch");
        // Tremolo con i default del modo: dip chiaramente udibile (>= 12 dB)
        m.find ("mode")->set (2.0f); m.applyModeDefaults(); m.reset();
        auto x = sine (sr, 1000, 0.5, 48128);
        for (size_t i = 0; i + block <= x.size(); i += block) m.run (x.data() + i, block);
        float mn = 1e9f, mx = 0; for (size_t i = 12000; i + 240 <= 48000; i += 240) { const float r = rms (x, i, i + 240); mn = std::min (mn, r); mx = std::max (mx, r); }
        std::printf ("       tremolo (default): inviluppo min %.3f max %.3f  (dip %.1f dB)\n", mn, mx, 20.0f * std::log10 (mx / std::max (mn, 1e-6f)));
        CHECK (mx / std::max (mn, 1e-6f) > 4.0f, "tremolo con i default: dip di almeno 12 dB");
    }

    std::printf ("[Compressore Dyna Comp]\n");
    {
        DynaComp c; c.prepare (sr, block); c.enabled->set (1.0f);
        c.find ("sens")->set (8.0f); c.find ("lvl")->set (5.0f); c.reset();
        auto lo = sine (sr, 220, 0.03, 48128), hi = sine (sr, 220, 0.5, 48128);
        for (size_t i = 0; i + block <= lo.size(); i += block) c.run (lo.data() + i, block);
        c.reset();
        for (size_t i = 0; i + block <= hi.size(); i += block) c.run (hi.data() + i, block);
        const float rl = rms (lo, 24000, 48000), rh = rms (hi, 24000, 48000);
        std::printf ("       ingresso 0.03 / 0.5 (24.4 dB di differenza) -> uscita %.3f / %.3f (%.1f dB)\n", rl, rh, 20.0f * std::log10 (rh / rl));
        CHECK (finite (lo) && finite (hi) && 20.0f * std::log10 (rh / rl) < 8.0f, "la differenza di livello si riduce di molto (sustain)");
        DynaComp d; d.prepare (sr, block); d.enabled->set (1.0f); d.find ("mix")->set (0.0f); d.reset();
        auto t = sine (sr, 220, 0.3, 12032); auto t0 = t;
        for (size_t i = 0; i + block <= t.size(); i += block) d.run (t.data() + i, block);
        float diff = 0; for (size_t i = 0; i < t.size(); ++i) diff = std::max (diff, std::abs (t[i] - t0[i]));
        CHECK (diff < 1e-4f, "Mix 0 % = segnale pulito");
    }

    std::printf ("[Banco patch / scene]\n");
    {
        RigEngine e;
        e.od.find ("drive")->set (8.0f); e.storeSlot (false, 0);
        e.od.find ("drive")->set (2.0f); e.storeSlot (false, 1);
        const int v0 = e.stateVersion.load();
        CHECK (e.recallSlot (false, 0) && std::abs (e.od.find ("drive")->get() - 8.0f) < 1e-4f && e.currentPatch == 0, "richiamo patch 1");
        CHECK (e.stateVersion.load() > v0, "la UI viene avvisata del cambio");
        CHECK (e.recallSlot (false, 1) && std::abs (e.od.find ("drive")->get() - 2.0f) < 1e-4f && e.currentPatch == 1, "richiamo patch 2");
        CHECK (! e.recallSlot (false, 2) && ! e.hasSlot (false, 2), "patch vuoto: nessun effetto");
        // scene dentro al patch corrente (2)
        e.od.enabled->set (1.0f); e.od.find ("drive")->set (9.0f); e.storeSlot (true, 0);
        e.od.enabled->set (0.0f); e.od.find ("drive")->set (1.0f); e.storeSlot (true, 1);
        e.setPreOrder ({ 5, 4, 3, 2, 1, 0 });
        CHECK (e.recallSlot (true, 0) && e.od.enabled->on() && std::abs (e.od.find ("drive")->get() - 9.0f) < 1e-4f, "scena 1: on + drive 9");
        CHECK (e.recallSlot (true, 1) && ! e.od.enabled->on() && std::abs (e.od.find ("drive")->get() - 1.0f) < 1e-4f, "scena 2: off + drive 1");
        CHECK (e.getPreOrder() == std::vector<int> ({ 5, 4, 3, 2, 1, 0 }), "le scene non toccano l'ordine dei moduli");
        // salvataggio e ripristino con il banco
        e.sceneMode = true; e.setSlotName (false, 0, "Clean"); e.setSlotName (true, 0, "Solo");
        auto xml = e.saveState();
        RigEngine e2; e2.loadState (*xml);
        CHECK (e2.sceneMode && e2.currentPatch == 1 && e2.hasSlot (false, 0) && e2.hasSlot (false, 1) && ! e2.hasSlot (false, 2), "banco ripristinato dal file");
        CHECK (e2.slotName (false, 0) == "Clean" && e2.slotName (true, 0) == "Solo" && e2.slotName (false, 1) == "Patch 1-2", "nomi salvati (e predefiniti)");
        CHECK (e2.recallSlot (true, 0) && std::abs (e2.od.find ("drive")->get() - 9.0f) < 1e-4f, "la scena sopravvive al salvataggio");
        CHECK (e2.recallSlot (false, 0) && std::abs (e2.od.find ("drive")->get() - 8.0f) < 1e-4f, "e anche il patch");
    }

    std::printf ("[Controlli di livello dei pedali: 0 = chiuso, 5 (meta' corsa) = unity, 10 = boost]\n");
    {
        auto run = [&] (MonoModule& m, double amp, int n = 24064)
        {
            m.reset();
            auto x = sine (sr, 220, amp, (size_t) n);
            for (size_t i = 0; i + (size_t) block <= x.size(); i += (size_t) block) m.run (x.data() + i, block);
            return rms (x, 12000, (size_t) n - 64);
        };
        auto db = [] (float a, float b) { return 20.0f * std::log10 (std::max (a, 1e-9f) / std::max (b, 1e-9f)); };
        CHECK (levelTaperGain (0.0f) == 0.0f && std::abs (levelTaperGain (5.0f) - 1.0f) < 1e-6f, "curva: 0 = silenzio esatto, 5 (meta' corsa) = guadagno 1");
        CHECK (std::abs (20.0f * std::log10 (levelTaperGain (10.0f)) - 12.0f) < 0.01f, "curva: 10 = +12 dB");
        struct P { const char* name; MonoModule* m; const char* lvlId; double amp; };
        TubeScreamer ts; Ds1 ds; DynaComp dc; MultiMod mm;
        mm.find ("mode")->set (1.0f); mm.find ("depth")->set (0.0f);   // chorus, depth 0: segnale + copia ritardata
        P pedals[] = { { "overdrive", &ts, "lvl", 0.2 }, { "distortion", &ds, "lvl", 0.2 }, { "compressore", &dc, "lvl", 0.126 }, { "multi mod", &mm, "lvl", 0.15 } };
        for (auto& p : pedals)
        {
            p.m->prepare (sr, block); p.m->enabled->set (1.0f);
            const double inRms = p.amp * 0.7071;
            auto at = [&] (float lvl) { p.m->find (p.lvlId)->set (lvl); return run (*p.m, p.amp); };
            const float l0 = at (0.0f), l3 = at (3.0f), l6 = at (5.0f), l8 = at (8.0f), l10 = at (10.0f);
            char msg[200];
            std::snprintf (msg, sizeof msg, "%s: livello 0 / 3 / 5 / 8 / 10 -> %.1f / %.1f / %.1f / %.1f / %.1f dB rispetto all'ingresso", p.name,
                           db (l0, (float) inRms), db (l3, (float) inRms), db (l6, (float) inRms), db (l8, (float) inRms), db (l10, (float) inRms));
            std::printf ("       %s\n", msg);
            std::snprintf (msg, sizeof msg, "%s: Level 0 chiude completamente il segnale", p.name);
            CHECK (l0 < 1e-6f, msg);
            std::snprintf (msg, sizeof msg, "%s: Level 10 = +12 dB rispetto a Level 5 (unity)", p.name);
            CHECK (std::abs (db (l10, l6) - 12.0f) < 0.6f, msg);
            std::snprintf (msg, sizeof msg, "%s: Level 3 sensibilmente piu' basso di Level 5 (-9..-13 dB)", p.name);
            CHECK (db (l3, l6) < -9.0f && db (l3, l6) > -13.0f, msg);
            if (std::string (p.name) == "overdrive" || std::string (p.name) == "distortion" || std::string (p.name) == "compressore")
            {
                std::snprintf (msg, sizeof msg, "%s: a Level 5 (meta' corsa) il livello e' ~ quello del segnale pulito (unity)", p.name);
                CHECK (std::abs (db (l6, (float) inRms)) < 2.0f, msg);
            }
        }
    }

    std::printf ("[Migrazione dei livelli (unity 6 -> 5)]\n");
    {
        // stesso guadagno prima e dopo la conversione, per ogni posizione salvata con la scala precedente
        auto oldGain = [] (float p) { return p <= 6.0f ? std::pow (p / 6.0f, 2.5f) : std::pow (10.0f, (p - 6.0f) * 3.0f / 20.0f); };
        float worst = 0;
        for (float p = 0.0f; p <= 10.001f; p += 0.25f)
            worst = std::max (worst, std::abs (levelTaperGain (migrateLevelV2toV3 (p)) - oldGain (p)) / std::max (oldGain (p), 1e-3f));
        CHECK (worst < 0.02f && std::abs (migrateLevelV2toV3 (6.0f) - 5.0f) < 1e-6f && std::abs (migrateLevelV2toV3 (10.0f) - 10.0f) < 1e-6f,
               "il guadagno dei vecchi preset resta uguale dopo la conversione (unity 6 diventa 5)");
        juce::XmlElement old ("DaniRigPreset"); old.setAttribute ("version", 2); old.setAttribute ("od.lvl", 6.0); old.setAttribute ("dist.lvl", 3.0); old.setAttribute ("comp.lvl", 8.0);
        RigEngine e; e.loadState (old);
        CHECK (std::abs (e.od.find ("lvl")->get() - 5.0f) < 1e-4f && std::abs (e.dist.find ("lvl")->get() - 2.5f) < 1e-4f && std::abs (e.comp.find ("lvl")->get() - 7.5f) < 1e-4f,
               "preset versione 2: livelli 6 / 3 / 8 -> 5 / 2.5 / 7.5");
        juce::XmlElement cur ("DaniRigPreset"); cur.setAttribute ("version", 3); cur.setAttribute ("od.lvl", 6.0);
        RigEngine e2; e2.loadState (cur);
        CHECK (std::abs (e2.od.find ("lvl")->get() - 6.0f) < 1e-4f, "preset versione 3: livello invariato");
        auto snap = e.saveSnapshot();
        CHECK (snap->getIntAttribute ("version") == 3, "le nuove scene sono marcate versione 3");
    }

    std::printf ("[Libreria: recenti e preferiti]\n");
    {
        const auto store = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("danirig_lib_test/library.xml");
        store.deleteFile();
        const juce::File a ("/tmp/a.nam"), b ("/tmp/b.nam"), c ("/tmp/c.wav");
        {
            FileLibrary lib (store);
            lib.addRecent (FileLibrary::Nam, a); lib.addRecent (FileLibrary::Nam, b); lib.addRecent (FileLibrary::Nam, a);
            CHECK (lib.getRecent (FileLibrary::Nam).size() == 2 && lib.getRecent (FileLibrary::Nam)[0] == a.getFullPathName(), "recenti: ordine dal piu' recente, nessun duplicato");
            for (int i = 0; i < 20; ++i) lib.addRecent (FileLibrary::Nam, juce::File ("/tmp/m" + juce::String (i) + ".nam"));
            CHECK (lib.getRecent (FileLibrary::Nam).size() == FileLibrary::kMaxRecent, "recenti: massimo 12");
            lib.addRecent (FileLibrary::Ir, c);
            CHECK (lib.getRecent (FileLibrary::Ir).size() == 1 && lib.getRecent (FileLibrary::Ir)[0] == c.getFullPathName(), "le liste NAM e IR sono separate");
            lib.setFavorite (FileLibrary::Nam, b, true); lib.setFavorite (FileLibrary::Nam, b, true);
            CHECK (lib.isFavorite (FileLibrary::Nam, b) && lib.getFavorites (FileLibrary::Nam).size() == 1 && ! lib.isFavorite (FileLibrary::Ir, b), "preferiti: nessun duplicato, separati per tipo");
        }
        {
            FileLibrary lib2 (store);                                   // nuova istanza = riavvio dell'app
            CHECK (lib2.isFavorite (FileLibrary::Nam, b) && lib2.getRecent (FileLibrary::Ir).size() == 1 && lib2.getRecent (FileLibrary::Nam).size() == FileLibrary::kMaxRecent,
                   "recenti e preferiti sopravvivono al riavvio");
            lib2.setFavorite (FileLibrary::Nam, b, false); lib2.clearRecent (FileLibrary::Nam);
            CHECK (! lib2.isFavorite (FileLibrary::Nam, b) && lib2.getRecent (FileLibrary::Nam).isEmpty() && lib2.getRecent (FileLibrary::Ir).size() == 1, "rimozione preferito e svuotamento recenti");
        }
        store.getParentDirectory().deleteRecursively();
    }

    std::printf ("[Analisi IR: spettro, durata, inviluppo]\n");
    {
        const juce::File ir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("danirig_analysis_ir.wav");
        ir.deleteFile();
        {
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::FileOutputStream> os (ir.createOutputStream());
            std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), 48000.0, 1, 24, {}, 0));
            if (w) { os.release(); juce::AudioBuffer<float> b (1, 64); b.clear(); for (int i = 0; i < 8; ++i) b.setSample (0, i, 0.125f); w->writeFromAudioSampleBuffer (b, 0, 64); }
        }
        const auto info = analyseIr (ir);
        CHECK (info.valid && info.sampleRate == 48000.0 && info.channels == 1 && std::abs (info.lengthMs - 64.0 * 1000.0 / 48000.0) < 0.01, "dati generali: 48 kHz, mono, 1.33 ms");
        auto at = [&] (double f) { int best = 0; for (int i = 0; i < IrInfo::kSpectrumPoints; ++i) if (std::abs (IrInfo::frequencyAt (i, info.sampleRate) - f) < std::abs (IrInfo::frequencyAt (best, info.sampleRate) - f)) best = i; return info.spectrumDb[(size_t) best]; };
        std::printf ("       spettro: 1 kHz %.1f dB, 6 kHz %.1f dB, 10 kHz %.1f dB\n", at (1000), at (6000), at (10000));
        CHECK (info.spectrumDb.size() == (size_t) IrInfo::kSpectrumPoints && at (10000) < at (1000) - 10.0f, "spettro: media mobile = passa-basso (10 kHz molto sotto 1 kHz)");
        CHECK (info.envelope.size() == (size_t) IrInfo::kEnvelopePoints && info.envelope[0] > 0.99f, "inviluppo: picco all'inizio");
        CHECK (std::abs (cabFilterDb (1000.0f, 70.0f, 14000.0f)) < 0.2f && cabFilterDb (70.0f, 70.0f, 14000.0f) < -2.9f && cabFilterDb (14000.0f, 70.0f, 14000.0f) < -2.9f && cabFilterDb (20.0f, 70.0f, 14000.0f) < -20.0f,
               "risposta dei filtri: -3 dB alle frequenze di taglio, piatta in mezzo");
        CHECK (! analyseIr (juce::File ("/tmp/non_esiste.wav")).valid, "file inesistente: analisi non valida");
        ir.deleteFile();
    }

    std::printf ("[Metadati dei profili NAM]\n");
    if (argc > 1 && juce::File (argv[1]).existsAsFile())
    {
        NamAmp m;
        m.loadModelAsync (juce::File (argv[1]));           // senza audio attivo: si leggono solo i metadati
        for (int i = 0; i < 200 && ! m.getInfo().loaded && m.getInfo().error.isEmpty(); ++i) juce::Thread::sleep (50);
        const auto in = m.getInfo();
        std::printf ("       %s | %s | %s\n", in.displayName().toRawUTF8(), in.technicalLine().toRawUTF8(), in.date.toRawUTF8());
        CHECK (in.loaded && in.error.isEmpty() && in.architecture.isNotEmpty() && in.version.isNotEmpty(), "metadati letti senza audio attivo: architettura e versione");
        CHECK (in.hasLoudness && in.hasInputDbu && in.modelRate > 0 && in.date.isNotEmpty(), "metadati: loudness, livello d'ingresso, sample rate e data");
        CHECK (in.technicalLine().contains ("kHz") && in.tooltip().contains (in.displayName()), "testo informativo per l'interfaccia");
    }
    else std::printf ("  (nessun modello passato: salto)\n");

    std::printf ("[5 banchi x 3 patch]\n");
    {
        RigEngine e;
        // riempie i 15 patch con un valore di drive diverso
        for (int b = 0; b < RigEngine::kBanks; ++b)
        {
            e.viewBank = b;
            for (int i = 0; i < 3; ++i) { e.od.find ("drive")->set ((float) (b * 3 + i) * 0.6f + 0.5f); e.storeSlot (false, i); }
        }
        bool all = true;
        for (int b = 0; b < RigEngine::kBanks; ++b)
        {
            e.viewBank = b;
            for (int i = 0; i < 3; ++i) all = all && e.recallSlot (false, i) && std::abs (e.od.find ("drive")->get() - ((float) (b * 3 + i) * 0.6f + 0.5f)) < 1e-3f;
        }
        CHECK (all, "15 patch distinti memorizzati e richiamati");
        e.viewBank = 3; e.recallSlot (false, 1);
        CHECK (e.loadedLabel() == "4-2" && e.loadedPatchName() == "Patch 4-2", "display: banco-patch e nome predefinito");
        e.setSlotName (false, 1, "Solo caldo");
        CHECK (e.loadedPatchName() == "Solo caldo", "display: nome rinominato");
        e.viewBank = 0;                                              // si sfoglia un altro banco senza cambiare il patch caricato
        CHECK (e.loadedLabel() == "4-2" && e.slotName (false, 1) == "Patch 1-2", "cambiare banco non cambia il patch caricato");
        e.od.find ("drive")->set (9.0f); e.storeSlot (true, 2);      // scena del patch caricato (4-2)
        CHECK (e.loadedSceneName() == "" || e.loadedSceneName().isNotEmpty(), "nome scena disponibile");
        e.viewBank = 2; e.sceneMode = true;
        auto xml = e.saveState();
        RigEngine e2; e2.loadState (*xml);
        CHECK (e2.viewBank == 2 && e2.currentBank == 3 && e2.currentPatch == 1 && e2.sceneMode, "banco visualizzato e patch caricato ripristinati");
        e2.sceneMode = false; e2.viewBank = 4; e2.recallSlot (false, 2);
        CHECK (std::abs (e2.od.find ("drive")->get() - (14.0f * 0.6f + 0.5f)) < 1e-3f, "patch 5-3 dopo il ripristino");
        e2.viewBank = 3; e2.recallSlot (false, 1);
        CHECK (e2.recallSlot (true, 2) && std::abs (e2.od.find ("drive")->get() - 9.0f) < 1e-4f, "la scena resta legata al suo patch");
        CHECK (e2.bankHasPatches (0) && e2.bankHasPatches (4), "indicatori dei banchi con patch");
        // compatibilità: preset a 3 slot (senza attributo bank) finisce nel banco 1
        juce::XmlElement old ("DaniRigPreset");
        auto* b = old.createNewChildElement ("Bank"); b->setAttribute ("currentPatch", 1);
        auto* p = b->createNewChildElement ("Patch"); p->setAttribute ("index", 1); p->setAttribute ("name", "Vecchio");
        auto* st = p->createNewChildElement ("DaniRigPreset"); st->setAttribute ("od.drive", 4.0);
        RigEngine e3; e3.loadState (old);
        CHECK (e3.currentBank == 0 && e3.currentPatch == 1 && e3.hasSlot (false, 1) && e3.slotName (false, 1) == "Vecchio", "compatibilità con i preset a 3 slot");
    }

    std::printf ("[Preset portatili: esporta con i file, percorsi relativi, file mancanti]\n");
    {
        auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("danirig_export_test");
        tmp.deleteRecursively(); tmp.createDirectory();
        auto srcA = tmp.getChildFile ("srcA"), srcB = tmp.getChildFile ("srcB"); srcA.createDirectory(); srcB.createDirectory();
        srcA.getChildFile ("amp.nam").replaceWithText ("{\"finto\":1}");
        srcB.getChildFile ("amp.nam").replaceWithText ("{\"altro\":2}");           // stesso nome, contenuto diverso
        srcA.getChildFile ("cab.wav").replaceWithText ("RIFFfinto");
        RigEngine e;
        e.amp.loadModelAsync (srcA.getChildFile ("amp.nam"));
        e.pedal.loadModelAsync (srcB.getChildFile ("amp.nam"));
        e.cab.loadIR (srcA.getChildFile ("cab.wav"));
        e.od.find ("drive")->set (7.0f); e.storeSlot (false, 0);
        juce::StringArray report; int nm = 0, ni = 0;
        const auto res = e.exportBundle (tmp.getChildFile ("out/MioSuono.dprig"), report, nm, ni);
        CHECK (res.wasOk() && report.isEmpty(), "export riuscito, nessun avviso");
        auto dir = tmp.getChildFile ("out/MioSuono");
        CHECK (dir.getChildFile ("MioSuono.dprig").existsAsFile(), "creato MioSuono/MioSuono.dprig");
        CHECK (nm == 2 && ni == 1, "copiati 2 modelli (stesso nome, contenuto diverso) e 1 IR");
        CHECK (dir.getChildFile ("models/amp.nam").existsAsFile() && dir.getChildFile ("models/amp_2.nam").existsAsFile(), "collisione di nomi risolta (amp.nam, amp_2.nam)");
        auto xml = juce::XmlDocument::parse (dir.getChildFile ("MioSuono.dprig"));
        CHECK (xml != nullptr && xml->getStringAttribute ("irPath") == "ir/cab.wav" && ! juce::File::isAbsolutePath (xml->getStringAttribute ("namPath")), "il .dprig contiene percorsi relativi");
        // copia l'intera cartella altrove, cancella i sorgenti, ricarica
        auto moved = tmp.getChildFile ("altro_pc/MioSuono"); dir.copyDirectoryTo (moved);
        srcA.deleteRecursively(); srcB.deleteRecursively(); dir.deleteRecursively();
        auto xml2 = juce::XmlDocument::parse (moved.getChildFile ("MioSuono.dprig"));
        RigEngine::resolveRelativePaths (*xml2, moved);
        RigEngine e2; e2.loadState (*xml2);
        CHECK (e2.amp.getModelPath() == moved.getChildFile ("models/amp.nam").getFullPathName() && e2.cab.getIRPath() == moved.getChildFile ("ir/cab.wav").getFullPathName(), "ricarico da un'altra cartella: i percorsi puntano ai file copiati");
        CHECK (! e2.hasWarnings.load(), "nessun avviso: i file ci sono");
        // il patch salvato nel banco usa a sua volta i percorsi risolti
        CHECK (e2.recallSlot (false, 0) && std::abs (e2.od.find ("drive")->get() - 7.0f) < 1e-4f && ! e2.hasWarnings.load(), "il patch del banco ritrova i suoi file");
        // file mancante -> avviso con il nome
        moved.getChildFile ("models/amp.nam").deleteFile();
        RigEngine e3; e3.loadState (*xml2);
        const auto w = e3.takeWarnings();
        std::printf ("       avvisi: %s\n", w.joinIntoString (" | ").toRawUTF8());
        CHECK (e3.hasWarnings.load() == false && w.size() >= 1 && w.joinIntoString ("").contains ("amp.nam"), "file mancante: avviso con il percorso");
        tmp.deleteRecursively();
    }

    std::printf ("[Ordine moduli + pedale NAM]\n");
    {
        RigEngine e;
        CHECK (RigEngine::unpack (RigEngine::pack ({ 2, 0, 4, 5, 3, 1 }), 6) == std::vector<int> ({ 2, 0, 4, 5, 3, 1 }), "pack/unpack dell'ordine");
        CHECK (e.getPreOrder() == std::vector<int> ({ 0, 5, 4, 2, 3, 1 }), "ordine di default: gate, comp, multi mod, od, dist, pedale NAM (poi l'ampli)");
        e.setPreOrder ({ 3, 1, 0, 4, 2, 5 }); e.setPostOrder ({ 1, 0 });
        auto xml = e.saveState();
        RigEngine e2; e2.loadState (*xml);
        CHECK (e2.getPreOrder() == std::vector<int> ({ 3, 1, 0, 4, 2, 5 }) && e2.getPostOrder() == std::vector<int> ({ 1, 0 }), "l'ordine viene salvato e ripristinato");
        xml->setAttribute ("preOrder", 0x1111);                       // permutazione non valida
        RigEngine e3; e3.loadState (*xml);
        CHECK (e3.getPreOrder() == std::vector<int> ({ 0, 5, 4, 2, 3, 1 }), "ordine corrotto -> default");
        CHECK (e.pedal.bass == nullptr && e.amp.bass != nullptr, "il pedale NAM non ha EQ, l'ampli sì");
        CHECK (e.pedal.enabled->get() == 0.0f, "il pedale NAM parte spento");
        if (argc > 1 && juce::File (argv[1]).existsAsFile())
        {
            NamAmp p ("nampedal", "NAM PEDAL", false);
            p.loadModelAsync (juce::File (argv[1])); p.prepare (sr, block);
            for (int i = 0; i < 200 && ! p.getInfo().loaded; ++i) juce::Thread::sleep (50);
            auto x = sine (sr, 220, 0.1, 24064);
            for (size_t i = 0; i + block <= x.size(); i += block) p.run (x.data() + i, block);
            CHECK (p.getInfo().loaded && finite (x) && rms (x, 12000, 24000) > 1e-4f, "pedale NAM: carica e suona");
            // livello del pedale NAM: 0 = chiuso, 10 = +12 dB rispetto a 6
            p.normalize->set (0.0f);
            auto lvl = [&] (float v)
            {
                p.outputDb->set (v); p.reset();
                auto y = sine (sr, 220, 0.1, 24064);
                for (size_t i = 0; i + block <= y.size(); i += block) p.run (y.data() + i, block);
                return rms (y, 12000, 24000);
            };
            const float o0 = lvl (0.0f), o6 = lvl (5.0f), o10 = lvl (10.0f);
            std::printf ("       pedale NAM: output 0 / 5 / 10 -> %.6f / %.4f / %.4f (10 vs 5: %.1f dB)\n", o0, o6, o10, 20.0f * std::log10 (o10 / o6));
            CHECK (o0 < 1e-6f, "pedale NAM: Output 0 chiude completamente il segnale");
            CHECK (std::abs (20.0f * std::log10 (o10 / o6) - 12.0f) < 0.6f, "pedale NAM: Output 10 = +12 dB rispetto a 5 (unity)");
            p.collectGarbage();
        }
    }

    if (argc > 1 && juce::File (argv[1]).existsAsFile())
    {
        std::printf ("[Prestazioni: 10 s di audio, 1 core]\n");
        for (int q = 0; q < 2; ++q)
            for (int blk : { 64, 128 })
            {
                NamAmp a; a.quality->set ((float) q); a.prepare (sr, blk); a.loadModelAsync (juce::File (argv[1]));
                for (int i = 0; i < 200 && ! a.getInfo().loaded; ++i) juce::Thread::sleep (50);
                auto x = sine (sr, 220, 0.1, (size_t) sr * 10 / (size_t) blk * (size_t) blk);
                for (size_t i = 0; i < 4800; i += (size_t) blk) a.run (x.data() + i, blk);
                const double t0 = juce::Time::getMillisecondCounterHiRes();
                for (size_t i = 0; i + (size_t) blk <= x.size(); i += (size_t) blk) a.run (x.data() + i, blk);
                const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
                std::printf ("       A2 %s, blocco %3d: %.1f %% di un core (%.1fx tempo reale)\n", q ? "Lite" : "Full", blk, ms / 100.0, 10000.0 / ms);
                a.collectGarbage();
            }
    }

    std::printf ("\n%s (%d errori)\n", failures ? "FALLITO" : "TUTTO OK", failures);
    return failures ? 1 : 0;
}
