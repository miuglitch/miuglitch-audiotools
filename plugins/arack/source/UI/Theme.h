#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Colours and type from Docs/DesignLanguage.md (dark theme, as in mockup 4b).
namespace Theme
{
    using juce::Colour;

    inline const Colour orange      { 0xffff5a1f };
    inline const Colour red         { 0xffe0322b };
    inline const Colour blue        { 0xff3a86d4 };
    inline const Colour cyan        { 0xff0f8a94 };
    inline const Colour frame       { 0xff3b3b3b };
    inline const Colour display     { 0xff0f0f0f };
    inline const Colour displayText { 0xfff4f4f2 };
    inline const Colour displayLabel{ 0xff8a8a86 };
    inline const Colour displayDiv  { 0xff262626 };
    inline const Colour displayCtrl { 0xff1d1d1d };
    inline const Colour logo        { 0xffc9c7c2 };

    inline const Colour bg          { 0xff0d0d0d };
    inline const Colour fg          { 0xfff2f2f0 };
    inline const Colour headerText  { 0xffffffff };
    inline const Colour headerSub   { 0xff7c7c78 };
    inline const Colour rowLine     { 0xff1e1e1e };
    inline const Colour hover       { 0xff232323 };
    inline const Colour bandA       { 0xff121212 };
    inline const Colour warm        { 0xff3a1a0e };
    inline const Colour muted       { 0xff3f3f3d };
    inline const Colour mutedText   { 0xff5c5b58 };
    inline const Colour btnBorder   { 0xff3b3b3b };
    inline const Colour btnText     { 0xff8c8c88 };
    inline const Colour panel       { 0xff0a0a0a };
    inline const Colour orangeText  { 0xffff7a45 };

    inline Colour laneColour (int i)
    {
        static const Colour c[] { orange, blue, cyan, red, Colour (0xffb36ae2), Colour (0xffe2b33a) };
        return c[(size_t) (std::abs (i) % 6)];
    }

    inline juce::Font font (float h, bool bold = false)
    {
        return juce::Font (juce::FontOptions ("Segoe UI", h, bold ? juce::Font::bold : juce::Font::plain));
    }

    inline juce::Font light (float h)
    {
        return juce::Font (juce::FontOptions().withName ("Segoe UI").withStyle ("Light").withHeight (h));
    }

    // Capitals with tracking, for labels and buttons.
    inline juce::Font caps (float h)
    {
        return font (h, true).withExtraKerningFactor (0.08f);
    }

    inline void drawCaps (juce::Graphics& g, const juce::String& s, juce::Rectangle<int> r, float h,
                          juce::Justification j = juce::Justification::centredLeft)
    {
        g.setFont (caps (h));
        g.drawText (s.toUpperCase(), r, j, true);
    }
}

class ARAckLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    ARAckLookAndFeel()
    {
        setDefaultSansSerifTypefaceName ("Segoe UI");
        setColour (juce::ResizableWindow::backgroundColourId, Theme::bg);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1d1d1d));
        setColour (juce::PopupMenu::textColourId, Theme::fg);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, Theme::orange);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::ScrollBar::thumbColourId, Theme::frame);
        setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        setColour (juce::TextButton::textColourOffId, Theme::btnText);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::ListBox::backgroundColourId, Theme::bg);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();

        if (on && (bool) b.getProperties()["accent"])
        {
            g.setColour (over ? Theme::orangeText : Theme::orange);
            g.fillRoundedRectangle (r, 5.0f);
            return;
        }
        g.setColour (over || down ? Theme::hover : juce::Colours::transparentBlack);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (on ? juce::Colour (0xff8c8c88) : Theme::btnBorder);
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override
    {
        return Theme::caps (juce::jmin (12.0f, (float) h * 0.45f));
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.setColour (b.getToggleState() ? juce::Colours::white : Theme::btnText);
        g.drawText (b.getButtonText().toUpperCase(), b.getLocalBounds(), juce::Justification::centred);
    }

    juce::Font getPopupMenuFont() override { return Theme::font (14.0f); }
};
