#include "ChainPanel.h"

using namespace juce;

namespace
{
    Path powerIcon (Rectangle<float> r)
    {
        Path p;
        const auto c = r.getCentre();
        const float rad = r.getWidth() * 0.34f;
        p.addCentredArc (c.x, c.y, rad, rad, 0.0f, 0.75f, MathConstants<float>::twoPi - 0.75f, true);
        p.startNewSubPath (c.x, c.y - rad - 1.5f);
        p.lineTo (c.x, c.y - 0.5f);
        return p;
    }
}

//==============================================================================
class ChainPanel::SlotRow final : public Component,
                                  public DragAndDropTarget
{
public:
    SlotRow (ChainPanel& o, int i) : owner (o), index (i) {}

    ValueTree slot() const { return owner.proc.session.slots().getChild (index); }
    int uid() const        { return slot()[IDs::uid]; }

    void paint (Graphics& g) override
    {
        auto s = slot();
        const bool hasPlugin = s[IDs::desc].toString().isNotEmpty();
        const bool enabled = s[IDs::enabled];
        const bool selected = hasPlugin && owner.proc.selectedSlotUid == uid();
        auto* inst = owner.proc.chain.getInstance (uid());
        const bool loading = owner.proc.chain.isLoading (uid());

        auto r = getLocalBounds();
        g.setColour (selected ? Theme::warm : (dragOver ? Theme::hover : Theme::bg));
        g.fillRect (r);

        if (dragOver)
        {
            g.setColour (Theme::orange);
            g.fillRect (r.removeFromTop (2));
            r = getLocalBounds();
        }

        g.setColour (Theme::rowLine);
        g.fillRect (r.removeFromBottom (1));

        // drag handle
        auto grip = gripArea().toFloat();
        if (hasPlugin)
        {
            g.setColour (Theme::btnText.withAlpha (0.6f));
            for (int k = -1; k <= 1; ++k)
                g.fillRect (grip.getCentreX() - 6.0f, grip.getCentreY() + (float) k * 4.0f - 0.75f, 12.0f, 1.5f);
        }

        // power button
        auto pw = powerArea().toFloat().reduced (0.5f);
        const bool lit = hasPlugin && enabled;
        g.setColour (lit ? Theme::orangeText : Theme::btnBorder);
        g.drawRoundedRectangle (pw, 5.0f, 1.0f);
        g.strokePath (powerIcon (pw), PathStrokeType (1.7f, PathStrokeType::curved, PathStrokeType::rounded));

        // name
        auto nameR = nameArea();
        String text;
        Colour col = Theme::fg;
        bool bold = true;

        if (! hasPlugin)                  { text = "Empty"; col = Theme::muted; bold = false; }
        else if (loading && inst == nullptr) { text = s[IDs::name].toString() + "  (loading)"; col = Theme::btnText; }
        else if (inst == nullptr)         { text = s[IDs::name].toString() + "  (missing)"; col = Theme::red; }
        else                              { text = s[IDs::name].toString(); col = enabled ? Theme::fg : Theme::mutedText; }

        g.setColour (col);
        g.setFont (Theme::font (13.5f, bold));
        g.drawText (text, nameR, Justification::centredLeft, true);

        // divider + latency
        if (owner.showLatency)
        {
            auto lr = latencyArea();
            g.setColour (Theme::frame);
            g.fillRect (lr.getX(), lr.getY(), 1, lr.getHeight());

            if (hasPlugin)
            {
                String lat = "off";
                if (enabled && inst != nullptr)
                    lat = String ((double) inst->getLatencySamples() / jmax (1.0, owner.proc.getSampleRate()) * 1000.0, 2);
                else if (enabled)
                    lat = "-";

                g.setColour (enabled ? Theme::headerSub.brighter (0.3f) : Theme::mutedText);
                g.setFont (Theme::font (12.0f));
                g.drawText (lat, lr.withTrimmedRight (10), Justification::centredRight);
            }
        }
    }

    Rectangle<int> gripArea() const    { return { 4, 0, 18, getHeight() }; }
    Rectangle<int> powerArea() const   { return { 26, (getHeight() - 22) / 2, 22, 22 }; }
    Rectangle<int> latencyArea() const { return { getWidth() - 62, 0, 62, getHeight() }; }
    Rectangle<int> nameArea() const
    {
        return { 56, 0, (owner.showLatency ? latencyArea().getX() : getWidth()) - 62, getHeight() };
    }

    void mouseDown (const MouseEvent& e) override
    {
        auto s = slot();
        const bool hasPlugin = s[IDs::desc].toString().isNotEmpty();
        dragStarted = false;

        if (e.mods.isPopupMenu())
        {
            hasPlugin ? owner.showSlotMenu (uid()) : owner.showLoadMenu (uid());
            return;
        }

        if (gripArea().contains (e.getPosition()) && hasPlugin)
            return; // drag handled in mouseDrag

        if (powerArea().contains (e.getPosition()))
        {
            if (hasPlugin)
                owner.proc.chain.setEnabled (uid(), ! (bool) s[IDs::enabled]);
            return;
        }

        if (! hasPlugin)
            owner.showLoadMenu (uid());
        else if (owner.onSelect)
            owner.onSelect (uid());
    }

    void mouseDrag (const MouseEvent& e) override
    {
        if (dragStarted || ! gripArea().contains (e.getMouseDownPosition()))
            return;

        if (slot()[IDs::desc].toString().isEmpty() || e.getDistanceFromDragStart() < 4)
            return;

        if (auto* c = DragAndDropContainer::findParentDragContainerFor (this))
        {
            dragStarted = true;
            c->startDragging ("slot:" + String (uid()), this);
        }
    }

    bool isInterestedInDragSource (const SourceDetails& d) override
    {
        return d.description.toString().startsWith ("slot:");
    }

    void itemDragEnter (const SourceDetails&) override { dragOver = true;  repaint(); }
    void itemDragExit (const SourceDetails&) override  { dragOver = false; repaint(); }

    void itemDropped (const SourceDetails& d) override
    {
        dragOver = false;
        const int from = d.description.toString().fromFirstOccurrenceOf (":", false, false).getIntValue();
        owner.proc.chain.moveSlot (from, index);
        owner.refresh();
    }

private:
    ChainPanel& owner;
    const int index;
    bool dragOver = false, dragStarted = false;
};

//==============================================================================
ChainPanel::ChainPanel (ARAckProcessor& p) : proc (p)
{
    for (int i = 0; i < Session::numSlots; ++i)
        addAndMakeVisible (rows.add (new SlotRow (*this, i)));

    latencyButton.setClickingTogglesState (true);
    latencyButton.setToggleState (true, dontSendNotification);
    latencyButton.onClick = [this]
    {
        showLatency = latencyButton.getToggleState();
        refresh();
    };
    addAndMakeVisible (latencyButton);
}

void ChainPanel::paint (Graphics& g)
{
    g.fillAll (Theme::bg);
    auto h = getLocalBounds().removeFromTop (headerHeight);
    g.setColour (Theme::bg);
    g.fillRect (h);
    g.setColour (Theme::headerText);
    Theme::drawCaps (g, "CHAIN", h.withTrimmedLeft (14), 11.5f);
    g.setColour (Theme::frame);
    g.fillRect (h.removeFromBottom (3));
}

void ChainPanel::resized()
{
    auto r = getLocalBounds();
    auto h = r.removeFromTop (headerHeight);
    latencyButton.setBounds (h.removeFromRight (82).reduced (8, 7).withTrimmedBottom (2));

    for (auto* row : rows)
        row->setBounds (r.removeFromTop (rowHeight));
}

void ChainPanel::refresh()
{
    for (auto* row : rows)
        row->repaint();
}

void ChainPanel::showLoadMenu (int slotUid)
{
    SharedResourcePointer<PluginCatalog> catalog;
    auto types = catalog->usableTypes();

    PopupMenu m;
    if (types.isEmpty())
        m.addItem (-1, "No plug-ins scanned yet", false);
    else
        KnownPluginList::addToMenu (m, types, KnownPluginList::sortByManufacturer);

    m.addSeparator();
    m.addItem (-2, "Scan for plug-ins...");

    m.showMenuAsync (PopupMenu::Options().withMousePosition(),
        [this, slotUid, types] (int result)
        {
            if (result == -2)
                openScanner();
            else if (result > 0)
            {
                const int idx = KnownPluginList::getIndexChosenByMenu (types, result);
                if (isPositiveAndBelow (idx, types.size()))
                {
                    proc.chain.loadPlugin (slotUid, types.getReference (idx));
                    if (onSelect) onSelect (slotUid);
                }
            }
        });
}

void ChainPanel::showSlotMenu (int slotUid)
{
    auto s = proc.session.slotByUid (slotUid);
    PopupMenu m;
    m.addItem (1, (bool) s[IDs::enabled] ? "Disable" : "Enable");
    m.addItem (2, "Replace...");
    m.addItem (3, "Remove");

    m.showMenuAsync (PopupMenu::Options().withMousePosition(), [this, slotUid] (int r)
    {
        auto slot = proc.session.slotByUid (slotUid);
        if (r == 1) proc.chain.setEnabled (slotUid, ! (bool) slot[IDs::enabled]);
        if (r == 2) showLoadMenu (slotUid);
        if (r == 3) proc.chain.removePlugin (slotUid);
    });
}

void ChainPanel::openScanner()
{
    SharedResourcePointer<PluginCatalog> catalog;

    auto* list = new PluginListComponent (catalog->formats, catalog->known, catalog->deadMansPedal(),
                                          catalog->props.getUserSettings(), true);
    list->setSize (700, 500);

    DialogWindow::LaunchOptions o;
    o.content.setOwned (list);
    o.dialogTitle = "ARAck - Plug-ins";
    o.dialogBackgroundColour = Theme::bg;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.launchAsync();
}

ChainPanel::~ChainPanel() = default;
