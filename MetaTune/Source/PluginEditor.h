#pragma once

#include "PluginProcessor.h"

// ---- brand colours (change these to restyle the whole plugin)
namespace mt
{
    const juce::Colour bg      { 0xff08040f };
    const juce::Colour panel   { 0xff1d0f33 };
    const juce::Colour accent  { 0xffb14cff };   // purple
    const juce::Colour neon    { 0xffe07bff };   // corrected pitch line
    const juce::Colour rawLine { 0xff8f86a8 };   // original pitch line
    const juce::Colour text    { 0xfff2eaff };
}

// Live pitch screen: grey = what you sang, glowing purple = what MetaTune outputs
class PitchView : public juce::Component, private juce::Timer
{
public:
    explicit PitchView (MetaTuneAudioProcessor& p);
    ~PitchView() override { stopTimer(); }
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }

    MetaTuneAudioProcessor& proc;
    float lo = 48.f, hi = 72.f;
};

class MetaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MetaLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;
};

class MetaTuneAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit MetaTuneAudioProcessorEditor (MetaTuneAudioProcessor&);
    ~MetaTuneAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setupKnob (Knob&, const juce::String& title, const juce::String& paramId, const juce::String& suffix);

    MetaTuneAudioProcessor& proc;
    MetaLookAndFeel laf;
    PitchView pitchView;

    juce::ComboBox keyBox, scaleBox;
    juce::TextButton fastButton { "FAST FLOW" }, hqButton { "HQ MODE" };
    Knob speed, amount, humanize, formant, mix;

    // attachments last so they are destroyed before the widgets they point at
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAtt, scaleAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   fastAtt, hqAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MetaTuneAudioProcessorEditor)
};
