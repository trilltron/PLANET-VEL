#include "PluginProcessor.h"
#include "UI/PluginEditor.h"

namespace PV
{
juce::AudioProcessor::BusesProperties PlanetVelProcessor::makeBuses()
{
    auto b = BusesProperties().withOutput ("Main", juce::AudioChannelSet::stereo(), true);
    for (int l = 0; l < kLayers; ++l)          // 4 layer outputs routable to the FL mixer (off by default)
        b = b.withOutput (juce::String ("Layer ") + juce::String::charToString ((juce::juce_wchar) ('A' + l)), juce::AudioChannelSet::stereo(), false);
    return b;
}

PlanetVelProcessor::PlanetVelProcessor()
    : juce::AudioProcessor (makeBuses()),
      apvts (*this, nullptr, "PLANETVEL", createLayout())
{
    for (int i = 0; i < NUM_PARAMS; ++i)
    {
        const auto& d = defs()[(size_t) i];
        params[(size_t) i] = static_cast<juce::RangedAudioParameter*> (apvts.getParameter (d.id));
        raw[(size_t) i] = apvts.getRawParameterValue (d.id);
        ranges[(size_t) i] = params[(size_t) i]->getNormalisableRange();
    }
    for (auto& c : ccMap) c.store (-1);
    for (auto& s : arpSteps) s.store (ArpStep().pack());
    for (auto& m : layerMeter) m.store (0.0f);
    meta = Preset::makeInit();
}

PlanetVelProcessor::~PlanetVelProcessor() { cancelPendingUpdate(); }

bool PlanetVelProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    if (l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    for (int i = 1; i < l.outputBuses.size(); ++i)
        if (! l.outputBuses[i].isDisabled() && l.outputBuses[i] != juce::AudioChannelSet::stereo()) return false;
    return l.inputBuses.isEmpty() || l.getMainInputChannelSet().isDisabled();
}

void PlanetVelProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    Library::get().ensureStarted();           // one-time ~230 ms table build (not in the constructor: keeps plugin scans instant)
    sr = sampleRate; maxBlock = juce::jmax (samplesPerBlock, 16);
    synth.prepare (sr); fx.prepare (sr, kChunk); arp.prepare (sr);
    for (int l = 0; l < kLayers; ++l) { layL[(size_t) l].assign ((size_t) kChunk + kSubBlock, 0.0f); layR[(size_t) l].assign ((size_t) kChunk + kSubBlock, 0.0f); }
    sendL.assign ((size_t) kChunk, 0.0f); sendR.assign ((size_t) kChunk, 0.0f);
    arpOut.ensureSize (16384); chunkMidi.ensureSize (16384);
    lfo1.rng = Rng (11); lfo2.rng = Rng (23); lfo1.nxt = lfo1.rng.bi(); lfo2.nxt = lfo2.rng.bi();
    setLatencySamples (0);
}

// ------------------------------------------------------------------------------------------- params
void PlanetVelProcessor::setParamNormalised (int i, float n, bool gesture)
{
    auto* p = params[(size_t) i];
    if (gesture) p->beginChangeGesture();
    p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, n));
    if (gesture) p->endChangeGesture();
}

void PlanetVelProcessor::applyMods (const float* b, float* out)
{
    std::memcpy (out, b, sizeof (float) * NUM_PARAMS);
    for (auto& r : routes)
    {
        const int s = r.src.load (std::memory_order_relaxed), d = r.dest.load (std::memory_order_relaxed);
        if (s == SRC_NONE || d < 0) continue;
        const float amt = r.amount.load (std::memory_order_relaxed), v = macroSrc[s];
        if (amt == 0.0f || v == 0.0f || defs()[(size_t) d].kind != Kind::Float) continue;
        const auto& rg = ranges[(size_t) d];
        const float n = juce::jlimit (0.0f, 1.0f, rg.convertTo0to1 (out[d]) + amt * v);
        out[d] = rg.convertFrom0to1 (n);
    }
}

void PlanetVelProcessor::buildLayerParams (const float* s, LayerParams (&out)[kLayers])
{
    bool anySolo = false;
    for (int l = 0; l < kLayers; ++l) anySolo |= s[lp (l, L_SOLO)] > 0.5f && s[lp (l, L_MUTE)] < 0.5f;
    for (int l = 0; l < kLayers; ++l)
    {
        auto& o = out[l]; auto V = [&] (int i) { return s[lp (l, i)]; };
        o.engine = (int) V (L_ENGINE); o.type = (int) V (L_TYPE); o.ftype = (int) V (L_FTYPE);
        o.enabled = V (L_MUTE) < 0.5f && (! anySolo || V (L_SOLO) > 0.5f);
        o.level = V (L_LEVEL); o.level *= o.level; o.pan = V (L_PAN); o.tune = V (L_TUNE); o.start = V (L_START);
        o.cutoff = V (L_CUTOFF); o.reso = V (L_RESO); o.drive = V (L_DRIVE); o.keytrk = V (L_KEYTRK); o.fenv = V (L_FENV); o.lfo = V (L_LFO);
        o.env = Env::make (V (L_ATK), V (L_DEC), V (L_SUS), V (L_REL), sr);
        o.spread = V (L_SPREAD); o.send = V (L_SEND); o.keylo = (int) V (L_KEYLO); o.keyhi = (int) V (L_KEYHI);
        o.uni = (int) V (L_UNI); o.detune = V (L_DETUNE);
        for (int k = 0; k < 6; ++k) o.p[k] = V (L_P1 + k);
        if (! o.enabled) o.level = 0.0f;
    }
}

// ------------------------------------------------------------------------------------------- audio
void PlanetVelProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    const auto t0 = juce::Time::getHighResolutionTicks();
    const int total = buffer.getNumSamples();
    buffer.clear();

    // transport
    double bpm = 120.0, ppq = 0.0; bool playing = false, ppqValid = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = juce::jlimit (20.0, 400.0, *b);
            if (auto q = pos->getPpqPosition()) { ppq = *q; ppqValid = true; }
            playing = pos->getIsPlaying();
        }
    lastBpm.store (bpm);
    transportBpm = bpm; transportPlaying = playing;

    // on-screen keyboard / wheels -> MIDI
    {
        int s1, z1, s2, z2; uiFifo.prepareToRead (uiFifo.getNumReady(), s1, z1, s2, z2);
        auto push = [&] (int s, int z) { for (int i = 0; i < z; ++i) { const int w = uiFifoData[(size_t) (s + i)]; midi.addEvent (juce::MidiMessage ((w >> 16) & 0xFF, (w >> 8) & 0xFF, w & 0xFF), 0); } };
        push (s1, z1); push (s2, z2); uiFifo.finishedRead (z1 + z2);
    }

    keyboardState.processNextMidiBuffer (midi, 0, total, true);   // on-screen keyboard notes in, incoming notes -> key lights

    const int nAux = juce::jmin (kLayers, getBusCount (false) - 1);
    bool auxOn[kLayers] = { false, false, false, false };
    float* auxL[kLayers] = { nullptr, nullptr, nullptr, nullptr }; float* auxR[kLayers] = { nullptr, nullptr, nullptr, nullptr };
    for (int l = 0; l < nAux; ++l)
        if (auto* b = getBus (false, 1 + l); b != nullptr && b->isEnabled() && b->getNumberOfChannels() >= 2)
        { auto bb = getBusBuffer (buffer, false, 1 + l); auxOn[l] = true; auxL[l] = bb.getWritePointer (0); auxR[l] = bb.getWritePointer (1); }
    auto mainBuf = getBusBuffer (buffer, false, 0);
    float* mL = mainBuf.getWritePointer (0); float* mR = mainBuf.getNumChannels() > 1 ? mainBuf.getWritePointer (1) : mL;

    if (total <= kChunk) processChunk (mL, mR, auxL, auxR, auxOn, total, midi, ppq, ppqValid);
    else
    {
        // oversized host buffers: split into chunks and re-time the MIDI events
        const double beatsPerSample = bpm / 60.0 / sr;
        for (int start = 0; start < total; start += kChunk)
        {
            const int n = juce::jmin (kChunk, total - start);
            chunkMidi.clear();
            for (const auto meta2 : midi) if (meta2.samplePosition >= start && meta2.samplePosition < start + n) chunkMidi.addEvent (meta2.getMessage(), meta2.samplePosition - start);
            float* aL[kLayers]; float* aR[kLayers];
            for (int l = 0; l < kLayers; ++l) { aL[l] = auxL[l] ? auxL[l] + start : nullptr; aR[l] = auxR[l] ? auxR[l] + start : nullptr; }
            processChunk (mL + start, mR + start, aL, aR, auxOn, n, chunkMidi, ppq + start * beatsPerSample, ppqValid);
        }
    }
    midi.clear();

    // meters
    float pl = 0, pr = 0; for (int i = 0; i < total; ++i) { pl = juce::jmax (pl, std::abs (mL[i])); pr = juce::jmax (pr, std::abs (mR[i])); }
    peakL.store (juce::jmax (pl, peakL.load() * 0.9f)); peakR.store (juce::jmax (pr, peakR.load() * 0.9f));
    int sp = scopePos.load(); for (int i = 0; i < total; i += 2) { scope[(size_t) sp] = 0.5f * (mL[i] + mR[i]); sp = (sp + 1) & 2047; } scopePos.store (sp);
    voiceCount.store (synth.activeVoices());
    const double used = (double) (juce::Time::getHighResolutionTicks() - t0) / (double) juce::Time::getHighResolutionTicksPerSecond();
    const float load = (float) (used / ((double) total / sr));
    cpuLoad.store (cpuLoad.load() * 0.92f + load * 0.08f);
}

void PlanetVelProcessor::processChunk (float* mainL, float* mainR, float* const* auxL, float* const* auxR, const bool* auxOn, int n,
                                       juce::MidiBuffer& midi, double ppq, bool ppqValid)
{
    // ---- base parameter values
    for (int i = 0; i < NUM_PARAMS; ++i) base[i] = raw[(size_t) i]->load (std::memory_order_relaxed);
    const int quality = settings.quality.load();
    synth.setPolyphony (settings.polyphony.load());
    const double bpm = transportBpm;

    // ---- arpeggiator
    ArpParams ap; ap.on = base[G_ARP_ON] > 0.5f; ap.pattern = (int) base[G_ARP_PAT]; ap.rate = (int) base[G_ARP_RATE]; ap.octaves = (int) std::round (base[G_ARP_OCT]);
    ap.gate = base[G_ARP_GATE]; ap.length = arpLength.load();
    ArpPattern& pat = arpSteps;
    arpOut.clear();
    arp.process (midi, arpOut, n, ap, pat, bpm, transportPlaying, ppq, ppqValid);

    for (int l = 0; l < kLayers; ++l) { std::fill (layL[(size_t) l].begin(), layL[(size_t) l].begin() + n, 0.0f); std::fill (layR[(size_t) l].begin(), layR[(size_t) l].begin() + n, 0.0f); }

    RenderCtx ctx; ctx.sr = sr; ctx.bpm = bpm; ctx.quality = quality; ctx.tuning = &tuning;

    // ---- event driven sub-block loop
    auto it = arpOut.begin(); const auto end = arpOut.end();
    int pos = 0;
    while (pos < n)
    {
        // modulation sources for this sub-block
        for (int i = 0; i < kNumMacros; ++i) macroSrc[SRC_M1 + i] = base[G_M1 + i];
        macroSrc[SRC_MODWHEEL] = modWheel.load(); macroSrc[SRC_AFTERTOUCH] = aftertouch.load();
        const double dt = (double) kSubBlock / sr;
        macroSrc[SRC_LFO1] = lfo1.run (base[G_LFO1_RATE], (int) base[G_LFO1_SHAPE], dt);
        macroSrc[SRC_LFO2] = lfo2.run (base[G_LFO2_RATE], (int) base[G_LFO2_SHAPE], dt);
        applyMods (base, snap);
        ctx.lfo1 = macroSrc[SRC_LFO1]; ctx.lfo2 = macroSrc[SRC_LFO2]; ctx.bend = bendSemis;
        buildLayerParams (snap, curLP);

        // orbit blend
        const int om = (int) snap[G_ORBIT_MODE];
        float ox = snap[G_ORBIT_X], oy = snap[G_ORBIT_Y];
        if (om == 0) { orbitPhase += kTwoPi * snap[G_ORBIT_RATE] * (float) dt; if (orbitPhase > kTwoPi) orbitPhase -= kTwoPi; ox += snap[G_ORBIT_RADIUS] * std::cos (orbitPhase); oy += snap[G_ORBIT_RADIUS] * std::sin (orbitPhase); }
        ox = juce::jlimit (0.0f, 1.0f, ox); oy = juce::jlimit (0.0f, 1.0f, oy);
        orbitXNow.store (ox); orbitYNow.store (oy);
        const float w[4] = { (1 - ox) * (1 - oy), ox * (1 - oy), (1 - ox) * oy, ox * oy };
        for (int l = 0; l < kLayers; ++l) curLP[l].orbitW = om == 1 ? 1.0f : std::sqrt (w[l]);

        // MIDI events at or before `pos`
        while (it != end && (*it).samplePosition <= pos)
        {
            const auto m = (*it).getMessage(); ++it;
            if (m.isNoteOn()) synth.noteOn (m.getNoteNumber(), m.getFloatVelocity(), curLP);
            else if (m.isNoteOff()) synth.noteOff (m.getNoteNumber());
            else if (m.isAllNotesOff() || m.isAllSoundOff()) synth.allNotesOff (m.isAllSoundOff());
            else if (m.isPitchWheel()) bendSemis = ((float) m.getPitchWheelValue() - 8192.0f) / 8192.0f * (float) settings.pitchBendRange.load();
            else if (m.isChannelPressure()) aftertouch.store ((float) m.getChannelPressureValue() / 127.0f);
            else if (m.isAftertouch()) aftertouch.store ((float) m.getAfterTouchValue() / 127.0f);
            else if (m.isController())
            {
                const int cc = m.getControllerNumber(), val = m.getControllerValue();
                if (cc == 1) modWheel.store ((float) val / 127.0f);
                else if (cc == 64) synth.sustain (val >= 64);
                else if (cc == 120 || cc == 123) synth.allNotesOff (cc == 120);
                else
                {
                    const int lt = learnTarget.load();
                    if (lt >= 0) { ccMap[(size_t) cc].store (lt); learnTarget.store (-1); }
                    const int target = ccMap[(size_t) cc].load();
                    if (target >= 0 && ccFifo.getFreeSpace() > 0)
                    { int s1, z1, s2, z2; ccFifo.prepareToWrite (1, s1, z1, s2, z2); ccFifoData[(size_t) s1] = { target, (float) val / 127.0f }; ccFifo.finishedWrite (1); triggerAsyncUpdate(); }
                }
            }
        }
        int next = n; if (it != end) next = juce::jmin (n, (*it).samplePosition);
        const int len = juce::jmax (1, juce::jmin (kSubBlock, next - pos));
        float* oL[kLayers]; float* oR[kLayers];
        for (int l = 0; l < kLayers; ++l) { oL[l] = layL[(size_t) l].data() + pos; oR[l] = layR[(size_t) l].data() + pos; }
        synth.render (curLP, ctx, oL, oR, len);
        pos += len;
    }

    // ---- layer routing: aux outputs or main (+ reverb send bus)
    std::fill (sendL.begin(), sendL.begin() + n, 0.0f); std::fill (sendR.begin(), sendR.begin() + n, 0.0f);
    for (int l = 0; l < kLayers; ++l)
    {
        const float* a = layL[(size_t) l].data(); const float* b = layR[(size_t) l].data();
        float act = 0; for (int i = 0; i < n; i += 8) act = juce::jmax (act, std::abs (a[i]));
        layerMeter[(size_t) l].store (juce::jmax (act, layerMeter[(size_t) l].load() * 0.9f));
        if (auxOn[l]) { for (int i = 0; i < n; ++i) { auxL[l][i] += a[i]; auxR[l][i] += b[i]; } continue; }
        const float snd = curLP[l].send;
        for (int i = 0; i < n; ++i) { mainL[i] += a[i]; mainR[i] += b[i]; sendL[(size_t) i] += a[i] * snd; sendR[(size_t) i] += b[i] * snd; }
    }

    // ---- FX rack (parameters from the last modulated snapshot)
    FxParams fp; const float* s = snap;
    fp.mfOn = s[G_MF_ON] > 0.5f; fp.mfType = (int) s[G_MF_TYPE]; fp.mfFreq = s[G_MF_FREQ]; fp.mfRes = s[G_MF_RES];
    fp.dlyOn = s[G_DLY_ON] > 0.5f; fp.dlySync = s[G_DLY_SYNC] > 0.5f; fp.dlyDiv = (int) s[G_DLY_DIV]; fp.dlyTime = s[G_DLY_TIME]; fp.dlyFb = s[G_DLY_FB]; fp.dlyMix = s[G_DLY_MIX];
    fp.revOn = s[G_REV_ON] > 0.5f; fp.revType = (int) s[G_REV_TYPE]; fp.revSize = s[G_REV_SIZE]; fp.revWet = s[G_REV_WET];
    fp.glOn = s[G_GL_ON] > 0.5f; fp.glType = (int) s[G_GL_TYPE]; fp.glRate = (int) s[G_GL_RATE]; fp.glMix = s[G_GL_MIX]; fp.glProb = s[G_GL_PROB];
    fp.limit = s[G_OUT_LIMIT] > 0.5f; fp.mono = (int) s[G_OUT_MONO] == 1; fp.volDb = s[G_OUT_VOL];
    fp.choMix = s[G_CHO_MIX]; fp.choRate = s[G_CHO_RATE]; fp.choDepth = s[G_CHO_DEPTH]; fp.drive = s[G_DRIVE]; fp.width = s[G_WIDTH];
    fp.eqLo = s[G_EQ_LO]; fp.eqMid = s[G_EQ_MID]; fp.eqHi = s[G_EQ_HI]; fp.master = s[G_MASTER];
    fp.bpm = bpm; fp.ppq = ppq; fp.playing = transportPlaying; fp.ppqValid = ppqValid; fp.quality = quality;
    fx.process (mainL, mainR, sendL.data(), sendR.data(), n, fp);
}

// --------------------------------------------------------------------------------------- UI helpers
void PlanetVelProcessor::uiMidi (const juce::MidiMessage& m)
{
    if (m.getRawDataSize() < 1 || uiFifo.getFreeSpace() < 1) return;
    const auto* d = m.getRawData(); const int sz = m.getRawDataSize();
    const int w = (d[0] << 16) | ((sz > 1 ? d[1] : 0) << 8) | (sz > 2 ? d[2] : 0);
    int s1, z1, s2, z2; uiFifo.prepareToWrite (1, s1, z1, s2, z2); uiFifoData[(size_t) s1] = w; uiFifo.finishedWrite (1);
}

void PlanetVelProcessor::handleAsyncUpdate()
{
    int s1, z1, s2, z2; ccFifo.prepareToRead (ccFifo.getNumReady(), s1, z1, s2, z2);
    auto apply = [&] (int s, int z) { for (int i = 0; i < z; ++i) { const auto& e = ccFifoData[(size_t) (s + i)]; setParamNormalised (e.first, e.second, false); } };
    apply (s1, z1); apply (s2, z2); ccFifo.finishedRead (z1 + z2);
}

bool PlanetVelProcessor::loadUserSample (const juce::File& f)
{
    if (! Library::get().loadUserSample (f)) return false;
    settings.userSamplePath = f.getFullPathName(); return true;
}
bool PlanetVelProcessor::loadTuning (const juce::File& f)
{
    Tuning t; if (! t.loadScl (f)) return false;
    tuning = t; settings.tuningFile = f.getFullPathName(); return true;
}

// ------------------------------------------------------------------------------------------ presets
void PlanetVelProcessor::loadPreset (const Preset& p, int programIndex)
{
    for (int i = 0; i < NUM_PARAMS; ++i)
        if (std::abs (params[(size_t) i]->getValue() - p.norm[(size_t) i]) > 1.0e-6f) params[(size_t) i]->setValueNotifyingHost (p.norm[(size_t) i]);
    for (int i = 0; i < kRouteSlots; ++i)
    { routes[(size_t) i].src.store (p.routes[(size_t) i].src); routes[(size_t) i].dest.store (p.routes[(size_t) i].dest); routes[(size_t) i].amount.store (p.routes[(size_t) i].amount); }
    for (int i = 0; i < kArpSteps; ++i) arpSteps[(size_t) i].store (p.arp[(size_t) i]);
    arpLength.store (p.arpLength);
    { const juce::ScopedLock l (metaLock); meta = p; }
    if (programIndex >= 0) currentProgram = programIndex;
    synth.allNotesOff (false);
    sendChangeMessage();
}

void PlanetVelProcessor::setCurrentProgram (int i)
{
    const auto& bank = PresetBank::factory();
    if (i >= 0 && i < (int) bank.size()) loadPreset (bank[(size_t) i], i);
}

Preset PlanetVelProcessor::captureCurrent() const
{
    Preset p; { const juce::ScopedLock l (metaLock); p = meta; }
    for (int i = 0; i < NUM_PARAMS; ++i) p.norm[(size_t) i] = params[(size_t) i]->getValue();
    for (int i = 0; i < kRouteSlots; ++i) p.routes[(size_t) i] = { routes[(size_t) i].src.load(), routes[(size_t) i].dest.load(), routes[(size_t) i].amount.load() };
    for (int i = 0; i < kArpSteps; ++i) p.arp[(size_t) i] = arpSteps[(size_t) i].load();
    p.arpLength = arpLength.load(); p.updateEngineMask(); return p;
}

void PlanetVelProcessor::saveUserPreset (const juce::String& name)
{
    auto p = captureCurrent(); p.name = name.isEmpty() ? "User Preset" : name; p.category = "User"; p.author = "User"; p.factory = false;
    PresetBank::saveUser (p); { const juce::ScopedLock l (metaLock); meta.name = p.name; meta.category = "User"; } sendChangeMessage();
}

void PlanetVelProcessor::toggleAB()
{
    abState[(size_t) abCurrent].resize (NUM_PARAMS);
    for (int i = 0; i < NUM_PARAMS; ++i) abState[(size_t) abCurrent][(size_t) i] = params[(size_t) i]->getValue();
    abCurrent = 1 - abCurrent;
    if (abState[(size_t) abCurrent].size() == (size_t) NUM_PARAMS)
        for (int i = 0; i < NUM_PARAMS; ++i) params[(size_t) i]->setValueNotifyingHost (abState[(size_t) abCurrent][(size_t) i]);
    sendChangeMessage();
}

static juce::File globalFile() { return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Trill Tron").getChildFile ("Planet Vel").getChildFile ("settings.xml"); }
void PlanetVelProcessor::loadGlobalSettings()
{
    if (globalLoaded) return;
    globalLoaded = true;
    if (auto xml = juce::XmlDocument::parse (globalFile()))
    {
        favorites.addTokens (xml->getStringAttribute ("favorites"), "|", ""); favorites.removeEmptyStrings();
        if (editorWidth == 1600 && xml->hasAttribute ("w")) { editorWidth = xml->getIntAttribute ("w", 1600); editorHeight = xml->getIntAttribute ("h", 1000); }
        settings.libraryPath = xml->getStringAttribute ("library");
    }
}
void PlanetVelProcessor::saveGlobalSettings()
{
    juce::XmlElement x ("PVGLOBAL"); x.setAttribute ("favorites", favorites.joinIntoString ("|")); x.setAttribute ("w", editorWidth); x.setAttribute ("h", editorHeight); x.setAttribute ("library", settings.libraryPath);
    auto f = globalFile(); f.getParentDirectory().createDirectory(); x.writeTo (f);
}
void PlanetVelProcessor::toggleFavorite (const juce::String& n) { if (favorites.contains (n)) favorites.removeString (n); else favorites.add (n); saveGlobalSettings(); }

// ----------------------------------------------------------------------------------------- state
// Versioned XML. Parameters are stored by stable string ID, so a project saved by any version
// (FL 20 ... FL 2025) restores correctly even when later versions append parameters.
void PlanetVelProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState(); std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml == nullptr) return;
    xml->setAttribute ("pvStateVersion", 1); xml->setAttribute ("program", currentProgram);
    { const juce::ScopedLock l (metaLock);
      xml->setAttribute ("presetName", meta.name); xml->setAttribute ("presetCategory", meta.category); xml->setAttribute ("presetAuthor", meta.author); xml->setAttribute ("presetTags", meta.tags);
      auto* mn = xml->createNewChildElement ("MACRONAMES"); for (int i = 0; i < kNumMacros; ++i) mn->setAttribute ("m" + juce::String (i), meta.macroNames[(size_t) i]); }
    auto* rs = xml->createNewChildElement ("ROUTES");
    for (auto& r : routes) { const int s = r.src.load(), d = r.dest.load(); if (s == SRC_NONE || d < 0) continue; auto* e = rs->createNewChildElement ("R"); e->setAttribute ("src", s); e->setAttribute ("dest", defs()[(size_t) d].id); e->setAttribute ("amt", (double) r.amount.load()); }
    auto* ar = xml->createNewChildElement ("ARP"); ar->setAttribute ("length", arpLength.load());
    for (int i = 0; i < kArpSteps; ++i) ar->setAttribute ("s" + juce::String (i), juce::String ((juce::int64) arpSteps[(size_t) i].load()));
    auto* st = xml->createNewChildElement ("SETTINGS");
    st->setAttribute ("quality", settings.quality.load()); st->setAttribute ("poly", settings.polyphony.load()); st->setAttribute ("pb", settings.pitchBendRange.load());
    st->setAttribute ("theme", settings.theme.load()); st->setAttribute ("anim", settings.animation.load()); st->setAttribute ("mpe", settings.mpe.load());
    st->setAttribute ("sample", settings.userSamplePath); st->setAttribute ("tuning", settings.tuningFile); st->setAttribute ("library", settings.libraryPath);
    st->setAttribute ("w", editorWidth); st->setAttribute ("h", editorHeight); st->setAttribute ("scale", uiScalePercent);
    auto* cc = xml->createNewChildElement ("CCMAP");
    for (int c = 0; c < 128; ++c) { const int t = ccMap[(size_t) c].load(); if (t >= 0) { auto* e = cc->createNewChildElement ("C"); e->setAttribute ("cc", c); e->setAttribute ("id", defs()[(size_t) t].id); } }
    copyXmlToBinary (*xml, dest);
}

void PlanetVelProcessor::setStateInformation (const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, size));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    currentProgram = xml->getIntAttribute ("program", 0);
    { const juce::ScopedLock l (metaLock);
      meta.name = xml->getStringAttribute ("presetName", "Init"); meta.category = xml->getStringAttribute ("presetCategory", "User");
      meta.author = xml->getStringAttribute ("presetAuthor", ""); meta.tags = xml->getStringAttribute ("presetTags", "");
      if (auto* mn = xml->getChildByName ("MACRONAMES")) for (int i = 0; i < kNumMacros; ++i) meta.macroNames[(size_t) i] = mn->getStringAttribute ("m" + juce::String (i), kMacroNames[i]); }
    for (auto& r : routes) { r.src.store (0); r.dest.store (-1); r.amount.store (0); }
    if (auto* rs = xml->getChildByName ("ROUTES"))
    {
        int k = 0;
        for (auto* e : rs->getChildIterator()) { if (k >= kRouteSlots) break; const int d = findParamById (e->getStringAttribute ("dest")); if (d < 0) continue; routes[(size_t) k].src.store (e->getIntAttribute ("src")); routes[(size_t) k].dest.store (d); routes[(size_t) k].amount.store ((float) e->getDoubleAttribute ("amt")); ++k; }
    }
    if (auto* ar = xml->getChildByName ("ARP"))
    {
        arpLength.store (ar->getIntAttribute ("length", 16));
        for (int i = 0; i < kArpSteps; ++i) arpSteps[(size_t) i].store (ar->hasAttribute ("s" + juce::String (i)) ? (uint32_t) ar->getStringAttribute ("s" + juce::String (i)).getLargeIntValue() : ArpStep().pack());
    }
    if (auto* st = xml->getChildByName ("SETTINGS"))
    {
        settings.quality = st->getIntAttribute ("quality", 1); settings.polyphony = st->getIntAttribute ("poly", 24); settings.pitchBendRange = st->getIntAttribute ("pb", 2);
        settings.theme = st->getIntAttribute ("theme", 0); settings.animation = st->getBoolAttribute ("anim", true); settings.mpe = st->getBoolAttribute ("mpe", false);
        settings.libraryPath = st->getStringAttribute ("library");
        editorWidth = st->getIntAttribute ("w", 1600); editorHeight = st->getIntAttribute ("h", 1000); uiScalePercent = st->getIntAttribute ("scale", 100);
        const auto sample = st->getStringAttribute ("sample"); if (sample.isNotEmpty() && juce::File (sample).existsAsFile()) loadUserSample (juce::File (sample));
        const auto tun = st->getStringAttribute ("tuning"); if (tun.isNotEmpty() && juce::File (tun).existsAsFile()) loadTuning (juce::File (tun));
    }
    for (auto& c : ccMap) c.store (-1);
    if (auto* cc = xml->getChildByName ("CCMAP")) for (auto* e : cc->getChildIterator()) { const int p = findParamById (e->getStringAttribute ("id")); const int c = e->getIntAttribute ("cc", -1); if (p >= 0 && c >= 0 && c < 128) ccMap[(size_t) c].store (p); }
    sendChangeMessage();
}

juce::AudioProcessorEditor* PlanetVelProcessor::createEditor() { return new PlanetVelEditor (*this); }
} // namespace PV

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PV::PlanetVelProcessor(); }

#include "Vst2Wrapper.inl"
