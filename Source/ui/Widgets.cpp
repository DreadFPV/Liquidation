#include "Widgets.h"
#include "../PluginProcessor.h"

namespace liq::ui
{
// ============================================================================
// Knob
// ============================================================================
Knob::Knob (Ctx& c, const juce::String& pid, const juce::String& l)
    : ctx (c), paramId (pid), label (l)
{
    ks = std::make_unique<KnobSlider>();
    addAndMakeVisible (*ks);
    ks->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    ks->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    ks->setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    ks->setMouseDragSensitivity (220);
    ks->setVelocityModeParameters (0.8, 1, 0.09, true, juce::ModifierKeys::shiftModifier);
    ks->onRightClick = [this] { showMenu(); };
    ks->onValueChange = [this] { repaint(); };
    ks->onDragStart = [this] { repaint(); };
    ks->onDragEnd = [this] { repaint(); };

    param = ctx.apvts.getParameter (paramId);
    jassert (param != nullptr);
    if (param != nullptr)
    {
        const auto range = param->getNormalisableRange();
        ks->getProperties().set ("bipolar", range.start < 0.0f && range.end > 0.0f);
        attachment = std::make_unique<APVTS::SliderAttachment> (ctx.apvts, paramId, *ks);
        ks->setDoubleClickReturnValue (true, range.convertFrom0to1 (param->getDefaultValue()));
        setTooltip (param->getName (64) + "\nDrag a modulator here to modulate. Right-click for options.");
    }
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromBottom (26);
    const int d = std::min (r.getWidth(), r.getHeight());
    ks->setBounds (r.withSizeKeepingCentre (d, d));
}

void Knob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    auto text = r.removeFromBottom (26);

    if (dropHover)
    {
        g.setColour (col::mod.withAlpha (0.25f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
    }

    const float real = param != nullptr ? param->convertFrom0to1 (param->getValue()) : 0.0f;
    juce::String value = formatter ? formatter (real)
                                   : (param != nullptr ? param->getCurrentValueAsText() : juce::String());
    if (param != nullptr && formatter == nullptr)
    {
        const auto unit = param->getLabel();
        if (unit.isNotEmpty() && ! value.endsWith (unit)) value << " " << unit;
    }

    g.setFont (font (11.5f, true));
    g.setColour (isEnabled() ? col::text : col::faint);
    g.drawFittedText (label, text.removeFromTop (13), juce::Justification::centred, 1);
    g.setFont (font (11.0f));
    g.setColour (ks->isMouseButtonDown() ? col::accent : col::dim);
    g.drawFittedText (value, text, juce::Justification::centred, 1);
}

void Knob::refreshModulation()
{
    if (param == nullptr) return;
    const int target = modTargetIds().indexOf (paramId) + 1;
    if (target <= 0) return;

    float amount = 0.0f;
    for (int s = 1; s <= numMatrixSlots; ++s)
    {
        const int src = (int) std::lround (ctx.apvts.getRawParameterValue (id::mx (s, "Src"))->load());
        const int dst = (int) std::lround (ctx.apvts.getRawParameterValue (id::mx (s, "Dst"))->load());
        if (src > 0 && dst == target)
            amount += ctx.apvts.getRawParameterValue (id::mx (s, "Amt"))->load();
    }
    float live = -1.0f;
    if (amount != 0.0f)
        live = param->convertTo0to1 (ctx.proc.modulatedValue (paramId));

    if (std::abs (amount - lastMod) > 1.0e-4f || std::abs (live - lastLive) > 0.002f)
    {
        lastMod = amount; lastLive = live;
        ks->getProperties().set ("modAmt", amount);
        ks->getProperties().set ("modLive", live);
        ks->repaint();
    }
}

bool Knob::isInterestedInDragSource (const SourceDetails& d)
{
    return d.description.toString().startsWith ("mod:") && modTargetIds().contains (paramId);
}

void Knob::itemDropped (const SourceDetails& d)
{
    dropHover = false;
    const int src = d.description.toString().fromFirstOccurrenceOf ("mod:", false, false).getIntValue();
    if (ctx.addModulation) ctx.addModulation (paramId, src);
    repaint();
}

void Knob::showMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader (param != nullptr ? param->getName (64) : paramId);
    m.addItem (1, "Reset to default");
    const bool modulatable = modTargetIds().contains (paramId);
    if (modulatable)
    {
        juce::PopupMenu add;
        const auto names = modSourceNames();
        for (int s = 1; s < names.size(); ++s) add.addItem (100 + s, names[s]);
        m.addSubMenu ("Modulate with", add);
        m.addItem (2, "Clear modulation", lastMod != 0.0f);
        m.addItem (3, "Open mod matrix");
    }
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int r)
    {
        if (r == 1 && param != nullptr) param->setValueNotifyingHost (param->getDefaultValue());
        else if (r == 2 && ctx.clearModulation) ctx.clearModulation (paramId);
        else if (r == 3 && ctx.showMatrix) ctx.showMatrix();
        else if (r > 100 && ctx.addModulation) ctx.addModulation (paramId, r - 100);
    });
}

// ============================================================================
ParamToggle::ParamToggle (APVTS& s, const juce::String& pid, const juce::String& buttonText)
{
    setButtonText (buttonText);
    setClickingTogglesState (true);
    attachment = std::make_unique<APVTS::ButtonAttachment> (s, pid, *this);
    if (auto* p = s.getParameter (pid)) setTooltip (p->getName (64));
}

ParamCombo::ParamCombo (APVTS& s, const juce::String& pid)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (pid)))
    {
        addItemList (p->choices, 1);
        setTooltip (p->getName (64));
    }
    attachment = std::make_unique<APVTS::ComboBoxAttachment> (s, pid, *this);
}

// ============================================================================
Section::Section (const juce::String& t, APVTS* s, const juce::String& powerParam) : title (t)
{
    if (s != nullptr && powerParam.isNotEmpty())
    {
        power = std::make_unique<juce::ToggleButton>();
        addAndMakeVisible (*power);
        powerAttachment = std::make_unique<APVTS::ButtonAttachment> (*s, powerParam, *power);
        power->setTooltip ("Enable / bypass " + t);
    }
}

void Section::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (col::panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (col::edge);
    g.drawRoundedRectangle (r, 8.0f, 1.0f);

    const bool on = power == nullptr || power->getToggleState();
    g.setColour (on ? accentColour : col::dim);
    g.fillRoundedRectangle (juce::Rectangle<float> (3.0f, 12.0f).withPosition (10.0f, 10.0f), 1.5f);
    g.setFont (font (12.5f, true));
    g.setColour (on ? col::text : col::dim);
    g.drawText (title.toUpperCase(), 18, 6, getWidth() - 80, 20, juce::Justification::centredLeft);
}

void Section::resized()
{
    if (power) power->setBounds (getWidth() - 44, 6, 38, 20);
}

// ============================================================================
ModChip::ModChip (Ctx& c, int s, const juce::String& n) : ctx (c), source (s), name (n)
{
    setTooltip ("Drag onto a knob to modulate it. Click to edit.");
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

void ModChip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (selected ? col::mod.withAlpha (0.22f) : col::panelHi);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (selected ? col::mod : col::edge);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);

    auto inner = r.reduced (8.0f, 5.0f);
    g.setFont (font (12.0f, true));
    g.setColour (col::text);
    g.drawText (name, inner.removeFromTop (15.0f), juce::Justification::centredLeft);
    g.setFont (font (10.5f));
    g.setColour (col::dim);
    g.drawText (subtitle, inner.removeFromTop (13.0f), juce::Justification::centredLeft);

    auto bar = inner.removeFromBottom (5.0f);
    g.setColour (col::track);
    g.fillRoundedRectangle (bar, 2.5f);
    g.setColour (col::mod);
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, value)), 2.5f);
}

void ModChip::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging && e.getDistanceFromDragStart() > 4)
        if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            dragging = true;
            c->startDragging ("mod:" + juce::String (source), this);
        }
}

void ModChip::mouseUp (const juce::MouseEvent&)
{
    if (! dragging && onClick) onClick();
    dragging = false;
}

// ============================================================================
void ScaleKeyboard::refresh()
{
    const auto s = ctx.proc.currentScale();
    if (s.mask != mask || s.root != root) { mask = s.mask; root = s.root; repaint(); }
}

void ScaleKeyboard::paint (juce::Graphics& g)
{
    static const bool black[12] { false, true, false, true, false, false, true, false, true, false, true, false };
    const float w = (float) getWidth() / 12.0f;
    for (int pc = 0; pc < 12; ++pc)
    {
        auto r = juce::Rectangle<float> (pc * w, 0, w, (float) getHeight()).reduced (1.5f, 0);
        g.setColour (mask[(size_t) pc] ? (pc == root ? col::amber : col::accent).withAlpha (black[pc] ? 0.55f : 0.8f)
                                       : (black[pc] ? col::track : col::panelHi));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (col::edge);
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
        g.setColour (mask[(size_t) pc] ? col::bg : col::dim);
        g.setFont (font (10.0f, true));
        g.drawText (noteNames()[pc], r, juce::Justification::centred);
    }
}

void ScaleKeyboard::mouseDown (const juce::MouseEvent& e)
{
    const int pc = juce::jlimit (0, 11, (int) (e.x / ((float) getWidth() / 12.0f)));
    // switch to Custom, seeding it with the current scale, then toggle the note
    const auto cur = ctx.proc.currentScale();
    for (int k = 0; k < 12; ++k)
        if (auto* p = ctx.apvts.getParameter (id::scaleNote (k)))
            p->setValueNotifyingHost (cur.mask[(size_t) k] ? 1.0f : 0.0f);
    if (auto* p = ctx.apvts.getParameter (id::scaleNote (pc)))
        p->setValueNotifyingHost (cur.mask[(size_t) pc] ? 0.0f : 1.0f);
    if (auto* t = ctx.apvts.getParameter (id::scaleType))
        t->setValueNotifyingHost (t->convertTo0to1 ((float) (scaleTypeNames().size() - 1)));
    refresh();
}

// ============================================================================
IntervalStrip::IntervalStrip (Ctx& c) : ctx (c)
{
    for (int i = 1; i <= numIntervals; ++i)
    {
        auto* s = sliders.add (new juce::Slider (juce::Slider::LinearVertical, juce::Slider::TextBoxBelow));
        s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 34, 14);
        s->getProperties().set ("bipolar", true);
        s->setTooltip ("Interval " + juce::String (i) + " (semitones in Chromatic, scale degrees otherwise)");
        addAndMakeVisible (s);
        attachments.push_back (std::make_unique<APVTS::SliderAttachment> (ctx.apvts, id::interval (i), *s));
        auto* t = toggles.add (new ParamToggle (ctx.apvts, id::intervalOn (i), juce::String (i)));
        addAndMakeVisible (t);
    }
}

void IntervalStrip::resized()
{
    auto r = getLocalBounds();
    const int w = r.getWidth() / numIntervals;
    for (int i = 0; i < numIntervals; ++i)
    {
        auto c = r.removeFromLeft (w).reduced (2, 0);
        toggles[i]->setBounds (c.removeFromBottom (18));
        c.removeFromBottom (2);
        sliders[i]->setBounds (c);
    }
}

void IntervalStrip::paint (juce::Graphics&) {}

// ============================================================================
void Meter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::track);
    g.fillRoundedRectangle (r, 2.0f);
    const float db = juce::Decibels::gainToDecibels (level, -60.0f);
    const float f = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
    const bool vertical = r.getHeight() > r.getWidth();
    auto fill = vertical ? r.withTrimmedTop (r.getHeight() * (1.0f - f)) : r.withWidth (r.getWidth() * f);
    g.setColour (db > -0.5f ? col::red : (db > -9.0f ? col::amber : col::accent));
    g.fillRoundedRectangle (fill, 2.0f);
}

// ============================================================================
void TapPatternView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::bg);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (col::edge);
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);

    auto& s = ctx.apvts;
    const int taps = (int) std::lround (s.getRawParameterValue (id::dlyTaps)->load());
    const int pattern = (int) std::lround (s.getRawParameterValue (id::dlyPattern)->load());
    const float shift = s.getRawParameterValue (id::dlyShift)->load();
    const bool on = s.getRawParameterValue (id::dlyOn)->load() > 0.5f;
    const float fb = s.getRawParameterValue (id::dlyFeedback)->load();

    const float midY = r.getCentreY();
    g.setColour (col::faint);
    g.drawHorizontalLine ((int) midY, r.getX() + 4, r.getRight() - 4);

    const int total = taps + 1;
    const float step = (r.getWidth() - 16.0f) / (float) (total + 2);
    juce::Random rnd (42);
    for (int k = 0; k <= taps + 2; ++k)
    {
        float v = 0.0f;
        if (k > 0 && k <= taps)
        {
            switch (pattern)
            {
                case shHold:      v = shift; break;
                case shAlternate: v = (k % 2 == 1) ? shift : -shift; break;
                case shRamp:      v = shift * k; break;
                case shPulse:     v = (k % 2 == 1) ? shift * ((k + 1) / 2) : 0; break;
                case shTriangle:  { static const int tri[8] { 1, 2, 1, 0, -1, -2, -1, 0 }; v = shift * tri[(k - 1) % 8]; break; }
                case shDiverge:   v = (k % 2 == 1) ? shift * ((k + 1) / 2) : -shift * (k / 2); break;
                case shRandom:    v = shift * rnd.nextFloat(); break;
                default:          v = (rnd.nextFloat() * 2 - 1) * 12.0f; break;
            }
        }
        const bool echo = k > taps;
        const float amp = k == 0 ? 1.0f : (echo ? fb * std::pow (0.8f, (float) (k - taps)) : 0.85f);
        const float x = r.getX() + 8.0f + step * (k + 0.5f);
        const float y = midY - juce::jlimit (-1.0f, 1.0f, v / 24.0f) * (r.getHeight() * 0.42f);
        const float h = 6.0f + amp * 10.0f;
        auto c = k == 0 ? col::text : (echo ? col::dim : col::accent);
        g.setColour ((on ? c : col::faint).withAlpha (echo ? 0.5f : 1.0f));
        g.fillRoundedRectangle (juce::Rectangle<float> (std::max (3.0f, step * 0.45f), h).withCentre ({ x, y }), 2.0f);
    }
}

// ============================================================================
Knob& Page::knob (const juce::String& pid, const juce::String& label, juce::Component* parent)
{
    auto* k = knobs.add (new Knob (ctx, pid, label));
    (parent != nullptr ? parent : this)->addAndMakeVisible (k);
    return *k;
}

ParamCombo& Page::combo (const juce::String& pid, juce::Component* parent)
{
    auto* c = new ParamCombo (ctx.apvts, pid);
    owned.add (c);
    (parent != nullptr ? parent : this)->addAndMakeVisible (c);
    return *c;
}

ParamToggle& Page::toggle (const juce::String& pid, const juce::String& text, juce::Component* parent)
{
    auto* t = new ParamToggle (ctx.apvts, pid, text);
    owned.add (t);
    (parent != nullptr ? parent : this)->addAndMakeVisible (t);
    return *t;
}

Section& Page::section (const juce::String& title, const juce::String& powerParam)
{
    auto* s = new Section (title, powerParam.isNotEmpty() ? &ctx.apvts : nullptr, powerParam);
    owned.add (s);
    addAndMakeVisible (s);
    return *s;
}

juce::Label& Page::caption (const juce::String& text, juce::Component* parent)
{
    auto* l = new juce::Label ({}, text);
    l->setFont (font (11.0f, true));
    l->setColour (juce::Label::textColourId, col::dim);
    l->setJustificationType (juce::Justification::centredLeft);
    owned.add (l);
    (parent != nullptr ? parent : this)->addAndMakeVisible (l);
    return *l;
}

void Page::row (juce::Rectangle<int> r, std::initializer_list<juce::Component*> comps, int gap)
{
    const int n = (int) comps.size();
    if (n == 0) return;
    const int w = (r.getWidth() - gap * (n - 1)) / n;
    for (auto* c : comps)
    {
        if (c != nullptr) c->setBounds (r.removeFromLeft (w));
        else r.removeFromLeft (w);
        r.removeFromLeft (gap);
    }
}
}
