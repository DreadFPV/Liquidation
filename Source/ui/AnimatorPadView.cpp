#include "AnimatorPadView.h"
#include "../PluginProcessor.h"

namespace liq::ui
{
void AnimatorPadView::update()
{
    const float x = ctx.proc.animator.cursorX.load(), y = ctx.proc.animator.cursorY.load();
    if (std::abs (x - cx) > 0.001f || std::abs (y - cy) > 0.001f) { cx = x; cy = y; repaint(); }
}

void AnimatorPadView::paint (juce::Graphics& g)
{
    auto r = padArea();
    g.setColour (col::bg);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (col::edge);
    g.drawRoundedRectangle (r, 8.0f, 1.0f);

    // grid
    g.setColour (col::track);
    for (int i = 1; i < 8; ++i)
    {
        const float fx = r.getX() + r.getWidth() * i / 8.0f, fy = r.getY() + r.getHeight() * i / 8.0f;
        g.drawVerticalLine ((int) fx, r.getY() + 4, r.getBottom() - 4);
        g.drawHorizontalLine ((int) fy, r.getX() + 4, r.getRight() - 4);
    }

    auto& s = ctx.apvts;
    const float hx = s.getRawParameterValue (id::macro1)->load();
    const float hy = s.getRawParameterValue (id::macro2)->load();
    const float depth = s.getRawParameterValue (id::animDepth)->load();
    const int mode = (int) std::lround (s.getRawParameterValue (id::animMode)->load());
    const int cycles = (int) std::lround (s.getRawParameterValue (id::animCycles)->load());
    const float warp = s.getRawParameterValue (id::animWarp)->load();

    auto toScreen = [&] (float x, float y)
    {
        return juce::Point<float> (r.getX() + juce::jlimit (0.0f, 1.0f, x) * r.getWidth(),
                                   r.getBottom() - juce::jlimit (0.0f, 1.0f, y) * r.getHeight());
    };

    // animator path
    if (depth > 0.001f)
    {
        juce::Path path;
        for (int i = 0; i <= 400; ++i)
        {
            float px, py;
            Animator::shapePoint (mode, cycles, warp, i / 400.0, px, py);
            const auto pt = toScreen (hx + px * depth * 0.5f, hy + py * depth * 0.5f);
            if (i == 0) path.startNewSubPath (pt); else path.lineTo (pt);
        }
        g.setColour (col::mod.withAlpha (0.55f));
        g.strokePath (path, juce::PathStrokeType (1.6f));
    }

    // handle
    const auto h = toScreen (hx, hy);
    g.setColour (col::accent.withAlpha (0.18f));
    g.fillEllipse (juce::Rectangle<float> (34.0f, 34.0f).withCentre (h));
    g.setColour (col::accent);
    g.drawEllipse (juce::Rectangle<float> (18.0f, 18.0f).withCentre (h), 2.0f);

    // live cursor
    const auto c = toScreen (cx, cy);
    g.setColour (juce::Colours::white);
    g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (c));

    g.setFont (font (10.5f, true));
    g.setColour (col::dim);
    g.drawText ("MACRO 1  >", r.reduced (8.0f).removeFromBottom (14.0f), juce::Justification::bottomRight);
    g.saveState();
    g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, r.getX() + 14.0f, r.getY() + 64.0f));
    g.drawText ("MACRO 2  >", juce::Rectangle<float> (r.getX() + 14.0f, r.getY() + 58.0f, 80.0f, 12.0f), juce::Justification::centredLeft);
    g.restoreState();
}

void AnimatorPadView::setFromMouse (juce::Point<float> p)
{
    auto r = padArea();
    const float x = juce::jlimit (0.0f, 1.0f, (p.x - r.getX()) / r.getWidth());
    const float y = juce::jlimit (0.0f, 1.0f, (r.getBottom() - p.y) / r.getHeight());
    ctx.apvts.getParameter (id::macro1)->setValueNotifyingHost (x);
    ctx.apvts.getParameter (id::macro2)->setValueNotifyingHost (y);
    repaint();
}

void AnimatorPadView::mouseDown (const juce::MouseEvent& e)
{
    ctx.apvts.getParameter (id::macro1)->beginChangeGesture();
    ctx.apvts.getParameter (id::macro2)->beginChangeGesture();
    dragging = true;
    setFromMouse (e.position);
}

void AnimatorPadView::mouseDrag (const juce::MouseEvent& e) { if (dragging) setFromMouse (e.position); }

void AnimatorPadView::mouseUp (const juce::MouseEvent&)
{
    if (! dragging) return;
    ctx.apvts.getParameter (id::macro1)->endChangeGesture();
    ctx.apvts.getParameter (id::macro2)->endChangeGesture();
    dragging = false;
}

void AnimatorPadView::mouseDoubleClick (const juce::MouseEvent&)
{
    for (auto* pid : { &id::macro1, &id::macro2 })
    {
        auto* p = ctx.apvts.getParameter (*pid);
        p->beginChangeGesture(); p->setValueNotifyingHost (0.0f); p->endChangeGesture();
    }
}
}
