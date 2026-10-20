#include "LiqLookAndFeel.h"

namespace liq::ui
{
juce::Font font (float size, bool bold)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

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
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::PopupMenu::headerTextColourId, col::accent);
    setColour (juce::TextButton::buttonColourId, col::panelHi);
    setColour (juce::TextButton::buttonOnColourId, col::accentDk);
    setColour (juce::TextButton::textColourOffId, col::dim);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::Slider::textBoxTextColourId, col::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::backgroundColourId, col::panelHi);
    setColour (juce::TextEditor::textColourId, col::text);
    setColour (juce::TextEditor::outlineColourId, col::edge);
    setColour (juce::TextEditor::focusedOutlineColourId, col::accent);
    setColour (juce::CaretComponent::caretColourId, col::accent);
    setColour (juce::TooltipWindow::backgroundColourId, col::panelHi);
    setColour (juce::TooltipWindow::textColourId, col::text);
    setColour (juce::AlertWindow::backgroundColourId, col::panel);
    setColour (juce::AlertWindow::textColourId, col::text);
    setColour (juce::ScrollBar::thumbColourId, col::faint);
}

void LiqLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                       float a0, float a1, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float r = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float line = std::max (2.5f, r * 0.13f);
    const bool enabled = s.isEnabled();
    const auto accent = enabled ? col::accent : col::faint;

    // body
    g.setColour (col::panelHi);
    g.fillEllipse (juce::Rectangle<float> (r * 1.25f, r * 1.25f).withCentre (c));
    g.setColour (col::edge);
    g.drawEllipse (juce::Rectangle<float> (r * 1.25f, r * 1.25f).withCentre (c), 1.0f);

    // track
    juce::Path track;
    track.addCentredArc (c.x, c.y, r - line, r - line, 0, a0, a1, true);
    g.setColour (col::track);
    g.strokePath (track, juce::PathStrokeType (line, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc (from centre for bipolar ranges)
    const bool bipolar = s.getProperties().getWithDefault ("bipolar", false);
    const float from = bipolar ? (a0 + a1) * 0.5f : a0;
    const float ang = a0 + pos * (a1 - a0);
    juce::Path val;
    val.addCentredArc (c.x, c.y, r - line, r - line, 0, std::min (from, ang), std::max (from, ang), true);
    g.setColour (accent);
    g.strokePath (val, juce::PathStrokeType (line, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // modulation ring
    const float modAmt = (float) (double) s.getProperties().getWithDefault ("modAmt", 0.0);
    if (std::abs (modAmt) > 0.001f && enabled)
    {
        const float end = juce::jlimit (a0, a1, ang + modAmt * (a1 - a0));
        juce::Path m;
        m.addCentredArc (c.x, c.y, r - line * 0.1f, r - line * 0.1f, 0, std::min (ang, end), std::max (ang, end), true);
        g.setColour (col::mod);
        g.strokePath (m, juce::PathStrokeType (line * 0.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // live modulated position
        const float live = (float) (double) s.getProperties().getWithDefault ("modLive", -1.0);
        if (live >= 0.0f)
        {
            const float la = a0 + live * (a1 - a0);
            const auto p = c.getPointOnCircumference (r - line * 0.1f, la);
            g.setColour (juce::Colours::white);
            g.fillEllipse (juce::Rectangle<float> (line, line).withCentre (p));
        }
    }

    // pointer
    const auto p1 = c.getPointOnCircumference (r * 0.18f, ang);
    const auto p2 = c.getPointOnCircumference (r * 0.56f, ang);
    g.setColour (enabled ? col::text : col::dim);
    g.drawLine ({ p1, p2 }, std::max (2.0f, line * 0.7f));
}

void LiqLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                       juce::Slider::SliderStyle style, juce::Slider& s)
{
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const bool vertical = style == juce::Slider::LinearVertical;
    const bool bipolar = s.getProperties().getWithDefault ("bipolar", false);
    g.setColour (col::track);
    if (vertical)
    {
        auto t = b.withSizeKeepingCentre (5.0f, b.getHeight());
        g.fillRoundedRectangle (t, 2.5f);
        const float zero = bipolar ? b.getCentreY() : b.getBottom();
        g.setColour (s.isEnabled() ? col::accent : col::faint);
        g.fillRoundedRectangle (t.withTop (std::min (zero, pos)).withBottom (std::max (zero, pos)), 2.5f);
        g.setColour (col::text);
        g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ b.getCentreX(), pos }));
    }
    else
    {
        auto t = b.withSizeKeepingCentre (b.getWidth(), 4.0f);
        g.fillRoundedRectangle (t, 2.0f);
        const float zero = bipolar ? b.getCentreX() : b.getX();
        g.setColour (s.isEnabled() ? (s.getProperties().getWithDefault ("modColour", false) ? col::mod : col::accent) : col::faint);
        g.fillRoundedRectangle (t.withLeft (std::min (zero, pos)).withRight (std::max (zero, pos)), 2.0f);
        g.setColour (col::text);
        g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ pos, b.getCentreY() }));
    }
}

void LiqLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hover, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const auto onCol = b.getProperties().getWithDefault ("modColour", false) ? col::mod : col::accent;
    g.setColour (on ? onCol.withAlpha (down ? 0.55f : 0.32f) : (hover ? col::edge : col::panelHi));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? onCol : col::edge);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void LiqLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.getToggleState() ? juce::Colours::white : (b.isEnabled() ? col::text.withAlpha (0.8f) : col::faint));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (3, 1), juce::Justification::centred, 1);
}

void LiqLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hover, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const float d = std::min (14.0f, r.getHeight() - 4.0f);
    auto box = juce::Rectangle<float> (d * 1.7f, d).withY (r.getCentreY() - d * 0.5f).withX (r.getX() + 2.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? col::accent.withAlpha (0.35f) : (hover ? col::edge : col::track));
    g.fillRoundedRectangle (box, d * 0.5f);
    g.setColour (on ? col::accent : col::faint);
    g.drawRoundedRectangle (box, d * 0.5f, 1.0f);
    const float kx = on ? box.getRight() - d * 0.5f : box.getX() + d * 0.5f;
    g.setColour (on ? col::accent : col::dim);
    g.fillEllipse (juce::Rectangle<float> (d - 4.0f, d - 4.0f).withCentre ({ kx, box.getCentreY() }));
    g.setColour (on ? col::text : col::dim);
    g.setFont (font (12.0f));
    g.drawFittedText (b.getButtonText(), r.withLeft (box.getRight() + 6.0f).toNearestInt(), juce::Justification::centredLeft, 1);
}

void LiqLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.isMouseOver (true) ? col::edge : col::panelHi);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (col::edge);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (col::dim);
    g.fillPath (arrow);
}

void LiqLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (4, 0, box.getWidth() - 22, box.getHeight());
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
    g.fillAll (col::panelHi);
    g.setColour (col::edge);
    g.drawRect (0, 0, w, h);
    g.setColour (col::text);
    g.setFont (font (12.5f));
    g.drawFittedText (text, juce::Rectangle<int> (w, h).reduced (6, 3), juce::Justification::centredLeft, 4);
}
}
