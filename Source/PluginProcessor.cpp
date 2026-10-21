#include "PluginProcessor.h"
#if ! LIQ_HEADLESS
 #include "PluginEditor.h"
#endif

using namespace liq;

LiquidationProcessor::LiquidationProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "LiquidationState", createParameterLayout()),
      presets (apvts)
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rawPtr[rp->getParameterID()] = apvts.getRawParameterValue (rp->getParameterID());

    const auto& ids = modTargetIds();
    for (int i = 0; i < ids.size(); ++i)
    {
        targetIndex[ids[i]] = i;
        targetParams.push_back (apvts.getParameter (ids[i]));
    }

    presets.onLoadState = [this] (const juce::ValueTree& t) { applyFullState (t); };
    presets.onBeforeFactoryLoad = [this]
    {
        for (auto& s : modSystem.seqSteps)
            for (int i = 0; i < numSeqSteps; ++i)
                s[(size_t) i] = (i % 4 == 0) ? 1.0f : ((i % 3 == 0) ? 0.6f : (float) ((i * 7) % 11) / 11.0f);
    };
    modSystem.prepare (48000.0);
    presets.load (0);   // fresh instances start from "Init" (macros pre-routed)
}

bool LiquidationProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in = layouts.getMainInputChannelSet();
    const auto ok = [] (const juce::AudioChannelSet& s)
    { return s == juce::AudioChannelSet::stereo() || s == juce::AudioChannelSet::mono(); };
    // mono -> mono, mono -> stereo, stereo -> stereo (mono tracks are common for kick / bass)
    if (! ok (in) || ! ok (out)) return false;
    if (in == juce::AudioChannelSet::stereo() && out == juce::AudioChannelSet::mono()) return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && ! ok (sc)) return false;
    }
    return true;
}

void LiquidationProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const juce::ScopedLock sl (prepareLock);
    sr = sampleRate;
    engine.prepare (sampleRate, subBlock);
    rack.prepare (sampleRate, subBlock);
    auto steps = modSystem.seqSteps;    // keep edited steps across re-prepare
    modSystem.prepare (sampleRate);
    modSystem.seqSteps = steps;
    animator.prepare (sampleRate);
    dryBuffer.setSize (2, subBlock, false, true, true);
    wetBuffer.setSize (2, subBlock, false, true, true);
    juce::ignoreUnused (samplesPerBlock);

    morph.prepare (sampleRate);
    modeler.prepare (sampleRate);
    limiter.prepare (sampleRate);
    mixReverb.prepare (sampleRate);
    const int align = morph.latency() + grainLatency;
    for (auto& d : alignMain) d.prepare (align);
    for (auto& d : alignAux) d.prepare (align);
    for (auto& d : alignModel) d.prepare (align);
    for (auto& d : grainBypass) d.prepare (grainLatency);
    transients.prepare (sampleRate, align);
    followDetector.prepare (sampleRate, (int64_t) sampleRate * 2);
    followPos = 0;
    setLatencySamples (totalLatency());
}

// ----------------------------------------------------------------------------
float LiquidationProcessor::raw (const juce::String& pid) const
{
    auto it = rawPtr.find (pid);
    return it != rawPtr.end() ? it->second->load (std::memory_order_relaxed) : 0.0f;
}

float LiquidationProcessor::mod (const juce::String& pid) const
{
    const float v = raw (pid);
    auto it = targetIndex.find (pid);
    if (it == targetIndex.end()) return v;
    const float off = modSystem.offsetFor (it->second);
    if (off == 0.0f) return v;
    auto* p = targetParams[(size_t) it->second];
    const float n = juce::jlimit (0.0f, 1.0f, p->convertTo0to1 (v) + off);
    return p->convertFrom0to1 (n);
}

float LiquidationProcessor::modulatedValue (const juce::String& pid) const { return mod (pid); }

Scale LiquidationProcessor::currentScale() const
{
    Scale s;
    const int type = choice (id::scaleType);
    const int root = choice (id::scaleRoot);
    std::array<bool, 12> mask {};
    if (type == scaleTypeNames().size() - 1)
        for (int pc = 0; pc < 12; ++pc) mask[(size_t) pc] = flag (id::scaleNote (pc));
    else
        mask = Scale::presetMask (type, root);
    s.set (mask, root, raw (id::globalTune));
    return s;
}

void LiquidationProcessor::buildEngineParams (EngineParams& p, double ppq, bool playing, double bpm)
{
    p.bpm = bpm; p.ppq = ppq; p.playing = playing;
    p.scale = currentScale();

    p.rateMode = choice (id::rateMode);
    p.grainRate = mod (id::grainRate);
    p.sizeMs = mod (id::grainSize);
    p.sizeRatio = mod (id::sizeRatio);
    p.sizeLink = flag (id::sizeLink);
    p.winShape = mod (id::winShape);
    p.winSkew = mod (id::winSkew);
    p.jitter = mod (id::jitter);

    p.playMode = choice (id::playMode);
    p.stretchMode = choice (id::stretchMode);
    p.stretchRetrig = choice (id::stretchRetrig);
    p.stretch = mod (id::stretch);
    p.stretchSync = flag (id::stretchSync);
    p.stretchSens = mod (id::stretchSens);
    p.stretchActive = flag (id::stretchActive);
    p.scrubPos = mod (id::scrubPos);
    p.scrubMode = choice (id::scrubMode);
    p.loopLength = choice (id::loopLength);
    p.recHold = flag (id::recHold);
    p.reverse = flag (id::reverse);
    p.revProb = mod (id::revProb);
    p.spray = mod (id::spray);
    p.blur = mod (id::blur);
    p.levelDb = mod (id::grainLevel);

    p.pitch = mod (id::pitch);
    p.pitchMode = choice (id::pitchMode);
    p.detune = mod (id::detune);
    p.retune = choice (id::retune);
    p.chordMode = choice (id::chordMode);
    p.arpRand = mod (id::arpRand);
    p.chordSpread = mod (id::chordSpread);
    for (int i = 0; i < numIntervals; ++i)
    {
        p.intervals[(size_t) i] = (int) std::lround (raw (id::interval (i + 1)));
        p.intervalOn[(size_t) i] = flag (id::intervalOn (i + 1));
    }

    p.fltOn = flag (id::fltOn);
    auto& f = p.filter;
    f.mode = choice (id::fltMode);
    f.cutoff = mod (id::fltCutoff);
    f.scaleLock = flag (id::fltScale);
    f.random = flag (id::fltRandom);
    f.modDepth = mod (id::fltModDepth);
    f.modRate = mod (id::fltModRate);
    f.reso = mod (id::fltReso);
    f.drive = mod (id::fltDrive);
    f.morph = mod (id::fltMorph);
    f.damp = mod (id::fltDamp);
    f.smooth = mod (id::fltSmooth);
    f.bits = mod (id::fltBits);
    f.mix = mod (id::fltMix);

    p.imgOn = flag (id::imgOn);
    p.spreadMode = choice (id::spreadMode);
    p.smoothSpread = flag (id::smoothSpread);
    p.spread = mod (id::spread);
    p.pan = mod (id::pan);
    p.imager.dimension = mod (id::dimension);
    p.imager.dimSize = mod (id::dimSize);
    p.imager.monoLows = flag (id::monoLows);
    p.imager.monoFreq = mod (id::monoFreq);
    p.imager.width = mod (id::width);
    p.imager.safeWidth = flag (id::safeWidth);

    p.dlyOn = flag (id::dlyOn);
    p.dlyPingPong = flag (id::dlyPingPong);
    p.dlySwap = flag (id::dlySwap);
    p.dlySync = flag (id::dlySync);
    p.dlyTimeMs = mod (id::dlyTime);
    p.dlyDiv = choice (id::dlyDiv);
    p.dlyTaps = (int) std::lround (raw (id::dlyTaps));
    p.dlyPattern = choice (id::dlyPattern);
    p.dlyShift = mod (id::dlyShift);
    p.delay.feedback = mod (id::dlyFeedback);
    p.delay.space = mod (id::dlySpace);
    p.delay.lowpass = mod (id::dlyLP);
    p.delay.highpass = mod (id::dlyHP);
    p.delay.mix = mod (id::dlyMix);
}

// ----------------------------------------------------------------------------
void LiquidationProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    // ---- transport
    double bpm = 120.0, ppq = 0.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto q = pos->getPpqPosition()) ppq = *q;
            playing = pos->getIsPlaying();
        }
    }
    bpm = juce::jlimit (20.0, 400.0, bpm);
    hostBpm.store (bpm, std::memory_order_relaxed);
    if (! playing) ppq = internalPpq;
    internalPpq = ppq + numSamples * bpm / 60.0 / sr;

    // ---- midi
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn()) engine.noteOn (m.getNoteNumber());
        else if (m.isNoteOff()) engine.noteOff (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            for (int nn = 0; nn < 128; ++nn) engine.noteOff (nn);
    }

    // ---- buses (read every input of a sub-block before writing its output: channels may alias)
    auto mainIn = getBusBuffer (buffer, true, 0);
    auto mainOut = getBusBuffer (buffer, false, 0);
    const bool scEnabled = getBusCount (true) > 1 && getBus (true, 1) != nullptr && getBus (true, 1)->isEnabled()
                           && getChannelCountOfBus (true, 1) > 0;
    juce::AudioBuffer<float> scIn;
    if (scEnabled) scIn = getBusBuffer (buffer, true, 1);
    const int numMainIn = mainIn.getNumChannels(), numMainOut = mainOut.getNumChannels();
    const int numSc = scEnabled ? scIn.getNumChannels() : 0;
    if (numMainOut == 0) return;

    const float inGain = juce::Decibels::decibelsToGain (raw (id::inputGain));

    // ---- modulator / animator settings (block rate)
    std::array<ModulatorParams, numModulators> mp;
    for (int m = 1; m <= numModulators; ++m)
    {
        auto& q = mp[(size_t) m - 1];
        q.type = choice (id::mod (m, "Type"));
        q.rateHz = raw (id::mod (m, "Rate"));
        q.sync = flag (id::mod (m, "Sync"));
        q.divBeats = divisions()[(size_t) juce::jlimit (0, (int) divisions().size() - 1, choice (id::mod (m, "Div")))].beats;
        q.shape = raw (id::mod (m, "Shape"));
        q.phase = raw (id::mod (m, "Phase"));
        q.steps = (int) std::lround (raw (id::mod (m, "Steps")));
        q.glide = raw (id::mod (m, "Glide"));
        q.attackMs = raw (id::mod (m, "Atk"));
        q.releaseMs = raw (id::mod (m, "Rel"));
        q.gainDb = raw (id::mod (m, "Gain"));
        q.bpm = bpm;
    }
    std::array<ModSystem::MatrixSlot, numMatrixSlots> matrix;
    for (int s = 1; s <= numMatrixSlots; ++s)
    {
        auto& slot = matrix[(size_t) s - 1];
        slot.src = choice (id::mx (s, "Src"));
        slot.dst = choice (id::mx (s, "Dst"));
        slot.amount = raw (id::mx (s, "Amt"));
    }
    const bool animMorph = choice (id::animTarget) == animToMorph;
    AnimatorParams ap;
    ap.x = animMorph ? raw (id::morphX) : raw (id::macro1);
    ap.y = animMorph ? raw (id::morphY) : raw (id::macro2);
    ap.depth = raw (id::animDepth); ap.drift = raw (id::animDrift);
    ap.mode = choice (id::animMode); ap.cycles = (int) std::lround (raw (id::animCycles));
    ap.warp = raw (id::animWarp); ap.rateHz = raw (id::animRate); ap.sync = flag (id::animSync);
    ap.divBeats = divisions()[(size_t) juce::jlimit (0, (int) divisions().size() - 1, choice (id::animDiv))].beats;

    const int routing = choice (id::routing);
    const bool morphOn = flag (id::morphOn);
    const int morphSrc = choice (id::morphSrc);
    const bool grainOn = flag (id::grainOn);
    const bool limitOn = flag (id::limitOn);
    const bool follow = flag (id::mdlFollow);

    // mixer mute / solo
    std::array<bool, numMixChannels> chOn {};
    {
        bool anySolo = false;
        for (int c = 0; c < numMixChannels; ++c) anySolo = anySolo || flag (id::solo (c));
        for (int c = 0; c < numMixChannels; ++c)
            chOn[(size_t) c] = anySolo ? flag (id::solo (c)) : ! flag (id::mute (c));
    }

    float outPeak = 0.0f, mainPeak = 0.0f, auxPeak = 0.0f;
    float bufMainL[subBlock], bufMainR[subBlock], bufAuxL[subBlock], bufAuxR[subBlock];
    float bufMdlL[subBlock], bufMdlR[subBlock], bufMorL[subBlock], bufMorR[subBlock];
    float alMainL[subBlock], alMainR[subBlock], alAuxL[subBlock], alAuxR[subBlock], alMdlL[subBlock], alMdlR[subBlock];
    float mixL[subBlock], mixR[subBlock], revL[subBlock], revR[subBlock];

    for (int start = 0; start < numSamples; start += subBlock)
    {
        const int n = std::min (subBlock, numSamples - start);
        const double subPpq = ppq + start * bpm / 60.0 / sr;

        // ---- read inputs
        const float auxG = juce::Decibels::decibelsToGain (mod (id::auxGain));
        for (int i = 0; i < n; ++i)
        {
            const float l = numMainIn > 0 ? mainIn.getSample (0, start + i) : 0.0f;
            const float r = numMainIn > 1 ? mainIn.getSample (1, start + i) : l;
            bufMainL[i] = l * inGain; bufMainR[i] = r * inGain;
            const float al = numSc > 0 ? scIn.getSample (0, start + i) : 0.0f;
            const float ar = numSc > 1 ? scIn.getSample (1, start + i) : al;
            bufAuxL[i] = al * auxG; bufAuxR[i] = ar * auxG;
            mainPeak = std::max (mainPeak, std::max (std::abs (bufMainL[i]), std::abs (bufMainR[i])));
            auxPeak = std::max (auxPeak, std::max (std::abs (bufAuxL[i]), std::abs (bufAuxR[i])));
        }

        // ---- modulation
        animator.advance (ap, n, subPpq, bpm, playing);
        const float m1 = animMorph ? raw (id::macro1) : animator.macro1();
        const float m2 = animMorph ? raw (id::macro2) : animator.macro2();
        modSystem.process (mp, matrix, m1, m2, n, subPpq, playing, mainPeak);
        buildEngineParams (engineParams, subPpq, playing, bpm);

        // ---- modeler (only rendered when somebody listens)
        const float gModel = chOn[3] ? mod (id::mixModel) : 0.0f;
        const bool needModel = (morphOn && morphSrc == srcModeler) || gModel > 0.0001f;
        float followMidi = -1.0f;
        if (follow)
        {
            for (int i = 0; i < n; ++i) followDetector.push (0.5f * (bufMainL[i] + bufMainR[i]), followPos++);
            followMidi = followDetector.pitchAt (followPos - followDetector.latency());
            if (followMidi > 0.0f) followMidi -= 12.0f * std::log2 (engineParams.scale.a4 / 440.0f);
        }
        if (needModel)
        {
            ModelerParams mpar;
            mpar.mode = choice (id::mdlMode);
            mpar.octave = (int) std::lround (raw (id::mdlOctave));
            mpar.rateBeats = divisions()[(size_t) juce::jlimit (0, (int) divisions().size() - 1, choice (id::mdlRate))].beats;
            mpar.tone = mod (id::mdlTone);
            mpar.motion = mod (id::mdlMotion);
            mpar.follow = follow;
            modeler.process (bufMdlL, bufMdlR, n, mpar, engineParams.scale, bpm, subPpq, followMidi);
        }
        else
        {
            std::fill (bufMdlL, bufMdlL + n, 0.0f);
            std::fill (bufMdlR, bufMdlR + n, 0.0f);
        }

        // ---- morph + formants (constant latency)
        MorphParams mo;
        mo.morphOn = morphOn;
        const float offX = mod (id::morphX) - raw (id::morphX), offY = mod (id::morphY) - raw (id::morphY);
        mo.x = juce::jlimit (0.0f, 1.0f, (animMorph ? animator.macro1() : raw (id::morphX)) + offX);
        mo.y = juce::jlimit (0.0f, 1.0f, (animMorph ? animator.macro2() : raw (id::morphY)) + offY);
        mo.algo = choice (id::morphAlgo);
        mo.gamma = mod (id::morphGamma);
        mo.detail = mod (id::morphDetail);
        mo.formantOn = flag (id::formOn);
        mo.formantShift = mod (id::formShift);
        const float* bL = morphSrc == srcAux ? bufAuxL : bufMdlL;
        const float* bR = morphSrc == srcAux ? bufAuxR : bufMdlR;
        morph.process (bufMainL, bufMainR, bL, bR, bufMorL, bufMorR, n, mo);

        // ---- grain engine
        float* wl = wetBuffer.getWritePointer (0);
        float* wr = wetBuffer.getWritePointer (1);
        if (grainOn)
        {
            engine.process (bufMorL, bufMorR, wl, wr, n, engineParams);
            for (int i = 0; i < n; ++i) { grainBypass[0].process (bufMorL[i]); grainBypass[1].process (bufMorR[i]); }
        }
        else
        {
            for (int i = 0; i < n; ++i) { wl[i] = grainBypass[0].process (bufMorL[i]); wr[i] = grainBypass[1].process (bufMorR[i]); }
        }

        // ---- time-aligned dry copies
        for (int i = 0; i < n; ++i)
        {
            alMainL[i] = alignMain[0].process (bufMainL[i]); alMainR[i] = alignMain[1].process (bufMainR[i]);
            alAuxL[i]  = alignAux[0].process (bufAuxL[i]);   alAuxR[i]  = alignAux[1].process (bufAuxR[i]);
            alMdlL[i]  = alignModel[0].process (bufMdlL[i]); alMdlR[i]  = alignModel[1].process (bufMdlR[i]);
        }

        // ---- transients: restore the original attack
        TransientParams tp;
        tp.on = flag (id::trOn);
        tp.sensitivity = mod (id::trSens);
        tp.lengthMs = mod (id::trLength);
        tp.amount = mod (id::trAmount);
        transients.process (bufMainL, bufMainR, alMainL, alMainR, wl, wr, n, tp);

        // ---- effects rack settings
        std::array<FxSlotParams, numFxSlots> fx;
        for (int k = 1; k <= numFxSlots; ++k)
        {
            auto& f = fx[(size_t) k - 1];
            f.type = choice (id::fx (k, "Type"));
            f.on = flag (id::fx (k, "On"));
            f.mix = mod (id::fx (k, "Mix"));
            f.a = mod (id::fx (k, "A")); f.b = mod (id::fx (k, "B"));
            f.c = mod (id::fx (k, "C")); f.d = mod (id::fx (k, "D"));
        }

        // ---- mixer
        const float gWet = chOn[0] ? mod (id::mixWet) : 0.0f;
        const float gDry = chOn[1] ? mod (id::mixDry) : 0.0f;
        const float gAux = chOn[2] ? mod (id::mixAux) : 0.0f;
        for (int i = 0; i < n; ++i) { wl[i] *= gWet; wr[i] *= gWet; }
        if (routing == routePost) rack.process (wl, wr, n, fx, bpm);
        for (int i = 0; i < n; ++i)
        {
            mixL[i] = wl[i] + alMainL[i] * gDry + alAuxL[i] * gAux + alMdlL[i] * gModel;
            mixR[i] = wr[i] + alMainR[i] * gDry + alAuxR[i] * gAux + alMdlR[i] * gModel;
        }
        if (routing == routePre) rack.process (mixL, mixR, n, fx, bpm);

        const float revAmt = mod (id::revMix);
        if (revAmt > 0.001f)
        {
            mixReverb.setParams (mod (id::revSize), mod (id::revDamp), 1.0f, 0.0f);
            mixReverb.process (mixL, mixR, revL, revR, n);
            for (int i = 0; i < n; ++i) { mixL[i] += revL[i] * revAmt * 1.3f; mixR[i] += revR[i] * revAmt * 1.3f; }
        }

        // ---- output
        const float outG = juce::Decibels::decibelsToGain (mod (id::outputGain));
        for (int i = 0; i < n; ++i) { mixL[i] *= outG; mixR[i] *= outG; }
        limiter.process (mixL, mixR, n, limitOn);

        for (int i = 0; i < n; ++i)
        {
            if (! std::isfinite (mixL[i])) mixL[i] = 0.0f;
            if (! std::isfinite (mixR[i])) mixR[i] = 0.0f;
            if (numMainOut >= 2)
            {
                mainOut.setSample (0, start + i, mixL[i]);
                mainOut.setSample (1, start + i, mixR[i]);
            }
            else
            {
                mainOut.setSample (0, start + i, 0.5f * (mixL[i] + mixR[i]));
            }
            outPeak = std::max (outPeak, std::max (std::abs (mixL[i]), std::abs (mixR[i])));
        }
    }

    for (int ch = numMainOut; ch < buffer.getNumChannels(); ++ch)
        if (ch >= 2) buffer.clear (ch, 0, numSamples);

    mainMeter = std::max (mainPeak, mainMeter * 0.85f);
    auxMeter = std::max (auxPeak, auxMeter * 0.85f);
    mainLevel.store (mainMeter, std::memory_order_relaxed);
    auxLevel.store (auxMeter, std::memory_order_relaxed);
    auxConnected.store (numSc > 0, std::memory_order_relaxed);
    outputLevel.store (outPeak, std::memory_order_relaxed);
}

// ----------------------------------------------------------------------------
juce::AudioProcessorEditor* LiquidationProcessor::createEditor()
{
   #if LIQ_HEADLESS
    return nullptr;
   #else
    return new LiquidationEditor (*this);
   #endif
}

bool LiquidationProcessor::hasEditor() const
{
   #if LIQ_HEADLESS
    return false;
   #else
    return true;
   #endif
}

int LiquidationProcessor::getNumPrograms()                 { return std::max (1, (int) PresetManager::factory().size()); }
int LiquidationProcessor::getCurrentProgram()              { return juce::jlimit (0, getNumPrograms() - 1, presets.currentIndex()); }
void LiquidationProcessor::setCurrentProgram (int index)   { if (index >= 0 && index < getNumPrograms()) presets.load (index); }
const juce::String LiquidationProcessor::getProgramName (int index)
{
    const auto& f = PresetManager::factory();
    return juce::isPositiveAndBelow (index, (int) f.size()) ? f[(size_t) index].name : juce::String();
}

juce::ValueTree LiquidationProcessor::fullState()
{
    auto state = apvts.copyState();
    juce::ValueTree seq ("SeqSteps");
    for (int m = 0; m < numModulators; ++m)
    {
        juce::StringArray vals;
        for (float v : modSystem.seqSteps[(size_t) m]) vals.add (juce::String (v, 4));
        seq.setProperty ("m" + juce::String (m), vals.joinIntoString (","), nullptr);
    }
    state.removeChild (state.getChildWithName ("SeqSteps"), nullptr);
    state.appendChild (seq, nullptr);
    state.setProperty ("presetName", presets.currentName(), nullptr);
    return state;
}

void LiquidationProcessor::applyFullState (const juce::ValueTree& tree)
{
    if (! tree.hasType (apvts.state.getType())) return;
    auto copy = tree.createCopy();
    if (auto seq = copy.getChildWithName ("SeqSteps"); seq.isValid())
    {
        for (int m = 0; m < numModulators; ++m)
        {
            juce::StringArray vals;
            vals.addTokens (seq.getProperty ("m" + juce::String (m)).toString(), ",", {});
            for (int i = 0; i < numSeqSteps && i < vals.size(); ++i)
                modSystem.seqSteps[(size_t) m][(size_t) i] = vals[i].getFloatValue();
        }
        copy.removeChild (seq, nullptr);
    }
    if (copy.hasProperty ("presetName"))
        presets.setCurrentName (copy.getProperty ("presetName").toString());
    apvts.replaceState (copy);
}

void LiquidationProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = fullState().createXml())
        copyXmlToBinary (*xml, destData);
}

void LiquidationProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        applyFullState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LiquidationProcessor();
}
