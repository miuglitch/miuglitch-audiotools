#include "PluginEditor.h"
#include "BinaryData.h"
#include "TimeFormat.h"

using namespace juce;

//==============================================================================
void IconButton::paintButton (Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat();
    if (! flat || over || down)
    {
        g.setColour (down ? Theme::frame : (over ? Colour (0xff2a2a2a) : Theme::displayCtrl));
        g.fillRoundedRectangle (r, 6.0f);
    }

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
    else if (kind == Kind::popout)
    {
        // classic pop-out: a box with an arrow leaving its top-right corner
        p.startNewSubPath (c.x + 1.0f, c.y - 6.0f);
        p.lineTo (c.x - 6.0f, c.y - 6.0f);
        p.lineTo (c.x - 6.0f, c.y + 6.0f);
        p.lineTo (c.x + 6.0f, c.y + 6.0f);
        p.lineTo (c.x + 6.0f, c.y - 1.0f);
        p.startNewSubPath (c.x - 1.0f, c.y + 1.0f);
        p.lineTo (c.x + 7.0f, c.y - 7.0f);
        p.startNewSubPath (c.x + 2.0f, c.y - 7.0f);
        p.lineTo (c.x + 7.0f, c.y - 7.0f);
        p.lineTo (c.x + 7.0f, c.y - 2.0f);
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

    g.setColour (getToggleState() ? Theme::orangeText : (isEnabled() ? Colour (0xffd6d6d3) : Theme::mutedText));
    g.strokePath (p, PathStrokeType (1.7f, PathStrokeType::curved, PathStrokeType::rounded));
}

//==============================================================================
HeaderBar::HeaderBar()
{
    setOpaque (true);
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
        const auto ct = ClipTime::format (clipMs >= 0.0 ? clipMs : 0.0);
        const String num = ct.value;
        auto f = Theme::light (24.0f);
        g.setFont (f);
        g.setColour (Theme::orange);
        g.drawText (num, b, Justification::centredLeft);
        g.setColour (Theme::displayLabel);
        g.setFont (Theme::caps (11.0f));
        g.drawText (ct.unit, b.withTrimmedLeft (GlyphArrangement::getStringWidthInt (f, num) + 4).withTrimmedTop (8), Justification::centredLeft);
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

PluginPanel::PluginPanel (ClipModel& m) : model (m)
{
    setOpaque (true);

    popoutButton.flat = true;
    popoutButton.setClickingTogglesState (true);
    popoutButton.setTooltip ("Open the plug-in in its own window");
    popoutButton.onClick = [this] { setFloating (popoutButton.getToggleState()); };
    addChildComponent (popoutButton);
}

PluginPanel::~PluginPanel()
{
    closeAll();
}

AudioProcessorEditor* PluginPanel::currentEditor() const
{
    auto it = editors.find (slotUid);
    return it != editors.end() ? it->second.get() : nullptr;
}

void PluginPanel::closeEditor (int uid)
{
    auto it = editors.find (uid);
    if (it == editors.end())
        return;

    if (floatWindow != nullptr && floatWindow->getContentComponent() == it->second.get())
    {
        floatWindow->clearContentComponent();
        floatWindow.reset();
        popoutButton.setToggleState (false, dontSendNotification);
    }

    it->second->removeComponentListener (this);
    removeChildComponent (it->second.get());
    editors.erase (it);
}

void PluginPanel::closeAll()
{
    while (! editors.empty())
        closeEditor (editors.begin()->first);

    floatWindow.reset();
}

void PluginPanel::showSlot (int uid)
{
    if (floatWindow != nullptr)
    {
        // switching slots docks the floating window back
        if (auto* cur = currentEditor())
        {
            floatWindow->clearContentComponent();
            floatWindow.reset();
            addChildComponent (cur);
        }
        popoutButton.setToggleState (false, dontSendNotification);
    }

    if (uid != slotUid)
        if (auto* cur = currentEditor())
            cur->setVisible (false);

    slotUid = uid;

    if (auto* inst = model.chain.getInstance (uid))
    {
        auto& ed = editors[uid];

        if (ed == nullptr)
        {
            ed.reset (inst->createEditorAndMakeActive());

            if (ed == nullptr)
                ed = std::make_unique<GenericAudioProcessorEditor> (*inst);

            ed->addComponentListener (this);
            addChildComponent (*ed);
        }

        resized();
        ed->setVisible (true);
    }

    refresh();
}

void PluginPanel::setFloating (bool shouldFloat)
{
    auto* ed = currentEditor();

    if (ed == nullptr)
    {
        popoutButton.setToggleState (false, dontSendNotification);
        return;
    }

    if (shouldFloat && floatWindow == nullptr)
    {
        removeChildComponent (ed);
        floatWindow = std::make_unique<FloatWindow> (model.session.slotByUid (slotUid)[IDs::name].toString(),
                                                     [this] { setFloating (false); });
        floatWindow->setContentNonOwned (ed, true);
        ed->setVisible (true);
        floatWindow->centreWithSize (floatWindow->getWidth(), floatWindow->getHeight());
        floatWindow->setVisible (true);
    }
    else if (! shouldFloat && floatWindow != nullptr)
    {
        floatWindow->clearContentComponent();
        floatWindow.reset();
        addChildComponent (ed);
        ed->setVisible (true);
    }

    popoutButton.setToggleState (floatWindow != nullptr, dontSendNotification);
    refresh();
}

Point<int> PluginPanel::dockedEditorSize() const
{
    if (auto* ed = currentEditor())
        if (floatWindow == nullptr)
            return { ed->getWidth(), ed->getHeight() };

    return {};
}

void PluginPanel::componentMovedOrResized (Component& c, bool, bool wasResized)
{
    if (wasResized && &c == currentEditor())
        refresh();
}

void PluginPanel::instanceGoing (int uid)
{
    closeEditor (uid);
}

void PluginPanel::refresh()
{
    // Plug-in finished loading after it was selected: show its window now.
    if (currentEditor() == nullptr && model.chain.getInstance (slotUid) != nullptr)
    {
        showSlot (slotUid);
        return;
    }

    popoutButton.setVisible (currentEditor() != nullptr);

    if (dockedEditorSize() != lastReportedSize)
    {
        lastReportedSize = dockedEditorSize();
        MessageManager::callAsync ([safe = Component::SafePointer<PluginPanel> (this)]
        {
            if (safe != nullptr && safe->onContentSizeChanged)
                safe->onContentSizeChanged();
        });
    }

    resized();
    repaint (getLocalBounds().removeFromTop (ChainPanel::headerHeight));

    if (currentEditor() == nullptr || floatWindow != nullptr)
        repaint();
}

void PluginPanel::paint (Graphics& g)
{
    g.fillAll (Theme::panel);

    auto h = getLocalBounds().removeFromTop (ChainPanel::headerHeight);
    g.setColour (Theme::bg);
    g.fillRect (h);

    auto slot = model.session.slotByUid (slotUid);
    const bool hasPlugin = slot.isValid() && slot[IDs::desc].toString().isNotEmpty();
    const int index = model.session.slots().indexOf (slot);

    g.setColour (hasPlugin ? Theme::orange : Theme::frame);
    g.fillRect (h.removeFromBottom (3));

    g.setColour (Theme::headerText);
    if (hasPlugin)
        Theme::drawCaps (g, String (index + 1).paddedLeft ('0', 2) + "  " + slot[IDs::name].toString()
                              + ((bool) slot[IDs::enabled] ? String() : String ("  (off)")),
                         h.withTrimmedLeft (14), 11.5f);
    else
        Theme::drawCaps (g, "Plug-in", h.withTrimmedLeft (14), 11.5f);

    String msg;
    if (! hasPlugin)                                         msg = "Select a slot, or search above to add a plug-in.";
    else if (model.chain.isLoading (slotUid))                msg = "Loading...";
    else if (model.chain.getInstance (slotUid) == nullptr)   msg = "Could not load this plug-in. " + model.chain.getLoadError (slotUid);
    else if (floatWindow != nullptr)                         msg = "The plug-in is open in its own window.";

    if (msg.isNotEmpty())
    {
        g.setColour (Theme::headerSub);
        g.setFont (Theme::font (14.0f));
        g.drawFittedText (msg, getLocalBounds().withTrimmedTop (ChainPanel::headerHeight).reduced (30), Justification::centred, 3);
    }
}

void PluginPanel::resized()
{
    auto r = getLocalBounds();
    auto h = r.removeFromTop (ChainPanel::headerHeight).withTrimmedBottom (3).reduced (8, 5);
    popoutButton.setBounds (h.removeFromRight (32));

    auto body = r.reduced (padding);

    if (auto* ed = currentEditor())
        if (floatWindow == nullptr)
            ed->setTopLeftPosition (body.getX() + jmax (0, (body.getWidth() - ed->getWidth()) / 2),
                                    body.getY() + jmax (0, (body.getHeight() - ed->getHeight()) / 2));
}

//==============================================================================
ARAckEditor::ARAckEditor (ARAckProcessor& p)
    : AudioProcessorEditor (&p), AudioProcessorEditorARAExtension (&p), proc (p)
{
    setOpaque (true);
    setLookAndFeel (&lnf);
    addAndMakeVisible (header);

    header.undoButton.onClick = [this] { if (model != nullptr) model->session.undo.undo(); };
    header.redoButton.onClick = [this] { if (model != nullptr) model->session.undo.redo(); };
    header.settingsButton.setEnabled (false);

    // Size is locked to fit the selected plug-in (with a minimum); the host can't stretch it.
    setResizable (true, false);
    setResizeLimits (defaultWidth, defaultHeight, defaultWidth, defaultHeight);
    setSize (defaultWidth, defaultHeight);
    setWantsKeyboardFocus (true);
    addMouseListener (this, true); // any click inside ARAck gives it keyboard focus (for Ctrl+F, Ctrl+Z)

    updateClip();
    startTimerHz (30);

    // Developer self-test (standalone only): ARACK_SELFTEST=<search text> types it into
    // the search box and adds the first result.
    const auto selfTest = SystemStats::getEnvironmentVariable ("ARACK_SELFTEST", {});
    if (selfTest.isNotEmpty() && proc.wrapperType == AudioProcessor::wrapperType_Standalone)
        Timer::callAfterDelay (1500, [safe = Component::SafePointer<ARAckEditor> (this), selfTest]
        {
            if (safe != nullptr && safe->view != nullptr)
                safe->view->chainPanel.runSelfTest (selfTest);
        });
}

ARAckEditor::~ARAckEditor()
{
    bindTo (nullptr, nullptr);
    setLookAndFeel (nullptr);
}

void ARAckEditor::bindTo (ClipModel* m, ARAAudioModification* mod)
{
    if (m == model && mod == boundModification)
        return;

    if (model != nullptr)
    {
        model->session.root.removeListener (this);
        model->chain.onBeforeInstanceRemoved = nullptr;
        model->chain.onChanged = nullptr;
    }

    view.reset(); // closes plug-in windows before anything they use goes away

    if (boundModification != nullptr)
        boundModification->removeListener (this);

    model = m;
    boundModification = mod;

    if (boundModification != nullptr)
        boundModification->addListener (this);

    if (model != nullptr)
    {
        view = std::make_unique<ClipView> (*model);
        addAndMakeVisible (view->chainPanel);
        addAndMakeVisible (view->pluginPanel);
        addAndMakeVisible (view->automation);

        view->chainPanel.onSelect = [this] (int uid) { select (uid); };
        view->pluginPanel.onContentSizeChanged = [this] { fitToPlugin(); };

        model->chain.onBeforeInstanceRemoved = [this] (int uid) { if (view != nullptr) view->pluginPanel.instanceGoing (uid); };
        model->chain.onChanged = [this] { anyChange(); };
        model->session.root.addListener (this);

        if (model->selectedSlotUid != 0)
            view->pluginPanel.showSlot (model->selectedSlotUid);
        else
            view->pluginPanel.refresh();
    }

    resized();
    fitToPlugin();
    repaint();
}

void ARAckEditor::willDestroyAudioModification (ARAAudioModification* mod)
{
    if (mod == boundModification)
    {
        lastRegion = nullptr;
        bindTo (nullptr, nullptr);
        header.setClip (-1.0, {});
    }
}

void ARAckEditor::fitToPlugin()
{
    // Fit the docked plug-in window exactly, never smaller than the default size.
    // Native plug-in windows can't be clipped, so they must never overlap the automation.
    Point<int> need;
    if (view != nullptr)
        need = view->pluginPanel.dockedEditorSize();

    const int pad = 2 * PluginPanel::padding + 2;
    const int w = jmax (defaultWidth, chainWidth + 4 + need.x + pad);
    const int h = jmax (defaultHeight, headerHeight + ChainPanel::headerHeight + need.y + pad + 4 + automationHeight + 10);

    setResizeLimits (w, h, w, h);

    if (w != getWidth() || h != getHeight())
    {
        arackLog ("Fit to plug-in " + String (need.x) + "x" + String (need.y) + " -> " + String (w) + "x" + String (h));
        setSize (w, h);
    }
}

void ARAckEditor::paint (Graphics& g)
{
    g.fillAll (Theme::bg);

    if (view != nullptr)
    {
        g.setColour (Theme::frame);
        g.fillRect (view->chainPanel.getRight(), header.getBottom(), 4, view->chainPanel.getHeight());
        g.fillRect (0, view->pluginPanel.getBottom(), getWidth(), 4);
    }
}

void ARAckEditor::resized()
{
    auto r = getLocalBounds();
    header.setBounds (r.removeFromTop (headerHeight));

    if (view == nullptr)
        return;

    // Gutter around the automation area so nothing overlaps its edges.
    auto autoArea = r.removeFromBottom (automationHeight + 10);
    view->automation.setBounds (autoArea.reduced (10, 0).withTrimmedBottom (10));
    r.removeFromBottom (4);

    view->chainPanel.setBounds (r.removeFromLeft (chainWidth));
    r.removeFromLeft (4);
    view->pluginPanel.setBounds (r);
}

void ARAckEditor::mouseDown (const MouseEvent&)
{
    if (! hasKeyboardFocus (true))
        grabKeyboardFocus();
}

bool ARAckEditor::keyPressed (const KeyPress& k)
{
    const auto mods = k.getModifiers();

    if (model != nullptr && mods.isCommandDown() && k.getKeyCode() == 'Z')
    {
        mods.isShiftDown() ? model->session.undo.redo() : model->session.undo.undo();
        return true;
    }

    if (model != nullptr && mods.isCommandDown() && k.getKeyCode() == 'Y')
    {
        model->session.undo.redo();
        return true;
    }

    if (view != nullptr && mods.isCommandDown() && k.getKeyCode() == 'F')
    {
        view->chainPanel.focusSearch();
        return true;
    }

    return false;
}

void ARAckEditor::select (int uid)
{
    if (model == nullptr || view == nullptr)
        return;

    model->selectedSlotUid = uid;
    view->pluginPanel.showSlot (uid);
    view->chainPanel.refresh();
}

void ARAckEditor::anyChange()
{
    if (view != nullptr)
    {
        view->pluginPanel.refresh();
        view->chainPanel.refresh();
        view->automation.repaint();
    }

    header.undoButton.setEnabled (model != nullptr && model->session.undo.canUndo());
    header.redoButton.setEnabled (model != nullptr && model->session.undo.canRedo());
}

void ARAckEditor::updateClip()
{
    auto* editorView = getARAEditorView();

    if (editorView == nullptr)
    {
        // Plain VST3 / standalone: one local clip model, no ARA clip.
        bindTo (&proc.localModel, nullptr);
        header.setClip (-1.0, {});
        if (view != nullptr)
            view->automation.setClip (nullptr, 0.0, 30000.0);
        return;
    }

    ARAPlaybackRegion* region = nullptr;
    String foundVia;

    // 1. The clip selected in the host.
    const auto selected = editorView->getViewSelection().getEffectivePlaybackRegions<ARAPlaybackRegion>();
    if (! selected.empty())
    {
        region = selected.front();
        foundVia = "selection";
    }

    // 2. The ARAck clip under the host playhead, otherwise the first ARAck clip.
    if (region == nullptr)
        if (auto* doc = editorView->getDocumentController()->getDocument<ARADocument>())
        {
            const double t = proc.hostTimeSeconds.load();
            ARAPlaybackRegion* first = nullptr;

            for (auto* seq : doc->getRegionSequences<ARARegionSequence>())
                for (auto* reg : seq->getPlaybackRegions<ARAPlaybackRegion>())
                {
                    if (first == nullptr) first = reg;
                    if (region == nullptr && t >= reg->getStartInPlaybackTime() && t < reg->getEndInPlaybackTime())
                        region = reg;
                }

            foundVia = region != nullptr ? "playhead" : "first in document";
            if (region == nullptr)
                region = first;
        }

    if (region != lastRegion)
    {
        lastRegion = region;

        if (region != nullptr)
        {
            auto* src = region->getAudioModification()->getAudioSource();
            arackLog ("Clip from " + foundVia + ": start " + String (region->getStartInPlaybackTime(), 3)
                      + " s, length " + String (region->getDurationInPlaybackTime(), 3)
                      + " s, offset in file " + String (region->getStartInAudioModificationTime(), 3)
                      + " s; file " + String (src->getDuration(), 3) + " s at " + String (src->getSampleRate())
                      + " Hz, " + String (src->getChannelCount()) + " ch; project " + String (proc.getSampleRate()) + " Hz");
        }
        else
        {
            arackLog ("Clip: none");
        }
    }

    if (region == nullptr)
    {
        bindTo (nullptr, nullptr);
        header.setClip (-1.0, {});
        return;
    }

    auto* mod = region->getAudioModification<ARAckAudioModification>();
    bindTo (&mod->model, mod);

    auto* source = mod->getAudioSource();
    String name;
    if (const char* n = source->getName())
        name = String::fromUTF8 (n);
    if (name.isEmpty())
        if (const char* n = region->getName())
            name = String::fromUTF8 (n);

    const double lengthMs = region->getDurationInPlaybackTime() * 1000.0;
    header.setClip (lengthMs, name);

    if (view != nullptr)
        view->automation.setClip (source, region->getStartInAudioModificationTime(), lengthMs);
}

void ARAckEditor::timerCallback()
{
    if (++tick % 6 == 0)
    {
        updateClip();
        header.undoButton.setEnabled (model != nullptr && model->session.undo.canUndo());
        header.redoButton.setEnabled (model != nullptr && model->session.undo.canRedo());
        if (view != nullptr)
            view->chainPanel.refresh();
    }

    if (view != nullptr && model != nullptr && model->playPositionMs.load() >= 0.0)
        view->automation.repaint();
}
