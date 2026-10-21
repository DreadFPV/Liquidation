#include "Blocks.h"
#include "../PluginProcessor.h"
#include "../dsp/EffectsRack.h"

namespace liq::ui
{
namespace
{
    constexpr int kH = 72;     // knob height
    constexpr int rowH = 22;   // combo / button height

    float rawOf (Ctx& c, const juce::String& pid) { return c.apvts.getRawParameterValue (pid)->load(); }
    int choiceOf (Ctx& c, const juce::String& pid) { return (int) std::lround (rawOf (c, pid)); }
    bool flagOf (Ctx& c, const juce::String& pid) { return rawOf (c, pid) > 0.5f; }
    void showIf (juce::Component* c, bool b) { if (c != nullptr && c->isVisible() != b) c->setVisible (b); }
    juce::String pct (float v) { return juce::String ((int) std::lround (v * 100)) + "%"; }

    void setupInfoLabel (juce::Label& l, float size = 12.5f)
    {
        l.setFont (fontLight (size));
        l.setColour (juce::Label::textColourId, col::dim);
        l.setJustificationType (juce::Justification::topLeft);
        l.setMinimumHorizontalScale (1.0f);
    }

    const juce::String kAlgoInfo[] {
        "Gemeinsame Frequenzen von A und B verschmelzen.",
        "A bekommt die Klangfarbe von B.",
        "B wird von A geformt - wie ein Vocoder.",
        "Pro Frequenz gewinnt das lautere Signal.",
        "A macht Platz, wo B spielt (Ducking im Spektrum)."
    };
}

// ============================================================================
// Small components
// ============================================================================
ChoiceButtons::ChoiceButtons (APVTS& s, const juce::String& pid, juce::StringArray labels, int cols) : columns (cols)
{
    auto* param = s.getParameter (pid);
    jassert (param != nullptr);
    getProperties().set ("helpId", pid);
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (labels[i]));
        b->setClickingTogglesState (false);
        b->onClick = [this, i] { if (attachment) attachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v)
    {
        current = (int) std::lround (v);
        for (int i = 0; i < buttons.size(); ++i) buttons[i]->setToggleState (i == current, juce::dontSendNotification);
    });
    attachment->sendInitialUpdate();
}

void ChoiceButtons::setTooltips (const juce::StringArray& tips)
{
    for (int i = 0; i < buttons.size() && i < tips.size(); ++i) buttons[i]->setTooltip (tips[i]);
}

void ChoiceButtons::resized()
{
    const int n = buttons.size();
    const int rows = (n + columns - 1) / columns;
    const int gap = 3;
    const int w = (getWidth() - gap * (columns - 1)) / columns;
    const int h = (getHeight() - gap * (rows - 1)) / std::max (1, rows);
    for (int i = 0; i < n; ++i)
        buttons[i]->setBounds ((i % columns) * (w + gap), (i / columns) * (h + gap), w, h);
}

PowerToggle::PowerToggle (APVTS& s, const juce::String& pid, const juce::String& buttonText)
{
    setButtonText (buttonText);
    attachment = std::make_unique<APVTS::ButtonAttachment> (s, pid, *this);
    getProperties().set ("helpId", pid);
}

// ----------------------------------------------------------------------------
FaderRow::FaderRow (Ctx& c, const juce::String& pid, const juce::String& l, int lw) : ctx (c), label (l), labelW (lw)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (slider);
    param = ctx.apvts.getParameter (pid);
    attachment = std::make_unique<APVTS::SliderAttachment> (ctx.apvts, pid, slider);
    getProperties().set ("helpId", pid);
    if (param != nullptr)
    {
        slider.setDoubleClickReturnValue (true, param->getNormalisableRange().convertFrom0to1 (param->getDefaultValue()));
        const auto range = param->getNormalisableRange();
        slider.getProperties().set ("bipolar", range.start < 0.0f && range.end > 0.0f);
    }
    slider.onValueChange = [this] { repaint(); };
}

void FaderRow::resized()
{
    slider.setBounds (getLocalBounds().withTrimmedLeft (labelW).withTrimmedRight (48));
}

void FaderRow::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawCaps (g, label, r.withWidth ((float) labelW - 4.0f), juce::Justification::centredLeft, 12.0f, col::text.withAlpha (0.8f));
    juce::String v = param != nullptr ? param->getCurrentValueAsText() : juce::String();
    if (param != nullptr && param->getLabel().isNotEmpty()) v << " " << param->getLabel();
    g.setFont (fontLight (12.5f));
    g.setColour (col::dim);
    g.drawText (v, r.removeFromRight (46.0f), juce::Justification::centredRight);
}

// ----------------------------------------------------------------------------
MixChannel::MixChannel (Ctx& c, int ch, const juce::String& pid, const juce::String& label, juce::Colour tag)
    : fader (c, pid, label, 62), mute (c.apvts, id::mute (ch), "M"), solo (c.apvts, id::solo (ch), "S"), tagColour (tag)
{
    addAndMakeVisible (fader);
    addAndMakeVisible (mute);
    addAndMakeVisible (solo);
    mute.setTooltip ("Stumm");
    solo.setTooltip ("Solo");
}

void MixChannel::resized()
{
    auto r = getLocalBounds();
    r.removeFromLeft (6);
    solo.setBounds (r.removeFromRight (20).reduced (0, 2));
    r.removeFromRight (3);
    mute.setBounds (r.removeFromRight (20).reduced (0, 2));
    r.removeFromRight (4);
    fader.setBounds (r);
}

void MixChannel::paint (juce::Graphics& g)
{
    g.setColour (tagColour);
    g.fillRect (0.0f, (float) getHeight() * 0.25f, 2.5f, (float) getHeight() * 0.5f);
}

// ----------------------------------------------------------------------------
ValueField::ValueField (Ctx& c, const juce::String& pid, const juce::String& label)
{
    slider.setSliderStyle (juce::Slider::LinearBar);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.getProperties().set ("valueField", label);
    getProperties().set ("helpId", pid);
    addAndMakeVisible (slider);
    attachment = std::make_unique<APVTS::SliderAttachment> (c.apvts, pid, slider);
    if (auto* p = c.apvts.getParameter (pid))
    {
        slider.setDoubleClickReturnValue (true, p->getNormalisableRange().convertFrom0to1 (p->getDefaultValue()));

    }
}

// ============================================================================
// Morph pad
// ============================================================================
void MorphPad::update()
{
    const bool moving = rawOf (ctx, id::mmDepth) > 0.001f;
    const float x = moving ? ctx.proc.morphAnimator.cursorX.load() : rawOf (ctx, id::morphX);
    const float y = moving ? ctx.proc.morphAnimator.cursorY.load() : rawOf (ctx, id::morphY);
    if (std::abs (x - cx) > 0.001f || std::abs (y - cy) > 0.001f) { cx = x; cy = y; }
    repaint();
}

void MorphPad::paint (juce::Graphics& g)
{
    auto r = area();
    const bool on = flagOf (ctx, id::morphOn);
    auto frame = chamfer (r, 7.0f);
    g.setColour (col::bg);
    g.fillPath (frame);

    const float hx = rawOf (ctx, id::morphX), hy = rawOf (ctx, id::morphY);
    const auto m = r.reduced (14.0f);   // handle area keeps a margin to the frame
    auto toScreen = [m] (float x, float y)
    {
        return juce::Point<float> (m.getX() + juce::jlimit (0.0f, 1.0f, x) * m.getWidth(),
                                   m.getBottom() - juce::jlimit (0.0f, 1.0f, y) * m.getHeight());
    };

    // "liquid" grid: lines bend towards the current point, the more the deeper the morph
    {
        g.saveState();
        g.reduceClipRegion (frame);
        const auto c = toScreen (cx, cy);
        const float strength = on ? 0.18f + 0.30f * cy : 0.0f;
        const float sigma2 = std::pow (r.getWidth() * 0.32f, 2.0f);
        auto warp = [&] (juce::Point<float> p)
        {
            const auto d = c - p;
            const float w = std::exp (-(d.x * d.x + d.y * d.y) / sigma2);
            return p + d * (strength * w);
        };
        const int n = 12;
        for (int pass = 0; pass < 2; ++pass)
            for (int i = 0; i <= n; ++i)
            {
                juce::Path path;
                for (int j = 0; j <= 40; ++j)
                {
                    const float t = (float) j / 40.0f, u = (float) i / (float) n;
                    juce::Point<float> p = pass == 0 ? juce::Point<float> (r.getX() + u * r.getWidth(), r.getY() + t * r.getHeight())
                                                     : juce::Point<float> (r.getX() + t * r.getWidth(), r.getY() + u * r.getHeight());
                    p = warp (p);
                    if (j == 0) path.startNewSubPath (p); else path.lineTo (p);
                }
                const float u = (float) i / (float) n;
                const auto colr = pass == 0 ? col::sigA.interpolatedWith (col::sigB, u) : col::line;
                g.setColour (colr.withAlpha (on ? (i == 0 || i == n ? 0.0f : 0.32f) : 0.12f));
                g.strokePath (path, juce::PathStrokeType (1.0f));
            }
        g.restoreState();
    }

    // axis captions
    drawCaps (g, "A", { r.getX() + 6, r.getBottom() - 20, 20, 16 }, juce::Justification::centredLeft, 14.0f, col::sigA);
    drawCaps (g, "B", { r.getRight() - 26, r.getBottom() - 20, 20, 16 }, juce::Justification::centredRight, 14.0f, col::sigB);
    drawCaps (g, "X-Fade", { r.getCentreX() - 20, r.getBottom() - 18, 80, 14 }, juce::Justification::centred, 11.0f, col::dim);
    {
        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, r.getRight() - 9, r.getCentreY()));
        drawCaps (g, "Morph", { r.getRight() - 49, r.getCentreY() - 7, 80, 14 }, juce::Justification::centred, 11.0f, col::dim);
        g.restoreState();
    }
    drawCaps (g, "Crossfade", { r.getX() + 22, r.getBottom() - 20, 90, 14 }, juce::Justification::centredLeft, 10.0f, col::faint);
    drawCaps (g, "Full morph", { r.getX() + 8, r.getY() + 5, 90, 12 }, juce::Justification::centredLeft, 10.0f, col::faint);

    // animator path
    const float depth = rawOf (ctx, id::mmDepth);
    if (on && depth > 0.001f)
    {
        juce::Path path;
        for (int i = 0; i <= 300; ++i)
        {
            float px, py;
            Animator::shapePoint (choiceOf (ctx, id::mmMode), 2, 0.3f, i / 300.0, px, py);
            const auto pt = toScreen (hx + px * depth * 0.5f, hy + py * depth * 0.5f);
            if (i == 0) path.startNewSubPath (pt); else path.lineTo (pt);
        }
        g.setColour (col::mod.withAlpha (0.6f));
        g.strokePath (path, juce::PathStrokeType (1.2f));
    }

    // handle + live point
    const auto h = toScreen (hx, hy);
    g.setColour ((on ? col::accent : col::dim).withAlpha (0.18f));
    g.fillEllipse (juce::Rectangle<float> (30.0f, 30.0f).withCentre (h));
    g.setColour (on ? col::accent : col::dim);
    g.drawEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre (h), 1.6f);
    g.drawLine (h.x - 4, h.y, h.x + 4, h.y, 1.2f);
    g.drawLine (h.x, h.y - 4, h.x, h.y + 4, 1.2f);
    const auto lp = toScreen (cx, cy);
    g.setColour (col::text);
    g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (lp));

    g.setColour (col::edge);
    g.strokePath (frame, juce::PathStrokeType (1.0f));
}

void MorphPad::setFromMouse (juce::Point<float> p)
{
    auto r = area().reduced (14.0f);
    const float x = juce::jlimit (0.0f, 1.0f, (p.x - r.getX()) / r.getWidth());
    const float y = juce::jlimit (0.0f, 1.0f, (r.getBottom() - p.y) / r.getHeight());
    ctx.apvts.getParameter (id::morphX)->setValueNotifyingHost (x);
    ctx.apvts.getParameter (id::morphY)->setValueNotifyingHost (y);
    repaint();
}

void MorphPad::mouseDown (const juce::MouseEvent& e)
{
    ctx.apvts.getParameter (id::morphX)->beginChangeGesture();
    ctx.apvts.getParameter (id::morphY)->beginChangeGesture();
    dragging = true;
    setFromMouse (e.position);
}

void MorphPad::mouseDrag (const juce::MouseEvent& e) { if (dragging) setFromMouse (e.position); }

void MorphPad::mouseUp (const juce::MouseEvent&)
{
    if (! dragging) return;
    ctx.apvts.getParameter (id::morphX)->endChangeGesture();
    ctx.apvts.getParameter (id::morphY)->endChangeGesture();
    dragging = false;
}

void MorphPad::mouseDoubleClick (const juce::MouseEvent&)
{
    for (auto pr : { std::pair<juce::String, float> { id::morphX, 0.5f }, std::pair<juce::String, float> { id::morphY, 1.0f } })
    {
        auto* p = ctx.apvts.getParameter (pr.first);
        p->beginChangeGesture(); p->setValueNotifyingHost (pr.second); p->endChangeGesture();
    }
}

// ----------------------------------------------------------------------------
void SpectrumStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    auto frame = chamfer (r, 5.0f);
    g.setColour (col::bg);
    g.fillPath (frame);
    auto& m = ctx.proc.morph;
    const int n = SpectralMorph::displayBins;
    auto pathFor = [&] (const std::array<std::atomic<float>, SpectralMorph::displayBins>& d, bool closed)
    {
        juce::Path p;
        for (int i = 0; i < n; ++i)
        {
            const float x = r.getX() + r.getWidth() * (float) i / (float) (n - 1);
            const float y = r.getBottom() - 2.0f - d[(size_t) i].load() * (r.getHeight() - 6.0f);
            if (i == 0) { if (closed) { p.startNewSubPath (x, r.getBottom()); p.lineTo (x, y); } else p.startNewSubPath (x, y); }
            else p.lineTo (x, y);
        }
        if (closed) { p.lineTo (r.getRight(), r.getBottom()); p.closeSubPath(); }
        return p;
    };
    const bool on = flagOf (ctx, id::morphOn);
    if (! on && ! flagOf (ctx, id::formOn))
    {
        g.setColour (col::edge);
        g.strokePath (frame, juce::PathStrokeType (1.0f));
        g.setFont (fontLight (12.5f));
        g.setColour (col::faint);
        g.drawText ("Spektrum A / B / Out - erscheint, wenn Morph oder Formants an sind", r.reduced (8.0f), juce::Justification::centred);
        return;
    }
    g.setColour (col::sigA.withAlpha (0.22f)); g.fillPath (pathFor (m.dispA, true));
    if (on) { g.setColour (col::sigB.withAlpha (0.22f)); g.fillPath (pathFor (m.dispB, true)); }
    g.setColour (col::text.withAlpha (0.85f)); g.strokePath (pathFor (m.dispOut, false), juce::PathStrokeType (1.2f));
    g.setColour (col::edge);
    g.strokePath (frame, juce::PathStrokeType (1.0f));

    auto legend = r.reduced (6.0f, 3.0f).removeFromTop (12.0f);
    drawCaps (g, "A", legend.removeFromLeft (16), juce::Justification::centredLeft, 10.5f, col::sigA);
    drawCaps (g, "B", legend.removeFromLeft (16), juce::Justification::centredLeft, 10.5f, on ? col::sigB : col::faint);
    drawCaps (g, "Out", legend.removeFromLeft (30), juce::Justification::centredLeft, 10.5f, col::text);
}

// ----------------------------------------------------------------------------
void NoteStrip::update()
{
    const auto s = ctx.proc.currentScale();
    const int n = ctx.proc.modeler.currentNote.load();
    if (s.mask != mask || n != note) { mask = s.mask; note = n; repaint(); }
}

void NoteStrip::paint (juce::Graphics& g)
{
    static const bool black[12] { false, true, false, true, false, false, true, false, true, false, true, false };
    const float w = (float) getWidth() / 12.0f;
    for (int pc = 0; pc < 12; ++pc)
    {
        auto r = juce::Rectangle<float> (pc * w, 0, w, (float) getHeight()).reduced (1.0f, 0);
        const bool sounding = note >= 0 && note % 12 == pc;
        g.setColour (sounding ? col::accent : (mask[(size_t) pc] ? col::panelHi.brighter (0.25f) : (black[pc] ? col::bg : col::panel)));
        g.fillRect (r);
        g.setColour (col::edge);
        g.drawRect (r, 1.0f);
        if (sounding || mask[(size_t) pc])
            drawCaps (g, noteNames()[pc], r, juce::Justification::centred, 10.0f, sounding ? col::bg : col::dim, true, 0.0f);
    }
}

// ----------------------------------------------------------------------------
SourceSwitch::SourceSwitch (Ctx& c) : ctx (c)
{
    setTooltip ("Signal B für MORPH: oben = Aux In (Sidechain), unten = Modeler. Klicken zum Umschalten."_de);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void SourceSwitch::update()
{
    const int s = choiceOf (ctx, id::morphSrc);
    if (s != src) { src = s; repaint(); }
}

void SourceSwitch::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float x0 = r.getX() + 4.0f, xOut = r.getRight() - 2.0f, yA = r.getY() + 7.0f, yM = r.getBottom() - 7.0f, yO = r.getCentreY();
    const bool aux = src == srcAux;
    // inactive input
    g.setColour (col::faint);
    g.drawLine (x0, aux ? yM : yA, x0 + 4.0f, aux ? yM : yA, 1.5f);
    // lever
    g.setColour (col::sigB);
    g.drawLine (x0, aux ? yA : yM, xOut - 4.0f, yO, 2.0f);
    g.drawLine (xOut - 4.0f, yO, xOut, yO, 2.0f);
    for (auto y : { yA, yM })
    {
        g.setColour (col::bg);
        g.fillEllipse (x0 - 3.0f, y - 3.0f, 6.0f, 6.0f);
        g.setColour (((aux && y == yA) || (! aux && y == yM)) ? col::sigB : col::faint);
        g.drawEllipse (x0 - 3.0f, y - 3.0f, 6.0f, 6.0f, 1.4f);
    }
}

void SourceSwitch::mouseUp (const juce::MouseEvent&)
{
    if (auto* p = ctx.apvts.getParameter (id::morphSrc))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (src == srcAux ? 1.0f : 0.0f);
        p->endChangeGesture();
    }
}

// ============================================================================
// Blocks
// ============================================================================
Block::Block (Ctx& c, const juce::String& title, const juce::String& powerParam) : Page (c)
{
    sec = &section (title, powerParam);
}

void Block::resized()
{
    sec->setBounds (getLocalBounds());
    layout (sec->content());
}

// ----------------------------------------------------------------------------
MainInBlock::MainInBlock (Ctx& c) : Block (c, "Main In"), gain (c, id::inputGain, "Gain", 40)
{
    sec->addAndMakeVisible (gain);
    sec->addAndMakeVisible (meter);
    sec->setAccent (col::sigA);
    sec->setHelp ("Main In - Signal A", "Das Audio deiner Spur. Gain passt den Eingangspegel an, bevor alles andere passiert.");
}

void MainInBlock::layout (juce::Rectangle<int> r)
{
    gain.setBounds (r.removeFromTop (22));
    r.removeFromTop (8);
    meter.setBounds (r.removeFromTop (6).withTrimmedLeft (40).withTrimmedRight (48));
}

void MainInBlock::update() { meter.setLevel (ctx.proc.mainLevel.load()); }

// ----------------------------------------------------------------------------
AuxInBlock::AuxInBlock (Ctx& c) : Block (c, "Aux In"), gain (c, id::auxGain, "Gain", 40)
{
    sec->addAndMakeVisible (gain);
    sec->addAndMakeVisible (meter);
    sec->addAndMakeVisible (status);
    setupInfoLabel (status, 12.0f);
    sec->setAccent (col::sigB);
    sec->setHelp ("Aux In - Sidechain",
                  "Ein zweites Signal von einer anderen Spur (Signal B).\n"
                  "Cubase: oben im Plugin-Fenster den Sidechain-Knopf aktivieren, dann auf der anderen Spur "
                  "einen Send auf 'Liquidation - Sidechain' legen.");
}

void AuxInBlock::layout (juce::Rectangle<int> r)
{
    gain.setBounds (r.removeFromTop (22));
    r.removeFromTop (8);
    meter.setBounds (r.removeFromTop (6).withTrimmedLeft (40).withTrimmedRight (48));
    r.removeFromTop (6);
    status.setBounds (r);
}

void AuxInBlock::update()
{
    meter.setLevel (ctx.proc.auxLevel.load());
    const bool connected = ctx.proc.auxConnected.load();
    const bool signal = ctx.proc.auxLevel.load() > 0.0005f;
    const juce::String t = ! connected ? "Sidechain nicht aktiv - siehe Hilfe (?)"
                                       : (signal ? "Sidechain-Signal liegt an" : "Sidechain aktiv, aber still");
    if (status.getText() != t)
    {
        status.setText (t, juce::dontSendNotification);
        status.setColour (juce::Label::textColourId, signal ? col::sigB : col::dim);
    }
}

// ----------------------------------------------------------------------------
ModelerBlock::ModelerBlock (Ctx& c)
    : Block (c, "Modeler"), modes (c.apvts, id::mdlMode, modelerModeNames(), 3), notes (c)
{
    sec->setAccent (col::sigB);
    sec->addAndMakeVisible (modes);
    modes.setTooltips ({ "Akkord-Fläche in deiner Tonart"_de, "Arpeggio im Songtempo"_de, "Grundton + Quinte, stehend"_de,
                         "Rollender Psy-Bass (Pause auf der Zählzeit)"_de, "Gesangsartige Vokal-Fläche"_de, "Gefiltertes Rauschen"_de });
    octave = &knob (id::mdlOctave, "Octave", sec);
    tone = &knob (id::mdlTone, "Tone", sec);
    motion = &knob (id::mdlMotion, "Motion", sec);
    tone->setFormatter (pct);
    motion->setFormatter (pct);
    octave->setFormatter ([] (float v) { const int o = (int) std::lround (v); return (o > 0 ? "+" : "") + juce::String (o); });
    rate = &combo (id::mdlRate, sec);
    follow = &toggle (id::mdlFollow, "Follow", sec);
    follow->setTooltip ("Folgt der Tonhöhe deiner Spur (Signal A)"_de);
    sec->addAndMakeVisible (notes);
    sec->addAndMakeVisible (meter);
    rateLabel.setText ("RATE", juce::dontSendNotification);
    rateLabel.setFont (font (12.0f, true));
    rateLabel.setColour (juce::Label::textColourId, col::text.withAlpha (0.8f));
    sec->addAndMakeVisible (rateLabel);
    hint.setText ("Interne Klangquelle für Signal B - in Tonart und Tempo. "_de
                  "Der Schalter rechts wählt Aux oder Modeler als B."_de, juce::dontSendNotification);
    setupInfoLabel (hint, 12.0f);
    sec->addAndMakeVisible (hint);
    sec->setHelp ("Modeler - interne Quelle",
                  "Erzeugt Signal B selbst, wenn du keinen Sidechain hast. Spielt immer in deiner Tonart und im Songtempo.\n"
                  "Mode = Klang, Octave = Lage, Tone = Helligkeit, Motion = Bewegung, Rate = Notenlänge (Arp / Psy Bass).\n"_de
                  "Follow: folgt der Tonhöhe deiner Spur."_de);
}

void ModelerBlock::layout (juce::Rectangle<int> r)
{
    modes.setBounds (r.removeFromTop (51));
    r.removeFromTop (10);
    notes.setBounds (r.removeFromTop (26));
    r.removeFromTop (4);
    meter.setBounds (r.removeFromTop (5));
    r.removeFromTop (10);
    row (r.removeFromTop (kH), { octave, tone, motion }, 2);
    r.removeFromTop (8);
    auto rr = r.removeFromTop (rowH);
    rateLabel.setBounds (rr.removeFromLeft (40));
    follow->setBounds (rr.removeFromRight (62));
    rr.removeFromRight (6);
    rate->setBounds (rr);
    r.removeFromTop (12);
    hint.setBounds (r);
}

void ModelerBlock::update()
{
    notes.update();
    meter.setLevel (ctx.proc.modeler.level.load());
    const int m = choiceOf (ctx, id::mdlMode);
    rate->setEnabled (m == mdlArp || m == mdlPsyBass || m == mdlMelody);
    rateLabel.setAlpha (rate->isEnabled() ? 1.0f : 0.4f);
    const auto t = m == mdlMelody
        ? "Melodie-Modus: Stil, Variante, Kick-Auslöser und MIDI-Export findest du auf der Play-Seite."_de
        : "Interne Klangquelle für Signal B - in Tonart und Tempo. Der Schalter rechts wählt Aux oder Modeler als B."_de;
    if (hint.getText() != t) hint.setText (t, juce::dontSendNotification);
}

// ----------------------------------------------------------------------------
MorphBlock::MorphBlock (Ctx& c)
    : Block (c, "Morph", id::morphOn), pad (c), algos (c.apvts, id::morphAlgo, morphAlgoNames(), 1), spectrum (c)
{
    sec->addAndMakeVisible (pad);
    sec->addAndMakeVisible (algos);
    algos.setTooltips ({ kAlgoInfo[0], kAlgoInfo[1], kAlgoInfo[2], kAlgoInfo[3], kAlgoInfo[4] });
    setupInfoLabel (algoInfo, 12.0f);
    sec->addAndMakeVisible (algoInfo);
    gamma = &knob (id::morphGamma, "Gamma", sec);
    detail = &knob (id::morphDetail, "Detail", sec);
    depth = &knob (id::mmDepth, "Motion", sec);
    speed = &knob (id::mmRate, "Speed", sec);
    gamma->setFormatter ([] (float v) { return juce::String (v, 2) + "x"; });
    detail->setFormatter (pct);
    depth->setFormatter (pct);
    speed->setFormatter ([] (float v) { return juce::String (v, 2) + " Hz"; });
    gamma->setTooltip ("Gamma: Kontrast im Spektrum. Über 1 = spitzer, unter 1 = weicher."_de);
    detail->setTooltip ("Detail: wie fein die Klangfarbe übertragen wird (Imprint, Carve, Duck, Formanten)."_de);
    depth->setTooltip ("Motion: bewegt den Punkt im Pad automatisch (0 = aus).");
    shape = &combo (id::mmMode, sec);
    div = &combo (id::mmDiv, sec);
    sync = &toggle (id::mmSync, "Sync", sec);
    motionLabel.setText ("MOTION", juce::dontSendNotification);
    motionLabel.setFont (font (12.0f, true));
    motionLabel.setColour (juce::Label::textColourId, col::text.withAlpha (0.8f));
    sec->addAndMakeVisible (motionLabel);
    sec->addAndMakeVisible (spectrum);
    warning.setFont (font (12.0f));
    warning.setColour (juce::Label::textColourId, col::accent);
    warning.setJustificationType (juce::Justification::centredRight);
    sec->addAndMakeVisible (warning);
    sec->setHelp ("Morph",
                  "Verschmilzt Signal A (deine Spur) mit Signal B (Sidechain oder Modeler) im Frequenzbereich.\n"
                  "Pad: links/rechts = Anteil A <-> B, unten/oben = normales Überblenden <-> volles Morphing.\n"_de
                  "Blend: gemeinsame Frequenzen.  Imprint: A bekommt die Klangfarbe von B.  Carve: B wird von A geformt (Vocoder).  "
                  "Weave: das lautere Signal gewinnt.  Duck: A macht Platz, wo B spielt.\n"
                  "Motion bewegt den Punkt automatisch.");
}

void MorphBlock::layout (juce::Rectangle<int> r)
{
    auto top = r.removeFromTop (226);
    pad.setBounds (top.removeFromLeft (226));
    top.removeFromLeft (10);
    algos.setBounds (top.removeFromTop (5 * 24 + 4 * 3));
    top.removeFromTop (8);
    algoInfo.setBounds (top);
    r.removeFromTop (8);
    row (r.removeFromTop (kH), { gamma, detail, depth, speed }, 2);
    div->setBounds (speed->getBounds().withSizeKeepingCentre (speed->getWidth() - 4, rowH).translated (0, -10));
    r.removeFromTop (6);
    auto m = r.removeFromTop (rowH);
    motionLabel.setBounds (m.removeFromLeft (64));
    shape->setBounds (m.removeFromLeft (100));
    m.removeFromLeft (6);
    sync->setBounds (m.removeFromLeft (56));
    r.removeFromTop (10);
    spectrum.setBounds (r);
    warning.setBounds (r.removeFromTop (18).reduced (6, 2));
}

void MorphBlock::update()
{
    pad.update();
    spectrum.repaint();
    const int a = choiceOf (ctx, id::morphAlgo);
    if (a != lastAlgo)
    {
        lastAlgo = a;
        algoInfo.setText (kAlgoInfo[juce::jlimit (0, 4, a)], juce::dontSendNotification);
    }
    const bool envAlgo = a == morphImprint || a == morphCarve || a == morphDuck;
    detail->setAlpha (envAlgo || flagOf (ctx, id::formOn) ? 1.0f : 0.5f);
    const bool s = flagOf (ctx, id::mmSync);
    showIf (speed, ! s);
    showIf (div, s);

    juce::String w;
    if (flagOf (ctx, id::morphOn) && ! ctx.proc.morph.hasSignalB())
        w = choiceOf (ctx, id::morphSrc) == srcAux ? "Kein Signal B: Sidechain fehlt" : "Kein Signal B";
    if (warning.getText() != w) warning.setText (w, juce::dontSendNotification);
}

// ----------------------------------------------------------------------------
FormantBlock::FormantBlock (Ctx& c) : Block (c, "Formants", id::formOn)
{
    shift = &knob (id::formShift, "Shift", sec);
    shift->setFormatter ([] (float v) { return (v > 0 ? "+" : "") + juce::String (v, 1) + " st"; });
    info.setText ("Verschiebt die Klangfarbe ohne die Tonhöhe: + heller / kleiner, - dunkler / grösser."_de,
                  juce::dontSendNotification);
    setupInfoLabel (info, 12.5f);
    sec->addAndMakeVisible (info);
    sec->setHelp ("Formants", "Verschiebt die Formanten (Klangfarbe) des Morph-Ergebnisses, ohne die Tonhöhe zu ändern. "_de
                              "Funktioniert auch ohne Morph, z. B. für Stimmen."_de);
}

void FormantBlock::layout (juce::Rectangle<int> r)
{
    shift->setBounds (r.removeFromLeft (96).withSizeKeepingCentre (96, std::min (r.getHeight(), 84)));
    r.removeFromLeft (12);
    info.setBounds (r.reduced (0, 10));
}

// ----------------------------------------------------------------------------
GrainBlock::GrainBlock (Ctx& c)
    : Block (c, "Grain Engine", id::grainOn), wave (c), mode (c.apvts, id::playMode, { "Stretch", "Scrub" }, 2)
{
    sec->addDetailsButton ([this] { if (ctx.openDetails) ctx.openDetails (0); });
    sec->addAndMakeVisible (wave);
    sec->addAndMakeVisible (mode);
    rateMode = &combo (id::rateMode, sec);
    chord = &combo (id::chordMode, sec);
    chord->setTooltip ("Chord / Arp: Grains als Akkord oder Arpeggio in deiner Tonart (Intervalle in Details)");
    rate = &knob (id::grainRate, "Rate", sec);
    rate->setFormatter ([this] (float v)
    {
        return GrainEngine::rateText (choiceOf (ctx, id::rateMode), v, ctx.proc.currentScale(), ctx.proc.hostBpm.load());
    });
    sizeRatio = &knob (id::sizeRatio, "Size", sec);
    sizeRatio->setFormatter ([] (float v) { return juce::String (v, 2) + "x"; });
    sizeMs = &knob (id::grainSize, "Size", sec);
    stretch = &knob (id::stretch, "Stretch", sec);
    stretch->setFormatter ([this] (float v)
    {
        const float sp = GrainEngine::stretchFactor (v, flagOf (ctx, id::stretchSync));
        if (sp <= 0.0f) return juce::String ("Freeze");
        return juce::String (1.0f / sp, 1.0f / sp < 10 ? 2 : 1) + "x";
    });
    position = &knob (id::scrubPos, "Position", sec);
    position->setFormatter (pct);
    pitch = &knob (id::pitch, "Pitch", sec);
    pitch->setFormatter ([this] (float v)
    {
        const bool deg = choiceOf (ctx, id::pitchMode) == pitchScale && ! ctx.proc.currentScale().chromatic;
        if (deg) { const int d = (int) std::lround (v); return (d > 0 ? "+" : "") + juce::String (d) + " Stufen"; }
        return (v > 0 ? "+" : "") + juce::String (v, 1) + " st";
    });
    spray = &knob (id::spray, "Spray", sec);
    blur = &knob (id::blur, "Blur", sec);
    reverse = &knob (id::revProb, "Reverse", sec);
    for (auto* k : { spray, blur, reverse }) k->setFormatter (pct);
    rate->setTooltip ("Wie oft ein neues Grain startet.");
    stretch->setTooltip ("Verlangsamt die Wiedergabe bis zum Einfrieren (Freeze), Tonhöhe bleibt."_de);
    spray->setTooltip ("Zufällige Startposition je Grain - macht den Klang diffuser."_de);
    blur->setTooltip ("Glättet Stretch und Freeze zu einer Fläche."_de);
    pitch->setTooltip ("Transponiert die Grains - im Scale-Modus in Stufen deiner Tonart.");
    sec->setHelp ("Grain Engine",
                  "Zerlegt das Signal in kurze Stücke (Grains) und setzt es neu zusammen.\n"_de
                  "Stretch: dehnt bis zum Einfrieren. Scrub: du fährst mit Position durch den aufgenommenen Loop.\n"_de
                  "Pitch transponiert in deiner Tonart. Chord/Arp macht Akkorde oder Arpeggios.\n"
                  "'Details' öffnet alle Einstellungen (Intervalle, Skala, Fenster, Reverse ...)."_de);
}

void GrainBlock::layout (juce::Rectangle<int> r)
{
    wave.setBounds (r.removeFromTop (110));
    r.removeFromTop (8);
    auto m = r.removeFromTop (rowH);
    mode.setBounds (m.removeFromLeft (130));
    m.removeFromLeft (8);
    rateMode->setBounds (m.removeFromLeft (100));
    m.removeFromLeft (8);
    chord->setBounds (m);
    r.removeFromTop (8);
    row (r.removeFromTop (kH), { rate, sizeRatio, stretch, pitch }, 2);
    sizeMs->setBounds (sizeRatio->getBounds());
    position->setBounds (stretch->getBounds());
    r.removeFromTop (4);
    row (r.removeFromTop (kH), { spray, blur, reverse, nullptr }, 2);
}

void GrainBlock::update()
{
    wave.update();
    const bool linked = flagOf (ctx, id::sizeLink);
    showIf (sizeMs, ! linked);
    showIf (sizeRatio, linked);
    const bool scrub = choiceOf (ctx, id::playMode) == playScrub;
    showIf (stretch, ! scrub);
    showIf (position, scrub);
    rate->repaint();
}

// ----------------------------------------------------------------------------
GrainFxBlock::GrainFxBlock (Ctx& c)
    : Block (c, "Grain FX"), fltOn (c.apvts, id::fltOn, "Filter"), imgOn (c.apvts, id::imgOn, "Imager"),
      dlyOn (c.apvts, id::dlyOn, "Delay")
{
    sec->addDetailsButton ([this] { if (ctx.openDetails) ctx.openDetails (1); });
    for (auto* t : { &fltOn, &imgOn, &dlyOn }) sec->addAndMakeVisible (t);
    fltMode = &combo (id::fltMode, sec);
    spreadMode = &combo (id::spreadMode, sec);
    pattern = &combo (id::dlyPattern, sec);
    cutoff = &knob (id::fltCutoff, "Cutoff", sec);
    reso = &knob (id::fltReso, "Reso", sec);
    spread = &knob (id::spread, "Spread", sec);
    width = &knob (id::width, "Width", sec);
    feedback = &knob (id::dlyFeedback, "Feedback", sec);
    mix = &knob (id::dlyMix, "Mix", sec);
    cutoff->setFormatter ([] (float v) { return v >= 1000 ? juce::String (v / 1000, 1) + "k" : juce::String ((int) v); });
    for (auto* k : { reso, spread, width, feedback, mix }) k->setFormatter (pct);
    sec->setHelp ("Grain FX",
                  "Effekte auf jedes einzelne Grain: Filter (12 Typen), Imager (Stereo-Verteilung der Grains) und "
                  "Grain-Delay (Echos aus Grains, auch mit Tonhöhen-Mustern). 'Details' für alle Parameter."_de);
}

void GrainFxBlock::layout (juce::Rectangle<int> r)
{
    const int h = (r.getHeight() - 8) / 3;
    auto rowLayout = [&] (juce::Rectangle<int> rr, juce::Component* t, juce::Component* c, juce::Component* k1, juce::Component* k2)
    {
        t->setBounds (rr.removeFromLeft (84).withSizeKeepingCentre (84, rowH));
        c->setBounds (rr.removeFromLeft (104).withSizeKeepingCentre (104, rowH));
        rr.removeFromLeft (6);
        row (rr, { k1, k2 }, 2);
    };
    rowLayout (r.removeFromTop (h), &fltOn, fltMode, cutoff, reso);
    r.removeFromTop (4);
    rowLayout (r.removeFromTop (h), &imgOn, spreadMode, spread, width);
    r.removeFromTop (4);
    rowLayout (r.removeFromTop (h), &dlyOn, pattern, feedback, mix);
}

void GrainFxBlock::update()
{
    auto dim = [] (bool on, std::initializer_list<juce::Component*> cs) { for (auto* c : cs) c->setAlpha (on ? 1.0f : 0.4f); };
    dim (fltOn.getToggleState(), { fltMode, cutoff, reso });
    dim (imgOn.getToggleState(), { spreadMode, spread, width });
    dim (dlyOn.getToggleState(), { pattern, feedback, mix });
}

// ----------------------------------------------------------------------------
TransientBlock::TransientBlock (Ctx& c) : Block (c, "Transients", id::trOn)
{
    sens = &knob (id::trSens, "Sense", sec);
    length = &knob (id::trLength, "Length", sec);
    amount = &knob (id::trAmount, "Amount", sec);
    sens->setFormatter (pct);
    amount->setFormatter (pct);
    length->setFormatter ([] (float v) { return juce::String ((int) std::lround (v)) + " ms"; });
    sec->setHelp ("Transients",
                  "Hält den Anschlag des Originals (z. B. den Punch einer Kick), während der Rest bearbeitet wird.\n"_de
                  "Sense = Empfindlichkeit, Length = wie lange der Original-Anschlag bleibt, Amount = wie stark.");
}

void TransientBlock::layout (juce::Rectangle<int> r) { row (r.removeFromTop (kH), { sens, length, amount }, 2); }

void TransientBlock::update()
{
    const float g = ctx.proc.transients.gateDisplay.load();
    if (std::abs (g - gate) > 0.02f) { gate = g; repaint(); }
}

void TransientBlock::paintOverChildren (juce::Graphics& g)
{
    auto led = juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ (float) getWidth() - 18.0f, 15.0f });
    g.setColour (gate > 0.05f ? col::accent.withAlpha (0.3f + 0.7f * gate) : col::faint);
    g.fillEllipse (led);
}

// ----------------------------------------------------------------------------
MixerBlock::MixerBlock (Ctx& c) : Block (c, "Mixer")
{
    channels.add (new MixChannel (c, 0, id::mixWet, "Morph", col::accent));
    channels.add (new MixChannel (c, 1, id::mixDry, "Main In", col::sigA));
    channels.add (new MixChannel (c, 2, id::mixAux, "Aux In", col::sigB));
    channels.add (new MixChannel (c, 3, id::mixModel, "Modeler", col::sigB.darker (0.3f)));
    for (auto* ch : channels) sec->addAndMakeVisible (ch);
    revLabel.setText ("REVERB", juce::dontSendNotification);
    revLabel.setFont (font (12.0f, true));
    revLabel.setColour (juce::Label::textColourId, col::text.withAlpha (0.8f));
    sec->addAndMakeVisible (revLabel);
    size = &knob (id::revSize, "Size", sec);
    damp = &knob (id::revDamp, "Damp", sec);
    amount = &knob (id::revMix, "Amount", sec);
    for (auto* k : { size, damp, amount }) k->setFormatter (pct);
    sec->setHelp ("Mixer",
                  "Pegel der vier Signale: Morph = das bearbeitete Signal, Main In = Original deiner Spur, "
                  "Aux In und Modeler = Signal B pur. M = stumm, S = solo.\n"
                  "Main In hochziehen = Dry/Wet. Reverb legt Raum auf die Summe.");
}

void MixerBlock::layout (juce::Rectangle<int> r)
{
    for (auto* ch : channels)
    {
        ch->setBounds (r.removeFromTop (26));
        r.removeFromTop (2);
    }
    r.removeFromTop (8);
    revLabel.setBounds (r.removeFromTop (16));
    row (r.removeFromTop (kH), { size, damp, amount }, 2);
}

// ----------------------------------------------------------------------------
EffectsBlock::Slot::Slot (Ctx& c, int index) : ctx (c), k (index), power (c.apvts, id::fx (index, "On"))
{
    addAndMakeVisible (power);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void EffectsBlock::Slot::resized() { power.setBounds (4, 0, 20, getHeight()); }

void EffectsBlock::Slot::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    auto p = chamfer (r, 4.0f);
    g.setColour (isMouseOver() ? col::panelHi.brighter (0.08f) : col::panelHi);
    g.fillPath (p);
    g.setColour (power.getToggleState() ? col::accent.withAlpha (0.6f) : col::edge);
    g.strokePath (p, juce::PathStrokeType (1.0f));
    drawCaps (g, name, r.withTrimmedLeft (26.0f), juce::Justification::centredLeft, 12.0f,
              power.getToggleState() ? col::text : col::dim, true, 0.05f);
}

void EffectsBlock::Slot::mouseUp (const juce::MouseEvent& e)
{
    if (e.x > 24 && ctx.openDetails) ctx.openDetails (2);
}

EffectsBlock::EffectsBlock (Ctx& c) : Block (c, "Effects"), routing (c.apvts, id::routing, { "Post", "Pre" }, 2)
{
    for (int k = 1; k <= numFxSlots; ++k)
    {
        auto* s = slots.add (new Slot (c, k));
        sec->addAndMakeVisible (s);
    }
    routing.setTooltips ({ "Post: Effekte nur auf das bearbeitete Signal (Morph)"_de, "Pre: Effekte auf die ganze Summe"_de });
    sec->addAndMakeVisible (routing);
    sec->setHelp ("Effects",
                  "Vier Effekt-Slots (Chorus, Compressor, Delay, Distortion, EQ, Filter, Reverb, Shifter). "
                  "Punkt links = ein/aus, Klick auf den Namen öffnet die Einstellungen.\n"_de
                  "Post = nur auf das bearbeitete Signal, Pre = auf die ganze Summe.");
}

void EffectsBlock::layout (juce::Rectangle<int> r)
{
    routing.setBounds (sec->getWidth() - 90, 6, 80, 18);
    const int w = (r.getWidth() - 4) / 2;
    for (int i = 0; i < slots.size(); ++i)
        slots[i]->setBounds (r.getX() + (i % 2) * (w + 4), r.getY() + (i / 2) * 28, w, 25);
}

void EffectsBlock::update()
{
    for (auto* s : slots)
    {
        const auto n = fxTypeNames()[choiceOf (ctx, id::fx (s->k, "Type"))];
        if (n != s->name) { s->name = n; s->repaint(); }
    }
}

// ----------------------------------------------------------------------------
OutputBlock::OutputBlock (Ctx& c) : Block (c, "Output"), gain (c, id::outputGain, "Gain", 40)
{
    sec->addAndMakeVisible (gain);
    limit = &toggle (id::limitOn, "Limit", sec);
    limit->setTooltip ("Limiter: verhindert Übersteuern (Decke -0.3 dB)"_de);
    sec->addAndMakeVisible (meter);
    gr.setFont (fontLight (12.0f));
    gr.setColour (juce::Label::textColourId, col::dim);
    gr.setJustificationType (juce::Justification::centredRight);
    sec->addAndMakeVisible (gr);
    sec->setHelp ("Output", "Ausgangspegel. Limit verhindert Übersteuerung, die Anzeige rechts zeigt die Pegelreduktion."_de);
}

void OutputBlock::layout (juce::Rectangle<int> r)
{
    gain.setBounds (r.removeFromTop (22));
    r.removeFromTop (8);
    auto rr = r.removeFromTop (20);
    limit->setBounds (rr.removeFromLeft (56));
    rr.removeFromLeft (8);
    gr.setBounds (rr.removeFromRight (56));
    rr.removeFromRight (4);
    meter.setBounds (rr.withSizeKeepingCentre (rr.getWidth(), 6));
}

void OutputBlock::update()
{
    meter.setLevel (ctx.proc.outputLevel.load());
    const float red = ctx.proc.limiter.reductionDb.load();
    const juce::String t = red > 0.1f ? "-" + juce::String (red, 1) + " dB" : juce::String();
    if (gr.getText() != t) gr.setText (t, juce::dontSendNotification);
}

// ============================================================================
// Canvas
// ============================================================================
namespace
{
    // block layout in canvas coordinates (canvas is 1280 x 656)
    const juce::Rectangle<int> rMain    {   14,  12, 190,  92 };
    const juce::Rectangle<int> rAux     {   14, 114, 190, 126 };
    const juce::Rectangle<int> rModeler {   14, 250, 190, 394 };
    const juce::Rectangle<int> rMorph   {  238,  12, 370, 492 };
    const juce::Rectangle<int> rFormant {  238, 516, 370, 128 };
    const juce::Rectangle<int> rGrain   {  628,  12, 370, 384 };
    const juce::Rectangle<int> rGrainFx {  628, 408, 370, 236 };
    const juce::Rectangle<int> rTrans   { 1018,  12, 248, 118 };
    const juce::Rectangle<int> rMixer   { 1018, 140, 248, 286 };
    const juce::Rectangle<int> rEffects { 1018, 436, 248, 104 };
    const juce::Rectangle<int> rOutput  { 1018, 550, 248,  94 };
    const juce::Rectangle<int> rSwitch  {  208, 170,  26, 140 };
}

SignalCanvas::SignalCanvas (Ctx& c)
    : ctx (c), mainIn (c), auxIn (c), modeler (c), morph (c), formant (c), grain (c), grainFx (c),
      transients (c), mixer (c), effects (c), output (c), sourceSwitch (c)
{
    for (auto* b : blocks()) addAndMakeVisible (b);
    addAndMakeVisible (sourceSwitch);
}

juce::Array<Block*> SignalCanvas::blocks() const
{
    auto* self = const_cast<SignalCanvas*> (this);
    return { &self->mainIn, &self->auxIn, &self->modeler, &self->morph, &self->formant, &self->grain,
             &self->grainFx, &self->transients, &self->mixer, &self->effects, &self->output };
}

void SignalCanvas::resized()
{
    mainIn.setBounds (rMain);       auxIn.setBounds (rAux);         modeler.setBounds (rModeler);
    morph.setBounds (rMorph);       formant.setBounds (rFormant);
    grain.setBounds (rGrain);       grainFx.setBounds (rGrainFx);
    transients.setBounds (rTrans);  mixer.setBounds (rMixer);       effects.setBounds (rEffects);
    output.setBounds (rOutput);
    sourceSwitch.setBounds (rSwitch);
}

void SignalCanvas::paint (juce::Graphics& g)
{
    g.fillAll (col::canvas);
    // subtle dot grid
    g.setColour (col::panel.brighter (0.06f));
    for (int y = 6; y < getHeight(); y += 16)
        for (int x = 6; x < getWidth(); x += 16)
            g.fillRect ((float) x, (float) y, 1.0f, 1.0f);

    const bool morphOn = flagOf (ctx, id::morphOn);
    const bool auxSel = choiceOf (ctx, id::morphSrc) == srcAux;

    auto wire = [&] (std::initializer_list<juce::Point<float>> pts, juce::Colour c, bool active, bool dashed = false)
    {
        juce::Path p;
        bool first = true;
        for (auto pt : pts) { if (first) p.startNewSubPath (pt); else p.lineTo (pt); first = false; }
        g.setColour (active ? c : col::faint);
        if (dashed)
        {
            juce::Path d;
            const float dash[] { 5.0f, 4.0f };
            juce::PathStrokeType (1.2f).createDashedStroke (d, p, dash, 2);
            g.fillPath (d);
        }
        else
        {
            g.strokePath (p, juce::PathStrokeType (active ? 1.6f : 1.2f));
        }
    };
    using P = juce::Point<float>;
    const float mainY = (float) rMain.getCentreY();
    const float auxY = (float) rSwitch.getY() + 7.0f;
    const float mdlY = (float) rSwitch.getBottom() - 7.0f;
    const float bY = (float) rSwitch.getCentreY();

    // inputs -> morph
    wire ({ P (rMain.getRight(), mainY), P (rMorph.getX(), mainY) }, col::sigA, true);
    wire ({ P (rAux.getRight(), (float) rAux.getCentreY()), P (rSwitch.getX() + 4.0f, (float) rAux.getCentreY()),
            P (rSwitch.getX() + 4.0f, auxY) }, col::sigB, auxSel);
    wire ({ P (rModeler.getRight(), mdlY), P (rSwitch.getX() + 4.0f, mdlY) }, col::sigB, ! auxSel);
    wire ({ P (rSwitch.getRight(), bY), P (rMorph.getX(), bY) }, col::sigB, morphOn);

    // morph -> formants -> grain -> grain fx -> transients -> mixer -> effects -> output
    const float mx = (float) rMorph.getCentreX();
    wire ({ P (mx, rMorph.getBottom()), P (mx, rFormant.getY()) }, col::line, true);
    const float fy = (float) rFormant.getCentreY();
    wire ({ P (rFormant.getRight(), fy), P (rFormant.getRight() + 10.0f, fy), P (rFormant.getRight() + 10.0f, rGrain.getY() + 120.0f),
            P (rGrain.getX(), rGrain.getY() + 120.0f) }, col::line, true);
    const float gx = (float) rGrain.getCentreX();
    wire ({ P (gx, rGrain.getBottom()), P (gx, rGrainFx.getY()) }, col::line, true);
    const float gy = (float) rGrainFx.getCentreY();
    wire ({ P (rGrainFx.getRight(), gy), P (rGrainFx.getRight() + 10.0f, gy), P (rGrainFx.getRight() + 10.0f, rTrans.getCentreY()),
            P (rTrans.getX(), rTrans.getCentreY()) }, col::line, true);
    const float cx4 = (float) rMixer.getCentreX();
    wire ({ P (cx4, rTrans.getBottom()), P (cx4, rMixer.getY()) }, col::line, true);
    wire ({ P (cx4, rMixer.getBottom()), P (cx4, rEffects.getY()) }, col::line, true);
    wire ({ P (cx4, rEffects.getBottom()), P (cx4, rOutput.getY()) }, col::line, true);

    // dry bus: originals straight to the mixer (time aligned)
    const float busY = (float) rModeler.getBottom() + 6.0f;
    wire ({ P (rModeler.getCentreX(), rModeler.getBottom()), P (rModeler.getCentreX(), busY),
            P (rMixer.getX() - 6.0f, busY), P (rMixer.getX() - 6.0f, rMixer.getY() + 60.0f), P (rMixer.getX(), rMixer.getY() + 60.0f) },
          col::dim, true, true);
}

void SignalCanvas::update()
{
    ++frame;
    for (auto* b : blocks()) b->update();
    sourceSwitch.update();
    // the wires only change when the routing changes
    const int state = (flagOf (ctx, id::morphOn) ? 1 : 0) + 2 * choiceOf (ctx, id::morphSrc);
    if (state != lastWireState) { lastWireState = state; repaint(); }
}

void SignalCanvas::refreshModulation()
{
    for (auto* b : blocks()) b->refreshModulation();
}

// ============================================================================
// Top bar
// ============================================================================
TopBar::TopBar (Ctx& c) : Page (c), tune (c, id::globalTune, "A4")
{
    addAndMakeVisible (presetBox);
    presetBox.setTextWhenNothingSelected ("Init");
    presetBox.setTooltip ("Presets");
    presetBox.onChange = [this]
    {
        const int i = presetBox.getSelectedId() - 1;
        if (i >= 0) ctx.proc.presets.load (i);
    };
    for (auto* b : { &prev, &next, &save, &help }) addAndMakeVisible (b);
    prev.onClick = [this]
    {
        const int n = ctx.proc.presets.numPresets();
        ctx.proc.presets.load ((ctx.proc.presets.currentIndex() - 1 + n) % n);
    };
    next.onClick = [this]
    {
        const int n = ctx.proc.presets.numPresets();
        ctx.proc.presets.load ((ctx.proc.presets.currentIndex() + 1) % n);
    };
    save.onClick = [this] { savePreset(); };
    save.setTooltip ("Aktuellen Klang als eigenes Preset speichern");
    help.setClickingTogglesState (true);
    help.setTooltip ("Hilfe-Modus: Maus über einen Block zeigt eine Erklärung"_de);

    root = &combo (id::scaleRoot);
    scale = &combo (id::scaleType);
    root->setTooltip ("Grundton deiner Tonart");
    scale->setTooltip ("Skala - gilt für alles, was 'in Key' arbeitet"_de);
    addAndMakeVisible (tune);
    for (auto* b : { &playTab, &editTab })
    {
        b->setRadioGroupId (4001);
        b->setClickingTogglesState (true);
        addAndMakeVisible (b);
    }
    playTab.getProperties().set ("helpTitle", juce::String ("Play"));
    playTab.getProperties().set ("helpText", "Spielen: Preset-Finder, XY-Pad, Melodie-Generator und die wichtigsten Regler."_de);
    editTab.getProperties().set ("helpTitle", juce::String ("Edit"));
    editTab.getProperties().set ("helpText", "Bearbeiten: der ganze Signalfluss mit allen Blöcken und Detailseiten."_de);
    playTab.onClick = [this] { if (playTab.getToggleState() && onPage) onPage (0); };
    editTab.onClick = [this] { if (editTab.getToggleState() && onPage) onPage (1); };
    refreshPresetList();
}

void TopBar::setPage (int p)
{
    (p == 0 ? playTab : editTab).setToggleState (true, juce::dontSendNotification);
}

void TopBar::refreshPresetList()
{
    presetBox.clear (juce::dontSendNotification);
    const auto names = ctx.proc.presets.allNames();
    const auto cats = ctx.proc.presets.allCategories();
    juce::String lastCat;
    for (int i = 0; i < names.size(); ++i)
    {
        if (cats[i] != lastCat) { presetBox.addSectionHeading (cats[i]); lastCat = cats[i]; }
        presetBox.addItem (names[i], i + 1);
    }
    lastCount = names.size();
}

void TopBar::savePreset()
{
    auto* w = new juce::AlertWindow ("Preset speichern", "Name für das neue Preset:"_de, juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", ctx.proc.presets.currentName());
    w->addButton ("Speichern", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Abbrechen", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int result)
    {
        if (result == 1)
        {
            const auto name = w->getTextEditorContents ("name");
            if (ctx.proc.presets.saveUser (name, ctx.proc.fullState()))
                refreshPresetList();
        }
    }), true);
}

void TopBar::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::bg);
    g.fillRect (r);
    g.setColour (col::edge);
    g.drawHorizontalLine (getHeight() - 1, 0, (float) getWidth());

    // mark: two overlapping rings (A + B)
    const float cy = r.getCentreY();
    g.setColour (col::sigA);
    g.drawEllipse (18.0f, cy - 9.0f, 18.0f, 18.0f, 1.8f);
    g.setColour (col::sigB);
    g.drawEllipse (28.0f, cy - 9.0f, 18.0f, 18.0f, 1.8f);
    drawCaps (g, "Liquidation", { 56.0f, cy - 16.0f, 200.0f, 22.0f }, juce::Justification::centredLeft, 21.0f, col::text, true, 0.22f);
    g.setFont (fontLight (12.0f).withExtraKerningFactor (0.06f));
    g.setColour (col::dim);
    g.drawText ("GRAIN  /  MORPH  -  JUSTCREATEIT", juce::Rectangle<float> (57.0f, cy + 5.0f, 220.0f, 14.0f), juce::Justification::centredLeft);

    drawCaps (g, "Key", { 724.0f, cy - 8.0f, 30.0f, 16.0f }, juce::Justification::centredLeft, 11.5f, col::dim);
}

void TopBar::resized()
{
    auto r = getLocalBounds().reduced (12, 13);
    r.removeFromLeft (290);
    auto presets = r.removeFromLeft (400);
    prev.setBounds (presets.removeFromLeft (28));
    presets.removeFromLeft (4);
    save.setBounds (presets.removeFromRight (60));
    presets.removeFromRight (4);
    next.setBounds (presets.removeFromRight (28));
    presets.removeFromRight (4);
    presetBox.setBounds (presets);

    r.removeFromLeft (52);
    root->setBounds (r.removeFromLeft (58));
    r.removeFromLeft (4);
    scale->setBounds (r.removeFromLeft (170));
    r.removeFromLeft (10);
    tune.setBounds (r.removeFromLeft (110));
    help.setBounds (r.removeFromRight (34));
    r.removeFromRight (10);
    editTab.setBounds (r.removeFromRight (54));
    r.removeFromRight (2);
    playTab.setBounds (r.removeFromRight (54));
}

void TopBar::update()
{
    const auto n = ctx.proc.presets.allNames().size();
    if (n != lastCount) refreshPresetList();
    const auto name = ctx.proc.presets.currentName();
    if (name != lastName || presetBox.getText() != name)
    {
        lastName = name;
        const int idx = ctx.proc.presets.currentIndex();
        if (presetBox.getItemText (presetBox.indexOfItemId (idx + 1)) == name)
            presetBox.setSelectedId (idx + 1, juce::dontSendNotification);
        else
            presetBox.setText (name, juce::dontSendNotification);
    }
}

// ============================================================================
// Modulation bar
// ============================================================================
ModBar::ModBar (Ctx& c) : Page (c)
{
    const auto names = modSourceNames();
    for (int s = 1; s <= 6; ++s)
    {
        auto chip = std::make_unique<ModChip> (ctx, s, names[s]);
        addAndMakeVisible (*chip);
        if (s >= 3) chip->onClick = [this, s] { if (onModulatorClicked) onModulatorClicked (s - 3); };
        chips[(size_t) s - 1] = std::move (chip);
    }
    m1 = &knob (id::macro1, "Macro 1");
    m2 = &knob (id::macro2, "Macro 2");
    m1->setFormatter (pct);
    m2->setFormatter (pct);
    addAndMakeVisible (matrixButton);
    matrixButton.onClick = [this] { if (onMatrix) onMatrix(); };
    matrixButton.getProperties().set ("modColour", true);
}

void ModBar::paint (juce::Graphics& g)
{
    g.setColour (col::bg);
    g.fillRect (getLocalBounds());
    g.setColour (col::edge);
    g.drawHorizontalLine (0, 0, (float) getWidth());
    drawCaps (g, "Modulation", { 14.0f, 4.0f, 120.0f, 14.0f }, juce::Justification::centredLeft, 11.5f, col::mod);
    g.setFont (fontLight (12.0f));
    g.setColour (col::dim);
    g.drawText ("Baustein auf einen Regler ziehen  -  Rechtsklick auf Regler: entfernen",
                juce::Rectangle<float> (110.0f, 4.0f, 500.0f, 14.0f), juce::Justification::centredLeft);
}

void ModBar::resized()
{
    auto r = getLocalBounds().reduced (12, 4).withTrimmedTop (16);
    matrixButton.setBounds (r.removeFromRight (120).withSizeKeepingCentre (120, 28));
    r.removeFromRight (12);
    m1->setBounds (r.removeFromLeft (64));
    chips[0]->setBounds (r.removeFromLeft (150).reduced (2, 6));
    r.removeFromLeft (10);
    m2->setBounds (r.removeFromLeft (64));
    chips[1]->setBounds (r.removeFromLeft (150).reduced (2, 6));
    r.removeFromLeft (14);
    const int w = (r.getWidth() - 3 * 8) / 4;
    for (int i = 2; i < 6; ++i)
    {
        chips[(size_t) i]->setBounds (r.removeFromLeft (w).reduced (0, 6));
        r.removeFromLeft (8);
    }
}

void ModBar::update()
{
    m1->setLabel (ctx.proc.macroName (1));
    m2->setLabel (ctx.proc.macroName (2));
    std::array<juce::StringArray, 7> targets;
    const auto names = modTargetNames();
    for (int slot = 1; slot <= numMatrixSlots; ++slot)
    {
        const int src = choiceOf (ctx, id::mx (slot, "Src"));
        const int dst = choiceOf (ctx, id::mx (slot, "Dst"));
        if (src > 0 && src < 7 && dst > 0 && dst < names.size())
            targets[(size_t) src].addIfNotAlreadyThere (names[dst]);
    }
    for (int s = 1; s <= 6; ++s)
    {
        auto& chip = *chips[(size_t) s - 1];
        chip.setValue (ctx.proc.modSystem.sourceValue[(size_t) s].load());
        juce::String sub = targets[(size_t) s].isEmpty() ? juce::String ("nicht belegt - auf Regler ziehen")
                                                          : "> " + targets[(size_t) s].joinIntoString (", ");
        if (s >= 3)
        {
            const int m = s - 2;
            const int type = choiceOf (ctx, id::mod (m, "Type"));
            if (targets[(size_t) s].isEmpty()) sub = modTypeNames()[type] + " - nicht belegt";
            chip.setTooltip (modTypeNames()[type] + ". Ziehen: auf einen Regler. Klick: bearbeiten.");
        }
        else
        {
            chip.setTooltip ("Ziehen: auf einen Regler. Der Regler links stellt den Wert ein.");
        }
        chip.setSubtitle (sub);
    }
}

// ============================================================================
// Help card
// ============================================================================
void HelpCard::show (const juce::String& t, const juce::String& text, juce::Rectangle<int> near, juce::Rectangle<int> limits)
{
    heading = t; body = text;
    juce::AttributedString s;
    s.append (body, fontLight (14.5f), col::text);
    juce::TextLayout tl;
    const float w = 330.0f;
    tl.createLayout (s, w - 28.0f);
    const int h = (int) tl.getHeight() + 50;
    juce::Rectangle<int> b ((int) w, h);
    if (near.getRight() + 12 + (int) w <= limits.getRight()) b.setPosition (near.getRight() + 12, near.getY());
    else if (near.getX() - 12 - (int) w >= limits.getX()) b.setPosition (near.getX() - 12 - (int) w, near.getY());
    else b.setPosition (near.getCentreX() - (int) w / 2, near.getBottom() - h);
    setBounds (b.constrainedWithin (limits));
    setVisible (true);
    toFront (false);
    repaint();
}

void HelpCard::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    auto p = chamfer (r, 8.0f);
    g.setColour (col::bg.withAlpha (0.97f));
    g.fillPath (p);
    g.setColour (col::accent);
    g.strokePath (p, juce::PathStrokeType (1.2f));
    drawCaps (g, heading, r.reduced (14.0f, 10.0f).removeFromTop (18.0f), juce::Justification::centredLeft, 14.0f, col::accent, true, 0.1f);
    juce::AttributedString s;
    s.append (body, fontLight (14.5f), col::text);
    juce::TextLayout tl;
    tl.createLayout (s, r.getWidth() - 28.0f);
    tl.draw (g, r.reduced (14.0f).withTrimmedTop (24.0f));
}

// ============================================================================
// Detail overlay
// ============================================================================
DetailOverlay::DetailOverlay (Ctx& c) : ctx (c)
{
    grainPage = std::make_unique<GrainPage> (ctx);
    shapePage = std::make_unique<ShapePage> (ctx);
    fxPage = std::make_unique<EffectsPage> (ctx);
    modPage = std::make_unique<ModPage> (ctx);
    for (juce::Component* p : { (juce::Component*) grainPage.get(), (juce::Component*) shapePage.get(),
                                (juce::Component*) fxPage.get(), (juce::Component*) modPage.get() })
        addChildComponent (p);
    addAndMakeVisible (closeButton);
    closeButton.onClick = [this] { close(); if (onClosed) onClosed(); };
    closeButton.setTooltip ("Zurück zur Übersicht (Esc)"_de);
    fxPage->update();
    setVisible (false);
}

void DetailOverlay::open (int page)
{
    current = juce::jlimit (0, 3, page);
    static const char* titles[] { "Grain Engine - Details", "Grain FX - Filter, Imager, Delay", "Effects", "Modulation" };
    title = titles[current];
    grainPage->setVisible (current == 0);
    shapePage->setVisible (current == 1);
    fxPage->setVisible (current == 2);
    modPage->setVisible (current == 3);
    setVisible (true);
    toFront (false);
    repaint();
}

void DetailOverlay::paint (juce::Graphics& g)
{
    g.fillAll (col::canvas);
    auto r = getLocalBounds().toFloat();
    g.setColour (col::accent);
    g.fillRect (r.removeFromTop (2.0f));
    drawCaps (g, title, { 16.0f, 8.0f, 600.0f, 22.0f }, juce::Justification::centredLeft, 16.0f, col::text, true, 0.12f);
}

void DetailOverlay::resized()
{
    closeButton.setBounds (getWidth() - 96, 8, 84, 24);
    auto area = getLocalBounds().reduced (12, 0).withTrimmedTop (40).withTrimmedBottom (10);
    for (juce::Component* p : { (juce::Component*) grainPage.get(), (juce::Component*) shapePage.get(),
                                (juce::Component*) fxPage.get(), (juce::Component*) modPage.get() })
        p->setBounds (area);
}

void DetailOverlay::update()
{
    switch (current)
    {
        case 0: grainPage->update(); break;
        case 1: shapePage->update(); break;
        case 2: fxPage->update(); break;
        case 3: modPage->update(); break;
        default: break;
    }
}

void DetailOverlay::refreshModulation()
{
    switch (current)
    {
        case 0: grainPage->refreshModulation(); break;
        case 1: shapePage->refreshModulation(); break;
        case 2: fxPage->refreshModulation(); break;
        case 3: modPage->refreshModulation(); break;
        default: break;
    }
}
}
