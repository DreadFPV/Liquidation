#include "WaveformDisplay.h"
#include "../PluginProcessor.h"

namespace liq::ui
{
WaveformDisplay::WaveformDisplay (Ctx& c) : ctx (c)
{
    loopLength = std::make_unique<ParamCombo> (ctx.apvts, id::loopLength);
    hold = std::make_unique<ParamToggle> (ctx.apvts, id::recHold, "HOLD");
    hold->setTooltip ("Record Hold: freeze the capture buffer (Scrub mode)");
    addAndMakeVisible (*loopLength);
    addAndMakeVisible (*hold);
    addAndMakeVisible (meter);
    info.setFont (font (11.5f));
    info.setColour (juce::Label::textColourId, col::dim);
    info.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (info);
}

void WaveformDisplay::resized()
{
    auto top = getLocalBounds().removeFromTop (26).reduced (8, 3);
    loopLength->setBounds (top.removeFromLeft (92));
    top.removeFromLeft (6);
    hold->setBounds (top.removeFromLeft (52));
    top.removeFromLeft (8);
    meter.setBounds (top.removeFromLeft (70).withSizeKeepingCentre (70, 6));
    info.setBounds (top);
}

void WaveformDisplay::updatePeaks()
{
    auto& eng = ctx.proc.engine;
    const int64_t ringSize = eng.bufferSize();
    if (ringSize <= blockSize) return;
    const size_t numBlocks = (size_t) (ringSize / blockSize);
    if (peaks.size() != numBlocks) { peaks.assign (numBlocks, 0.0f); peaksDoneUpTo = 0; }

    const int64_t head = eng.writeHead();
    const int64_t lastComplete = head / blockSize;
    if (lastComplete < peaksDoneUpTo) peaksDoneUpTo = 0;   // engine was reset
    int64_t from = std::max (peaksDoneUpTo, lastComplete - (int64_t) numBlocks + 1);
    const auto& L = eng.bufferL();
    const auto& R = eng.bufferR();
    if (L.size() != (size_t) ringSize) return;

    for (int64_t b = from; b < lastComplete; ++b)
    {
        float pk = 0.0f;
        const int64_t s0 = b * blockSize;
        for (int64_t s = s0; s < s0 + blockSize; s += 2)
        {
            const size_t i = (size_t) (s % ringSize);
            pk = std::max (pk, std::max (std::abs (L[i]), std::abs (R[i])));
        }
        peaks[(size_t) (b % (int64_t) numBlocks)] = pk;
    }
    peaksDoneUpTo = lastComplete;
}

void WaveformDisplay::update()
{
    auto& d = ctx.proc.engine.display;
    updatePeaks();

    regionStart = d.regionStart.load();
    regionLen = std::max<int64_t> (1, d.regionLength.load());
    playhead = d.playhead.load();
    spray = d.sprayLen.load();
    recording = d.recording.load();
    scrub = d.isScrub.load();
    inputLevel = d.inputLevel.load();
    meter.setLevel (inputLevel);

    // fade old trails
    for (auto& t : trails) t.life -= 0.12f;
    trails.erase (std::remove_if (trails.begin(), trails.end(), [] (const Trail& t) { return t.life <= 0.0f; }), trails.end());

    dots.clear();
    const int n = d.numGrains.load();
    for (int i = 0; i < n; ++i)
    {
        GrainDot g { d.grainPos[(size_t) i].load(), d.grainTrail[(size_t) i].load(),
                     d.grainAmp[(size_t) i].load(), (int) d.grainSide[(size_t) i].load() };
        dots.push_back (g);
        if (trails.size() < 400) trails.push_back ({ g.x, 1.0f, g.side });
    }

    markerX.clear();
    if (! scrub)
    {
        const int nm = d.numMarkers.load();
        for (int k = 0; k < std::min (nm, EngineDisplay::maxMarkers); ++k)
        {
            const int64_t m = d.markers[(size_t) k].load();
            const float x = (float) ((double) (m - regionStart) / (double) regionLen);
            if (x >= 0.0f && x <= 1.0f) markerX.push_back (x);
        }
    }
    else
    {
        const int sm = (int) std::lround (ctx.apvts.getRawParameterValue (id::scrubMode)->load());
        const double gridBeats = scrubModeBeats (sm);
        const double loopBeats = loopLengthBeats ((int) std::lround (ctx.apvts.getRawParameterValue (id::loopLength)->load()));
        if (gridBeats > 0.0)
            for (double b = gridBeats; b < loopBeats - 1e-6; b += gridBeats)
                markerX.push_back ((float) (b / loopBeats));
    }

    // info text
    juce::String txt;
    const float note = d.detectedPitch.load();
    if (note > 0.0f)
    {
        const int nn = (int) std::lround (note);
        txt << "Pitch " << noteNames()[nn % 12] << juce::String (nn / 12 - 1) << "   ";
    }
    if (! scrub)
        txt << "Stretched " << juce::String (d.stretchedBpm.load(), 1) << " BPM";
    else
        txt << (recording ? "Recording loop" : "Loop held");
    info.setText (txt, juce::dontSendNotification);

    repaint();
}

void WaveformDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (col::panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (col::edge);
    g.drawRoundedRectangle (r, 8.0f, 1.0f);

    auto wave = getLocalBounds().toFloat().reduced (8.0f).withTrimmedTop (22.0f);
    g.setColour (col::bg);
    g.fillRoundedRectangle (wave, 5.0f);

    const float midY = wave.getCentreY();
    const float halfH = wave.getHeight() * 0.46f;

    // grid markers
    for (float mx : markerX)
    {
        const float x = wave.getX() + mx * wave.getWidth();
        g.setColour ((scrub ? col::faint : col::amber.withAlpha (0.5f)));
        g.drawVerticalLine ((int) x, wave.getY() + 2, wave.getBottom() - 2);
    }

    // waveform
    if (! peaks.empty())
    {
        const int64_t numBlocks = (int64_t) peaks.size();
        const int w = (int) wave.getWidth();
        juce::Path p;
        std::vector<float> cols (w, 0.0f);
        const double blocksPerPx = (double) regionLen / blockSize / std::max (1, w);
        const int64_t firstBlock = regionStart / blockSize;
        const int64_t newest = peaksDoneUpTo;
        for (int x = 0; x < w; ++x)
        {
            const int64_t b0 = firstBlock + (int64_t) (x * blocksPerPx);
            const int64_t b1 = std::max (b0 + 1, firstBlock + (int64_t) ((x + 1) * blocksPerPx));
            float pk = 0.0f;
            for (int64_t b = b0; b < b1; ++b)
                if (b >= 0 && b < newest && b > newest - numBlocks)
                    pk = std::max (pk, peaks[(size_t) (b % numBlocks)]);
            cols[(size_t) x] = std::min (1.0f, std::sqrt (pk));   // gentle compression for visibility
        }
        p.startNewSubPath (wave.getX(), midY);
        for (int x = 0; x < w; ++x) p.lineTo (wave.getX() + (float) x, midY - cols[(size_t) x] * halfH);
        for (int x = w - 1; x >= 0; --x) p.lineTo (wave.getX() + (float) x, midY + cols[(size_t) x] * halfH);
        p.closeSubPath();
        juce::ColourGradient grad (col::accent.withAlpha (0.55f), wave.getX(), midY - halfH,
                                   col::accentDk.withAlpha (0.35f), wave.getX(), midY, true);
        g.setGradientFill (grad);
        g.fillPath (p);
    }

    // spray range + playhead
    const float px = wave.getX() + juce::jlimit (0.0f, 1.0f, playhead) * wave.getWidth();
    const float sw = spray * wave.getWidth();
    if (sw > 1.0f)
    {
        g.setColour (col::amber.withAlpha (0.13f));
        if (scrub) g.fillRect (juce::Rectangle<float> (px - sw, wave.getY(), sw * 2.0f, wave.getHeight()).getIntersection (wave));
        else       g.fillRect (juce::Rectangle<float> (px - sw, wave.getY(), sw, wave.getHeight()).getIntersection (wave));
    }

    // grain trails
    for (auto& t : trails)
    {
        const float x = wave.getX() + t.x * wave.getWidth();
        g.setColour ((t.side < 0 ? col::left : col::right).withAlpha (0.18f * t.life));
        g.fillRect (juce::Rectangle<float> (2.0f, halfH * 0.9f).withCentre ({ x, t.side < 0 ? midY - halfH * 0.5f : midY + halfH * 0.5f }));
    }
    // active grains
    for (auto& d : dots)
    {
        const float x = wave.getX() + d.x * wave.getWidth();
        const float h = halfH * (0.35f + 0.6f * d.amp);
        const float cy = d.side < 0 ? midY - halfH * 0.5f : midY + halfH * 0.5f;
        g.setColour ((d.side < 0 ? col::left : col::right).withAlpha (0.9f));
        g.fillRoundedRectangle (juce::Rectangle<float> (2.5f, h).withCentre ({ x, cy }), 1.2f);
    }

    g.setColour (col::text);
    g.fillRect (juce::Rectangle<float> (2.0f, wave.getHeight()).withCentre ({ px, midY }));
    g.setColour (col::accent);
    g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ px, wave.getY() + 4.0f }));

    // record indicator
    g.setColour (recording ? col::red : col::dim);
    g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ wave.getRight() - 8.0f, wave.getY() + 8.0f }));

    if (scrub)
    {
        g.setFont (font (10.5f));
        g.setColour (col::dim);
        g.drawText ("click / drag to scrub", wave.reduced (6.0f), juce::Justification::bottomRight);
    }
}

void WaveformDisplay::setScrubFromMouse (float x)
{
    auto wave = getLocalBounds().toFloat().reduced (8.0f).withTrimmedTop (22.0f);
    const float v = juce::jlimit (0.0f, 1.0f, (x - wave.getX()) / wave.getWidth());
    if (auto* p = ctx.apvts.getParameter (id::scrubPos)) p->setValueNotifyingHost (v);
}

void WaveformDisplay::mouseDown (const juce::MouseEvent& e)
{
    if (! scrub || e.y < 26) return;
    if (auto* p = ctx.apvts.getParameter (id::scrubPos)) p->beginChangeGesture();
    draggingScrub = true;
    setScrubFromMouse ((float) e.x);
}

void WaveformDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingScrub) setScrubFromMouse ((float) e.x);
}

void WaveformDisplay::mouseUp (const juce::MouseEvent&)
{
    if (draggingScrub)
        if (auto* p = ctx.apvts.getParameter (id::scrubPos)) p->endChangeGesture();
    draggingScrub = false;
}
}
