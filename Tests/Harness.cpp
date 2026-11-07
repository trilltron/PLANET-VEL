// Headless DSP / preset test harness for PLANET VEL (no audio device, no GUI).
//   PlanetVelTests export <dir>            write all factory presets as .pvpreset + round-trip check
//   PlanetVelTests render [step] [wavDir]  render every Nth preset, check for NaN/Inf/silence/clipping, measure CPU
//   PlanetVelTests stress                  sample-rate / block-size / quality / polyphony / random-automation torture test
#include "PluginProcessor.h"
#include "UI/PluginEditor.h"
#include "Vst2.h"
#include <set>

using namespace PV;

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { ++failures; std::printf ("  FAIL: %s\n", (msg)); } } while (0)

struct RenderResult { float peak = 0, rms = 0; bool bad = false; double cpu = 0; int voices = 0; float tailRms = 0; };

static void playChord (PlanetVelProcessor& p, juce::AudioBuffer<float>& out, double sr, int block, double seconds, double noteSeconds,
                       std::initializer_list<int> notes, RenderResult& r, int velocity = 100, juce::AudioBuffer<float>* keep = nullptr)
{
    const int total = (int) (seconds * sr); int done = 0; double rmsAcc = 0, tailAcc = 0; long cnt = 0, tailCnt = 0; double tNotes = 0;
    bool released = false, started = false; double usedSec = 0;
    out.setSize (2, block, false, true, true);
    if (keep) keep->setSize (2, total);
    while (done < total)
    {
        const int n = juce::jmin (block, total - done);
        juce::MidiBuffer midi;
        if (! started) { for (int k : notes) midi.addEvent (juce::MidiMessage::noteOn (1, k, (juce::uint8) velocity), 0); started = true; }
        if (! released && tNotes >= noteSeconds) { for (int k : notes) midi.addEvent (juce::MidiMessage::noteOff (1, k), 0); released = true; }
        juce::AudioBuffer<float> b (2, n); b.clear();
        const auto t0 = juce::Time::getHighResolutionTicks();
        p.processBlock (b, midi);
        usedSec += (double) (juce::Time::getHighResolutionTicks() - t0) / (double) juce::Time::getHighResolutionTicksPerSecond();
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                const float s = b.getSample (c, i);
                if (! std::isfinite (s) || std::abs (s) > 8.0f) r.bad = true;
                r.peak = juce::jmax (r.peak, std::abs (s));
                if (tNotes < noteSeconds) { rmsAcc += (double) s * s; ++cnt; } else { tailAcc += (double) s * s; ++tailCnt; }
            }
        if (keep) for (int c = 0; c < 2; ++c) keep->copyFrom (c, done, b, c, 0, n);
        r.voices = juce::jmax (r.voices, p.voiceCount.load());
        done += n; tNotes += (double) n / sr;
    }
    r.rms = cnt ? (float) std::sqrt (rmsAcc / (double) cnt) : 0.0f; r.tailRms = tailCnt ? (float) std::sqrt (tailAcc / (double) tailCnt) : 0.0f;
    r.cpu = usedSec / seconds * 100.0;
}

static int cmdExport (const juce::File& dir)
{
    dir.createDirectory(); const auto& bank = PresetBank::factory();
    std::printf ("Exporting %d factory presets to %s\n", (int) bank.size(), dir.getFullPathName().toRawUTF8());
    std::map<juce::String, int> perCat; int macroFail = 0, modWheelFail = 0;
    for (auto& p : bank)
    {
        perCat[p.category]++;
        auto catDir = dir.getChildFile (juce::File::createLegalFileName (p.category)); catDir.createDirectory();
        auto f = catDir.getChildFile (juce::File::createLegalFileName (p.name) + ".pvpreset"); f.replaceWithText (p.toJson());
        Preset back; if (! Preset::fromJson (f.loadFileAsString(), back)) { CHECK (false, "reload failed"); continue; }
        for (int i = 0; i < NUM_PARAMS; ++i) if (std::abs (back.norm[(size_t) i] - p.norm[(size_t) i]) > 1e-4f) { CHECK (false, (p.name + " param mismatch " + defs()[(size_t) i].id).toRawUTF8()); break; }
        bool macro[8] = {}, wheel = false;
        for (auto& r : p.routes) { if (r.src >= SRC_M1 && r.src <= SRC_M8 && r.dest >= 0) macro[r.src - SRC_M1] = true; if (r.src == SRC_MODWHEEL && r.dest >= 0) wheel = true; }
        for (bool m : macro) if (! m) { ++macroFail; break; }
        if (! wheel) ++modWheelFail;
    }
    for (auto& kv : perCat) std::printf ("  %-22s %d\n", kv.first.toRawUTF8(), kv.second);
    CHECK (bank.size() >= 600, "need 600+ presets");
    CHECK (macroFail == 0, "every preset must map all 8 macros");
    CHECK (modWheelFail == 0, "every preset must map the mod wheel");
    std::printf ("  presets=%d  macro-unmapped=%d  modwheel-unmapped=%d\n", (int) bank.size(), macroFail, modWheelFail);
    return failures;
}

static int cmdRender (int step, const juce::File& wavDir)
{
    Library::get().waitUntilReady();
    PlanetVelProcessor p; p.setRateAndBufferSizeDetails (48000.0, 512); p.prepareToPlay (48000.0, 512);
    const auto& bank = PresetBank::factory();
    struct Agg { double rms = 0, cpu = 0, peak = 0; int n = 0, silent = 0, bad = 0, hot = 0; };
    std::map<juce::String, Agg> agg; juce::AudioBuffer<float> tmp;
    if (wavDir != juce::File()) wavDir.createDirectory();
    std::set<juce::String> wavDone;
    for (int i = 0; i < (int) bank.size(); i += step)
    {
        const auto& pr = bank[(size_t) i]; p.loadPreset (pr, i);
        RenderResult r; juce::AudioBuffer<float> keep;
        const bool wantWav = wavDir != juce::File() && wavDone.insert (pr.category).second;
        playChord (p, tmp, 48000.0, 512, wantWav ? 10.0 : 6.0, wantWav ? 4.0 : 2.5, { 48, 55, 60, 64 }, r, 100, wantWav ? &keep : nullptr);
        if (wantWav)
        {
            juce::WavAudioFormat wav; auto f = wavDir.getChildFile (juce::File::createLegalFileName (pr.category + " - " + pr.name) + ".wav");
            std::unique_ptr<juce::FileOutputStream> os (f.createOutputStream());
            if (os) { std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), 48000.0, 2, 16, {}, 0)); if (w) { os.release(); w->writeFromAudioSampleBuffer (keep, 0, keep.getNumSamples()); } }
        }
        auto& a = agg[pr.category]; a.n++; a.rms += r.rms; a.cpu += r.cpu; a.peak = juce::jmax (a.peak, (double) r.peak);
        if (r.rms < 1.0e-4f && r.tailRms < 1.0e-4f) { a.silent++; std::printf ("  SILENT: %s / %s\n", pr.category.toRawUTF8(), pr.name.toRawUTF8()); }
        if (r.bad) { a.bad++; std::printf ("  NaN/Inf/explosion: %s\n", pr.name.toRawUTF8()); }
        if (r.peak > 1.0f) a.hot++;
    }
    std::printf ("\n%-22s %5s %9s %9s %8s %7s %5s\n", "category", "n", "avgRMS dB", "peak dB", "CPU %", "silent", "bad");
    for (auto& kv : agg)
    {
        auto& a = kv.second;
        std::printf ("%-22s %5d %9.1f %9.1f %8.2f %7d %5d\n", kv.first.toRawUTF8(), a.n, juce::Decibels::gainToDecibels ((float) (a.rms / a.n)), juce::Decibels::gainToDecibels ((float) a.peak), a.cpu / a.n, a.silent, a.bad);
        CHECK (a.bad == 0, "NaN/Inf found"); CHECK (a.silent == 0, "silent preset found"); CHECK (a.hot == 0, "output exceeded 0 dBFS");
    }
    return failures;
}

// Loudness calibration: renders every preset and writes Source/FactoryCalibration.inc (per-preset output volume + master),
// so the factory bank sits at a consistent level (loudest 100 ms window of a 4-note chord ~ -17 dBFS RMS).
static int cmdCalibrate (const juce::File& outFile)
{
    Library::get().waitUntilReady();
    const auto& bank = PresetBank::factory(); const double sr = 48000.0; const float target = -17.0f;
    juce::String text = "// generated by PlanetVelTests calibrate - do not edit\n// { outVolDb, master }\nstatic const float kFactoryCal[][2] = {\n";
    float worst = 0, best = 0;
    for (size_t i = 0; i < bank.size(); ++i)
    {
        PlanetVelProcessor p; p.setRateAndBufferSizeDetails (sr, 512); p.prepareToPlay (sr, 512); p.loadPreset (bank[i]);
        // measure at neutral output settings
        p.param (G_OUT_VOL)->setValueNotifyingHost (toNormalised (G_OUT_VOL, 0.0f)); p.param (G_MASTER)->setValueNotifyingHost (toNormalised (G_MASTER, 0.75f));
        RenderResult r; juce::AudioBuffer<float> keep, tmp; playChord (p, tmp, sr, 512, 7.0, 5.0, { 48, 55, 60, 64 }, r, 100, &keep);
        const int win = (int) (0.1 * sr); float loud = 1.0e-6f;
        for (int s0 = 0; s0 + win <= keep.getNumSamples(); s0 += win / 2)
        { double acc = 0; for (int c = 0; c < 2; ++c) { auto* d = keep.getReadPointer (c) + s0; for (int k = 0; k < win; ++k) acc += (double) d[k] * d[k]; } loud = juce::jmax (loud, (float) std::sqrt (acc / (2.0 * win))); }
        const float db = juce::Decibels::gainToDecibels (loud), gain = juce::jlimit (-24.0f, 14.0f, target - db);
        float vol = juce::jmin (6.0f, gain), master = 0.75f;
        if (gain > 6.0f) master = juce::jmin (1.0f, std::sqrt (0.84375f * juce::Decibels::decibelsToGain (gain - 6.0f) / 1.5f));
        text << "    { " << juce::String (vol, 2) << "f, " << juce::String (master, 3) << "f },\n";
        worst = juce::jmin (worst, gain); best = juce::jmax (best, gain);
    }
    text << "};\n"; outFile.replaceWithText (text);
    std::printf ("calibration written to %s (trim range %.1f .. +%.1f dB)\n", outFile.getFullPathName().toRawUTF8(), worst, best);
    return 0;
}

// Renders the editor to a PNG (software renderer, works under Xvfb). Used to eyeball the layout against the reference design.
static int cmdSnapshot (const juce::File& out, int page, int program, int scalePct)
{
    Library::get().waitUntilReady();
    PlanetVelProcessor p; p.prepareToPlay (48000.0, 512);
    p.setCurrentProgram (program);
    juce::MidiBuffer m; juce::AudioBuffer<float> b (2, 512); b.clear();
    for (int n : { 60, 64, 67, 72 }) m.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);
    for (int i = 0; i < 40; ++i) { p.processBlock (b, m); m.clear(); }
    std::unique_ptr<PlanetVelEditor> ed (new PlanetVelEditor (p));
    ed->setSize (1600 * scalePct / 100, 1000 * scalePct / 100); ed->setPage (page);
    juce::Thread::sleep (100);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    out.deleteFile(); juce::PNGImageFormat png; juce::FileOutputStream os (out);
    const bool ok = os.openedOk() && png.writeImageToStream (img, os);
    std::printf ("snapshot %dx%d -> %s (%s)\n", img.getWidth(), img.getHeight(), out.getFullPathName().toRawUTF8(), ok ? "ok" : "FAILED");
    ed.reset(); return ok ? 0 : 1;
}

// Loads the BUILT VST3 bundle through JUCE's VST3 host (what a DAW does) and exercises it.
static int cmdVst3 (const juce::File& bundle)
{
    juce::AudioPluginFormatManager fm; fm.addFormat (new juce::VST3PluginFormat());
    juce::OwnedArray<juce::PluginDescription> descs;
    for (auto* f : fm.getFormats()) f->findAllTypesForFile (descs, bundle.getFullPathName());
    CHECK (descs.size() == 1, "scanner should find exactly one plugin in the bundle");
    if (descs.isEmpty()) return 1;
    const auto& d = *descs[0];
    std::printf ("scanned: %s | %s | manufacturer '%s' | synth=%d | uid=%d\n", d.name.toRawUTF8(), d.category.toRawUTF8(), d.manufacturerName.toRawUTF8(), (int) d.isInstrument, d.uniqueId);
    CHECK (d.isInstrument, "must be flagged as instrument");
    juce::String err;
    for (int cycle = 0; cycle < 12; ++cycle)                                   // create / destroy repeatedly (scanner, "make unique", clone channel)
    {
        auto inst = fm.createPluginInstance (d, 48000.0, 512, err);
        CHECK (inst != nullptr, ("instantiate failed: " + err).toRawUTF8()); if (! inst) return 1;
        if (cycle == 0)
        {
            std::printf ("params=%d programs=%d outputBuses=%d inputBuses=%d\n", (int) inst->getParameters().size(), inst->getNumPrograms(), inst->getBusCount (false), inst->getBusCount (true));
            // The VST3 wrapper may append its own Bypass / Program-change entries AFTER ours; ours must be first and in order.
            CHECK (inst->getParameters().size() >= NUM_PARAMS && inst->getParameters().size() <= NUM_PARAMS + 3, "host sees every parameter (plus <=3 wrapper entries)");
            bool order = true; for (int i = 0; i < NUM_PARAMS; ++i) order &= inst->getParameters()[i]->getName (64) == defs()[(size_t) i].name;
            CHECK (order, "parameter order/indices are stable (FL automation)");
            CHECK (inst->getNumPrograms() == (int) PresetBank::factory().size(), "factory presets exposed as host programs");
            CHECK (inst->getBusCount (false) == 5, "main + 4 layer outputs");
            // stable parameter order: names at fixed indices
            CHECK (inst->getParameters()[G_MASTER]->getName (64) == "Master", "param 0 is Master"); CHECK (inst->getParameters()[lp (3, L_P6)]->getName (64) == "D Param 6", "last param is D Param 6");
            // enable all layer buses (FL routing dialog) and render
            auto layout = inst->getBusesLayout();
            for (int b = 1; b < inst->getBusCount (false); ++b) layout.outputBuses.set (b, juce::AudioChannelSet::stereo());
            CHECK (inst->setBusesLayout (layout), "all 4 aux outputs enable");
            inst->prepareToPlay (48000.0, 512);
            juce::AudioBuffer<float> buf (10, 512); float peak = 0;
            inst->setCurrentProgram (0);
            for (int blk = 0; blk < 60; ++blk)
            {
                buf.clear(); juce::MidiBuffer m; if (blk == 0) m.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
                inst->processBlock (buf, m); for (int c = 0; c < 10; ++c) peak = juce::jmax (peak, buf.getMagnitude (c, 0, 512));
            }
            std::printf ("rendered via VST3 wrapper with 5 stereo outs: peak %.3f\n", peak); CHECK (peak > 0.001f, "audio through VST3 wrapper");
            juce::MemoryBlock mb; inst->setCurrentProgram (77); inst->getStateInformation (mb);
            auto inst2 = fm.createPluginInstance (d, 44100.0, 256, err); inst2->setStateInformation (mb.getData(), (int) mb.getSize());
            CHECK (inst2->getParameters()[G_MASTER]->getValue() == inst->getParameters()[G_MASTER]->getValue(), "state restores across instances");
            CHECK (std::abs (inst2->getParameters()[lp (0, L_CUTOFF)]->getValue() - inst->getParameters()[lp (0, L_CUTOFF)]->getValue()) < 1e-5f, "layer state restores");
            // editor open/close/resizes (reentrancy, no parameter reset while resizing)
            const float before = inst->getParameters()[lp (0, L_CUTOFF)]->getValue();
            for (int i = 0; i < 5; ++i)
            {
                std::unique_ptr<juce::AudioProcessorEditor> ed (inst->createEditor()); CHECK (ed != nullptr, "editor created");
                if (ed) { CHECK (ed->isResizable(), "editor resizable"); for (auto sz : { 960, 1280, 1600, 2400, 3840, 1000 }) ed->setSize (sz, sz * 10 / 16); ed->setSize (1600, 1000); }
            }
            CHECK (inst->getParameters()[lp (0, L_CUTOFF)]->getValue() == before, "resizing/closing the editor must not touch parameters");
        }
        inst.reset();
    }
    return failures;
}

// Drives the native VST2 entry point (VSTPluginMain) the way a VST2 host (FL Studio 20.0-20.6) does.
static pvvst2::intptr fakeHost (pvvst2::AEffect*, int32_t op, int32_t, pvvst2::intptr, void*, float) { return op == 1 ? 2400 : 0; }
static int cmdVst2()
{
    using namespace pvvst2;
    auto* fx = VSTPluginMain (&fakeHost);
    CHECK (fx != nullptr && fx->magic == 0x56737450, "VSTPluginMain returns an AEffect with 'VstP' magic");
    if (fx == nullptr) return 1;
    CHECK (fx->numParams == PV::NUM_PARAMS && fx->numPrograms == (int) PresetBank::factory().size(), "params/programs exposed");
    CHECK ((fx->flags & 256) != 0 && fx->numOutputs == 2 && fx->uniqueID == 0x5076656c, "synth flag, stereo out, unique id 'Pvel'");
    char name[64] = {}; fx->dispatcher (fx, 45, 0, 0, name, 0); CHECK (juce::String (name) == "Planet Vel", "effect name");
    char pn[64] = {}; fx->dispatcher (fx, 8, 0, 0, pn, 0); CHECK (juce::String (pn) == "Master", "param 0 name");
    fx->dispatcher (fx, 10, 0, 0, nullptr, 48000.0f); fx->dispatcher (fx, 11, 0, 512, nullptr, 0); fx->dispatcher (fx, 12, 0, 1, nullptr, 0);   // rate, block, resume
    fx->dispatcher (fx, 2, 0, 5, nullptr, 0);                                                                                                  // program 5
    VstMidiEvent me {}; me.type = 1; me.byteSize = sizeof (me); me.deltaFrames = 0; me.midiData[0] = (char) 0x90; me.midiData[1] = 60; me.midiData[2] = 100;
    VstEvents evs {}; evs.numEvents = 1; evs.events[0] = reinterpret_cast<VstEvent*> (&me);
    fx->dispatcher (fx, 25, 0, 0, &evs, 0);
    std::vector<float> l (512), r (512); float* out[2] = { l.data(), r.data() }; float peak = 0; Library::get().waitUntilReady();
    for (int b = 0; b < 60; ++b) { fx->processReplacing (fx, nullptr, out, 512); for (int i = 0; i < 512; ++i) peak = juce::jmax (peak, std::abs (l[(size_t) i]), std::abs (r[(size_t) i])); }
    std::printf ("VST2 entry point: %d params, %d programs, peak %.3f\n", fx->numParams, fx->numPrograms, peak); CHECK (peak > 0.001f, "audio through the VST2 entry point");
    void* chunk = nullptr; const auto sz = fx->dispatcher (fx, 23, 0, 0, &chunk, 0); CHECK (sz > 100 && chunk != nullptr, "VST2 chunk (state) saved");
    if (chunk) { juce::MemoryBlock keep (chunk, (size_t) sz); fx->dispatcher (fx, 24, 0, (pvvst2::intptr) sz, keep.getData(), 0); }
    fx->setParameter (fx, G_MASTER, 0.5f); CHECK (std::abs (fx->getParameter (fx, G_MASTER) - 0.5f) < 1e-3f, "parameter round trip");
    fx->dispatcher (fx, 1, 0, 0, nullptr, 0);                                                                                                  // effClose
    return failures;
}

static int cmdStress()
{
    Library::get().waitUntilReady();
    const auto& bank = PresetBank::factory();
    Rng rng (99);
    const double rates[] = { 44100, 48000, 88200, 96000, 192000 }; const int blocks[] = { 16, 64, 100, 512, 1024, 4096, 8192 };
    for (double sr : rates)
        for (int qi = 0; qi < 3; ++qi)
        {
            PlanetVelProcessor p; p.settings.quality = qi; p.settings.polyphony = 64; p.setRateAndBufferSizeDetails (sr, 512); p.prepareToPlay (sr, 512);
            bool bad = false; double worst = 0;
            for (int rep = 0; rep < 6; ++rep)
            {
                p.loadPreset (bank[(size_t) (rng.u32() % bank.size())]);
                const int block = blocks[rng.u32() % 7];
                RenderResult r; juce::AudioBuffer<float> tmp;
                // random automation during playback + a dense chord
                for (int k = 0; k < 20; ++k) p.param ((int) (rng.u32() % NUM_PARAMS))->setValueNotifyingHost (rng.next());
                playChord (p, tmp, sr, block, 1.5, 1.0, { 36, 43, 48, 52, 55, 60, 64, 67, 71, 72, 76, 79 }, r, 127);
                bad |= r.bad; worst = juce::jmax (worst, r.cpu);
            }
            std::printf ("sr=%6.0f quality=%d  %s  worst CPU %.1f%%\n", sr, qi, bad ? "BAD" : "ok", worst);
            CHECK (! bad, "NaN/Inf under stress");
        }
    // voice-stealing / 64 voice torture + all notes off + sustain pedal
    {
        PlanetVelProcessor p; p.settings.polyphony = 64; p.prepareToPlay (48000.0, 256); p.loadPreset (bank[0]);
        juce::MidiBuffer m; for (int n = 24; n < 24 + 90; ++n) m.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 90), n);
        m.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 100);
        juce::AudioBuffer<float> b (2, 256); b.clear(); p.processBlock (b, m);
        CHECK (p.voiceCount.load() <= 64, "voice limit respected");
        std::printf ("voices after 90 note-ons (limit 64): %d\n", p.voiceCount.load());
        juce::MidiBuffer off; off.addEvent (juce::MidiMessage::allNotesOff (1), 0); p.processBlock (b, off);
    }
    // live sample-rate / buffer-size changes on ONE instance (hosts call prepareToPlay again without recreating the plugin)
    {
        PlanetVelProcessor p; p.loadPreset (bank[5]); bool bad = false;
        for (double sr : { 44100.0, 96000.0, 48000.0, 192000.0, 44100.0, 22050.0 })
        {
            p.setRateAndBufferSizeDetails (sr, 256); p.prepareToPlay (sr, 256); RenderResult r; juce::AudioBuffer<float> tmp;
            playChord (p, tmp, sr, 256, 0.6, 0.35, { 60, 64 }, r); bad |= r.bad;
        }
        CHECK (! bad, "live sample-rate changes"); std::printf ("live sample-rate changes (22.05k..192k): %s\n", bad ? "BAD" : "ok");
    }
    // state save / restore round trip incl. mod routes + arp
    {
        PlanetVelProcessor a, b; a.loadPreset (bank[42], 42);
        a.param (G_MASTER)->setValueNotifyingHost (0.33f); a.arpLength = 11; a.arpSteps[3].store (ArpStep { false, 77, 5, 3, 60 }.pack());
        juce::MemoryBlock mb; a.getStateInformation (mb); b.setStateInformation (mb.getData(), (int) mb.getSize());
        CHECK (std::abs (b.param (G_MASTER)->getValue() - 0.33f) < 1e-4f, "param restored");
        CHECK (b.arpLength.load() == 11 && b.arpSteps[3].load() == a.arpSteps[3].load(), "arp restored");
        int same = 0; for (int i = 0; i < kRouteSlots; ++i) same += a.routes[(size_t) i].dest.load() == b.routes[(size_t) i].dest.load() && a.routes[(size_t) i].src.load() == b.routes[(size_t) i].src.load();
        CHECK (same == kRouteSlots, "routes restored"); CHECK (b.currentName() == a.currentName(), "preset name restored");
        std::printf ("state round trip: %s (preset '%s')\n", failures ? "FAIL" : "ok", b.currentName().toRawUTF8());
    }
    // arpeggiator timing: 1/8 notes at 120 bpm -> a note every 0.25 s
    {
        Arp arp; arp.prepare (48000.0); ArpPattern pat; for (auto& s : pat) s.store (ArpStep().pack());
        ArpParams ap; ap.on = true; ap.pattern = 0; ap.rate = 3; ap.octaves = 1; ap.length = 16; ap.gate = 0.5f;
        juce::MidiBuffer in, out; in.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0); in.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0);
        std::vector<int> starts; int offset = 0;
        for (int blk = 0; blk < 400; ++blk) { out.clear(); arp.process (in, out, 480, ap, pat, 120.0, false, 0.0, false); in.clear(); for (auto m : out) if (m.getMessage().isNoteOn()) starts.push_back (offset + m.samplePosition); offset += 480; }
        bool ok = starts.size() >= 8; for (size_t i = 1; i < starts.size(); ++i) ok &= std::abs ((int) (starts[i] - starts[i - 1]) - 12000) <= 1;
        CHECK (ok, "arp 1/8 spacing at 120bpm should be 12000 samples"); std::printf ("arp: %d steps, spacing %s\n", (int) starts.size(), ok ? "exact" : "WRONG");
    }
    return failures;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::String cmd = argc > 1 ? argv[1] : "export";
    int rc = 0;
    if (cmd == "export") rc = cmdExport (juce::File (argc > 2 ? argv[2] : "Presets"));
    else if (cmd == "render") rc = cmdRender (argc > 2 ? juce::jmax (1, std::atoi (argv[2])) : 10, argc > 3 ? juce::File (argv[3]) : juce::File());
    else if (cmd == "stress") rc = cmdStress();
    else if (cmd == "vst2") rc = cmdVst2();
    else if (cmd == "libtime") { const auto t0 = juce::Time::getMillisecondCounterHiRes(); Library::get().waitUntilReady(); std::printf ("library (wavetables + granular sources) ready after %.0f ms\n", juce::Time::getMillisecondCounterHiRes() - t0); }
    else if (cmd == "vst3") rc = cmdVst3 (juce::File (argc > 2 ? argv[2] : "Planet Vel.vst3"));
    else if (cmd == "snapshot") rc = cmdSnapshot (juce::File (argc > 2 ? argv[2] : "ui.png"), argc > 3 ? std::atoi (argv[3]) : 0, argc > 4 ? std::atoi (argv[4]) : 0, argc > 5 ? std::atoi (argv[5]) : 100);
    else if (cmd == "calibrate") rc = cmdCalibrate (juce::File (argc > 2 ? argv[2] : "FactoryCalibration.inc"));
    std::printf ("\n%s (%d failures)\n", rc == 0 ? "ALL CHECKS PASSED" : "FAILURES", rc);
    return rc == 0 ? 0 : 1;
}
