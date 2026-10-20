#include "PluginProcessor.h"
#if ! LIQ_HEADLESS
 #include "PluginEditor.h"
#endif

using namespace liq;

LiquidationProcessor::LiquidationProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
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
}

bool LiquidationProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo()) return false;
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void LiquidationProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    engine.prepare (sampleRate, subBlock);
    rack.prepare (sampleRate, subBlock);
    auto steps = modSystem.seqSteps;    // keep edited steps across re-prepare
    modSystem.prepare (sampleRate);
    modSystem.seqSteps = steps;
    animator.prepare (sampleRate);
    dryBuffer.setSize (2, std::max (samplesPerBlock, subBlock), false, true, true);
    wetBuffer.setSize (2, std::max (samplesPerBlock, subBlock), false, true, true);
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
    const int numIn = getTotalNumInputChannels();

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

    // ---- input
    if (numIn == 1) buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);
    for (int ch = std::max (numIn, 2); ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, numSamples);

    const float inGain = juce::Decibels::decibelsToGain (raw (id::inputGain));
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

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
    AnimatorParams ap;
    ap.x = raw (id::macro1); ap.y = raw (id::macro2);
    ap.depth = raw (id::animDepth); ap.drift = raw (id::animDrift);
    ap.mode = choice (id::animMode); ap.cycles = (int) std::lround (raw (id::animCycles));
    ap.warp = raw (id::animWarp); ap.rateHz = raw (id::animRate); ap.sync = flag (id::animSync);
    ap.divBeats = divisions()[(size_t) juce::jlimit (0, (int) divisions().size() - 1, choice (id::animDiv))].beats;

    const int routing = choice (id::routing);
    float outPeak = 0.0f;

    for (int start = 0; start < numSamples; start += subBlock)
    {
        const int n = std::min (subBlock, numSamples - start);
        const double subPpq = ppq + start * bpm / 60.0 / sr;

        float* dl = dryBuffer.getWritePointer (0);
        float* dr = dryBuffer.getWritePointer (1);
        float peak = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            dl[i] = L[start + i] * inGain;
            dr[i] = R[start + i] * inGain;
            peak = std::max (peak, std::max (std::abs (dl[i]), std::abs (dr[i])));
        }

        animator.advance (ap, n, subPpq, bpm, playing);
        modSystem.process (mp, matrix, animator.macro1(), animator.macro2(), n, subPpq, playing, peak);
        buildEngineParams (engineParams, subPpq, playing, bpm);

        float* wl = wetBuffer.getWritePointer (0);
        float* wr = wetBuffer.getWritePointer (1);
        engine.process (dl, dr, wl, wr, n, engineParams);

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

        const float dw = mod (id::dryWet);
        const float dryG = std::min (1.0f, 2.0f * (1.0f - dw));
        const float wetG = std::min (1.0f, 2.0f * dw);
        const float outG = juce::Decibels::decibelsToGain (mod (id::outputGain));

        if (routing == routePost)
        {
            rack.process (wl, wr, n, fx, bpm);
            for (int i = 0; i < n; ++i)
            {
                L[start + i] = (dl[i] * dryG + wl[i] * wetG) * outG;
                R[start + i] = (dr[i] * dryG + wr[i] * wetG) * outG;
            }
        }
        else
        {
            for (int i = 0; i < n; ++i)
            {
                wl[i] = dl[i] * dryG + wl[i] * wetG;
                wr[i] = dr[i] * dryG + wr[i] * wetG;
            }
            rack.process (wl, wr, n, fx, bpm);
            for (int i = 0; i < n; ++i)
            {
                L[start + i] = wl[i] * outG;
                R[start + i] = wr[i] * outG;
            }
        }

        for (int i = 0; i < n; ++i)
            outPeak = std::max (outPeak, std::max (std::abs (L[start + i]), std::abs (R[start + i])));
    }

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
