#include "AutomationPanel.h"

using namespace juce;

AutomationPanel::AutomationPanel (ARAckProcessor& p) : proc (p)
{
    thumbnail.addChangeListener (this);
}

AutomationPanel::~AutomationPanel()
{
    thumbnail.removeChangeListener (this);
    if (audioSource != nullptr)
        audioSource->removeListener (this);
}

void AutomationPanel::setClip (ARAAudioSource* source, double startSeconds, double lengthMs)
{
    clipStartSeconds = startSeconds;
    clipLengthMs = lengthMs > 1.0 ? lengthMs : 30000.0;

    if (source != audioSource)
    {
        if (audioSource != nullptr)
            audioSource->removeListener (this);

        audioSource = source;
        thumbnail.clear();

        if (audioSource != nullptr)
        {
            audioSource->addListener (this);
            thumbnail.setReader (new ARAAudioSourceReader (audioSource), (int64) (pointer_sized_int) audioSource);
        }
    }

    repaint();
}

void AutomationPanel::willDestroyAudioSource (ARAAudioSource* s)
{
    if (s == audioSource)
    {
        audioSource->removeListener (this);
        audioSource = nullptr;
        thumbnail.clear();
        repaint();
    }
}

//==============================================================================
Rectangle<int> AutomationPanel::areaBounds() const
{
    return getLocalBounds().withTrimmedTop (tabsHeight).withTrimmedBottom (rulerHeight);
}

Rectangle<int> AutomationPanel::rulerBounds() const
{
    return getLocalBounds().removeFromBottom (rulerHeight);
}

float AutomationPanel::msToX (double ms) const
{
    auto a = areaBounds();
    return (float) a.getX() + (float) (ms / clipLengthMs) * (float) a.getWidth();
}

double AutomationPanel::xToMs (float x) const
{
    auto a = areaBounds();
    return jlimit (0.0, clipLengthMs, (double) (x - (float) a.getX()) / jmax (1, a.getWidth()) * clipLengthMs);
}

float AutomationPanel::valueToY (float v) const
{
    auto a = areaBounds().toFloat().reduced (0.0f, 8.0f);
    return a.getBottom() - v * a.getHeight();
}

float AutomationPanel::yToValue (float y) const
{
    auto a = areaBounds().toFloat().reduced (0.0f, 8.0f);
    return jlimit (0.0f, 1.0f, (a.getBottom() - y) / jmax (1.0f, a.getHeight()));
}

ValueTree AutomationPanel::selectedLane() const
{
    auto lanes = proc.session.lanes();
    auto l = lanes.getChildWithProperty (IDs::uid, selectedLaneUid);
    if (! l.isValid() && lanes.getNumChildren() > 0)
        l = lanes.getChild (lanes.getNumChildren() - 1);
    return l;
}

AudioProcessorParameter* AutomationPanel::paramFor (const ValueTree& lane) const
{
    if (auto* inst = proc.chain.getInstance (lane[IDs::slotUid]))
        return inst->getParameters()[(int) lane[IDs::param]];
    return nullptr;
}

String AutomationPanel::valueText (const ValueTree& lane, float v) const
{
    if (auto* p = paramFor (lane))
    {
        auto t = p->getText (v, 24);
        auto label = p->getLabel();
        return label.isNotEmpty() && ! t.endsWith (label) ? t + " " + label : t;
    }
    return String (v, 2);
}

float AutomationPanel::valueAt (const ValueTree& lane, double ms) const
{
    std::vector<std::pair<double, float>> pts;
    for (auto pt : lane)
        pts.emplace_back ((double) pt[IDs::ms], (float) pt[IDs::value]);

    if (pts.empty())
    {
        auto* p = paramFor (lane);
        return p != nullptr ? p->getValue() : 0.5f;
    }

    std::sort (pts.begin(), pts.end());
    if (ms <= pts.front().first) return pts.front().second;
    if (ms >= pts.back().first)  return pts.back().second;

    for (size_t i = 1; i < pts.size(); ++i)
        if (pts[i].first >= ms)
        {
            const auto t = (ms - pts[i - 1].first) / jmax (1.0e-9, pts[i].first - pts[i - 1].first);
            return (float) (pts[i - 1].second + (pts[i].second - pts[i - 1].second) * t);
        }

    return pts.back().second;
}

ValueTree AutomationPanel::pointAt (Point<float> pos) const
{
    auto lane = selectedLane();
    for (auto pt : lane)
        if (Point<float> (msToX (pt[IDs::ms]), valueToY (pt[IDs::value])).getDistanceFrom (pos) < 7.0f)
            return pt;
    return {};
}

double AutomationPanel::rulerStep() const
{
    const double target = clipLengthMs / jmax (1.0, (double) areaBounds().getWidth() / 90.0);
    for (double s : { 1.0, 2.0, 5.0, 10.0, 25.0, 50.0, 100.0, 250.0, 500.0, 1000.0, 2500.0, 5000.0, 10000.0, 25000.0, 60000.0, 300000.0 })
        if (s >= target)
            return s;
    return 600000.0;
}

std::vector<AutomationPanel::Tab> AutomationPanel::layoutTabs() const
{
    std::vector<Tab> tabs;
    int x = 10;
    auto f = Theme::caps (11.0f);
    auto vf = Theme::font (12.0f);

    for (auto lane : proc.session.lanes())
    {
        const auto name = lane[IDs::paramName].toString().toUpperCase();
        const auto val = valueText (lane, valueAt (lane, 0.0));
        const int w = 12 + 7 + GlyphArrangement::getStringWidthInt (f, name) + 8
                    + GlyphArrangement::getStringWidthInt (vf, val) + 22;
        Tab t;
        t.lane = lane;
        t.bounds = { x, 0, w, tabsHeight };
        t.box = { x + 10, (tabsHeight - 12) / 2, 12, 12 };
        tabs.push_back (t);
        x += w;
    }

    return tabs;
}

//==============================================================================
void AutomationPanel::paint (Graphics& g)
{
    g.fillAll (Theme::bg);
    paintArea (g);
    paintTabs (g);
    paintRuler (g);
}

void AutomationPanel::paintTabs (Graphics& g)
{
    auto r = getLocalBounds().removeFromTop (tabsHeight);
    g.setColour (Theme::bg);
    g.fillRect (r);
    g.setColour (Theme::rowLine);
    g.fillRect (r.removeFromBottom (1));

    auto sel = selectedLane();
    auto tabs = layoutTabs();

    if (tabs.empty())
    {
        g.setColour (Theme::headerSub);
        g.setFont (Theme::font (12.5f));
        g.drawText ("Touch a control in a plug-in to add its automation here.",
                    getLocalBounds().removeFromTop (tabsHeight).withTrimmedLeft (14), Justification::centredLeft);
        return;
    }

    for (auto& t : tabs)
    {
        const bool active = t.lane[IDs::active];
        const bool isSel = t.lane == sel;
        const auto c = Theme::laneColour ((int) t.lane[IDs::uid] - 1);
        const float alpha = ! active ? 1.0f : (isSel ? 1.0f : 0.45f);
        const auto textCol = active ? c.withAlpha (alpha) : Theme::headerSub;

        auto box = t.box.toFloat();
        if (active) { g.setColour (c.withAlpha (alpha)); g.fillRoundedRectangle (box, 3.0f); }
        else        { g.setColour (Theme::mutedText);    g.drawRoundedRectangle (box.reduced (0.5f), 3.0f, 1.0f); }

        auto tr = t.bounds.withLeft (t.box.getRight() + 7);
        const auto name = t.lane[IDs::paramName].toString().toUpperCase();
        g.setColour (textCol);
        g.setFont (Theme::caps (11.0f));
        g.drawText (name, tr, Justification::centredLeft);

        tr.removeFromLeft (GlyphArrangement::getStringWidthInt (Theme::caps (11.0f), name) + 8);
        g.setFont (Theme::font (12.0f));
        g.drawText (valueText (t.lane, valueAt (t.lane, jmax (0.0, proc.clipPositionMs.load()))), tr, Justification::centredLeft);

        if (isSel && active)
        {
            g.setColour (c);
            g.fillRect (t.bounds.withTop (t.bounds.getBottom() - 3));
        }
    }
}

void AutomationPanel::paintArea (Graphics& g)
{
    auto a = areaBounds();
    g.setColour (Theme::bg);
    g.fillRect (a);

    // waveform underneath
    if (audioSource != nullptr && thumbnail.getTotalLength() > 0.0)
    {
        g.setColour (Colour (0xff2e2e2e));
        thumbnail.drawChannels (g, a.reduced (0, 8), clipStartSeconds, clipStartSeconds + clipLengthMs / 1000.0, 1.0f);
    }

    // grid in step with the ruler
    const double step = rulerStep();
    for (double t = 0.0; t <= clipLengthMs; t += step)
    {
        g.setColour (Colours::white.withAlpha (0.05f));
        g.fillRect ((int) msToX (t), a.getY(), 1, a.getHeight());
    }

    auto sel = selectedLane();

    auto drawLane = [&] (const ValueTree& lane, bool isSel)
    {
        const auto c = Theme::laneColour ((int) lane[IDs::uid] - 1);
        const float alpha = isSel ? 1.0f : 0.3f;

        std::vector<std::pair<double, float>> pts;
        for (auto pt : lane)
            pts.emplace_back ((double) pt[IDs::ms], (float) pt[IDs::value]);
        std::sort (pts.begin(), pts.end());

        Path p;
        if (pts.empty())
        {
            const float y = valueToY (valueAt (lane, 0.0));
            p.startNewSubPath ((float) a.getX(), y);
            p.lineTo ((float) a.getRight(), y);
            Path dashed;
            const float dashes[] { 4.0f, 4.0f };
            PathStrokeType (1.5f).createDashedStroke (dashed, p, dashes, 2);
            g.setColour (c.withAlpha (alpha * 0.8f));
            g.fillPath (dashed);
            return;
        }

        p.startNewSubPath ((float) a.getX(), valueToY (pts.front().second));
        for (auto& [ms, v] : pts)
            p.lineTo (msToX (ms), valueToY (v));
        p.lineTo ((float) a.getRight(), valueToY (pts.back().second));

        if (isSel)
        {
            Path fill (p);
            fill.lineTo ((float) a.getRight(), (float) a.getBottom());
            fill.lineTo ((float) a.getX(), (float) a.getBottom());
            fill.closeSubPath();
            g.setColour (c.withAlpha (0.12f));
            g.fillPath (fill);
        }

        g.setColour (c.withAlpha (alpha));
        g.strokePath (p, PathStrokeType (2.0f));

        if (isSel)
            for (auto& [ms, v] : pts)
            {
                Rectangle<float> sq (msToX (ms) - 3.5f, valueToY (v) - 3.5f, 7.0f, 7.0f);
                g.setColour (Theme::bg);
                g.fillRect (sq);
                g.setColour (c);
                g.drawRect (sq, 1.5f);
            }
    };

    for (auto lane : proc.session.lanes())
        if ((bool) lane[IDs::active] && lane != sel)
            drawLane (lane, false);

    if (sel.isValid() && (bool) sel[IDs::active])
        drawLane (sel, true);

    // playhead
    const double pos = proc.clipPositionMs.load();
    if (pos >= 0.0 && pos <= clipLengthMs)
    {
        g.setColour (Theme::orange);
        g.fillRect (msToX (pos) - 1.0f, (float) a.getY(), 2.0f, (float) a.getHeight());
    }

    // hover line + time box
    if (hoverX >= (float) a.getX())
    {
        const double ms = xToMs (hoverX);
        g.setColour (Theme::displayText);
        g.fillRect (hoverX, (float) a.getY(), 1.0f, (float) a.getHeight());

        String line1 = String (ms, 2) + " ms";
        String line2;
        if (sel.isValid())
            line2 = sel[IDs::paramName].toString() + " " + valueText (sel, valueAt (sel, ms));

        auto f1 = Theme::font (11.5f, true), f2 = Theme::font (10.5f);
        const int w = jmax (GlyphArrangement::getStringWidthInt (f1, line1), GlyphArrangement::getStringWidthInt (f2, line2)) + 14;
        Rectangle<int> box ((int) hoverX + 8, a.getY() + 8, w, line2.isEmpty() ? 22 : 36);
        if (box.getRight() > a.getRight()) box.setX ((int) hoverX - 8 - w);

        g.setColour (Theme::displayText);
        g.fillRoundedRectangle (box.toFloat(), 4.0f);
        g.setColour (Colour (0xff0d0d0d));
        g.setFont (f1);
        g.drawText (line1, box.reduced (7, 4).removeFromTop (14), Justification::centredLeft);
        if (line2.isNotEmpty())
        {
            g.setColour (Colour (0xff5c5b58));
            g.setFont (f2);
            g.drawText (line2, box.reduced (7, 4).withTrimmedTop (15), Justification::centredLeft);
        }
    }

    if (audioSource == nullptr)
    {
        g.setColour (Theme::mutedText);
        g.setFont (Theme::caps (10.0f));
        g.drawText ("NO ARA CLIP", a.reduced (10, 6), Justification::bottomRight);
    }
}

void AutomationPanel::paintRuler (Graphics& g)
{
    auto r = rulerBounds();
    g.setColour (Theme::bg);
    g.fillRect (r);
    g.setColour (Theme::rowLine);
    g.fillRect (r.removeFromTop (1));

    const double step = rulerStep();
    const double minor = step / 5.0;
    g.setFont (Theme::font (10.5f, true));

    for (int i = 0; ; ++i)
    {
        const double t = i * minor;
        if (t > clipLengthMs + 0.001) break;
        const int x = (int) msToX (t);

        if (i % 5 == 0)
        {
            g.setColour (Colour (0xff7c7c78));
            g.fillRect (x, r.getY(), 1, r.getHeight());
            String label = String ((int64) t);
            if (step < 10.0) label = String (t, 1);
            g.drawText (label, x + 4, r.getY() + 2, 70, r.getHeight() - 2, Justification::centredLeft);
        }
        else
        {
            g.setColour (Theme::frame);
            g.fillRect (x, r.getBottom() - 5, 1, 5);
        }
    }

    g.setColour (Theme::headerSub);
    g.drawText ("MS", r.withTrimmedRight (6), Justification::centredRight);
}

//==============================================================================
void AutomationPanel::mouseMove (const MouseEvent& e)
{
    hoverX = areaBounds().contains (e.getPosition()) ? (float) e.x : -1.0f;
    repaint();
}

void AutomationPanel::mouseExit (const MouseEvent&)
{
    hoverX = -1.0f;
    repaint();
}

void AutomationPanel::mouseDown (const MouseEvent& e)
{
    auto& um = proc.session.undo;

    if (e.y < tabsHeight)
    {
        for (auto& t : layoutTabs())
        {
            if (! t.bounds.contains (e.getPosition()))
                continue;

            if (e.mods.isPopupMenu())
            {
                PopupMenu m;
                m.addItem (1, "Remove lane");
                m.addItem (2, "Clear points");
                m.showMenuAsync (PopupMenu::Options().withMousePosition(), [this, lane = t.lane] (int r)
                {
                    auto& u = proc.session.undo;
                    if (r == 1) { u.beginNewTransaction ("Remove lane"); proc.session.lanes().removeChild (lane, &u); }
                    if (r == 2) { u.beginNewTransaction ("Clear points"); auto l = lane; l.removeAllChildren (&u); }
                });
            }
            else if (t.box.expanded (4).contains (e.getPosition()))
            {
                um.beginNewTransaction ((bool) t.lane[IDs::active] ? "Turn lane off" : "Turn lane on");
                t.lane.setProperty (IDs::active, ! (bool) t.lane[IDs::active], &um);
            }
            else
            {
                selectedLaneUid = t.lane[IDs::uid];
            }

            repaint();
            return;
        }
        return;
    }

    if (! areaBounds().contains (e.getPosition()))
        return;

    auto lane = selectedLane();
    if (! lane.isValid() || ! (bool) lane[IDs::active])
        return;

    dragPoint = pointAt (e.position);

    if (! dragPoint.isValid())
    {
        um.beginNewTransaction ("Add point");
        ValueTree pt { IDs::POINT };
        pt.setProperty (IDs::ms, xToMs (e.position.x), nullptr);
        pt.setProperty (IDs::value, yToValue (e.position.y), nullptr);
        lane.appendChild (pt, &um);
        dragPoint = pt;
    }
    else
    {
        um.beginNewTransaction ("Move point");
    }

    repaint();
}

void AutomationPanel::mouseDrag (const MouseEvent& e)
{
    hoverX = (float) jlimit (areaBounds().getX(), areaBounds().getRight(), e.x);

    if (dragPoint.isValid())
    {
        auto& um = proc.session.undo;
        dragPoint.setProperty (IDs::ms, xToMs (e.position.x), &um);
        dragPoint.setProperty (IDs::value, yToValue (e.position.y), &um);
    }

    repaint();
}

void AutomationPanel::mouseUp (const MouseEvent&)
{
    dragPoint = {};
}

void AutomationPanel::mouseDoubleClick (const MouseEvent& e)
{
    auto pt = pointAt (e.position);
    if (pt.isValid())
    {
        auto& um = proc.session.undo;
        um.beginNewTransaction ("Delete point");
        pt.getParent().removeChild (pt, &um);
        dragPoint = {};
        repaint();
    }
}
