#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;

//==============================================================================
void IconButton::paintButton (Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (down ? Theme::frame : (over ? Colour (0xff2a2a2a) : Theme::displayCtrl));
    g.fillRoundedRectangle (r, 6.0f);

    auto c = r.getCentre();
    Path p;

    if (kind == Kind::undo || kind == Kind::redo)
    {
        // curved arrow
        p.startNewSubPath (c.x - 5.0f, c.y - 3.0f);
        p.lineTo (c.x + 2.0f, c.y - 3.0f);
        p.cubicTo (c.x + 7.0f, c.y - 3.0f, c.x + 7.0f, c.y + 6.0f, c.x + 2.0f, c.y + 6.0f);
        p.lineTo (c.x - 1.0f, c.y + 6.0f);
        p.startNewSubPath (c.x - 2.0f, c.y - 6.5f);
        p.lineTo (c.x - 5.5f, c.y - 3.0f);
        p.lineTo (c.x - 2.0f, c.y + 0.5f);

        if (kind == Kind::redo)
            p.applyTransform (AffineTransform::scale (-1.0f, 1.0f, c.x, c.y));
    }
    else
    {
        // cog: 8 teeth around a ring
        const int teeth = 8;
        const float rOut = 8.0f, rIn = 6.0f;
        for (int i = 0; i < teeth * 2; ++i)
        {
            const float a0 = MathConstants<float>::twoPi * (float) i / (float) (teeth * 2);
            const float a1 = MathConstants<float>::twoPi * (float) (i + 1) / (float) (teeth * 2);
            const float rad = (i % 2 == 0) ? rOut : rIn;
            const Point<float> p0 (c.x + rad * std::sin (a0), c.y - rad * std::cos (a0));
            const Point<float> p1 (c.x + rad * std::sin (a1), c.y - rad * std::cos (a1));
            if (i == 0) p.startNewSubPath (p0); else p.lineTo (p0);
            p.lineTo (p1);
        }
        p.closeSubPath();
        p.addEllipse (c.x - 2.6f, c.y - 2.6f, 5.2f, 5.2f);
    }

    g.setColour (isEnabled() ? Colour (0xffd6d6d3) : Theme::mutedText);
    g.strokePath (p, PathStrokeType (1.7f, PathStrokeType::curved, PathStrokeType::rounded));
}

//==============================================================================
HeaderBar::HeaderBar()
{
    if (auto xml = parseXML (String::createStringFromData (BinaryData::miulogo_svg, BinaryData::miulogo_svgSize)))
        logo = Drawable::createFromSVG (*xml);

    if (logo != nullptr)
        logo->replaceColour (Colours::black, Theme::logo);

    for (auto* b : { (Button*) &undoButton, (Button*) &redoButton, (Button*) &settingsButton })
        addAndMakeVisible (b);

    undoButton.setTooltip ("Undo");
    redoButton.setTooltip ("Redo");
    settingsButton.setTooltip ("Settings (coming later)");
}

void HeaderBar::setClip (double lengthMs, const String& fileName)
{
    if (lengthMs != clipMs || fileName != file)
    {
        clipMs = lengthMs;
        file = fileName;
        repaint();
    }
}

void HeaderBar::paint (Graphics& g)
{
    auto r = getLocalBounds();
    g.fillAll (Theme::display);
    g.setColour (Theme::frame);
    g.fillRect (r.removeFromBottom (4));
    r = r.reduced (14, 8);

    // logo block
    auto logoBlock = r.removeFromLeft (80);
    if (logo != nullptr)
        logo->drawWithin (g, logoBlock.removeFromTop (18).toFloat(), RectanglePlacement::xLeft | RectanglePlacement::yTop, 1.0f);
    g.setColour (Theme::orange);
    Theme::drawCaps (g, "ARACK", logoBlock.withTrimmedTop (4).removeFromTop (14), 11.0f);

    auto block = [&] (int width, const String& label, std::function<void (Rectangle<int>)> drawValue)
    {
        g.setColour (Theme::displayDiv);
        g.fillRect (r.getX() + 10, r.getY(), 1, r.getHeight());
        auto b = r.removeFromLeft (width).withTrimmedLeft (22);
        g.setColour (Theme::displayLabel);
        Theme::drawCaps (g, label, b.removeFromTop (14), 10.5f);
        drawValue (b);
    };

    block (220, "Clip length", [&] (Rectangle<int> b)
    {
        const String num = clipMs >= 0.0 ? String (clipMs, 2) : String ("0.00");
        auto f = Theme::light (24.0f);
        g.setFont (f);
        g.setColour (Theme::orange);
        g.drawText (num, b, Justification::centredLeft);
        g.setColour (Theme::displayLabel);
        g.setFont (Theme::caps (11.0f));
        g.drawText ("MS", b.withTrimmedLeft (GlyphArrangement::getStringWidthInt (f, num) + 4).withTrimmedTop (8), Justification::centredLeft);
    });

    block (r.getWidth() - 150, "ARA file", [&] (Rectangle<int> b)
    {
        g.setFont (Theme::font (17.0f));
        g.setColour (file.isNotEmpty() ? Theme::displayText : Theme::mutedText);
        g.drawText (file.isNotEmpty() ? file : String ("No ARA clip"), b, Justification::centredLeft, true);
    });
}

void HeaderBar::resized()
{
    auto r = getLocalBounds().withTrimmedBottom (4).reduced (14, 12);
    settingsButton.setBounds (r.removeFromRight (36));
    r.removeFromRight (8);
    redoButton.setBounds (r.removeFromRight (36));
    r.removeFromRight (8);
    undoButton.setBounds (r.removeFromRight (36));
}

//==============================================================================
class PluginPanel::FloatWindow final : public DocumentWindow
{
public:
    FloatWindow (const String& name, std::function<void()> onCloseIn)
        : DocumentWindow (name, Theme::bg, DocumentWindow::closeButton), onClose (std::move (onCloseIn))
    {
        setUsingNativeTitleBar (true);
        setAlwaysOnTop (true);
    }

    void closeButtonPressed() override { if (onClose) onClose(); }

private:
    std::function<void()> onClose;
};

PluginPanel::PluginPanel (ARAckProcessor& p) : proc (p)
{
    viewport.setScrollBarsShown (true, true);
    addAndMakeVisible (viewport);

    floatButton.setClickingTogglesState (true);
    floatButton.onClick = [this] { setFloating (floatButton.getToggleState()); };
    addAndMakeVisible (floatButton);

    onButton.setClickingTogglesState (true);
    onButton.onClick = [this]
    {
        if (slotUid != 0)
            proc.chain.setEnabled (slotUid, onButton.getToggleState());
    };
    addAndMakeVisible (onButton);
}

PluginPanel::~PluginPanel()
{
    closeEditor();
}

void PluginPanel::closeEditor()
{
    viewport.setViewedComponent (nullptr, false);

    if (floatWindow != nullptr)
    {
        floatWindow->clearContentComponent();
        floatWindow.reset();
    }

    editor.reset();
}

void PluginPanel::showSlot (int uid)
{
    if (uid == slotUid && (editor != nullptr || proc.chain.getInstance (uid) == nullptr))
    {
        refresh();
        return;
    }

    const bool wasFloating = floatWindow != nullptr;
    closeEditor();
    slotUid = uid;

    if (auto* inst = proc.chain.getInstance (uid))
    {
        editor.reset (inst->createEditorAndMakeActive());

        if (editor == nullptr)
            editor = std::make_unique<GenericAudioProcessorEditor> (*inst);

        if (wasFloating)
            setFloating (true);
        else
            viewport.setViewedComponent (editor.get(), false);
    }

    refresh();
}

void PluginPanel::setFloating (bool shouldFloat)
{
    if (editor == nullptr)
    {
        floatButton.setToggleState (false, dontSendNotification);
        return;
    }

    if (shouldFloat && floatWindow == nullptr)
    {
        viewport.setViewedComponent (nullptr, false);
        floatWindow = std::make_unique<FloatWindow> (proc.session.slotByUid (slotUid)[IDs::name].toString(),
                                                     [this] { setFloating (false); });
        floatWindow->setContentNonOwned (editor.get(), true);
        floatWindow->centreWithSize (floatWindow->getWidth(), floatWindow->getHeight());
        floatWindow->setVisible (true);
    }
    else if (! shouldFloat && floatWindow != nullptr)
    {
        floatWindow->clearContentComponent();
        floatWindow.reset();
        viewport.setViewedComponent (editor.get(), false);
    }

    floatButton.setToggleState (floatWindow != nullptr, dontSendNotification);
    resized();
    repaint();
}

void PluginPanel::instanceGoing (int uid)
{
    if (uid == slotUid)
        closeEditor();
}

void PluginPanel::refresh()
{
    auto slot = proc.session.slotByUid (slotUid);
    const bool hasPlugin = slot.isValid() && slot[IDs::desc].toString().isNotEmpty();

    onButton.setVisible (hasPlugin);
    floatButton.setVisible (editor != nullptr);
    onButton.setToggleState (hasPlugin && (bool) slot[IDs::enabled], dontSendNotification);
    onButton.setButtonText (onButton.getToggleState() ? "On" : "Off");

    // Plug-in finished loading after it was selected: show its interface now.
    if (editor == nullptr && proc.chain.getInstance (slotUid) != nullptr)
    {
        const int uid = slotUid;
        slotUid = 0;
        showSlot (uid);
        return;
    }

    resized();
    repaint();
}

void PluginPanel::paint (Graphics& g)
{
    g.fillAll (Theme::panel);

    auto h = getLocalBounds().removeFromTop (ChainPanel::headerHeight);
    g.setColour (Theme::bg);
    g.fillRect (h);

    auto slot = proc.session.slotByUid (slotUid);
    const bool hasPlugin = slot.isValid() && slot[IDs::desc].toString().isNotEmpty();
    const int index = proc.session.slots().indexOf (slot);

    g.setColour (hasPlugin ? Theme::orange : Theme::frame);
    g.fillRect (h.removeFromBottom (3));

    g.setColour (Theme::headerText);
    if (hasPlugin)
        Theme::drawCaps (g, String (index + 1).paddedLeft ('0', 2) + "  " + slot[IDs::name].toString(), h.withTrimmedLeft (14), 11.5f);
    else
        Theme::drawCaps (g, "Plug-in", h.withTrimmedLeft (14), 11.5f);

    auto body = getLocalBounds().withTrimmedTop (ChainPanel::headerHeight).reduced (10);
    g.setColour (Theme::displayDiv);
    g.drawRect (body);

    String msg;
    if (! hasPlugin)                                   msg = "Select a slot, or click an empty slot to load a plug-in.";
    else if (proc.chain.isLoading (slotUid))            msg = "Loading...";
    else if (proc.chain.getInstance (slotUid) == nullptr) msg = "Could not load this plug-in. " + proc.chain.getLoadError (slotUid);
    else if (floatWindow != nullptr)                   msg = "Plug-in window is floating.";

    if (msg.isNotEmpty())
    {
        g.setColour (Theme::headerSub);
        g.setFont (Theme::font (14.0f));
        g.drawFittedText (msg, body.reduced (20), Justification::centred, 3);
    }
}

void PluginPanel::resized()
{
    auto r = getLocalBounds();
    auto h = r.removeFromTop (ChainPanel::headerHeight).withTrimmedBottom (3).reduced (8, 6);
    onButton.setBounds (h.removeFromRight (56));
    h.removeFromRight (6);
    floatButton.setBounds (h.removeFromRight (66));

    auto body = r.reduced (11);
    if (editor != nullptr && floatWindow == nullptr)
    {
        // centre the plug-in's own window inside the panel
        const int w = jmin (body.getWidth(), editor->getWidth() + 2);
        const int hh = jmin (body.getHeight(), editor->getHeight() + 2);
        viewport.setBounds (body.withSizeKeepingCentre (w, hh));
    }
    else
    {
        viewport.setBounds ({});
    }
}

//==============================================================================
ARAckEditor::ARAckEditor (ARAckProcessor& p)
    : AudioProcessorEditor (&p), AudioProcessorEditorARAExtension (&p),
      proc (p), chainPanel (p), pluginPanel (p), automation (p)
{
    setLookAndFeel (&lnf);

    addAndMakeVisible (header);
    addAndMakeVisible (chainPanel);
    addAndMakeVisible (pluginPanel);
    addAndMakeVisible (automation);

    header.undoButton.onClick = [this] { proc.session.undo.undo(); };
    header.redoButton.onClick = [this] { proc.session.undo.redo(); };
    header.settingsButton.setEnabled (false);

    chainPanel.onSelect = [this] (int uid) { select (uid); };

    proc.chain.onBeforeInstanceRemoved = [this] (int uid) { pluginPanel.instanceGoing (uid); };
    proc.chain.onChanged = [this] { anyChange(); };

    proc.session.root.addListener (this);

    if (proc.selectedSlotUid != 0)
        pluginPanel.showSlot (proc.selectedSlotUid);

    // Host-driven resizing only (no extra corner grip): ARA hosts resize the editor themselves.
    setResizable (true, false);
    setResizeLimits (900, 600, 2400, 1600);
    setSize (1100, 720);
    setWantsKeyboardFocus (true);

    pluginPanel.refresh();
    updateClipInfo();
    startTimerHz (30);
}

ARAckEditor::~ARAckEditor()
{
    proc.session.root.removeListener (this);
    proc.chain.onBeforeInstanceRemoved = nullptr;
    proc.chain.onChanged = nullptr;
    setLookAndFeel (nullptr);
}

void ARAckEditor::paint (Graphics& g)
{
    g.fillAll (Theme::bg);

    // 4 px frames between the panels
    g.setColour (Theme::frame);
    const int top = header.getBottom();
    g.fillRect (chainPanel.getRight(), top, 4, chainPanel.getHeight());
    g.fillRect (0, automation.getY() - 4, getWidth(), 4);
}

void ARAckEditor::resized()
{
    auto r = getLocalBounds();
    header.setBounds (r.removeFromTop (60));

    const int autoH = jmax (200, (int) (r.getHeight() * 0.32f));
    // Gutter around the automation area so nothing overlaps its edges.
    automation.setBounds (r.removeFromBottom (autoH).reduced (10, 0).withTrimmedBottom (10));
    r.removeFromBottom (4);

    chainPanel.setBounds (r.removeFromLeft (270));
    r.removeFromLeft (4);
    pluginPanel.setBounds (r);
}

bool ARAckEditor::keyPressed (const KeyPress& k)
{
    const auto mods = k.getModifiers();

    if (mods.isCommandDown() && k.getKeyCode() == 'Z')
    {
        mods.isShiftDown() ? proc.session.undo.redo() : proc.session.undo.undo();
        return true;
    }

    if (mods.isCommandDown() && k.getKeyCode() == 'Y')
    {
        proc.session.undo.redo();
        return true;
    }

    return false;
}

void ARAckEditor::select (int uid)
{
    proc.selectedSlotUid = uid;
    pluginPanel.showSlot (uid);
    chainPanel.refresh();
}

void ARAckEditor::anyChange()
{
    // If the selected slot was emptied (or undo removed its plug-in), keep the panel in step.
    pluginPanel.refresh();
    chainPanel.refresh();
    automation.repaint();

    header.undoButton.setEnabled (proc.session.undo.canUndo());
    header.redoButton.setEnabled (proc.session.undo.canRedo());
}

void ARAckEditor::updateClipInfo()
{
    ARAPlaybackRegion* region = nullptr;
    String foundVia;

    // 1. The clip selected in the host.
    if (auto* view = getARAEditorView())
    {
        const auto selected = view->getViewSelection().getEffectivePlaybackRegions<ARAPlaybackRegion>();
        if (! selected.empty())
        {
            region = selected.front();
            foundVia = "selection";
        }
    }

    // 2. The clips this instance is rendering.
    if (auto* r = (region == nullptr ? proc.getPlaybackRenderer<ARAckPlaybackRenderer>() : nullptr))
    {
        const auto& regions = r->getPlaybackRegions();
        const double t = proc.hostTimeSeconds.load();

        for (auto* reg : regions)
            if (t >= reg->getStartInPlaybackTime() && t < reg->getEndInPlaybackTime())
                region = reg;

        if (region == nullptr && ! regions.empty())
            region = regions.front();

        if (region != nullptr)
            foundVia = "renderer";
    }

    // 3. Any clip in the ARA document.
    if (region == nullptr)
        if (auto* view = getARAEditorView())
            if (auto* doc = view->getDocumentController()->getDocument<ARADocument>())
                for (auto* seq : doc->getRegionSequences<ARARegionSequence>())
                    if (region == nullptr && ! seq->getPlaybackRegions<ARAPlaybackRegion>().empty())
                    {
                        region = seq->getPlaybackRegions<ARAPlaybackRegion>().front();
                        foundVia = "document";
                    }

    if (region != lastRegion)
    {
        lastRegion = region;
        arackLog ("Clip: ARA editor view " + String (isARAEditorView() ? "yes" : "no")
                  + ", playback renderer " + String (proc.isPlaybackRenderer() ? "yes" : "no")
                  + ", region " + (region != nullptr ? "from " + foundVia : String ("none")));
    }

    if (region != nullptr)
    {
        auto* source = region->getAudioModification()->getAudioSource();
        String name;
        if (const char* n = source->getName())
            name = String::fromUTF8 (n);
        if (name.isEmpty())
            if (const char* n = region->getName())
                name = String::fromUTF8 (n);

        const double lengthMs = region->getDurationInPlaybackTime() * 1000.0;
        header.setClip (lengthMs, name);
        automation.setClip (source, region->getStartInAudioModificationTime(), lengthMs);
    }
    else
    {
        header.setClip (-1.0, {});
        automation.setClip (nullptr, 0.0, 30000.0);
    }
}

void ARAckEditor::timerCallback()
{
    static int tick = 0;
    if (++tick % 6 == 0)
    {
        updateClipInfo();
        header.undoButton.setEnabled (proc.session.undo.canUndo());
        header.redoButton.setEnabled (proc.session.undo.canRedo());
        chainPanel.refresh();
    }

    if (proc.hostPlaying.load())
        automation.repaint();
}
