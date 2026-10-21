#include "PlayPage.h"
#include "../PluginProcessor.h"
#include "../dsp/Melody.h"

namespace liq::ui
{
namespace
{
    constexpr int kH = 72;
    constexpr int rowH = 22;
    float rawOf (Ctx& c, const juce::String& pid) { return c.apvts.getRawParameterValue (pid)->load(); }
    int choiceOf (Ctx& c, const juce::String& pid) { return (int) std::lround (rawOf (c, pid)); }
    bool flagOf (Ctx& c, const juce::String& pid) { return rawOf (c, pid) > 0.5f; }
    juce::String pct (float v) { return juce::String ((int) std::lround (v * 100)) + "%"; }

    void setParam (Ctx& c, const juce::String& pid, float real)
    {
        if (auto* p = c.apvts.getParameter (pid))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (real));
            p->endChangeGesture();
        }
    }

    void caps (juce::Label& l, const juce::String& text)
    {
        l.setText (text.toUpperCase(), juce::dontSendNotification);
        l.setFont (font (11.5f, true).withExtraKerningFactor (0.08f));
        l.setColour (juce::Label::textColourId, col::dim);
        l.setJustificationType (juce::Justification::centredLeft);
    }

    MelodySettings currentMelody (Ctx& c)
    {
        MelodySettings s;
        s.style = choiceOf (c, id::mlStyle);
        s.variation = (int) std::lround (c.proc.modulatedValue (id::mlVar));
        s.density = c.proc.modulatedValue (id::mlDensity);
        s.range = (int) std::lround (c.proc.modulatedValue (id::mlRange));
        s.steps = melodyLengthSteps (choiceOf (c, id::mlLength));
        return s;
    }

    int melodyRoot (Ctx& c, const Scale& scale)
    {
        const int octave = (int) std::lround (rawOf (c, id::mdlOctave));
        return 48 + scale.root + 12 * octave + Modeler::styleOctaveOffset (choiceOf (c, id::mlStyle));
    }
}

// ============================================================================
// Macro pad
// ============================================================================
void MacroPad::update()
{
    const float x = ctx.proc.animator.cursorX.load(), y = ctx.proc.animator.cursorY.load();
    const auto r = inner();
    trail[(size_t) trailPos] = { r.getX() + x * r.getWidth(), r.getBottom() - y * r.getHeight() };
    trailPos = (trailPos + 1) % (int) trail.size();
    cx = x; cy = y;

    const auto n1 = ctx.proc.macroName (1), n2 = ctx.proc.macroName (2);
    getProperties().set ("helpTitle", juce::String ("XY-Pad"));
    getProperties().set ("helpText", juce::String::fromUTF8 ("Ziehen = zwei Klang-Eigenschaften gleichzeitig. Links/rechts: ")
                                         + n1 + ", unten/oben: " + n2 + juce::String::fromUTF8 (". Doppelklick = zurück in die Ecke."));
    repaint();
}

void MacroPad::paint (juce::Graphics& g)
{
    auto a = area();
    auto frame = chamfer (a, 8.0f);
    g.setColour (col::bg);
    g.fillPath (frame);

    const auto r = inner();
    // calm grid, as in version 1
    g.setColour (col::track);
    for (int i = 0; i <= 8; ++i)
    {
        const float fx = r.getX() + r.getWidth() * (float) i / 8.0f, fy = r.getY() + r.getHeight() * (float) i / 8.0f;
        g.drawVerticalLine ((int) fx, r.getY(), r.getBottom());
        g.drawHorizontalLine ((int) fy, r.getX(), r.getRight());
    }
    g.setColour (col::edge);
    g.drawVerticalLine ((int) r.getCentreX(), r.getY(), r.getBottom());
    g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());

    auto& s = ctx.apvts;
    const float hx = s.getRawParameterValue (id::macro1)->load();
    const float hy = s.getRawParameterValue (id::macro2)->load();
    const float depth = s.getRawParameterValue (id::animDepth)->load();
    const int mode = (int) std::lround (s.getRawParameterValue (id::animMode)->load());
    const int cycles = (int) std::lround (s.getRawParameterValue (id::animCycles)->load());
    const float warp = s.getRawParameterValue (id::animWarp)->load();
    auto toScreen = [r] (float x, float y)
    {
        return juce::Point<float> (r.getX() + juce::jlimit (0.0f, 1.0f, x) * r.getWidth(),
                                   r.getBottom() - juce::jlimit (0.0f, 1.0f, y) * r.getHeight());
    };

    // animator figure
    if (depth > 0.001f)
    {
        juce::Path path;
        for (int i = 0; i <= 500; ++i)
        {
            float px, py;
            Animator::shapePoint (mode, cycles, warp, i / 500.0, px, py);
            const auto pt = toScreen (hx + px * depth * 0.5f, hy + py * depth * 0.5f);
            if (i == 0) path.startNewSubPath (pt); else path.lineTo (pt);
        }
        g.setColour (col::mod.withAlpha (0.5f));
        g.strokePath (path, juce::PathStrokeType (1.4f));
        // fading trail of the live point
        for (int k = 0; k < (int) trail.size(); ++k)
        {
            const int idx = (trailPos + k) % (int) trail.size();
            const float age = (float) k / (float) trail.size();
            if (trail[(size_t) idx].x <= 0.0f) continue;
            g.setColour (col::mod.withAlpha (0.05f + 0.45f * age));
            g.fillEllipse (juce::Rectangle<float> (2.0f + 3.0f * age, 2.0f + 3.0f * age).withCentre (trail[(size_t) idx]));
        }
    }

    // handle + live cursor
    const auto h = toScreen (hx, hy);
    g.setColour (col::accent.withAlpha (0.16f));
    g.fillEllipse (juce::Rectangle<float> (40.0f, 40.0f).withCentre (h));
    g.setColour (col::accent);
    g.drawEllipse (juce::Rectangle<float> (20.0f, 20.0f).withCentre (h), 2.0f);
    const auto c = toScreen (cx, cy);
    g.setColour (col::text);
    g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (c));

    // axis names
    drawCaps (g, ctx.proc.macroName (1) + "  >", r.withTrimmedTop (r.getHeight() - 18.0f).withTrimmedRight (4.0f),
              juce::Justification::centredRight, 12.0f, col::text.withAlpha (0.8f));
    g.saveState();
    g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, r.getX() + 11.0f, r.getY() + 4.0f));
    drawCaps (g, ctx.proc.macroName (2) + "  >", { r.getX() + 11.0f - 260.0f, r.getY() - 5.0f, 260.0f, 16.0f },
              juce::Justification::centredRight, 12.0f, col::text.withAlpha (0.8f));
    g.restoreState();

    // nothing assigned yet
    bool used[3] { false, false, false };
    for (int slot = 1; slot <= numMatrixSlots; ++slot)
    {
        const int src = (int) std::lround (s.getRawParameterValue (id::mx (slot, "Src"))->load());
        const int dst = (int) std::lround (s.getRawParameterValue (id::mx (slot, "Dst"))->load());
        if (src >= 1 && src <= 2 && dst > 0) used[src] = true;
    }
    if (! used[1] && ! used[2])
    {
        auto box = r.withSizeKeepingCentre (r.getWidth() - 60.0f, 46.0f).withY (r.getY() + 14.0f);
        g.setColour (col::bg.withAlpha (0.9f));
        g.fillPath (chamfer (box, 5.0f));
        g.setFont (font (13.0f));
        g.setColour (col::accent);
        g.drawFittedText (juce::String::fromUTF8 ("Das Pad steuert noch nichts.\nPreset laden oder unten 'Macro' auf einen Regler ziehen."),
                          box.reduced (8.0f, 4.0f).toNearestInt(), juce::Justification::centred, 2);
    }

    g.setColour (col::edge);
    g.strokePath (frame, juce::PathStrokeType (1.0f));
}

void MacroPad::setFromMouse (juce::Point<float> p)
{
    auto r = inner();
    const float x = juce::jlimit (0.0f, 1.0f, (p.x - r.getX()) / r.getWidth());
    const float y = juce::jlimit (0.0f, 1.0f, (r.getBottom() - p.y) / r.getHeight());
    ctx.apvts.getParameter (id::macro1)->setValueNotifyingHost (x);
    ctx.apvts.getParameter (id::macro2)->setValueNotifyingHost (y);
    repaint();
}

void MacroPad::mouseDown (const juce::MouseEvent& e)
{
    ctx.apvts.getParameter (id::macro1)->beginChangeGesture();
    ctx.apvts.getParameter (id::macro2)->beginChangeGesture();
    dragging = true;
    setFromMouse (e.position);
}

void MacroPad::mouseDrag (const juce::MouseEvent& e) { if (dragging) setFromMouse (e.position); }

void MacroPad::mouseUp (const juce::MouseEvent&)
{
    if (! dragging) return;
    ctx.apvts.getParameter (id::macro1)->endChangeGesture();
    ctx.apvts.getParameter (id::macro2)->endChangeGesture();
    dragging = false;
}

void MacroPad::mouseDoubleClick (const juce::MouseEvent&)
{
    setParam (ctx, id::macro1, 0.0f);
    setParam (ctx, id::macro2, 0.0f);
}

// ============================================================================
// Mix knob (Dry/Wet over the two mixer channels)
// ============================================================================
MixKnob::MixKnob (Ctx& c) : ctx (c)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    slider.setRange (0.0, 1.0);
    slider.setDoubleClickReturnValue (true, 1.0);
    slider.setMouseDragSensitivity (220);
    addAndMakeVisible (slider);
    getProperties().set ("helpId", juce::String ("quickMix"));
    slider.onDragStart = [this]
    {
        for (auto pid : { id::mixWet, id::mixDry }) ctx.apvts.getParameter (pid)->beginChangeGesture();
    };
    slider.onDragEnd = [this]
    {
        for (auto pid : { id::mixWet, id::mixDry }) ctx.apvts.getParameter (pid)->endChangeGesture();
    };
    slider.onValueChange = [this]
    {
        if (internalChange) return;
        const float v = (float) slider.getValue();
        auto set = [this] (const juce::String& pid, float real)
        { auto* p = ctx.apvts.getParameter (pid); p->setValueNotifyingHost (p->convertTo0to1 (real)); };
        set (id::mixWet, std::min (1.0f, 2.0f * v));
        set (id::mixDry, std::min (1.0f, 2.0f * (1.0f - v)));
        repaint();
    };
}

void MixKnob::resized()
{
    auto r = getLocalBounds();
    r.removeFromBottom (26);
    const int d = std::min (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (d, d));
}

void MixKnob::update()
{
    if (slider.isMouseButtonDown()) return;
    const float wet = rawOf (ctx, id::mixWet), dry = rawOf (ctx, id::mixDry);
    float v = 0.5f;
    if (wet + dry > 1.0e-4f) v = wet >= 1.0f ? 1.0f - 0.5f * std::min (1.0f, dry) : 0.5f * std::min (1.0f, wet);
    if (std::abs ((float) slider.getValue() - v) > 0.002f)
    {
        internalChange = true;
        slider.setValue (v, juce::dontSendNotification);
        internalChange = false;
        repaint();
    }
}

void MixKnob::paint (juce::Graphics& g)
{
    auto text = getLocalBounds().removeFromBottom (26);
    drawCaps (g, "Mix", text.removeFromTop (13).toFloat(), juce::Justification::centred, 12.0f, col::text.withAlpha (0.85f), true, 0.05f);
    const float v = (float) slider.getValue();
    juce::String s = v >= 0.999f ? juce::String ("nur Effekt") : (v <= 0.001f ? juce::String ("nur Original") : pct (v) + " Effekt");
    g.setFont (fontLight (12.5f));
    g.setColour (slider.isMouseButtonDown() ? col::accent : col::dim);
    g.drawFittedText (s, text, juce::Justification::centred, 1);
}

// ============================================================================
// Preset list + finder
// ============================================================================
void PresetList::setItems (const std::vector<int>& idx, bool stripGenre)
{
    items = idx;
    strip = stripGenre;
    setSize (getWidth(), std::max (1, (int) items.size()) * rowHeight());
    repaint();
}

void PresetList::paint (juce::Graphics& g)
{
    const auto& f = PresetManager::factory();
    const int current = ctx.proc.presets.currentIndex();
    for (int i = 0; i < (int) items.size(); ++i)
    {
        auto r = juce::Rectangle<float> (0.0f, (float) (i * rowHeight()), (float) getWidth(), (float) rowHeight());
        const int pi = items[(size_t) i];
        const bool isCurrent = pi == current;
        if (i == hover) { g.setColour (col::panelHi); g.fillRect (r); }
        if (isCurrent)
        {
            g.setColour (col::accent.withAlpha (0.12f)); g.fillRect (r);
            g.setColour (col::accent); g.fillRect (r.withWidth (3.0f));
        }
        juce::String name = pi < (int) f.size() ? f[(size_t) pi].name : juce::String();
        if (strip && name.contains (" - ")) name = name.fromFirstOccurrenceOf (" - ", false, false);
        g.setFont (font (14.0f));
        g.setColour (isCurrent ? col::accent : col::text.withAlpha (0.9f));
        g.drawText (name, r.withTrimmedLeft (12.0f).withTrimmedRight (80.0f), juce::Justification::centredLeft, true);
        const auto tag = f[(size_t) pi].source == "Alle" ? f[(size_t) pi].genre : f[(size_t) pi].source;
        drawCaps (g, tag, r.withTrimmedRight (8.0f), juce::Justification::centredRight, 10.5f, col::faint, true, 0.05f);
        g.setColour (col::panel.brighter (0.05f));
        g.drawHorizontalLine ((int) r.getBottom() - 1, r.getX() + 8.0f, r.getRight() - 8.0f);
    }
    if (items.empty())
    {
        g.setFont (fontLight (13.0f));
        g.setColour (col::dim);
        g.drawText (juce::String::fromUTF8 ("Keine Presets für diese Kombination."), getLocalBounds().withHeight (40), juce::Justification::centred);
    }
}

void PresetList::mouseMove (const juce::MouseEvent& e)
{
    const int h = e.y / rowHeight();
    if (h != hover)
    {
        hover = juce::isPositiveAndBelow (h, (int) items.size()) ? h : -1;
        if (hover >= 0)
        {
            const int pi = items[(size_t) hover];
            getProperties().set ("helpTitle", PresetManager::factory()[(size_t) pi].name);
            getProperties().set ("helpText", ctx.proc.presets.descriptionOf (pi) + juce::String::fromUTF8 ("  -  Klick lädt das Preset."));
        }
        repaint();
    }
}

void PresetList::mouseExit (const juce::MouseEvent&) { hover = -1; repaint(); }

void PresetList::mouseUp (const juce::MouseEvent& e)
{
    const int h = e.y / rowHeight();
    if (juce::isPositiveAndBelow (h, (int) items.size()))
    {
        ctx.proc.presets.load (items[(size_t) h]);
        repaint();
        if (onLoaded) onLoaded();
    }
}

PresetFinder::PresetFinder (Ctx& c) : Page (c), list (c)
{
    sec = &section ("Preset-Finder");
    sec->setHelp ("Preset-Finder", juce::String::fromUTF8 (
        "Wähle, was auf deiner Spur liegt (Quelle), und deine Musikrichtung (Genre). "
        "Die Liste zeigt nur passende Presets. Einfach durchklicken, während Cubase spielt."));
    caps (sourceLabel, "Was liegt auf der Spur?");
    caps (genreLabel, "Genre");
    countLabel.setFont (fontLight (12.0f));
    countLabel.setColour (juce::Label::textColourId, col::faint);
    countLabel.setJustificationType (juce::Justification::centredRight);
    for (auto* l : { &sourceLabel, &genreLabel, &countLabel }) sec->addAndMakeVisible (l);

    juce::StringArray sources { "Alle" };
    sources.addArray (sourceNames());
    for (auto& s : sources)
    {
        auto* b = sourceButtons.add (new juce::TextButton (s));
        b->setRadioGroupId (2001);
        b->setClickingTogglesState (true);
        b->onClick = [this] { refreshList(); if (onSelectionChanged) onSelectionChanged(); };
        sec->addAndMakeVisible (b);
    }
    juce::StringArray gs { "Alle" };
    for (auto& g : genres()) gs.add (g.name);
    gs.add ("Sound Design");
    for (auto& s : gs)
    {
        auto* b = genreButtons.add (new juce::TextButton (s));
        b->setRadioGroupId (2002);
        b->setClickingTogglesState (true);
        b->onClick = [this] { refreshList(); if (onSelectionChanged) onSelectionChanged(); };
        sec->addAndMakeVisible (b);
    }

    // restore the last selection
    const auto savedSrc = ctx.apvts.state.getProperty ("finderSource", "Alle").toString();
    const auto savedGen = ctx.apvts.state.getProperty ("finderGenre", "Alle").toString();
    bool found = false;
    for (auto* b : sourceButtons) if (b->getButtonText() == savedSrc) { b->setToggleState (true, juce::dontSendNotification); found = true; }
    if (! found) sourceButtons[0]->setToggleState (true, juce::dontSendNotification);
    found = false;
    for (auto* b : genreButtons) if (b->getButtonText() == savedGen) { b->setToggleState (true, juce::dontSendNotification); found = true; }
    if (! found) genreButtons[0]->setToggleState (true, juce::dontSendNotification);

    viewport.setViewedComponent (&list, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (6);
    sec->addAndMakeVisible (viewport);
    list.onLoaded = [this] { if (onSelectionChanged) onSelectionChanged(); };
    refreshList();
}

juce::String PresetFinder::source() const
{
    for (auto* b : sourceButtons) if (b->getToggleState()) return b->getButtonText();
    return "Alle";
}

juce::String PresetFinder::genre() const
{
    for (auto* b : genreButtons) if (b->getToggleState()) return b->getButtonText();
    return "Alle";
}

void PresetFinder::refreshList()
{
    const auto src = source(), gen = genre();
    ctx.apvts.state.setProperty ("finderSource", src, nullptr);
    ctx.apvts.state.setProperty ("finderGenre", gen, nullptr);
    std::vector<int> idx;
    const auto& f = PresetManager::factory();
    for (int i = 0; i < (int) f.size(); ++i)
    {
        const auto& p = f[(size_t) i];
        const bool srcOk = src == "Alle" || p.source == src || p.source == "Alle";
        const bool genOk = gen == "Alle" || p.genre == gen || (p.genre == "Alle" && gen != "Sound Design");
        if (srcOk && genOk) idx.push_back (i);
    }
    list.setItems (idx, gen != "Alle" && gen != "Sound Design");
    countLabel.setText (juce::String ((int) idx.size()) + " Presets", juce::dontSendNotification);
    viewport.setViewPosition (0, 0);
}

void PresetFinder::resized()
{
    sec->setBounds (getLocalBounds());
    auto c = sec->content();
    sourceLabel.setBounds (c.removeFromTop (16));
    c.removeFromTop (4);
    auto layoutChips = [&c] (juce::OwnedArray<juce::TextButton>& bs, int cols)
    {
        const int gap = 3;
        const int w = (c.getWidth() - gap * (cols - 1)) / cols;
        const int rows = (bs.size() + cols - 1) / cols;
        for (int i = 0; i < bs.size(); ++i)
            bs[i]->setBounds (c.getX() + (i % cols) * (w + gap), c.getY() + (i / cols) * (22 + gap), w, 22);
        c.removeFromTop (rows * (22 + gap));
    };
    layoutChips (sourceButtons, 3);
    c.removeFromTop (10);
    auto gl = c.removeFromTop (16);
    genreLabel.setBounds (gl);
    countLabel.setBounds (gl);
    c.removeFromTop (4);
    layoutChips (genreButtons, 3);
    c.removeFromTop (10);
    viewport.setBounds (c);
    list.setSize (c.getWidth() - 8, list.getHeight());
    refreshList();
}

void PresetFinder::update()
{
    const int cur = ctx.proc.presets.currentIndex();
    if (cur != lastPreset) { lastPreset = cur; list.repaint(); }
}

// ============================================================================
// Tips
// ============================================================================
TipsView::TipsView (Ctx& c) : ctx (c)
{
    addChildComponent (applyScale);
    applyScale.onClick = [this]
    {
        if (scaleType >= 0) setParam (ctx, id::scaleType, (float) scaleType);
    };
    setSelection ("Alle", "Alle");
}

void TipsView::setSelection (const juce::String& source, const juce::String& genre)
{
    src = source; gen = genre;
    scaleType = -1;
    for (auto& g : genres())
        if (g.name == gen) scaleType = g.scaleType;
    applyScale.setVisible (scaleType >= 0);
    if (scaleType >= 0)
        applyScale.setButtonText (juce::String::fromUTF8 ("Skala übernehmen: ") + scaleTypeNames()[scaleType]);
    repaint();
}

void TipsView::resized() { applyScale.setBounds (getLocalBounds().removeFromBottom (26).withTrimmedLeft (0).withWidth (std::min (300, getWidth()))); }

void TipsView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    juce::String head = juce::String::fromUTF8 ("So startest du");
    if (src != "Alle" || gen != "Alle")
        head << ":  " << (src == "Alle" ? juce::String ("alle Quellen") : src)
             << juce::String::fromUTF8 (" · ") << (gen == "Alle" ? juce::String ("alle Genres") : gen);
    drawCaps (g, head, r.removeFromTop (18.0f), juce::Justification::centredLeft, 12.5f, col::accent, true, 0.08f);
    r.removeFromTop (6.0f);

    juce::AttributedString s;
    s.setLineSpacing (2.0f);
    s.append (sourceTips (src), fontLight (14.0f), col::text.withAlpha (0.9f));
    for (auto& gi : genres())
    {
        if (gi.name != gen) continue;
        s.append ("\n\n", fontLight (8.0f), col::text);
        s.append (gi.name + "  ", font (14.0f, true), col::accent);
        s.append (gi.bpm + " BPM\n", font (14.0f), col::text);
        s.append ("Skala: " + gi.scaleHint + "\n", fontLight (14.0f), col::text.withAlpha (0.9f));
        s.append (gi.character, fontLight (14.0f), col::dim);
    }
    juce::TextLayout tl;
    tl.createLayout (s, r.getWidth());
    tl.draw (g, r.withTrimmedBottom (applyScale.isVisible() ? 32.0f : 0.0f));
}

// ============================================================================
// Phrase view + MIDI export
// ============================================================================
void PhraseView::update()
{
    const auto s = currentMelody (ctx);
    const auto scale = ctx.proc.currentScale();
    const juce::String key = juce::String (s.style) + "/" + juce::String (s.variation) + "/" + juce::String (s.density, 2) + "/"
                             + juce::String (s.range) + "/" + juce::String (s.steps) + "/" + juce::String (scale.root)
                             + "/" + juce::String ((int) scale.mask[1]) + juce::String ((int) scale.mask[3]);
    const int p = choiceOf (ctx, id::mdlMode) == mdlMelody ? ctx.proc.modeler.melodyStep.load() : -1;
    if (key != lastKey || p != playing) { lastKey = key; playing = p; repaint(); }
}

void PhraseView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    auto frame = chamfer (r, 5.0f);
    g.setColour (col::bg);
    g.fillPath (frame);

    const auto s = currentMelody (ctx);
    const auto ph = generatePhrase (s);
    const auto scale = ctx.proc.currentScale();
    const int root = melodyRoot (ctx, scale);
    int lo = 127, hi = 0;
    for (int i = 0; i < ph.length; ++i)
        if (ph.steps[(size_t) i].on)
        {
            const int n = stepNote (ph.steps[(size_t) i], scale, root);
            lo = std::min (lo, n); hi = std::max (hi, n + (ph.steps[(size_t) i].chord ? 7 : 0));
        }
    if (lo > hi) { lo = root; hi = root + 12; }
    lo -= 1; hi += 1;
    auto in = r.reduced (6.0f, 6.0f);
    const float cw = in.getWidth() / (float) ph.length;
    const float rh = in.getHeight() / (float) std::max (1, hi - lo + 1);

    // beat lines
    for (int i = 0; i <= ph.length; i += 4)
    {
        g.setColour (i % 16 == 0 ? col::edge : col::track);
        g.drawVerticalLine ((int) (in.getX() + cw * (float) i), in.getY(), in.getBottom());
    }
    if (playing >= 0 && playing < ph.length)
    {
        g.setColour (col::accent.withAlpha (0.12f));
        g.fillRect (juce::Rectangle<float> (in.getX() + cw * (float) playing, in.getY(), cw, in.getHeight()));
    }
    for (int i = 0; i < ph.length; ++i)
    {
        const auto& st = ph.steps[(size_t) i];
        if (! st.on) continue;
        const int n = stepNote (st, scale, root);
        auto drawNote = [&] (int note)
        {
            const float y = in.getBottom() - (float) (note - lo + 1) * rh;
            auto box = juce::Rectangle<float> (in.getX() + cw * (float) i + 0.5f, y, cw * (float) st.length - 1.0f, std::max (2.0f, rh - 1.0f));
            g.setColour ((i == playing ? col::text : col::accent).withAlpha (0.45f + 0.55f * st.velocity));
            g.fillRect (box);
        };
        drawNote (n);
        if (st.chord) { drawNote (scale.shiftDegrees (n, 2)); drawNote (scale.shiftDegrees (n, 4)); }
    }
    g.setColour (col::edge);
    g.strokePath (frame, juce::PathStrokeType (1.0f));
}

MidiDragButton::MidiDragButton (Ctx& c) : juce::TextButton (juce::String::fromUTF8 ("MIDI ziehen  >>")), ctx (c)
{
    getProperties().set ("helpTitle", juce::String ("MIDI ziehen"));
    getProperties().set ("helpText", juce::String::fromUTF8 (
        "Ziehe diesen Knopf auf eine Spur in Cubase: die aktuelle Melodie landet dort als MIDI-Part. "
        "Klick öffnet den Ordner mit der MIDI-Datei."));
    onClick = [this] { if (! dragging) writeMidi().revealToUser(); };
}

juce::File MidiDragButton::writeMidi() const
{
    const auto s = currentMelody (ctx);
    const auto ph = generatePhrase (s);
    const auto scale = ctx.proc.currentScale();
    const int root = melodyRoot (ctx, scale);
    const double stepBeats = divisions()[(size_t) juce::jlimit (0, (int) divisions().size() - 1, choiceOf (ctx, id::mdlRate))].beats;
    const int ppq = 960;
    const int stepTicks = (int) std::lround (stepBeats * ppq);

    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::lround (60000000.0 / ctx.proc.hostBpm.load())), 0);
    auto addNote = [&] (int note, int start, int len, float vel)
    {
        const auto v = (juce::uint8) juce::jlimit (1, 127, (int) std::lround (vel * 110.0f + 17.0f));
        seq.addEvent (juce::MidiMessage::noteOn (1, juce::jlimit (0, 127, note), v), start);
        seq.addEvent (juce::MidiMessage::noteOff (1, juce::jlimit (0, 127, note)), start + len);
    };
    for (int i = 0; i < ph.length; ++i)
    {
        const auto& st = ph.steps[(size_t) i];
        if (! st.on) continue;
        const int n = stepNote (st, scale, root);
        const int len = std::max (1, (int) (stepTicks * st.length * (st.slide ? 1.05 : 0.9)));
        addNote (n, i * stepTicks, len, st.velocity);
        if (st.chord) { addNote (scale.shiftDegrees (n, 2), i * stepTicks, len, st.velocity); addNote (scale.shiftDegrees (n, 4), i * stepTicks, len, st.velocity); }
    }
    seq.updateMatchedPairs();
    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (ppq);
    mf.addTrack (seq);

    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Liquidation MIDI");
    dir.createDirectory();
    const auto name = juce::File::createLegalFileName ("Liquidation " + melodyStyleNames()[s.style] + " V" + juce::String (s.variation)
                                                       + " " + noteNames()[scale.root] + ".mid");
    auto file = dir.getChildFile (name);
    file.deleteFile();
    juce::FileOutputStream os (file);
    if (os.openedOk()) mf.writeTo (os);
    return file;
}

void MidiDragButton::mouseDrag (const juce::MouseEvent& e)
{
    juce::TextButton::mouseDrag (e);
    if (! dragging && e.getDistanceFromDragStart() > 6)
    {
        dragging = true;
        const auto f = writeMidi();
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this,
                                                                   [this] { dragging = false; });
    }
}

void MidiDragButton::mouseUp (const juce::MouseEvent& e)
{
    const bool wasDragging = dragging;
    juce::TextButton::mouseUp (e);
    if (wasDragging) dragging = false;
}

// ============================================================================
// Melody panel
// ============================================================================
MelodyPanel::MelodyPanel (Ctx& c) : Page (c), phrase (c), midi (c)
{
    addAndMakeVisible (activeButton);
    activeButton.setClickingTogglesState (false);
    activeButton.getProperties().set ("helpTitle", juce::String ("Melodie an / aus"));
    activeButton.getProperties().set ("helpText", juce::String::fromUTF8 (
        "Schaltet den Generator ein: der Modeler spielt die Melodie, im Mixer wird 'Modeler' hörbar. Main In (deine Spur) bleibt."));
    activeButton.onClick = [this]
    {
        const bool on = choiceOf (ctx, id::mdlMode) == mdlMelody && rawOf (ctx, id::mixModel) > 0.001f;
        if (on) setParam (ctx, id::mixModel, 0.0f);
        else
        {
            setParam (ctx, id::mdlMode, (float) mdlMelody);
            if (rawOf (ctx, id::mixModel) < 0.05f) setParam (ctx, id::mixModel, 0.6f);
        }
    };
    for (auto* b : { &prevVar, &nextVar }) addAndMakeVisible (b);
    prevVar.onClick = [this] { setParam (ctx, id::mlVar, juce::jlimit (1.0f, 99.0f, rawOf (ctx, id::mlVar) - 1.0f)); };
    nextVar.onClick = [this] { setParam (ctx, id::mlVar, juce::jlimit (1.0f, 99.0f, rawOf (ctx, id::mlVar) + 1.0f)); };
    prevVar.getProperties().set ("helpId", id::mlVar);
    nextVar.getProperties().set ("helpId", id::mlVar);

    style = &combo (id::mlStyle);
    trigger = &combo (id::mlTrigger);
    length = &combo (id::mlLength);
    rate = &combo (id::mdlRate);
    caps (styleLabel, "Stil"); caps (triggerLabel, juce::String::fromUTF8 ("Auslöser")); caps (lengthLabel, juce::String::fromUTF8 ("Länge")); caps (rateLabel, "Noten");
    for (auto* l : { &styleLabel, &triggerLabel, &lengthLabel, &rateLabel }) addAndMakeVisible (l);

    variation = &knob (id::mlVar, "Variante");
    variation->setFormatter ([] (float v) { return juce::String ((int) std::lround (v)); });
    density = &knob (id::mlDensity, "Dichte");
    density->setFormatter (pct);
    range = &knob (id::mlRange, "Umfang");
    range->setFormatter ([] (float v) { const int o = (int) std::lround (v); return juce::String (o) + (o == 1 ? " Oktave" : " Oktaven"); });
    glide = &knob (id::mlGlide, "Glide");
    glide->setFormatter (pct);
    octave = &knob (id::mdlOctave, "Oktave");
    octave->setFormatter ([] (float v) { const int o = (int) std::lround (v); return (o > 0 ? "+" : "") + juce::String (o); });
    tone = &knob (id::mdlTone, "Ton");
    tone->setFormatter (pct);
    sens = &knob (id::mlSens, "Kick");
    sens->setFormatter (pct);
    level = &knob (id::mixModel, "Pegel");
    level->setFormatter ([] (float v) { return v <= 0.0001f ? juce::String ("aus") : juce::String (20.0f * std::log10 (v), 1) + " dB"; });

    addAndMakeVisible (phrase);
    addAndMakeVisible (midi);
    status.setFont (fontLight (12.5f));
    status.setColour (juce::Label::textColourId, col::dim);
    addAndMakeVisible (status);
}

void MelodyPanel::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (rowH);
    activeButton.setBounds (top.removeFromLeft (58));
    top.removeFromLeft (10);
    styleLabel.setBounds (top.removeFromLeft (30));
    style->setBounds (top.removeFromLeft (128));
    top.removeFromLeft (10);
    triggerLabel.setBounds (top.removeFromLeft (62));
    trigger->setBounds (top);
    r.removeFromTop (8);
    phrase.setBounds (r.removeFromTop (88));
    r.removeFromTop (6);
    row (r.removeFromTop (kH), { variation, density, range, glide }, 2);
    {
        auto vb = variation->getBounds();
        prevVar.setBounds (vb.getX(), vb.getY() + 16, 18, 22);
        nextVar.setBounds (vb.getRight() - 18, vb.getY() + 16, 18, 22);
    }
    r.removeFromTop (6);
    auto mid = r.removeFromTop (rowH);
    lengthLabel.setBounds (mid.removeFromLeft (46));
    length->setBounds (mid.removeFromLeft (110));
    mid.removeFromLeft (14);
    rateLabel.setBounds (mid.removeFromLeft (46));
    rate->setBounds (mid.removeFromLeft (110));
    r.removeFromTop (6);
    row (r.removeFromTop (kH), { octave, tone, sens, level }, 2);
    r.removeFromTop (8);
    auto bottom = r.removeFromTop (28);
    midi.setBounds (bottom.removeFromLeft (150));
    bottom.removeFromLeft (10);
    status.setBounds (bottom);
}

void MelodyPanel::update()
{
    phrase.update();
    const bool on = choiceOf (ctx, id::mdlMode) == mdlMelody && rawOf (ctx, id::mixModel) > 0.001f;
    if (activeButton.getToggleState() != on)
    {
        activeButton.setToggleState (on, juce::dontSendNotification);
        activeButton.setButtonText (on ? "An" : "Aus");
    }
    const int trig = choiceOf (ctx, id::mlTrigger);
    sens->setAlpha (trig == trigGrid ? 0.4f : 1.0f);
    juce::String t;
    if (! on) t = juce::String::fromUTF8 ("Aus - 'Aus' klicken zum Einschalten.");
    else
    {
        static const char* desc[] { "spielt im Takt", "jeder Kick = nächste Note", "klingt nur mit der Kick", "spielt zwischen den Kicks" };
        t = juce::String::fromUTF8 (desc[juce::jlimit (0, 3, trig)]);
        if (trig != trigGrid) t << "  -  Kicks: " << juce::String (ctx.proc.onsets.hits.load() % 1000);
    }
    if (status.getText() != t) status.setText (t, juce::dontSendNotification);
}

// ============================================================================
// Play page
// ============================================================================
PlayPage::PlayPage (Ctx& c) : Page (c), finder (c), pad (c), mix (c), melody (c), tips (c)
{
    addAndMakeVisible (finder);
    padSec = &section ("XY-Pad");
    padSec->setHelp ("XY-Pad", juce::String::fromUTF8 (
        "Bewegt zwei Klang-Eigenschaften gleichzeitig - welche, steht an den Achsen und unter dem Pad. "
        "Die Figur (unten) bewegt den Punkt automatisch: Grösse auf 0 = Pad steht still."));
    padSec->addAndMakeVisible (pad);
    figure = &combo (id::animMode, padSec);
    figDiv = &combo (id::animDiv, padSec);
    figSync = &toggle (id::animSync, "Sync", padSec);
    figDepth = &knob (id::animDepth, juce::String::fromUTF8 ("Grösse"), padSec);
    figRate = &knob (id::animRate, "Tempo", padSec);
    figCycles = &knob (id::animCycles, "Schleifen", padSec);
    figWarp = &knob (id::animWarp, "Form", padSec);
    figDrift = &knob (id::animDrift, "Drift", padSec);
    for (auto* k : { figDepth, figWarp, figDrift }) k->setFormatter (pct);
    figRate->setFormatter ([] (float v) { return juce::String (v, 2) + " Hz"; });
    caps (figLabel, "Figur");
    padSec->addAndMakeVisible (figLabel);
    for (auto* l : { &hint1, &hint2 })
    {
        l->setFont (fontLight (13.0f));
        l->setColour (juce::Label::textColourId, col::dim);
        padSec->addAndMakeVisible (l);
    }

    soundSec = &section ("Klang");
    soundSec->setHelp ("Klang", juce::String::fromUTF8 (
        "Die wichtigsten Regler auf einen Blick. Mix = Original gegen Effekt. Alles Weitere auf der Edit-Seite."));
    soundSec->addAndMakeVisible (mix);
    stretch = &knob (id::stretch, "Stretch", soundSec);
    stretch->setFormatter ([this] (float v)
    {
        const float sp = GrainEngine::stretchFactor (v, flagOf (ctx, id::stretchSync));
        return sp <= 0.0f ? juce::String ("Freeze") : juce::String (1.0f / sp, 1.0f / sp < 10 ? 2 : 1) + "x";
    });
    pitch = &knob (id::pitch, "Pitch", soundSec);
    pitch->setFormatter ([this] (float v)
    {
        const bool deg = choiceOf (ctx, id::pitchMode) == pitchScale && ! ctx.proc.currentScale().chromatic;
        if (deg) { const int d = (int) std::lround (v); return (d > 0 ? "+" : "") + juce::String (d) + " Stufen"; }
        return (v > 0 ? "+" : "") + juce::String (v, 1) + " st";
    });
    rate = &knob (id::grainRate, "Grains", soundSec);
    rate->setFormatter ([this] (float v)
    {
        return GrainEngine::rateText (choiceOf (ctx, id::rateMode), v, ctx.proc.currentScale(), ctx.proc.hostBpm.load());
    });
    spray = &knob (id::spray, "Spray", soundSec);
    blur = &knob (id::blur, "Blur", soundSec);
    room = &knob (id::revMix, "Raum", soundSec);
    output = &knob (id::outputGain, "Output", soundSec);
    for (auto* k : { spray, blur, room }) k->setFormatter (pct);
    output->setFormatter ([] (float v) { return juce::String (v, 1) + " dB"; });

    lowerSec = &section ("Melodie-Generator");
    lowerSec->setHelp ("Melodie-Generator", juce::String::fromUTF8 (
        "Schreibt fertige Melodien in deiner Tonart. Variante = andere Melodie, Dichte = mehr/weniger Noten. "
        "Auslöser 'Kick ...' koppelt die Melodie an die Kick deiner Spur. 'MIDI ziehen' übernimmt sie nach Cubase."));
    lowerSec->addAndMakeVisible (melody);
    lowerSec->addChildComponent (tips);
    for (auto* b : { &tabMelody, &tabTips })
    {
        b->setRadioGroupId (3001);
        b->setClickingTogglesState (true);
        lowerSec->addAndMakeVisible (b);
    }
    tabMelody.onClick = [this] { showLower (0); };
    tabTips.onClick = [this] { showLower (1); };

    finder.onSelectionChanged = [this]
    {
        tips.setSelection (finder.source(), finder.genre());
        const bool melodyPreset = choiceOf (ctx, id::mdlMode) == mdlMelody && rawOf (ctx, id::mixModel) > 0.001f;
        showLower (melodyPreset ? 0 : 1);
    };
    tips.setSelection (finder.source(), finder.genre());
    showLower (1);
}

void PlayPage::showLower (int t)
{
    lowerTab = t;
    melody.setVisible (t == 0);
    tips.setVisible (t == 1);
    (t == 0 ? tabMelody : tabTips).setToggleState (true, juce::dontSendNotification);
    lowerSec->setTitle (t == 0 ? juce::String ("Melodie-Generator") : juce::String ("Tipps"));
    if (t == 0) lowerSec->setHelp ("Melodie-Generator", juce::String::fromUTF8 (
        "Schreibt fertige Melodien in deiner Tonart. Variante = andere Melodie, Dichte = mehr/weniger Noten. "
        "Auslöser 'Kick ...' koppelt die Melodie an die Kick deiner Spur. 'MIDI ziehen' übernimmt sie nach Cubase."));
    else lowerSec->setHelp ("Tipps", juce::String::fromUTF8 ("Hinweise für die gewählte Quelle und das Genre."));
}

void PlayPage::resized()
{
    finder.setBounds (14, 12, 318, 632);
    padSec->setBounds (346, 12, 470, 632);
    soundSec->setBounds (830, 12, 436, 196);
    lowerSec->setBounds (830, 220, 436, 424);

    {
        auto c = padSec->content();
        pad.setBounds (c.removeFromTop (420).withSizeKeepingCentre (420, 420));
        c.removeFromTop (6);
        hint1.setBounds (c.removeFromTop (17));
        hint2.setBounds (c.removeFromTop (17));
        c.removeFromTop (8);
        auto fr = c.removeFromTop (rowH);
        figLabel.setBounds (fr.removeFromLeft (46));
        figure->setBounds (fr.removeFromLeft (110));
        fr.removeFromLeft (8);
        figSync->setBounds (fr.removeFromLeft (60));
        fr.removeFromLeft (8);
        figDiv->setBounds (fr.removeFromLeft (100));
        c.removeFromTop (6);
        row (c.removeFromTop (kH), { figDepth, figRate, figCycles, figWarp, figDrift }, 2);
    }
    {
        auto c = soundSec->content();
        row (c.removeFromTop (kH), { &mix, stretch, pitch, rate }, 2);
        c.removeFromTop (8);
        row (c.removeFromTop (kH), { spray, blur, room, output }, 2);
    }
    {
        tabMelody.setBounds (lowerSec->getWidth() - 160, 6, 72, 18);
        tabTips.setBounds (lowerSec->getWidth() - 84, 6, 72, 18);
        auto c = lowerSec->content();
        melody.setBounds (c);
        tips.setBounds (c);
    }
}

void PlayPage::update()
{
    const int cur = ctx.proc.presets.currentIndex();
    const auto curName = ctx.proc.presets.currentName();
    if (cur != lastPreset || curName != lastPresetName)
    {
        const bool first = lastPreset == -2;
        lastPreset = cur; lastPresetName = curName;
        const bool melodyOn = choiceOf (ctx, id::mdlMode) == mdlMelody && rawOf (ctx, id::mixModel) > 0.001f;
        if (melodyOn) showLower (0);
        else if (! first) showLower (1);
    }
    finder.update();
    pad.update();
    mix.update();
    if (melody.isVisible()) melody.update();
    const auto h1 = ctx.proc.macroHint (1), h2 = ctx.proc.macroHint (2);
    const juce::String t1 = juce::String::fromUTF8 ("Links/rechts  ") + ctx.proc.macroName (1) + (h1.isNotEmpty() ? " - " + h1 : juce::String());
    const juce::String t2 = juce::String::fromUTF8 ("Unten/oben  ") + ctx.proc.macroName (2) + (h2.isNotEmpty() ? " - " + h2 : juce::String());
    if (hint1.getText() != t1) hint1.setText (t1, juce::dontSendNotification);
    if (hint2.getText() != t2) hint2.setText (t2, juce::dontSendNotification);
    const bool s = flagOf (ctx, id::animSync);
    if (figRate->isVisible() == s) figRate->setVisible (! s);
    figDiv->setEnabled (s);
}

void PlayPage::refreshModulation()
{
    Page::refreshModulation();
    melody.refreshModulation();
}

// ============================================================================
void InfoBar::setText (const juce::String& title, const juce::String& body, const juce::String& more, const juce::String& less)
{
    if (title == t && body == b && more == m && less == l) return;
    t = title; b = body; m = more; l = less;
    repaint();
}

void InfoBar::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::canvas);
    g.fillRect (r);
    g.setColour (col::edge);
    g.drawHorizontalLine (0, 0.0f, r.getWidth());
    auto in = r.reduced (14.0f, 0.0f);
    drawCaps (g, "Info", in.removeFromLeft (40.0f), juce::Justification::centredLeft, 11.0f, col::accent, true, 0.12f);

    juce::AttributedString s;
    s.setWordWrap (juce::AttributedString::none);
    if (t.isEmpty())
    {
        s.append (juce::String::fromUTF8 ("Maus über einen Regler, Block oder ein Preset: hier steht, was er macht und was beim Drehen passiert."),
                  fontLight (14.0f), col::dim);
    }
    else
    {
        s.append (t + "   ", font (14.0f, true), col::text);
        s.append (b, fontLight (14.0f), col::text.withAlpha (0.85f));
        if (m.isNotEmpty()) { s.append ("    Mehr: ", font (13.0f, true), col::accent); s.append (m, fontLight (14.0f), col::dim); }
        if (l.isNotEmpty()) { s.append ("    Weniger: ", font (13.0f, true), col::accent); s.append (l, fontLight (14.0f), col::dim); }
    }
    s.setJustification (juce::Justification::centredLeft);
    s.draw (g, in);
}
}
