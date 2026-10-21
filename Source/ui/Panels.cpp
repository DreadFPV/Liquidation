#include "Panels.h"
#include "../PluginProcessor.h"
#include "../dsp/EffectsRack.h"

namespace liq::ui
{
namespace
{
    constexpr int knobH = 80;
    constexpr int comboH = 24;

    float rawOf (Ctx& c, const juce::String& pid) { return c.apvts.getRawParameterValue (pid)->load(); }
    int choiceOf (Ctx& c, const juce::String& pid) { return (int) std::lround (rawOf (c, pid)); }
    bool flagOf (Ctx& c, const juce::String& pid) { return rawOf (c, pid) > 0.5f; }

    void showIf (juce::Component* c, bool b) { if (c->isVisible() != b) c->setVisible (b); }
}

// ============================================================================
// GRAIN PAGE
// ============================================================================
GrainPage::GrainPage (Ctx& c) : Page (c), waveform (c)
{
    addAndMakeVisible (waveform);

    grainSec = &section ("Grain");
    playSec  = &section ("Playback");
    pitchSec = &section ("Pitch");

    // ---- grain
    rate = &knob (id::grainRate, "Rate", grainSec);
    rate->setFormatter ([this] (float v)
    {
        return GrainEngine::rateText (choiceOf (ctx, id::rateMode), v, ctx.proc.currentScale(), ctx.proc.hostBpm.load());
    });
    rateMode = &combo (id::rateMode, grainSec);
    sizeMs = &knob (id::grainSize, "Size", grainSec);
    sizeRatio = &knob (id::sizeRatio, "Size", grainSec);
    sizeRatio->setFormatter ([] (float v) { return juce::String (v, 2) + "x rate"; });
    link = &toggle (id::sizeLink, "LINK", grainSec);
    link->setTooltip ("Grain Size Link: size follows the grain rate");
    shape = &knob (id::winShape, "Shape", grainSec);
    skew = &knob (id::winSkew, "Skew", grainSec);
    jitter = &knob (id::jitter, "Jitter", grainSec);
    for (auto* k : { shape, skew }) k->setFormatter ([] (float v) { return juce::String ((int) std::lround (v * 100)) + "%"; });
    jitter->setFormatter ([] (float v) { return juce::String ((int) std::lround (v * 100)) + "%"; });

    // ---- playback
    playMode = &combo (id::playMode, playSec);
    reverse = &toggle (id::reverse, "REVERSE", playSec);
    activate = &toggle (id::stretchActive, "ACTIVE", playSec);
    activate->setTooltip ("Stretch Activate: off returns to the live input, on starts a new stretch");
    stretchMode = &combo (id::stretchMode, playSec);
    retrig = &combo (id::stretchRetrig, playSec);
    stretchSync = &toggle (id::stretchSync, "SYNC", playSec);
    lblStretchMode = &caption ("TRIGGER", playSec);
    lblRetrig = &caption ("RETRIGGER", playSec);
    lblScrubMode = &caption ("SCRUB GRID", playSec);
    stretch = &knob (id::stretch, "Stretch", playSec);
    stretch->setFormatter ([this] (float v)
    {
        const float sp = GrainEngine::stretchFactor (v, flagOf (ctx, id::stretchSync));
        if (sp <= 0.0f) return juce::String ("Freeze");
        return juce::String (1.0f / sp, 1 / sp < 10 ? 2 : 1) + "x";
    });
    sens = &knob (id::stretchSens, "Sensitivity", playSec);
    scrubPos = &knob (id::scrubPos, "Position", playSec);
    scrubMode = &combo (id::scrubMode, playSec);
    spray = &knob (id::spray, "Spray", playSec);
    blur = &knob (id::blur, "Blur", playSec);
    revProb = &knob (id::revProb, "Reverse %", playSec);
    level = &knob (id::grainLevel, "Level", playSec);
    level->setFormatter ([] (float v) { return juce::String (v, 1) + " dB"; });
    for (auto* k : { sens, scrubPos, spray, blur, revProb })
        k->setFormatter ([] (float v) { return juce::String ((int) std::lround (v * 100)) + "%"; });

    // ---- pitch
    pitch = &knob (id::pitch, "Pitch", pitchSec);
    pitch->setFormatter ([this] (float v)
    {
        const bool scale = choiceOf (ctx, id::pitchMode) == pitchScale && ! ctx.proc.currentScale().chromatic;
        if (scale) { const int d = (int) std::lround (v); return (d > 0 ? "+" : "") + juce::String (d) + " deg"; }
        return (v > 0 ? "+" : "") + juce::String (v, 1) + " st";
    });
    detune = &knob (id::detune, "Detune", pitchSec);
    detune->setFormatter ([] (float v) { return juce::String ((int) std::lround (v * 100)) + " ct"; });
    pitchMode = &combo (id::pitchMode, pitchSec);
    retune = &combo (id::retune, pitchSec);
    chordMode = &combo (id::chordMode, pitchSec);
    arpRand = &knob (id::arpRand, "Arp Random", pitchSec);
    spreadK = &knob (id::chordSpread, "Chord Spread", pitchSec);
    for (auto* k : { arpRand, spreadK }) k->setFormatter ([] (float v) { return juce::String ((int) std::lround (v * 100)) + "%"; });
    intervals = std::make_unique<IntervalStrip> (ctx);
    pitchSec->addAndMakeVisible (*intervals);
    keyboard = std::make_unique<ScaleKeyboard> (ctx);
    pitchSec->addAndMakeVisible (*keyboard);
}

void GrainPage::resized()
{
    auto r = getLocalBounds();
    waveform.setBounds (r.removeFromTop (176));
    r.removeFromTop (8);
    grainSec->setBounds (r.removeFromLeft (232));
    r.removeFromLeft (8);
    playSec->setBounds (r.removeFromLeft (318));
    r.removeFromLeft (8);
    pitchSec->setBounds (r);

    // grain
    {
        auto c = grainSec->content();
        auto top = c.removeFromTop (knobH);
        row (top, { rate, sizeMs });
        sizeRatio->setBounds (sizeMs->getBounds());
        auto combos = c.removeFromTop (comboH);
        row (combos, { rateMode, link });
        c.removeFromTop (12);
        row (c.removeFromTop (knobH), { shape, skew, jitter });
    }
    // playback
    {
        auto c = playSec->content();
        row (c.removeFromTop (comboH), { playMode, reverse, activate });
        c.removeFromTop (8);
        auto labels = c.removeFromTop (14);
        auto combos = c.removeFromTop (comboH);
        row (labels, { lblStretchMode, lblRetrig, nullptr });
        row (combos, { stretchMode, retrig, stretchSync });
        lblScrubMode->setBounds (lblStretchMode->getBounds());
        scrubMode->setBounds (stretchMode->getBounds());
        c.removeFromTop (8);
        auto k1 = c.removeFromTop (knobH);
        row (k1, { stretch, sens, scrubPos, level });
        c.removeFromTop (6);
        row (c.removeFromTop (knobH), { spray, blur, revProb, nullptr });
    }
    // pitch
    {
        auto c = pitchSec->content();
        auto top = c.removeFromTop (knobH);
        auto combos = top.removeFromRight (std::max (110, top.getWidth() / 2));
        row (top, { pitch, detune });
        combos = combos.withSizeKeepingCentre (combos.getWidth(), comboH * 2 + 6);
        pitchMode->setBounds (combos.removeFromTop (comboH));
        combos.removeFromTop (6);
        retune->setBounds (combos);
        c.removeFromTop (4);
        auto mid = c.removeFromTop (knobH);
        auto cm = mid.removeFromLeft (mid.getWidth() / 3);
        chordMode->setBounds (cm.withSizeKeepingCentre (cm.getWidth() - 4, comboH));
        row (mid, { arpRand, spreadK });
        c.removeFromTop (2);
        keyboard->setBounds (c.removeFromBottom (24));
        c.removeFromBottom (6);
        intervals->setBounds (c);
    }
}

void GrainPage::update()
{
    waveform.update();
    keyboard->refresh();

    const bool linked = flagOf (ctx, id::sizeLink);
    showIf (sizeMs, ! linked);
    showIf (sizeRatio, linked);

    const bool scrub = choiceOf (ctx, id::playMode) == playScrub;
    for (auto* c : std::initializer_list<juce::Component*> { stretchMode, retrig, stretchSync, stretch, sens, activate, lblStretchMode, lblRetrig })
        showIf (c, ! scrub);
    for (auto* c : std::initializer_list<juce::Component*> { scrubPos, scrubMode, lblScrubMode })
        showIf (c, scrub);

    const int sm = choiceOf (ctx, id::stretchMode);
    retrig->setEnabled (sm == stretchGrid);
    sens->setEnabled (sm == stretchTransient);

    const int cm = choiceOf (ctx, id::chordMode);
    arpRand->setEnabled (cm == chordArp);
    spreadK->setEnabled (cm == chordChord);
    intervals->setEnabled (cm != chordOff);
    intervals->setAlpha (cm != chordOff ? 1.0f : 0.45f);
    retune->setEnabled (choiceOf (ctx, id::pitchMode) == pitchScale);

    const int rm = choiceOf (ctx, id::rateMode);
    if (rm != lastRateMode) { lastRateMode = rm; rate->repaint(); }
    rate->repaint();
}

// ============================================================================
// SHAPE PAGE (filter, imager, delay)
// ============================================================================
ShapePage::ShapePage (Ctx& c) : Page (c)
{
    fltSec = &section ("Grain Filter", id::fltOn);
    imgSec = &section ("Grain Imager", id::imgOn);
    dlySec = &section ("Grain Delay", id::dlyOn);

    auto pct = [] (float v) { return juce::String ((int) std::lround (v * 100)) + "%"; };

    fltMode = &combo (id::fltMode, fltSec);
    fltScale = &toggle (id::fltScale, "SCALE", fltSec);
    fltRandom = &toggle (id::fltRandom, "RANDOM", fltSec);
    cutoff = &knob (id::fltCutoff, "Cutoff", fltSec);
    reso = &knob (id::fltReso, "Resonance", fltSec);
    drive = &knob (id::fltDrive, "Drive", fltSec);
    modDepth = &knob (id::fltModDepth, "Mod Depth", fltSec);
    modRate = &knob (id::fltModRate, "Mod Rate", fltSec);
    morph = &knob (id::fltMorph, "Morph", fltSec);
    damp = &knob (id::fltDamp, "Damping", fltSec);
    smooth = &knob (id::fltSmooth, "Smooth", fltSec);
    bits = &knob (id::fltBits, "Bits", fltSec);
    fmix = &knob (id::fltMix, "Mix", fltSec);
    cutoff->setFormatter ([] (float v) { return v >= 1000 ? juce::String (v / 1000, 2) + " kHz" : juce::String ((int) v) + " Hz"; });
    modRate->setFormatter ([] (float v) { return juce::String (v, 2) + " /grain"; });
    bits->setFormatter ([] (float v) { return juce::String (v, 1) + " bit"; });
    for (auto* k : { reso, drive, modDepth, morph, damp, smooth, fmix }) k->setFormatter (pct);

    spreadMode = &combo (id::spreadMode, imgSec);
    smoothSpread = &toggle (id::smoothSpread, "SMOOTH", imgSec);
    monoLows = &toggle (id::monoLows, "MONO LOWS", imgSec);
    safeWidth = &toggle (id::safeWidth, "SAFE", imgSec);
    spread = &knob (id::spread, "Spread", imgSec);
    pan = &knob (id::pan, "Pan", imgSec);
    dim = &knob (id::dimension, "Dimension", imgSec);
    dimSize = &knob (id::dimSize, "Dim Size", imgSec);
    monoFreq = &knob (id::monoFreq, "Mono Freq", imgSec);
    width = &knob (id::width, "Width", imgSec);
    for (auto* k : { spread, dim, dimSize, width }) k->setFormatter (pct);
    pan->setFormatter ([] (float v)
    {
        const int p = (int) std::lround (v * 100);
        return p == 0 ? juce::String ("C") : (p < 0 ? "L" + juce::String (-p) : "R" + juce::String (p));
    });

    ping = &toggle (id::dlyPingPong, "PING PONG", dlySec);
    swap = &toggle (id::dlySwap, "SWAP", dlySec);
    sync = &toggle (id::dlySync, "SYNC", dlySec);
    tapView = std::make_unique<TapPatternView> (ctx);
    dlySec->addAndMakeVisible (*tapView);
    dlyTime = &knob (id::dlyTime, "Time", dlySec);
    dlyDiv = &combo (id::dlyDiv, dlySec);
    taps = &knob (id::dlyTaps, "Taps", dlySec);
    shift = &knob (id::dlyShift, "Shift", dlySec);
    shift->setFormatter ([this] (float v)
    {
        const bool deg = choiceOf (ctx, id::pitchMode) == pitchScale && ! ctx.proc.currentScale().chromatic;
        const int s = (int) std::lround (v);
        return deg ? (s > 0 ? "+" : "") + juce::String (s) + " deg" : (v > 0 ? "+" : "") + juce::String (v, 1) + " st";
    });
    pattern = &combo (id::dlyPattern, dlySec);
    feedback = &knob (id::dlyFeedback, "Feedback", dlySec);
    space = &knob (id::dlySpace, "Space", dlySec);
    dmix = &knob (id::dlyMix, "Mix", dlySec);
    lp = &knob (id::dlyLP, "Lowpass", dlySec);
    hp = &knob (id::dlyHP, "Highpass", dlySec);
    for (auto* k : { feedback, space, dmix }) k->setFormatter (pct);
    for (auto* k : { lp, hp }) k->setFormatter ([] (float v) { return v >= 1000 ? juce::String (v / 1000, 1) + " kHz" : juce::String ((int) v) + " Hz"; });
}

void ShapePage::resized()
{
    auto r = getLocalBounds();
    const int w = (r.getWidth() - 16) / 3;
    fltSec->setBounds (r.removeFromLeft (w)); r.removeFromLeft (8);
    imgSec->setBounds (r.removeFromLeft (w)); r.removeFromLeft (8);
    dlySec->setBounds (r);

    {
        auto c = fltSec->content();
        row (c.removeFromTop (comboH), { fltMode, fltScale, fltRandom });
        c.removeFromTop (10);
        row (c.removeFromTop (knobH), { cutoff, reso, drive });
        c.removeFromTop (6);
        row (c.removeFromTop (knobH), { modDepth, modRate, morph });
        c.removeFromTop (6);
        row (c.removeFromTop (knobH), { damp, smooth, bits });
        c.removeFromTop (6);
        row (c.removeFromTop (knobH), { nullptr, fmix, nullptr });
    }
    {
        auto c = imgSec->content();
        row (c.removeFromTop (comboH), { spreadMode, smoothSpread });
        c.removeFromTop (10);
        row (c.removeFromTop (knobH), { spread, pan });
        c.removeFromTop (6);
        row (c.removeFromTop (knobH), { dim, dimSize });
        c.removeFromTop (10);
        row (c.removeFromTop (comboH), { monoLows, safeWidth });
        c.removeFromTop (6);
        row (c.removeFromTop (knobH), { monoFreq, width });
    }
    {
        auto c = dlySec->content();
        row (c.removeFromTop (comboH), { ping, swap, sync });
        c.removeFromTop (8);
        tapView->setBounds (c.removeFromTop (70));
        c.removeFromTop (8);
        auto k = c.removeFromTop (knobH);
        row (k, { dlyTime, taps, shift });
        dlyDiv->setBounds (dlyTime->getBounds().withSizeKeepingCentre (dlyTime->getWidth(), comboH));
        c.removeFromTop (4);
        pattern->setBounds (c.removeFromTop (comboH).reduced (30, 0));
        c.removeFromTop (8);
        row (c.removeFromTop (knobH), { feedback, space, dmix });
        c.removeFromTop (4);
        row (c.removeFromTop (knobH), { lp, hp });
    }
}

void ShapePage::update()
{
    const int m = choiceOf (ctx, id::fltMode);
    const bool basic = m <= fNotch;
    drive->setEnabled (basic || m == fMorph || m == fVowel || m == fModal || m == fComb || m == fFlange || m == fPhaser || m == fAllpass);
    morph->setEnabled (m == fMorph || m == fVowel || m == fModal || m == fPhaser || m == fAllpass || m == fFlange);
    damp->setEnabled (m == fComb || m == fFlange);
    smooth->setEnabled (m == fReducer);
    bits->setEnabled (m == fReducer);
    reso->setEnabled (m != fReducer);
    modRate->setEnabled (! flagOf (ctx, id::fltRandom));
    monoFreq->setEnabled (flagOf (ctx, id::monoLows));

    const bool s = flagOf (ctx, id::dlySync);
    showIf (dlyTime, ! s);
    showIf (dlyDiv, s);
    tapView->repaint();
}

// ============================================================================
// EFFECTS PAGE
// ============================================================================
EffectsPage::EffectsPage (Ctx& c) : Page (c)
{
    for (int k = 1; k <= numFxSlots; ++k)
    {
        auto& s = slots[(size_t) k - 1];
        s.sec = &section ("Effect " + juce::String (k), id::fx (k, "On"));
        s.type = &combo (id::fx (k, "Type"), s.sec);
        s.mix = &knob (id::fx (k, "Mix"), "Mix", s.sec);
        s.mix->setFormatter ([] (float v) { return juce::String ((int) std::lround (v * 100)) + "%"; });
        const char* f[] { "A", "B", "C", "D" };
        for (int i = 0; i < 4; ++i)
            s.k[(size_t) i] = &knob (id::fx (k, f[i]), f[i], s.sec);
    }
}

void EffectsPage::resized()
{
    auto r = getLocalBounds();
    const int w = (r.getWidth() - 8 * (numFxSlots - 1)) / numFxSlots;
    for (auto& s : slots)
    {
        s.sec->setBounds (r.removeFromLeft (w));
        r.removeFromLeft (8);
        auto c = s.sec->content();
        s.type->setBounds (c.removeFromTop (comboH));
        c.removeFromTop (14);
        row (c.removeFromTop (knobH), { s.k[0], s.k[1] });
        c.removeFromTop (10);
        row (c.removeFromTop (knobH), { s.k[2], s.k[3] });
        c.removeFromTop (18);
        row (c.removeFromTop (knobH), { nullptr, s.mix, nullptr });
    }
}

void EffectsPage::update()
{
    for (size_t k = 0; k < slots.size(); ++k)
    {
        auto& s = slots[k];
        const int type = choiceOf (ctx, id::fx ((int) k + 1, "Type"));
        if (type == s.lastType) continue;
        s.lastType = type;
        const auto labels = fxKnobLabels (type);
        for (int i = 0; i < 4; ++i)
        {
            s.k[(size_t) i]->setLabel (labels[i]);
            s.k[(size_t) i]->setFormatter ([type, i] (float v) { return fxKnobText (type, i, v); });
        }
    }
}

// ============================================================================
// MOD PAGE
// ============================================================================
void StepEditor::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::bg);
    g.fillRoundedRectangle (r, 5.0f);
    const int steps = (int) std::lround (ctx.apvts.getRawParameterValue (id::mod (m + 1, "Steps"))->load());
    const auto& vals = ctx.proc.modSystem.seqSteps[(size_t) m];
    const float w = r.getWidth() / (float) steps;
    const float phase = ctx.proc.modSystem.modPhase[(size_t) m].load();
    const int active = juce::jlimit (0, steps - 1, (int) (phase * steps));
    for (int i = 0; i < steps; ++i)
    {
        auto bar = juce::Rectangle<float> (r.getX() + i * w, r.getY(), w, r.getHeight()).reduced (2.0f, 4.0f);
        g.setColour (col::track);
        g.fillRoundedRectangle (bar, 2.0f);
        const float v = juce::jlimit (0.0f, 1.0f, vals[(size_t) i]);
        g.setColour (i == active ? col::mod : col::mod.withAlpha (0.55f));
        g.fillRoundedRectangle (bar.withTop (bar.getBottom() - bar.getHeight() * v), 2.0f);
    }
}

void StepEditor::edit (const juce::MouseEvent& e)
{
    const int steps = (int) std::lround (ctx.apvts.getRawParameterValue (id::mod (m + 1, "Steps"))->load());
    const int i = juce::jlimit (0, steps - 1, (int) (e.position.x / ((float) getWidth() / (float) steps)));
    const float v = juce::jlimit (0.0f, 1.0f, 1.0f - (e.position.y - 4.0f) / (float) (getHeight() - 8));
    ctx.proc.modSystem.seqSteps[(size_t) m][(size_t) i] = e.mods.isRightButtonDown() ? 0.0f : v;
    repaint();
}

void ModShapeView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::bg);
    g.fillRoundedRectangle (r, 5.0f);
    const int type = (int) std::lround (ctx.apvts.getRawParameterValue (id::mod (m + 1, "Type"))->load());
    auto inner = r.reduced (6.0f);
    if (type == modLfo)
    {
        const float shape = ctx.apvts.getRawParameterValue (id::mod (m + 1, "Shape"))->load();
        const float ph0 = ctx.apvts.getRawParameterValue (id::mod (m + 1, "Phase"))->load();
        juce::Path p;
        juce::Random rnd (7);
        float held = rnd.nextFloat();
        for (int i = 0; i <= 200; ++i)
        {
            double ph = i / 200.0 * 2.0 + ph0;
            if ((int) ph != (int) ((i - 1) / 200.0 * 2.0 + ph0)) held = rnd.nextFloat();
            ph -= std::floor (ph);
            const float v = Modulator::lfoShape (shape, ph, held, held);
            const float x = inner.getX() + inner.getWidth() * i / 200.0f;
            const float y = inner.getBottom() - v * inner.getHeight();
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (col::mod);
        g.strokePath (p, juce::PathStrokeType (2.0f));
        const float ph = ctx.proc.modSystem.modPhase[(size_t) m].load();
        const float x = inner.getX() + inner.getWidth() * ph * 0.5f;
        g.setColour (col::text.withAlpha (0.6f));
        g.drawVerticalLine ((int) x, inner.getY(), inner.getBottom());
    }
    else if (type == modEnvelope)
    {
        const float v = ctx.proc.modSystem.sourceValue[(size_t) m + 3].load();
        g.setColour (col::track);
        g.fillRoundedRectangle (inner.withHeight (14.0f).withCentre (inner.getCentre()), 4.0f);
        g.setColour (col::mod);
        g.fillRoundedRectangle (inner.withHeight (14.0f).withCentre (inner.getCentre()).withWidth (inner.getWidth() * v), 4.0f);
        g.setFont (font (11.0f));
        g.setColour (col::dim);
        g.drawText ("Follows the input level", inner, juce::Justification::centredTop);
    }
}

ModPage::ModPage (Ctx& c) : Page (c)
{
    for (int m = 0; m < numModulators; ++m)
    {
        auto& p = mods[(size_t) m];
        const int k = m + 1;
        p.sec = &section ("Modulator " + juce::String (k));
        p.sec->setAccent (col::mod);
        p.type = &combo (id::mod (k, "Type"), p.sec);
        p.sync = &toggle (id::mod (k, "Sync"), "SYNC", p.sec);
        p.sync->getProperties().set ("modColour", true);
        p.div = &combo (id::mod (k, "Div"), p.sec);
        p.rate = &knob (id::mod (k, "Rate"), "Rate", p.sec);
        p.shape = &knob (id::mod (k, "Shape"), "Shape", p.sec);
        p.shape->setFormatter ([] (float v)
        {
            static const char* n[] { "Sine", "Triangle", "Saw", "Square", "Random" };
            return juce::String (n[juce::jlimit (0, 4, (int) std::lround (v * 4.0f))]);
        });
        p.phase = &knob (id::mod (k, "Phase"), "Phase", p.sec);
        p.phase->setFormatter ([] (float v) { return juce::String ((int) std::lround (v * 360)) + " deg"; });
        p.steps = &knob (id::mod (k, "Steps"), "Steps", p.sec);
        p.glide = &knob (id::mod (k, "Glide"), "Glide", p.sec);
        p.glide->setFormatter ([] (float v) { return juce::String ((int) std::lround (v * 100)) + "%"; });
        p.atk = &knob (id::mod (k, "Atk"), "Attack", p.sec);
        p.rel = &knob (id::mod (k, "Rel"), "Release", p.sec);
        p.gain = &knob (id::mod (k, "Gain"), "Gain", p.sec);
        p.stepEditor = std::make_unique<StepEditor> (ctx, m);
        p.shapeView = std::make_unique<ModShapeView> (ctx, m);
        p.sec->addAndMakeVisible (*p.stepEditor);
        p.sec->addAndMakeVisible (*p.shapeView);
    }

    matrixSec = &section ("Mod Matrix");
    matrixSec->setAccent (col::mod);
    for (int s = 1; s <= numMatrixSlots; ++s)
    {
        auto& rw = rows[(size_t) s - 1];
        rw.src = &combo (id::mx (s, "Src"), matrixSec);
        rw.dst = &combo (id::mx (s, "Dst"), matrixSec);
        auto* amt = new juce::Slider (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
        amt->setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 18);
        amt->getProperties().set ("bipolar", true);
        amt->getProperties().set ("modColour", true);
        amt->setDoubleClickReturnValue (true, 0.0);
        owned.add (amt);
        matrixSec->addAndMakeVisible (amt);
        amtAttachments.push_back (std::make_unique<APVTS::SliderAttachment> (ctx.apvts, id::mx (s, "Amt"), *amt));
        amt->textFromValueFunction = [] (double v)
        {
            const int pc = (int) std::lround (v * 100.0);
            return (pc > 0 ? "+" : "") + juce::String (pc) + "%";
        };
        amt->valueFromTextFunction = [] (const juce::String& t) { return t.retainCharacters ("-0123456789.").getDoubleValue() / 100.0; };
        amt->updateText();
        rw.amt = amt;
        auto* clr = new juce::TextButton ("x");
        clr->setTooltip ("Clear this slot");
        owned.add (clr);
        matrixSec->addAndMakeVisible (clr);
        clr->onClick = [this, s]
        {
            for (auto f : { "Src", "Dst", "Amt" })
                if (auto* p = ctx.apvts.getParameter (id::mx (s, f))) p->setValueNotifyingHost (p->getDefaultValue());
        };
        rw.clear = clr;
    }
    selectModulator (0);
}

void ModPage::selectModulator (int m)
{
    current = juce::jlimit (0, numModulators - 1, m);
    for (int i = 0; i < numModulators; ++i) mods[(size_t) i].sec->setVisible (i == current);
}

void ModPage::resized()
{
    auto r = getLocalBounds();
    auto left = r.removeFromLeft (360);
    r.removeFromLeft (8);
    matrixSec->setBounds (r);

    for (auto& p : mods)
    {
        p.sec->setBounds (left);
        auto c = p.sec->content();
        row (c.removeFromTop (comboH), { p.type, p.sync });
        c.removeFromTop (8);
        auto viz = c.removeFromTop (110);
        p.stepEditor->setBounds (viz);
        p.shapeView->setBounds (viz);
        c.removeFromTop (10);
        auto k1 = c.removeFromTop (knobH);
        row (k1, { p.rate, p.shape, p.phase });
        p.div->setBounds (p.rate->getBounds().withSizeKeepingCentre (p.rate->getWidth(), comboH));
        p.steps->setBounds (p.shape->getBounds());
        p.glide->setBounds (p.phase->getBounds());
        c.removeFromTop (8);
        row (c.removeFromTop (knobH), { p.atk, p.rel, p.gain });
    }

    auto c = matrixSec->content();
    auto header = c.removeFromTop (16);
    juce::ignoreUnused (header);
    const int rh = std::min (30, c.getHeight() / numMatrixSlots);
    for (auto& rw : rows)
    {
        auto line = c.removeFromTop (rh).reduced (0, 3);
        rw.clear->setBounds (line.removeFromRight (24));
        line.removeFromRight (6);
        rw.src->setBounds (line.removeFromLeft (96));
        line.removeFromLeft (6);
        rw.dst->setBounds (line.removeFromLeft (170));
        line.removeFromLeft (8);
        rw.amt->setBounds (line);
    }
}

void ModPage::update()
{
    auto& p = mods[(size_t) current];
    const int k = current + 1;
    const int type = choiceOf (ctx, id::mod (k, "Type"));
    const bool sync = flagOf (ctx, id::mod (k, "Sync"));
    const bool env = type == modEnvelope, seq = type == modSequencer, lfo = type == modLfo;
    showIf (p.sync, ! env);
    showIf (p.rate, ! env && ! sync);
    showIf (p.div, ! env && sync);
    showIf (p.shape, lfo);
    showIf (p.phase, lfo);
    showIf (p.steps, seq);
    showIf (p.glide, seq);
    showIf (p.atk, env);
    showIf (p.rel, env);
    showIf (p.gain, env);
    showIf (p.stepEditor.get(), seq);
    showIf (p.shapeView.get(), ! seq);
    p.stepEditor->repaint();
    p.shapeView->repaint();

    for (auto& rw : rows)
    {
        const bool used = rw.src->getSelectedItemIndex() > 0 && rw.dst->getSelectedItemIndex() > 0;
        rw.amt->setAlpha (used ? 1.0f : 0.4f);
    }
}

}
