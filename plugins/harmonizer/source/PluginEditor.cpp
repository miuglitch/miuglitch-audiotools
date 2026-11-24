#include "PluginEditor.h"

namespace
{
    const juce::Colour kBg      { 0xff101018 };
    const juce::Colour kPanel   { 0xff1b1b28 };
    const juce::Colour kAccent  { 0xff9ad8ff };
    const juce::Colour kAccent2 { 0xffe6a8ff };
    const juce::Colour kText    { 0xffd8d8e8 };

    constexpr int kWidth       = 640;
    constexpr int kBaseHeight  = 330;
    constexpr int kKeysHeight  = 150;

    const char* kNoteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    bool isBlack (int pc) { return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }
}

//==============================================================================
// CustomScaleKeyboard
//==============================================================================

juce::Rectangle<float> CustomScaleKeyboard::keyBounds (int note) const
{
    const auto area   = getLocalBounds().toFloat();
    const float whiteW = area.getWidth() / 14.0f;
    static const int whiteIndex[12] = { 0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6 };
    const int oct = note / 12, pc = note % 12;
    const float x0 = (float) (oct * 7 + whiteIndex[pc]) * whiteW;

    if (! isBlack (pc))
        return { x0, 0.0f, whiteW, area.getHeight() };

    const float blackW = whiteW * 0.6f;
    return { x0 + whiteW - blackW * 0.5f, 0.0f, blackW, area.getHeight() * 0.6f };
}

int CustomScaleKeyboard::noteAt (juce::Point<float> p) const
{
    for (int n = 0; n < 24; ++n)                       // black keys sit on top
        if (isBlack (n % 12) && keyBounds (n).contains (p)) return n;
    for (int n = 0; n < 24; ++n)
        if (! isBlack (n % 12) && keyBounds (n).contains (p)) return n;
    return -1;
}

void CustomScaleKeyboard::applyAt (juce::Point<float> p)
{
    const int n = noteAt (p);
    if (n < 0 || n == lastNote_) return;
    lastNote_ = n;
    const auto bit = 1u << n;
    mask_ = paintOn_ ? (mask_ | bit) : (mask_ & ~bit);
    repaint();
    if (onChange) onChange (mask_);
}

void CustomScaleKeyboard::mouseDown (const juce::MouseEvent& e)
{
    const int n = noteAt (e.position);
    if (n < 0) return;
    paintOn_  = ((mask_ >> n) & 1u) == 0;   // click toggles; drag paints the same way
    lastNote_ = -1;
    applyAt (e.position);
}

void CustomScaleKeyboard::mouseDrag (const juce::MouseEvent& e) { applyAt (e.position); }

void CustomScaleKeyboard::paint (juce::Graphics& g)
{
    auto drawKey = [&] (int n)
    {
        auto r = keyBounds (n).reduced (1.0f, 0.0f);
        const bool on = ((mask_ >> n) & 1u) != 0;
        const bool black = isBlack (n % 12);
        const auto onCol = n < 12 ? kAccent : kAccent2;

        g.setColour (on ? onCol : (black ? juce::Colour (0xff22222e) : juce::Colour (0xffe8e8f0)));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);

        if (! black || on)
        {
            g.setColour (on ? juce::Colours::black : juce::Colour (0xff606070));
            g.setFont (11.0f);
            juce::String name (kNoteNames[n % 12]);
            if (n % 12 == 0) name << (n < 12 ? " low" : " high");
            g.drawFittedText (name, r.removeFromBottom (18.0f).toNearestInt(),
                              juce::Justification::centred, 1);
        }
    };
    for (int n = 0; n < 24; ++n) if (! isBlack (n % 12)) drawKey (n);
    for (int n = 0; n < 24; ++n) if (  isBlack (n % 12)) drawKey (n);
}

//==============================================================================
// Editor
//==============================================================================

SpectralHarmonizerAudioProcessorEditor::SpectralHarmonizerAudioProcessorEditor (
        SpectralHarmonizerAudioProcessor& p)
    : AudioProcessorEditor (&p), processor_ (p)
{
    setupSlider (amount_,       ParamID::Amount,       "Amount",    true);
    setupSlider (sustain_,      ParamID::Sustain,      "Sustain",   true);
    setupSlider (harmonics_,    ParamID::Harmonics,    "Harmonics", false);
    setupSlider (bendRange_,    ParamID::BendRange,    "Bend",      false);
    setupSlider (mpeBendRange_, ParamID::MpeBendRange, "MPE Bend",  false);

    setupCombo (root_,        ParamID::Root,        "Root");
    setupCombo (scale_,       ParamID::Scale,       "Scale");
    setupCombo (source_,      ParamID::Source,      "Targets from");
    setupCombo (midiTargets_, ParamID::MidiTargets, "MIDI notes as");
    setupCombo (fftSize_,     ParamID::FftSize,     "FFT");

    for (auto* b : { &latch_, &mpe_ })
    {
        b->setColour (juce::ToggleButton::textColourId, kText);
        b->setColour (juce::ToggleButton::tickColourId, kAccent);
        addAndMakeVisible (*b);
    }
    latchAttach_ = std::make_unique<ButtonAttachment> (processor_.apvts, ParamID::Latch, latch_);
    mpeAttach_   = std::make_unique<ButtonAttachment> (processor_.apvts, ParamID::Mpe,   mpe_);

    keyboard_.setMask (processor_.getCustomMask());
    keyboard_.onChange = [this] (std::uint32_t m) { processor_.setCustomMask (m); };
    addChildComponent (keyboard_);

    clearButton_.onClick = [this]
    {
        keyboard_.setMask (0);
        processor_.setCustomMask (0);
    };
    fromScaleButton_.setTooltip ("Fill both octaves from the last chosen scale and root");
    fromScaleButton_.onClick = [this]
    {
        const auto& s  = GlitchCore::kScales[(size_t) lastRealScale_];
        const int root = (int) processor_.apvts.getRawParameterValue (ParamID::Root)->load();
        std::uint32_t m = 0;
        for (int n = 0; n < 24; ++n)
            if ((s.mask >> (((n - root) % 12 + 12) % 12)) & 1u) m |= 1u << n;
        keyboard_.setMask (m);
        processor_.setCustomMask (m);
    };
    addChildComponent (clearButton_);
    addChildComponent (fromScaleButton_);

    statusLabel_.setColour (juce::Label::textColourId, kAccent);
    statusLabel_.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (statusLabel_);

    const int sc = (int) processor_.apvts.getRawParameterValue (ParamID::Scale)->load();
    if (sc < GlitchCore::kCustomScaleIndex) lastRealScale_ = sc;

    setSize (kWidth, kBaseHeight);
    updateCustomVisibility();
    startTimerHz (15);
}

SpectralHarmonizerAudioProcessorEditor::~SpectralHarmonizerAudioProcessorEditor()
{
    stopTimer();
}

void SpectralHarmonizerAudioProcessorEditor::setupSlider (LabelledSlider& s, const juce::String& id,
                                                          const juce::String& text, bool rotary)
{
    s.slider.setSliderStyle (rotary ? juce::Slider::RotaryHorizontalVerticalDrag
                                    : juce::Slider::LinearHorizontal);
    s.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    if (! rotary) s.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 18);
    s.slider.setColour (juce::Slider::rotarySliderFillColourId, kAccent);
    s.slider.setColour (juce::Slider::thumbColourId, kAccent2);
    s.slider.setColour (juce::Slider::trackColourId, kAccent.withAlpha (0.6f));
    s.slider.setColour (juce::Slider::textBoxTextColourId, kText);
    s.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.label.setText (text, juce::dontSendNotification);
    s.label.setColour (juce::Label::textColourId, kText);
    s.label.setJustificationType (rotary ? juce::Justification::centred : juce::Justification::centredLeft);
    addAndMakeVisible (s.slider);
    addAndMakeVisible (s.label);
    s.attach = std::make_unique<SliderAttachment> (processor_.apvts, id, s.slider);
}

void SpectralHarmonizerAudioProcessorEditor::setupCombo (LabelledCombo& c, const juce::String& id,
                                                         const juce::String& text)
{
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (processor_.apvts.getParameter (id)))
        c.box.addItemList (param->choices, 1);
    c.label.setText (text, juce::dontSendNotification);
    c.label.setColour (juce::Label::textColourId, kText);
    addAndMakeVisible (c.box);
    addAndMakeVisible (c.label);
    c.attach = std::make_unique<ComboAttachment> (processor_.apvts, id, c.box);
}

void SpectralHarmonizerAudioProcessorEditor::updateCustomVisibility()
{
    const int sc = (int) processor_.apvts.getRawParameterValue (ParamID::Scale)->load();
    const bool custom = sc >= GlitchCore::kCustomScaleIndex;
    if (! custom) lastRealScale_ = sc;
    if (custom == customShown_ && getHeight() > 0) return;

    customShown_ = custom;
    keyboard_.setVisible (custom);
    clearButton_.setVisible (custom);
    fromScaleButton_.setVisible (custom);
    root_.box.setEnabled (! custom);      // custom notes are absolute
    setSize (kWidth, kBaseHeight + (custom ? kKeysHeight : 0));
}

void SpectralHarmonizerAudioProcessorEditor::timerCallback()
{
    updateCustomVisibility();
    keyboard_.setMask (processor_.getCustomMask());

    const int notes = processor_.getHeldNoteCount();
    juce::String status;
    if (processor_.isFollowingMidi())
        status = notes > 0 ? "Following MIDI: " + juce::String (notes) + (notes == 1 ? " note" : " notes")
                           : "Waiting for MIDI";
    else
        status = "Following scale";
    status << "   |   latency " << processor_.getLatencySamples() << " smp";
    statusLabel_.setText (status, juce::dontSendNotification);
}

void SpectralHarmonizerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    g.setColour (kPanel);
    g.fillRoundedRectangle (getLocalBounds().reduced (8).toFloat(), 6.0f);

    g.setColour (kText);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawText ("SPECTRAL HARMONIZER", 20, 14, 300, 26, juce::Justification::centredLeft);

    if (customShown_)
    {
        g.setColour (kText.withAlpha (0.7f));
        g.setFont (12.0f);
        g.drawText ("Custom scale: click or drag keys. The two-octave pattern repeats across the range.",
                    20, kBaseHeight - 6, kWidth - 40, 16, juce::Justification::centredLeft);
    }
}

void SpectralHarmonizerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (20);
    auto header = area.removeFromTop (26);
    statusLabel_.setBounds (header.removeFromRight (340));
    area.removeFromTop (10);

    auto main = area.removeFromTop (kBaseHeight - 80);

    // Left: the two big dials
    auto dials = main.removeFromLeft (230);
    auto placeDial = [] (LabelledSlider& s, juce::Rectangle<int> r)
    {
        s.label.setBounds (r.removeFromTop (18));
        s.slider.setBounds (r.reduced (4));
    };
    placeDial (amount_,  dials.removeFromLeft (115).removeFromTop (150));
    placeDial (sustain_, dials.removeFromTop (150));

    // Right: target settings
    main.removeFromLeft (16);
    auto row = [&main] () { auto r = main.removeFromTop (24); main.removeFromTop (6); return r; };
    auto placeCombo = [] (LabelledCombo& c, juce::Rectangle<int> r)
    {
        c.label.setBounds (r.removeFromLeft (110));
        c.box.setBounds (r);
    };
    {
        auto r = row();
        placeCombo (root_, r.removeFromLeft (190));
        r.removeFromLeft (10);
        fftSize_.label.setBounds (r.removeFromLeft (34));
        fftSize_.box.setBounds (r);
    }
    placeCombo (scale_,       row());
    placeCombo (source_,      row());
    placeCombo (midiTargets_, row());

    auto placeLinear = [] (LabelledSlider& s, juce::Rectangle<int> r)
    {
        s.label.setBounds (r.removeFromLeft (110));
        s.slider.setBounds (r);
    };
    placeLinear (harmonics_, row());
    {
        auto r = row();
        latch_.setBounds (r.removeFromLeft (110));
        mpe_.setBounds (r.removeFromLeft (80));
    }
    {
        auto r = row();
        placeLinear (bendRange_, r.removeFromLeft (r.getWidth() / 2 + 40));
        mpeBendRange_.label.setBounds (r.removeFromLeft (64));
        mpeBendRange_.slider.setBounds (r);
    }
    bendRange_.label.setText ("Bend (st)", juce::dontSendNotification);

    if (customShown_)
    {
        auto keys = getLocalBounds().reduced (20).removeFromBottom (kKeysHeight - 24);
        auto buttons = keys.removeFromRight (100);
        clearButton_.setBounds (buttons.removeFromTop (28));
        buttons.removeFromTop (8);
        fromScaleButton_.setBounds (buttons.removeFromTop (28));
        keyboard_.setBounds (keys.withTrimmedRight (12));
    }
}
