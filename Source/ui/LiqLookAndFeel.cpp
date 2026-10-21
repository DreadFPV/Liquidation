#include "LiqLookAndFeel.h"
#include "BinaryData.h"

namespace liq::ui
{
namespace
{
    juce::Typeface::Ptr face (int weight)
    {
        static juce::Typeface::Ptr regular = juce::Typeface::createSystemTypefaceFor (
            BinaryData::BarlowSemiCondensedRegular_ttf, (size_t) BinaryData::BarlowSemiCondensedRegular_ttfSize);
        static juce::Typeface::Ptr medium = juce::Typeface::createSystemTypefaceFor (
            BinaryData::BarlowSemiCondensedMedium_ttf, (size_t) BinaryData::BarlowSemiCondensedMedium_ttfSize);
        static juce::Typeface::Ptr semibold = juce::Typeface::createSystemTypefaceFor (
            BinaryData::BarlowSemiCondensedSemiBold_ttf, (size_t) BinaryData::BarlowSemiCondensedSemiBold_ttfSize);
        return weight == 0 ? regular : (weight == 2 ? semibold : medium);
    }
}

juce::Font font (float size, bool bold)
{
    return juce::Font (juce::FontOptions (face (bold ? 2 : 1)).withHeight (size));
}

juce::Font fontLight (float size)
{
    return juce::Font (juce::FontOptions (face (0)).withHeight (size));
}

juce::Path chamfer (juce::Rectangle<float> r, float c)
{
    juce::Path p;
    p.startNewSubPath (r.getX() + c, r.getY());
    p.lineTo (r.getRight(), r.getY());
    p.lineTo (r.getRight(), r.getBottom() - c);
    p.lineTo (r.getRight() - c, r.getBottom());
    p.lineTo (r.getX(), r.getBottom());
    p.lineTo (r.getX(), r.getY() + c);
    p.closeSubPath();
    return p;
}

void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, bool active, juce::Colour accent, float cut)
{
    auto p = chamfer (r.reduced (0.5f), cut);
    g.setColour (col::panel);
    g.fillPath (p);
    g.setColour (active ? col::edge.brighter (0.15f) : col::edge);
    g.strokePath (p, juce::PathStrokeType (1.0f));
    // small accent notch on the cut corner
    g.setColour ((active ? accent : col::faint).withAlpha (active ? 0.9f : 0.6f));
    g.drawLine (r.getX() + 1.5f, r.getY() + cut + 1.0f, r.getX() + cut + 1.0f, r.getY() + 1.5f, 2.0f);
}

void drawCaps (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, juce::Justification j,
               float size, juce::Colour c, bool bold, float tracking)
{
    auto f = font (size, bold).withExtraKerningFactor (tracking);
    g.setFont (f);
    g.setColour (c);
    g.drawText (text.toUpperCase(), r, j, false);
}

// ============================================================================
LiqLookAndFeel::LiqLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, col::bg);
    setColour (juce::Label::textColourId, col::text);
    setColour (juce::ComboBox::backgroundColourId, col::panelHi);
    setColour (juce::ComboBox::textColourId, col::text);
    setColour (juce::ComboBox::outlineColourId, col::edge);
    setColour (juce::ComboBox::arrowColourId, col::dim);
    setColour (juce::PopupMenu::backgroundColourId, col::panelHi);
    setColour (juce::PopupMenu::textColourId, col::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accentDk);
    setColour (juce::PopupMenu::highlightedTextColourId, col::accent.brighter (0.4f));
    setColour (juce::PopupMenu::headerTextColourId, col::accent);
    setColour (juce::TextButton::buttonColourId, col::panelHi);
    setColour (juce::TextButton::buttonOnColourId, col::accentDk);
    setColour (juce::TextButton::textColourOffId, col::dim);
    setColour (juce::TextButton::textColourOnId, col::accent);
    setColour (juce::Slider::textBoxTextColourId, col::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::backgroundColourId, col::bg);
    setColour (juce::TextEditor::textColourId, col::text);
    setColour (juce::TextEditor::outlineColourId, col::edge);
    setColour (juce::TextEditor::focusedOutlineColourId, col::accent);
    setColour (juce::CaretComponent::caretColourId, col::accent);
    setColour (juce::TooltipWindow::backgroundColourId, col::panelHi);
    setColour (juce::TooltipWindow::textColourId, col::text);
    setColour (juce::AlertWindow::backgroundColourId, col::panel);
    setColour (juce::AlertWindow::textColourId, col::text);
    setColour (juce::AlertWindow::outlineColourId, col::edge);
    setColour (juce::ScrollBar::thumbColourId, col::faint);
}

juce::Typeface::Ptr LiqLookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    return face (f.isBold() ? 2 : 1);
}

void LiqLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                       float a0, float a1, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float r = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const bool enabled = s.isEnabled();
    const float trackR = r - 4.0f;

    // scale ticks
    g.setColour (col::faint.withAlpha (enabled ? 0.9f : 0.4f));
    for (int t = 0; t <= 10; ++t)
    {
        const float a = a0 + (a1 - a0) * (float) t / 10.0f;
        const float len = (t == 0 || t == 5 || t == 10) ? 3.5f : 2.0f;
        g.drawLine (juce::Line<float> (c.getPointOnCircumference (r, a), c.getPointOnCircumference (r - len, a)), 1.0f);
    }

    // track + value arc
    juce::Path track;
    track.addCentredArc (c.x, c.y, trackR - 2.0f, trackR - 2.0f, 0, a0, a1, true);
    g.setColour (col::track);
    g.strokePath (track, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

    const bool bipolar = s.getProperties().getWithDefault ("bipolar", false);
    const float from = bipolar ? (a0 + a1) * 0.5f : a0;
    const float ang = a0 + pos * (a1 - a0);
    juce::Path val;
    val.addCentredArc (c.x, c.y, trackR - 2.0f, trackR - 2.0f, 0, std::min (from, ang), std::max (from, ang), true);
    g.setColour (enabled ? col::accent : col::faint);
    g.strokePath (val, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

    // body
    const float bodyR = trackR - 6.0f;
    auto body = juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (c);
    g.setGradientFill (juce::ColourGradient (col::panelHi.brighter (0.08f), c.x, body.getY(),
                                             col::panel.darker (0.2f), c.x, body.getBottom(), false));
    g.fillEllipse (body);
    g.setColour (col::bg.withAlpha (0.9f));
    g.drawEllipse (body, 1.0f);

    // modulation ring
    const float modAmt = (float) (double) s.getProperties().getWithDefault ("modAmt", 0.0);
    if (std::abs (modAmt) > 0.001f && enabled)
    {
        const float end = juce::jlimit (a0, a1, ang + modAmt * (a1 - a0));
        juce::Path m;
        m.addCentredArc (c.x, c.y, trackR + 1.5f, trackR + 1.5f, 0, std::min (ang, end), std::max (ang, end), true);
        g.setColour (col::mod);
        g.strokePath (m, juce::PathStrokeType (2.0f));
        const float live = (float) (double) s.getProperties().getWithDefault ("modLive", -1.0);
        if (live >= 0.0f)
        {
            const auto p = c.getPointOnCircumference (trackR - 2.0f, a0 + live * (a1 - a0));
            g.setColour (col::text);
            g.fillEllipse (juce::Rectangle<float> (4.5f, 4.5f).withCentre (p));
        }
    }

    // pointer
    g.setColour (enabled ? col::text : col::dim);
    g.drawLine (juce::Line<float> (c.getPointOnCircumference (bodyR * 0.25f, ang), c.getPointOnCircumference (bodyR - 1.5f, ang)), 2.0f);
}

void LiqLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                       juce::Slider::SliderStyle style, juce::Slider& s)
{
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    if (style == juce::Slider::LinearBar)
    {
        // compact value field
        auto frame = chamfer (b.reduced (0.5f), 4.0f);
        g.setColour (col::panelHi);
        g.fillPath (frame);
        g.setColour (s.isMouseOverOrDragging() ? col::faint.brighter (0.2f) : col::edge);
        g.strokePath (frame, juce::PathStrokeType (1.0f));
        const auto label = s.getProperties().getWithDefault ("valueField", "").toString();
        auto inner = b.reduced (8.0f, 0.0f);
        drawCaps (g, label, inner, juce::Justification::centredLeft, 11.5f, col::dim);
        g.setFont (font (13.5f));
        g.setColour (s.isMouseButtonDown() ? col::accent : col::text);
        g.drawText (s.getTextFromValue (s.getValue()), inner, juce::Justification::centredRight);
        return;
    }
    const bool vertical = style == juce::Slider::LinearVertical;
    const bool bipolar = s.getProperties().getWithDefault ("bipolar", false);
    const auto fillCol = s.isEnabled() ? (s.getProperties().getWithDefault ("modColour", false) ? col::mod : col::accent) : col::faint;
    if (vertical)
    {
        auto t = b.withSizeKeepingCentre (3.0f, b.getHeight());
        g.setColour (col::track); g.fillRect (t);
        const float zero = bipolar ? b.getCentreY() : b.getBottom();
        g.setColour (fillCol);
        g.fillRect (t.withTop (std::min (zero, pos)).withBottom (std::max (zero, pos)));
        g.setColour (col::text);
        g.fillRect (juce::Rectangle<float> (12.0f, 4.0f).withCentre ({ b.getCentreX(), pos }));
    }
    else
    {
        auto t = b.withSizeKeepingCentre (b.getWidth(), 3.0f);
        g.setColour (col::track); g.fillRect (t);
        const float zero = bipolar ? b.getCentreX() : b.getX();
        g.setColour (fillCol);
        g.fillRect (t.withLeft (std::min (zero, pos)).withRight (std::max (zero, pos)));
        g.setColour (col::text);
        g.fillRect (juce::Rectangle<float> (4.0f, 12.0f).withCentre ({ pos, b.getCentreY() }));
    }
}

void LiqLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hover, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const auto acc = b.getProperties().getWithDefault ("modColour", false) ? col::mod : col::accent;
    auto p = chamfer (r, 4.0f);
    g.setColour (on ? acc.withAlpha (down ? 0.35f : 0.18f) : (hover ? col::panelHi.brighter (0.08f) : col::panelHi));
    g.fillPath (p);
    g.setColour (on ? acc.withAlpha (0.85f) : (hover ? col::faint.brighter (0.2f) : col::edge));
    g.strokePath (p, juce::PathStrokeType (1.0f));
}

void LiqLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const bool on = b.getToggleState();
    const auto acc = b.getProperties().getWithDefault ("modColour", false) ? col::mod : col::accent;
    g.setFont (font (12.5f, true).withExtraKerningFactor (0.06f));
    g.setColour (! b.isEnabled() ? col::faint : (on ? acc.brighter (0.2f) : col::text.withAlpha (0.75f)));
    g.drawFittedText (b.getButtonText().toUpperCase(), b.getLocalBounds().reduced (3, 1), juce::Justification::centred, 1);
}

void LiqLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hover, bool)
{
    // power switch: ring with a vertical bar
    auto r = b.getLocalBounds().toFloat();
    const float d = std::min (15.0f, r.getHeight() - 2.0f);
    auto ring = juce::Rectangle<float> (d, d).withCentre ({ r.getX() + d * 0.5f + 1.0f, r.getCentreY() });
    const bool on = b.getToggleState();
    const auto c = on ? col::accent : (hover ? col::dim : col::faint);
    juce::Path arc;
    arc.addCentredArc (ring.getCentreX(), ring.getCentreY(), d * 0.42f, d * 0.42f, 0,
                       juce::degreesToRadians (35.0f), juce::degreesToRadians (325.0f), true);
    g.setColour (c);
    g.strokePath (arc, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.drawLine (ring.getCentreX(), ring.getY() + 0.5f, ring.getCentreX(), ring.getCentreY(), 1.8f);
    if (on)
    {
        g.setColour (col::accent.withAlpha (0.18f));
        g.fillEllipse (ring.expanded (3.0f));
    }
    if (b.getButtonText().isNotEmpty())
        drawCaps (g, b.getButtonText(), r.withLeft (ring.getRight() + 6.0f), juce::Justification::centredLeft,
                  12.5f, on ? col::text : col::dim);
}

void LiqLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    auto p = chamfer (r, 4.0f);
    g.setColour (box.isMouseOver (true) ? col::panelHi.brighter (0.08f) : col::panelHi);
    g.fillPath (p);
    g.setColour (box.isMouseOver (true) ? col::faint.brighter (0.2f) : col::edge);
    g.strokePath (p, juce::PathStrokeType (1.0f));
    const float ax = (float) w - 11.0f, ay = (float) h * 0.5f;
    juce::Path chevron;
    chevron.startNewSubPath (ax - 3.5f, ay - 1.5f);
    chevron.lineTo (ax, ay + 2.0f);
    chevron.lineTo (ax + 3.5f, ay - 1.5f);
    g.setColour (col::dim);
    g.strokePath (chevron, juce::PathStrokeType (1.4f));
}

void LiqLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (5, 0, box.getWidth() - 22, box.getHeight());
    label.setFont (getComboBoxFont (box));
}

void LiqLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (col::panelHi);
    g.setColour (col::edge);
    g.drawRect (0, 0, w, h);
}

void LiqLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    g.fillAll (col::bg);
    g.setColour (col::accent.withAlpha (0.6f));
    g.drawRect (0, 0, w, h);
    g.setColour (col::text);
    g.setFont (font (14.0f));
    g.drawFittedText (text, juce::Rectangle<int> (w, h).reduced (8, 4), juce::Justification::centredLeft, 6);
}

juce::Rectangle<int> LiqLookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                                      juce::Rectangle<int> parentArea)
{
    juce::AttributedString s;
    s.append (tipText, font (14.0f));
    juce::TextLayout tl;
    tl.createLayout (s, 300.0f);
    const int w = (int) tl.getWidth() + 18, h = (int) tl.getHeight() + 10;
    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6,
                                 w, h).constrainedWithin (parentArea);
}
}
