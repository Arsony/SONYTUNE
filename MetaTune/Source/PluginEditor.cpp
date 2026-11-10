#include "PluginEditor.h"

namespace
{
    juce::String noteName (int midi)
    {
        static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const int pc = ((midi % 12) + 12) % 12;
        return juce::String (names[pc]) + juce::String (midi / 12 - 1);
    }
}

// ============================================================================
//  Look and feel
// ============================================================================
MetaLookAndFeel::MetaLookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, mt::panel);
    setColour (juce::ComboBox::textColourId,       mt::text);
    setColour (juce::ComboBox::outlineColourId,    mt::accent.withAlpha (0.5f));
    setColour (juce::ComboBox::arrowColourId,      mt::accent);
    setColour (juce::PopupMenu::backgroundColourId,            mt::panel);
    setColour (juce::PopupMenu::textColourId,                  mt::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, mt::accent);
    setColour (juce::PopupMenu::highlightedTextColourId,       juce::Colours::white);
    setColour (juce::TextButton::buttonColourId,   mt::panel);
    setColour (juce::TextButton::buttonOnColourId, mt::accent);
    setColour (juce::TextButton::textColourOffId,  mt::text.withAlpha (0.8f));
    setColour (juce::TextButton::textColourOnId,   juce::Colours::white);
    setColour (juce::Slider::textBoxTextColourId,    mt::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId,            mt::text);
}

void MetaLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float pos, float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (10.f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto  centre = bounds.getCentre();
    const float angle  = startAngle + pos * (endAngle - startAngle);
    const juce::PathStrokeType stroke (6.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.f, startAngle, endAngle, true);
    g.setColour (mt::panel.brighter (0.15f));
    g.strokePath (track, stroke);

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius, radius, 0.f, startAngle, angle, true);
    g.setColour (mt::accent.withAlpha (0.25f));                       // glow
    g.strokePath (value, juce::PathStrokeType (12.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (mt::neon);
    g.strokePath (value, stroke);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -radius + 9.f, 3.f, radius * 0.38f, 1.5f);
    g.setColour (mt::text);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

// ============================================================================
//  Pitch view
// ============================================================================
PitchView::PitchView (MetaTuneAudioProcessor& p) : proc (p)
{
    startTimerHz (30);
}

void PitchView::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a0b2e), b.getX(), b.getY(),
                                             juce::Colour (0xff070310), b.getRight(), b.getBottom(), false));
    g.fillRoundedRectangle (b, 12.f);
    g.setColour (mt::accent.withAlpha (0.25f));
    g.drawRoundedRectangle (b.reduced (0.5f), 12.f, 1.f);

    const auto area = b.reduced (6.f).withTrimmedLeft (44.f).reduced (0.f, 10.f);

    // ---- copy the history out of the audio engine (oldest -> newest)
    constexpr int N = metatune::Engine::kVizSize;
    float raw[N], out[N];
    const int w = proc.engine.vizWrite.load (std::memory_order_acquire);
    for (int i = 0; i < N; ++i)
    {
        const int src = (((w - N + i) % N) + N) % N;
        raw[i] = proc.engine.vizRaw[(size_t) src].load (std::memory_order_relaxed);
        out[i] = proc.engine.vizOut[(size_t) src].load (std::memory_order_relaxed);
    }

    // ---- vertical range follows the singer
    float mn = 1.0e9f, mx = -1.0e9f;
    for (int i = 0; i < N; ++i)
    {
        if (raw[i] >= 0.f) { mn = juce::jmin (mn, raw[i], out[i]); mx = juce::jmax (mx, raw[i], out[i]); }
    }
    if (mx >= mn)
    {
        float tLo = mn - 3.f, tHi = mx + 3.f;
        if (tHi - tLo < 16.f) { const float c = 0.5f * (tHi + tLo); tLo = c - 8.f; tHi = c + 8.f; }
        lo += (tLo - lo) * 0.08f;
        hi += (tHi - hi) * 0.08f;
    }

    auto toY = [&] (float m) { return area.getBottom() - (m - lo) / (hi - lo) * area.getHeight(); };

    // ---- note grid: notes that are IN the chosen key/scale are bright
    const int key  = (int) proc.apvts.getRawParameterValue ("key")->load();
    const int mask = metatune::scaleMask ((int) proc.apvts.getRawParameterValue ("scale")->load());
    g.setFont (juce::Font (juce::FontOptions (11.f)));
    for (int m = (int) std::ceil (lo); m <= (int) std::floor (hi); ++m)
    {
        const bool allowed = metatune::isAllowedNote (m, key, mask);
        const float y = toY ((float) m);
        g.setColour (mt::accent.withAlpha (allowed ? 0.35f : 0.06f));
        g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
        if (allowed)
        {
            g.setColour (mt::text.withAlpha (0.7f));
            g.drawText (noteName (m), (int) b.getX() + 4, (int) y - 8, 38, 16, juce::Justification::centredRight);
        }
    }

    // ---- traces
    auto buildPath = [&] (const float* v)
    {
        juce::Path p;
        bool pen = false;
        for (int i = 0; i < N; ++i)
        {
            if (v[i] < 0.f) { pen = false; continue; }
            const float x = area.getX() + area.getWidth() * (float) i / (float) (N - 1);
            if (! pen) { p.startNewSubPath (x, toY (v[i])); pen = true; }
            else         p.lineTo (x, toY (v[i]));
        }
        return p;
    };

    const auto rawPath = buildPath (raw);
    const auto outPath = buildPath (out);

    g.setColour (mt::rawLine.withAlpha (0.65f));
    g.strokePath (rawPath, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (mt::accent.withAlpha (0.12f));
    g.strokePath (outPath, juce::PathStrokeType (10.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (mt::accent.withAlpha (0.30f));
    g.strokePath (outPath, juce::PathStrokeType (5.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (mt::neon);
    g.strokePath (outPath, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // ---- "lock" indicator at the head of the line
    if (out[N - 1] >= 0.f)
    {
        const float y = toY (out[N - 1]);
        g.setColour (mt::neon.withAlpha (0.35f));
        g.fillEllipse (area.getRight() - 12.f, y - 9.f, 18.f, 18.f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (area.getRight() - 7.f, y - 4.f, 8.f, 8.f);
        g.setColour (mt::text);
        g.setFont (juce::Font (juce::FontOptions (13.f, juce::Font::bold)));
        g.drawText (noteName ((int) std::lround (out[N - 1])), (int) area.getRight() - 70, (int) y - 28, 60, 16,
                    juce::Justification::centredRight);
    }

    // ---- latency read-out
    const double sr = juce::jmax (1.0, proc.getSampleRate());
    g.setColour (mt::text.withAlpha (0.45f));
    g.setFont (juce::Font (juce::FontOptions (11.f)));
    g.drawText ("LATENCY " + juce::String (1000.0 * proc.engine.getLatencySamples() / sr, 1) + " ms",
                (int) b.getRight() - 150, (int) b.getY() + 6, 140, 14, juce::Justification::centredRight);
}

// ============================================================================
//  Editor
// ============================================================================
MetaTuneAudioProcessorEditor::MetaTuneAudioProcessorEditor (MetaTuneAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), pitchView (p)
{
    setLookAndFeel (&laf);
    setSize (900, 580);

    addAndMakeVisible (pitchView);

    keyBox.addItemList (juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    for (int i = 0; i < metatune::kNumScales; ++i) scaleBox.addItem (metatune::scaleName (i), i + 1);
    addAndMakeVisible (keyBox);
    addAndMakeVisible (scaleBox);
    keyAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "key",   keyBox);
    scaleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "scale", scaleBox);

    for (auto* btn : { &fastButton, &hqButton })
    {
        btn->setClickingTogglesState (true);
        addAndMakeVisible (*btn);
    }
    fastAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "fast", fastButton);
    hqAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "hq",   hqButton);

    setupKnob (speed,    "SPEED",    "speed",    " ms");
    setupKnob (amount,   "AMOUNT",   "amount",   " %");
    setupKnob (humanize, "HUMANIZE", "humanize", " %");
    setupKnob (formant,  "FORMANT",  "formant",  " st");
    setupKnob (mix,      "MIX",      "mix",      " %");
}

MetaTuneAudioProcessorEditor::~MetaTuneAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void MetaTuneAudioProcessorEditor::setupKnob (Knob& k, const juce::String& title, const juce::String& paramId,
                                              const juce::String& suffix)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 20);
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    k.slider.setTextValueSuffix (suffix);
    k.slider.setNumDecimalPlacesToDisplay (1);
    addAndMakeVisible (k.slider);

    k.label.setText (title, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::Font (juce::FontOptions (13.f, juce::Font::bold)));
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k.slider);
}

void MetaTuneAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (mt::bg);

    g.setColour (mt::text);
    g.setFont (juce::Font (juce::FontOptions (36.f, juce::Font::bold)));
    g.drawText ("METATUNE", 24, 10, 400, 44, juce::Justification::centredLeft);

    g.setColour (mt::accent);
    g.setFont (juce::Font (juce::FontOptions (12.f, juce::Font::bold)));
    g.drawText ("PITCH LOCK  /  CRUSHAUDIO", 26, 48, 400, 16, juce::Justification::centredLeft);
}

void MetaTuneAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (20);
    r.removeFromTop (56);                                   // title area
    pitchView.setBounds (r.removeFromTop (250));
    r.removeFromTop (14);

    auto row = r.removeFromTop (34);
    const int gap = 10, wBox = (row.getWidth() - 3 * gap) / 4;
    keyBox.setBounds    (row.removeFromLeft (wBox));  row.removeFromLeft (gap);
    scaleBox.setBounds  (row.removeFromLeft (wBox));  row.removeFromLeft (gap);
    fastButton.setBounds (row.removeFromLeft (wBox)); row.removeFromLeft (gap);
    hqButton.setBounds  (row);

    r.removeFromTop (12);
    const int kw = r.getWidth() / 5;
    Knob* knobs[5] = { &speed, &amount, &humanize, &formant, &mix };
    for (auto* k : knobs)
    {
        auto col = r.removeFromLeft (kw);
        k->label.setBounds (col.removeFromTop (20));
        k->slider.setBounds (col);
    }
}
