#include "AutomationPanel.h"
#include "TimeFormat.h"

using namespace juce;

//==============================================================================
// ADD AUTO, drawn vertically on the left edge.
class AutomationPanel::VerticalToggle final : public Button
{
public:
    VerticalToggle() : Button ("Add auto")
    {
        setClickingTogglesState (true);
        setTooltip ("Toggle to add parameter automation");
    }

    void paintButton (Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        const bool on = getToggleState();

        if (on)
        {
            g.setColour (over ? Theme::orangeText : Theme::orange);
            g.fillRoundedRectangle (r, 5.0f);
        }
        else
        {
            g.setColour (over ? Theme::hover : Theme::bg);
            g.fillRoundedRectangle (r, 5.0f);
            g.setColour (Theme::btnBorder);
            g.drawRoundedRectangle (r, 5.0f, 1.0f);
        }

        Graphics::ScopedSaveState s (g);
        g.addTransform (AffineTransform::rotation (-MathConstants<float>::halfPi, r.getCentreX(), r.getCentreY()));
        auto tr = Rectangle<float> (r.getHeight(), r.getWidth()).withCentre (r.getCentre());
        g.setColour (on ? Colours::white : Theme::btnText);
        g.setFont (Theme::caps (11.5f));
        g.drawText ("ADD AUTO", tr, Justification::centred);
    }
};

//==============================================================================
// Min/max peaks of the clip, read once in the background from the host's audio.
// Covers exactly the clip (region) range so the waveform always fills the window.
class AutomationPanel::Peaks final : private Thread
{
public:
    Peaks (ARAAudioSource* s, double startSec, double lengthSec, std::function<void()> onUpdate)
        : Thread ("ARAck waveform"), source (s), reader (std::make_unique<ARAAudioSourceReader> (s)),
          start (startSec), length (lengthSec), update (std::move (onUpdate))
    {
        mins.resize (numBins, 0.0f);
        maxs.resize (numBins, 0.0f);
        startThread (Priority::low);
    }

    ~Peaks() override { stopThread (3000); }

    static constexpr int numBins = 6000;

    bool matches (ARAAudioSource* s, double st, double len) const
    {
        return source == s && std::abs (st - start) < 1.0e-6 && std::abs (len - length) < 1.0e-6;
    }

    ARAAudioSource* const source;
    std::vector<float> mins, maxs;
    std::atomic<int> binsDone { 0 };
    std::atomic<bool> failed { false };

private:
    void run() override
    {
        const double sr = reader->sampleRate;
        const auto first = (int64) (start * sr);
        const auto total = jmax ((int64) 1, (int64) (length * sr));
        AudioBuffer<float> buf ((int) jmax (1u, reader->numChannels), 4096);

        for (int b = 0; b < numBins && ! threadShouldExit(); ++b)
        {
            const auto s0 = first + total * b / numBins;
            const auto s1 = first + total * (b + 1) / numBins;
            float lo = 0.0f, hi = 0.0f;

            for (auto s = s0; s < s1 && ! threadShouldExit(); s += buf.getNumSamples())
            {
                const int n = (int) jmin ((int64) buf.getNumSamples(), s1 - s);
                if (! reader->read (&buf, 0, n, s, true, true))
                {
                    failed = true;
                    return;
                }

                for (int c = 0; c < buf.getNumChannels(); ++c)
                {
                    auto mm = buf.findMinMax (c, 0, n);
                    lo = jmin (lo, mm.getStart());
                    hi = jmax (hi, mm.getEnd());
                }
            }

            mins[(size_t) b] = lo;
            maxs[(size_t) b] = hi;
            binsDone = b + 1;

            if (b % 300 == 299 || b == numBins - 1)
                MessageManager::callAsync (update);
        }
    }

    std::unique_ptr<ARAAudioSourceReader> reader;
    double start, length;
    std::function<void()> update;
};

//==============================================================================
AutomationPanel::AutomationPanel (ClipModel& m) : model (m)
{
    setOpaque (true);
    setWantsKeyboardFocus (true);

    addAutoButton = std::make_unique<VerticalToggle>();
    addAutoButton->setToggleState (model.chain.recordAutomation.load(), dontSendNotification);
    addAutoButton->onClick = [this]
    {
        model.chain.recordAutomation.store (addAutoButton->getToggleState());
        repaint();
    };
    addAndMakeVisible (*addAutoButton);
}

AutomationPanel::~AutomationPanel()
{
    peaks.reset();
    if (audioSource != nullptr)
        audioSource->removeListener (this);
}

void AutomationPanel::resized()
{
    addAutoButton->setBounds (0, 2, sideWidth - 4, getHeight() - 4);
}

void AutomationPanel::setClip (ARAAudioSource* source, double startSeconds, double lengthMs)
{
    const double newLength = lengthMs > 1.0 ? lengthMs : 30000.0;

    // A different clip (or length) always starts fitted to the window.
    if (std::abs (newLength - clipLengthMs) > 0.5 || source != audioSource || std::abs (startSeconds - clipStartSeconds) > 1.0e-6)
    {
        clipLengthMs = newLength;
        clipStartSeconds = startSeconds;
        viewStart = 0.0;
        viewEnd = clipLengthMs;
        repaint();
    }

    if (source != audioSource)
    {
        peaks.reset();

        if (audioSource != nullptr)
            audioSource->removeListener (this);

        audioSource = source;

        if (audioSource != nullptr)
            audioSource->addListener (this);
    }

    // (Re)read the waveform when the clip range changes, or retry if the host refused.
    if (audioSource != nullptr
        && (peaks == nullptr || ! peaks->matches (audioSource, clipStartSeconds, clipLengthMs / 1000.0) || peaks->failed))
    {
        peaks.reset();
        peaks = std::make_unique<Peaks> (audioSource, clipStartSeconds, clipLengthMs / 1000.0,
                                         [safe = Component::SafePointer<AutomationPanel> (this)]
                                         {
                                             if (safe != nullptr) safe->repaint (safe->lanesBounds());
                                         });
    }
}

void AutomationPanel::willDestroyAudioSource (ARAAudioSource* s)
{
    if (s == audioSource)
    {
        peaks.reset();
        audioSource->removeListener (this);
        audioSource = nullptr;
        repaint();
    }
}

//==============================================================================
Rectangle<int> AutomationPanel::content() const     { return getLocalBounds().withTrimmedLeft (sideWidth + 6); }
Rectangle<int> AutomationPanel::areaBounds() const  { return content().withTrimmedTop (tabsHeight).withTrimmedBottom (rulerHeight); }
Rectangle<int> AutomationPanel::zoomStrip() const   { return areaBounds().removeFromTop (zoomStripHeight); }
Rectangle<int> AutomationPanel::lanesBounds() const { return areaBounds().withTrimmedTop (zoomStripHeight); }
Rectangle<int> AutomationPanel::rulerBounds() const { return content().removeFromBottom (rulerHeight); }

float AutomationPanel::msToX (double ms) const
{
    auto a = areaBounds();
    return (float) a.getX() + (float) ((ms - viewStart) / jmax (1.0e-6, viewEnd - viewStart)) * (float) a.getWidth();
}

double AutomationPanel::xToMs (float x) const
{
    auto a = areaBounds();
    const double t = viewStart + (double) (x - (float) a.getX()) / jmax (1, a.getWidth()) * (viewEnd - viewStart);
    return jlimit (0.0, clipLengthMs, t);
}

float AutomationPanel::valueToY (float v) const
{
    auto a = lanesBounds().toFloat().reduced (0.0f, 8.0f);
    return a.getBottom() - v * a.getHeight();
}

float AutomationPanel::yToValue (float y) const
{
    auto a = lanesBounds().toFloat().reduced (0.0f, 8.0f);
    return jlimit (0.0f, 1.0f, (a.getBottom() - y) / jmax (1.0f, a.getHeight()));
}

void AutomationPanel::setView (double start, double end)
{
    const double span = jlimit (10.0, clipLengthMs, end - start);
    start = jlimit (0.0, clipLengthMs - span, start);
    viewStart = start;
    viewEnd = start + span;
    repaint();
}

void AutomationPanel::zoomAround (double anchorMs, float anchorX, double factor)
{
    auto a = areaBounds();
    const double span = jlimit (10.0, clipLengthMs, (viewEnd - viewStart) * factor);
    const double frac = (double) (anchorX - (float) a.getX()) / jmax (1, a.getWidth());
    const double start = anchorMs - frac * span;
    setView (start, start + span);
}

ValueTree AutomationPanel::selectedLane() const
{
    auto lanes = model.session.lanes();
    auto l = lanes.getChildWithProperty (IDs::uid, selectedLaneUid);
    if (! l.isValid() && lanes.getNumChildren() > 0)
        l = lanes.getChild (lanes.getNumChildren() - 1);
    return l;
}

AudioProcessorParameter* AutomationPanel::paramFor (const ValueTree& lane) const
{
    if (auto* inst = model.chain.getInstance (lane[IDs::slotUid]))
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

bool AutomationPanel::isSelected (const ValueTree& pt) const
{
    return selection.contains (pt);
}

double AutomationPanel::rulerStep() const
{
    const double span = viewEnd - viewStart;
    const double target = span / jmax (1.0, (double) areaBounds().getWidth() / 100.0);
    for (double s : { 1.0, 2.0, 5.0, 10.0, 25.0, 50.0, 100.0, 250.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0,
                      15000.0, 30000.0, 60000.0, 120000.0, 300000.0 })
        if (s >= target)
            return s;
    return 600000.0;
}

std::vector<AutomationPanel::Tab> AutomationPanel::layoutTabs() const
{
    std::vector<Tab> tabs;
    int x = content().getX();
    auto f = Theme::caps (11.0f);
    auto vf = Theme::font (12.0f);

    for (auto lane : model.session.lanes())
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
    auto r = content().removeFromTop (tabsHeight);
    g.setColour (Theme::rowLine);
    g.fillRect (r.removeFromBottom (1));

    auto sel = selectedLane();

    for (auto& t : layoutTabs())
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
        g.drawText (valueText (t.lane, valueAt (t.lane, jmax (0.0, model.playPositionMs.load()))), tr, Justification::centredLeft);

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
    auto strip = zoomStrip();
    auto lanesArea = lanesBounds();

    g.setColour (Theme::bg);
    g.fillRect (a);

    // waveform underneath: exactly the clip range, so it fills the window when fitted
    if (peaks != nullptr && peaks->binsDone > 0)
    {
        auto wr = lanesArea.reduced (0, 6).toFloat();
        const float mid = wr.getCentreY(), half = wr.getHeight() * 0.5f;
        const int done = peaks->binsDone;
        g.setColour (Colour (0xff2e2e2e));

        for (int x = (int) wr.getX(); x < (int) wr.getRight(); ++x)
        {
            const double t0 = xToMs ((float) x), t1 = xToMs ((float) x + 1.0f);
            int b0 = (int) (t0 / clipLengthMs * Peaks::numBins);
            int b1 = jmax (b0 + 1, (int) (t1 / clipLengthMs * Peaks::numBins));
            b0 = jlimit (0, Peaks::numBins - 1, b0);
            b1 = jmin (b1, done);

            float lo = 0.0f, hi = 0.0f;
            for (int b = b0; b < b1; ++b)
            {
                lo = jmin (lo, peaks->mins[(size_t) b]);
                hi = jmax (hi, peaks->maxs[(size_t) b]);
            }

            if (hi > lo)
                g.fillRect ((float) x, mid - hi * half, 1.0f, jmax (1.0f, (hi - lo) * half));
        }
    }

    // grid in step with the ruler
    const double step = rulerStep();
    g.setColour (Colours::white.withAlpha (0.05f));
    for (double t = std::ceil (viewStart / step) * step; t <= viewEnd; t += step)
        g.fillRect ((int) msToX (t), lanesArea.getY(), 1, lanesArea.getHeight());

    // zoom strip
    g.setColour (Theme::bandA);
    g.fillRect (strip);
    g.setColour (Theme::rowLine);
    g.fillRect (strip.removeFromBottom (1));

    if (viewStart > 0.0 || viewEnd < clipLengthMs)
    {
        const float x0 = (float) a.getX() + (float) (viewStart / clipLengthMs) * (float) a.getWidth();
        const float x1 = (float) a.getX() + (float) (viewEnd / clipLengthMs) * (float) a.getWidth();
        g.setColour (Theme::frame);
        g.fillRoundedRectangle (x0, (float) a.getY() + 8.0f, jmax (4.0f, x1 - x0), 5.0f, 2.5f);
    }
    else
    {
        g.setColour (Theme::mutedText);
        g.setFont (Theme::caps (9.5f));
        g.drawText ("DRAG TO ZOOM", a.getX() + 8, a.getY(), 200, zoomStripHeight - 1, Justification::centredLeft);
    }

    auto sel = selectedLane();
    Graphics::ScopedSaveState clip (g);
    g.reduceClipRegion (lanesArea);

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

        const float left = jmin ((float) a.getX(), msToX (pts.front().first));
        const float right = jmax ((float) a.getRight(), msToX (pts.back().first));
        p.startNewSubPath (left, valueToY (pts.front().second));
        for (auto& [ms, v] : pts)
            p.lineTo (msToX (ms), valueToY (v));
        p.lineTo (right, valueToY (pts.back().second));

        if (isSel)
        {
            Path fill (p);
            fill.lineTo (right, (float) a.getBottom());
            fill.lineTo (left, (float) a.getBottom());
            fill.closeSubPath();
            g.setColour (c.withAlpha (0.12f));
            g.fillPath (fill);
        }

        g.setColour (c.withAlpha (alpha));
        g.strokePath (p, PathStrokeType (2.0f));

        if (isSel)
            for (auto pt : lane)
            {
                Rectangle<float> sq (msToX (pt[IDs::ms]) - 3.5f, valueToY (pt[IDs::value]) - 3.5f, 7.0f, 7.0f);
                g.setColour (isSelected (pt) ? c : Theme::bg);
                g.fillRect (sq);
                g.setColour (isSelected (pt) ? Colours::white : c);
                g.drawRect (sq, 1.5f);
            }
    };

    for (auto lane : model.session.lanes())
        if ((bool) lane[IDs::active] && lane != sel)
            drawLane (lane, false);

    if (sel.isValid() && (bool) sel[IDs::active])
        drawLane (sel, true);

    // pencil stroke while drawing
    if (drag == Drag::pencil && pencil.size() > 1)
    {
        Path p;
        p.startNewSubPath (msToX (pencil.front().first), valueToY (pencil.front().second));
        for (auto& [ms, v] : pencil)
            p.lineTo (msToX (ms), valueToY (v));
        g.setColour (Colours::white.withAlpha (0.85f));
        g.strokePath (p, PathStrokeType (1.5f));
    }

    // box selection
    if (drag == Drag::box)
    {
        g.setColour (Theme::orange.withAlpha (0.12f));
        g.fillRect (boxRect);
        g.setColour (Theme::orange);
        g.drawRect (boxRect, 1);
    }

    // playhead
    const double pos = model.playPositionMs.load();
    if (pos >= viewStart && pos <= viewEnd)
    {
        g.setColour (Theme::orange);
        g.fillRect (msToX (pos) - 1.0f, (float) lanesArea.getY(), 2.0f, (float) lanesArea.getHeight());
    }

    // hover line + time box (larger text)
    if (hoverX >= (float) a.getX() && drag != Drag::box)
    {
        const double ms = xToMs (hoverX);
        g.setColour (Theme::displayText);
        g.fillRect (hoverX, (float) lanesArea.getY(), 1.0f, (float) lanesArea.getHeight());

        const auto ct = ClipTime::format (ms, clipLengthMs);
        String line2;
        if (sel.isValid())
            line2 = sel[IDs::paramName].toString() + " " + valueText (sel, valueAt (sel, ms));

        auto f1 = Theme::font (15.0f, true), f2 = Theme::font (13.0f), fu = Theme::caps (10.5f);
        const int line1W = GlyphArrangement::getStringWidthInt (f1, ct.value) + 6 + GlyphArrangement::getStringWidthInt (fu, ct.unit);
        const int w = jmax (line1W, GlyphArrangement::getStringWidthInt (f2, line2)) + 18;
        Rectangle<int> box ((int) hoverX + 10, lanesArea.getY() + 8, w, line2.isEmpty() ? 28 : 46);
        if (box.getRight() > a.getRight()) box.setX ((int) hoverX - 10 - w);

        g.setColour (Theme::displayText);
        g.fillRoundedRectangle (box.toFloat(), 5.0f);

        auto inner = box.reduced (9, 5);
        auto l1 = inner.removeFromTop (18);
        g.setColour (Colour (0xff0d0d0d));
        g.setFont (f1);
        g.drawText (ct.value, l1, Justification::centredLeft);
        g.setColour (Colour (0xff7c7c78));
        g.setFont (fu);
        g.drawText (ct.unit, l1.withTrimmedLeft (GlyphArrangement::getStringWidthInt (f1, ct.value) + 6), Justification::centredLeft);

        if (line2.isNotEmpty())
        {
            g.setColour (Colour (0xff3b3b3b));
            g.setFont (f2);
            g.drawText (line2, inner, Justification::centredLeft);
        }
    }

    if (audioSource == nullptr)
    {
        g.setColour (Theme::mutedText);
        g.setFont (Theme::caps (10.0f));
        g.drawText ("NO ARA CLIP", lanesArea.reduced (10, 6), Justification::bottomRight);
    }
}

void AutomationPanel::paintRuler (Graphics& g)
{
    auto r = rulerBounds();
    g.setColour (Theme::rowLine);
    g.fillRect (r.removeFromTop (1));

    const double step = rulerStep();
    const double minor = step / 5.0;
    g.setFont (Theme::font (10.5f, true));

    for (auto i = (int64) std::ceil (viewStart / minor - 1.0e-9); (double) i * minor <= viewEnd + 0.001; ++i)
    {
        const double t = (double) i * minor;
        const int x = (int) msToX (t);

        if (i % 5 == 0)
        {
            g.setColour (Colour (0xff7c7c78));
            g.fillRect (x, r.getY(), 1, r.getHeight());
            const auto label = clipLengthMs < 10000.0 ? String ((int64) std::llround (t)) : ClipTime::format (t, clipLengthMs).value;
            g.drawText (label, x + 4, r.getY() + 2, 80, r.getHeight() - 2, Justification::centredLeft);
        }
        else
        {
            g.setColour (Theme::frame);
            g.fillRect (x, r.getBottom() - 5, 1, 5);
        }
    }

    g.setColour (Theme::headerSub);
    g.setFont (Theme::caps (9.5f));
    g.drawText (ClipTime::format (clipLengthMs).unit, r.withTrimmedRight (6), Justification::centredRight);
}

//==============================================================================
void AutomationPanel::mouseMove (const MouseEvent& e)
{
    const bool inStrip = zoomStrip().contains (e.getPosition());
    setMouseCursor (inStrip ? MouseCursor::UpDownLeftRightResizeCursor : MouseCursor::NormalCursor);
    hoverX = areaBounds().contains (e.getPosition()) ? (float) e.x : -1.0f;
    repaint (areaBounds());
}

void AutomationPanel::mouseExit (const MouseEvent&)
{
    hoverX = -1.0f;
    repaint (areaBounds());
}

void AutomationPanel::mouseDown (const MouseEvent& e)
{
    auto& um = model.session.undo;
    drag = Drag::none;

    if (e.mods.isMiddleButtonDown() && areaBounds().contains (e.getPosition()))
    {
        drag = Drag::pan;
        dragStartView0 = viewStart;
        dragStartView1 = viewEnd;
        setMouseCursor (MouseCursor::DraggingHandCursor);
        return;
    }

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
                    auto& u = model.session.undo;
                    if (r == 1) { u.beginNewTransaction ("Remove lane"); model.session.lanes().removeChild (lane, &u); }
                    if (r == 2) { u.beginNewTransaction ("Clear points"); auto l = lane; l.removeAllChildren (&u); }
                    selection.clear();
                });
            }
            else if (t.box.expanded (4).contains (e.getPosition()))
            {
                um.beginNewTransaction ((bool) t.lane[IDs::active] ? "Turn lane off" : "Turn lane on");
                t.lane.setProperty (IDs::active, ! (bool) t.lane[IDs::active], &um);
            }
            else if ((int) t.lane[IDs::uid] != selectedLaneUid)
            {
                selectedLaneUid = t.lane[IDs::uid];
                selection.clear();
            }

            repaint();
            return;
        }
        return;
    }

    if (zoomStrip().contains (e.getPosition()))
    {
        drag = Drag::zoom;
        dragStartView0 = viewStart;
        dragStartView1 = viewEnd;
        dragAnchorMs = xToMs ((float) e.x);
        return;
    }

    if (! lanesBounds().contains (e.getPosition()))
        return;

    auto lane = selectedLane();
    if (! lane.isValid() || ! (bool) lane[IDs::active])
        return;

    if (e.mods.isAltDown())
    {
        drag = Drag::pencil;
        pencil.clear();
        pencil.emplace_back (xToMs (e.position.x), yToValue (e.position.y));
        return;
    }

    auto pt = pointAt (e.position);

    if (pt.isValid())
    {
        if (! isSelected (pt))
        {
            if (! e.mods.isShiftDown()) selection.clear();
            selection.add (pt);
        }

        drag = Drag::movePoints;
        moveOrigins.clear();
        for (auto& s : selection)
            moveOrigins.emplace_back ((double) s[IDs::ms], (float) s[IDs::value]);

        um.beginNewTransaction (selection.size() > 1 ? "Move points" : "Move point");
    }
    else
    {
        drag = Drag::pendingClick; // a click adds a point, a drag makes a box
    }

    repaint (areaBounds());
}

void AutomationPanel::mouseDrag (const MouseEvent& e)
{
    auto a = areaBounds();

    switch (drag)
    {
        case Drag::pan:
        {
            const double span = dragStartView1 - dragStartView0;
            const double start = dragStartView0 - (double) e.getDistanceFromDragStartX() / jmax (1, a.getWidth()) * span;
            setView (start, start + span);
            return;
        }

        case Drag::zoom:
        {
            // Ableton style: drag down to zoom in, up to zoom out, sideways to scroll.
            const double factor = std::pow (1.01, -(double) e.getDistanceFromDragStartY());
            const double span = jlimit (10.0, clipLengthMs, (dragStartView1 - dragStartView0) * factor);
            const double frac = (double) (e.getMouseDownX() - a.getX()) / jmax (1, a.getWidth());
            double start = dragAnchorMs - frac * span;
            start -= (double) e.getDistanceFromDragStartX() / jmax (1, a.getWidth()) * span;
            setView (start, start + span);
            return;
        }

        case Drag::pencil:
        {
            const double ms = xToMs (e.position.x);
            if (std::abs (ms - pencil.back().first) > 0.5)
                pencil.emplace_back (ms, yToValue (e.position.y));
            break;
        }

        case Drag::movePoints:
        {
            auto& um = model.session.undo;
            const double dMs = xToMs ((float) e.x) - xToMs ((float) e.getMouseDownX());
            const float dV = yToValue ((float) e.y) - yToValue ((float) e.getMouseDownY());

            for (int i = 0; i < selection.size() && i < (int) moveOrigins.size(); ++i)
            {
                auto pt = selection.getReference (i);
                pt.setProperty (IDs::ms, jlimit (0.0, clipLengthMs, moveOrigins[(size_t) i].first + dMs), &um);
                pt.setProperty (IDs::value, jlimit (0.0f, 1.0f, moveOrigins[(size_t) i].second + dV), &um);
            }
            break;
        }

        case Drag::pendingClick:
            if (e.getDistanceFromDragStart() < 4)
                break;
            drag = Drag::box;
            [[fallthrough]];

        case Drag::box:
            boxRect = Rectangle<int> (e.getMouseDownPosition(), e.getPosition()).getIntersection (lanesBounds());
            break;

        case Drag::none:
            break;
    }

    hoverX = (float) jlimit (a.getX(), a.getRight(), e.x);
    repaint (a);
}

void AutomationPanel::mouseUp (const MouseEvent& e)
{
    auto& um = model.session.undo;

    if (drag == Drag::pendingClick)
    {
        auto lane = selectedLane();
        um.beginNewTransaction ("Add point");
        ValueTree pt { IDs::POINT };
        pt.setProperty (IDs::ms, xToMs (e.position.x), nullptr);
        pt.setProperty (IDs::value, yToValue (e.position.y), nullptr);
        lane.appendChild (pt, &um);
        selection.clear();
        selection.add (pt);
    }
    else if (drag == Drag::box)
    {
        if (! e.mods.isShiftDown())
            selection.clear();

        for (auto pt : selectedLane())
            if (boxRect.toFloat().contains (msToX (pt[IDs::ms]), valueToY (pt[IDs::value])))
                selection.addIfNotAlreadyThere (pt);
    }
    else if (drag == Drag::pencil)
    {
        finishPencil();
    }

    if (drag == Drag::pan)
        setMouseCursor (MouseCursor::NormalCursor);

    drag = Drag::none;
    repaint (areaBounds());
}

// Replace the points under the pencil stroke with a cleaned-up version of it
// (Ramer-Douglas-Peucker on screen coordinates, ~2 px tolerance). One undo step.
void AutomationPanel::finishPencil()
{
    auto lane = selectedLane();
    if (! lane.isValid() || pencil.size() < 2)
    {
        pencil.clear();
        return;
    }

    std::sort (pencil.begin(), pencil.end());
    std::vector<Point<float>> screen;
    for (auto& [ms, v] : pencil)
        screen.emplace_back (msToX (ms), valueToY (v));

    std::vector<bool> keep (screen.size(), false);
    keep.front() = true;
    keep.back() = true;

    std::function<void (size_t, size_t)> rdp = [&] (size_t i0, size_t i1)
    {
        if (i1 <= i0 + 1) return;
        const Line<float> line (screen[i0], screen[i1]);
        float maxD = 0.0f;
        size_t idx = i0;
        for (size_t i = i0 + 1; i < i1; ++i)
        {
            Point<float> nearest;
            const float d = line.getDistanceFromPoint (screen[i], nearest);
            if (d > maxD) { maxD = d; idx = i; }
        }
        if (maxD > 2.0f)
        {
            keep[idx] = true;
            rdp (i0, idx);
            rdp (idx, i1);
        }
    };
    rdp (0, screen.size() - 1);

    const double from = pencil.front().first, to = pencil.back().first;
    auto& um = model.session.undo;
    um.beginNewTransaction ("Draw automation");

    for (int i = lane.getNumChildren(); --i >= 0;)
    {
        const double ms = lane.getChild (i)[IDs::ms];
        if (ms >= from && ms <= to)
            lane.removeChild (i, &um);
    }

    selection.clear();
    for (size_t i = 0; i < pencil.size(); ++i)
    {
        if (! keep[i]) continue;
        ValueTree pt { IDs::POINT };
        pt.setProperty (IDs::ms, pencil[i].first, nullptr);
        pt.setProperty (IDs::value, pencil[i].second, nullptr);
        lane.appendChild (pt, &um);
    }

    pencil.clear();
}

void AutomationPanel::mouseDoubleClick (const MouseEvent& e)
{
    if (zoomStrip().contains (e.getPosition()))
    {
        setView (0.0, clipLengthMs); // fit
        return;
    }

    auto pt = pointAt (e.position);
    if (pt.isValid())
    {
        auto& um = model.session.undo;
        um.beginNewTransaction ("Delete point");
        selection.removeAllInstancesOf (pt);
        pt.getParent().removeChild (pt, &um);
        drag = Drag::none;
        repaint (areaBounds());
    }
}

bool AutomationPanel::keyPressed (const KeyPress& k)
{
    if ((k == KeyPress::deleteKey || k == KeyPress::backspaceKey) && ! selection.isEmpty())
    {
        auto& um = model.session.undo;
        um.beginNewTransaction (selection.size() > 1 ? "Delete points" : "Delete point");
        for (auto& pt : selection)
            if (pt.getParent().isValid())
                pt.getParent().removeChild (pt, &um);
        selection.clear();
        repaint (areaBounds());
        return true;
    }

    return false;
}

void AutomationPanel::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    if (! areaBounds().contains (e.getPosition()))
        return;

    if (e.mods.isCommandDown())
    {
        const double factor = std::pow (0.8, (double) w.deltaY * 4.0);
        zoomAround (xToMs ((float) e.x), (float) e.x, factor);
        return;
    }

    // Plain wheel scrolls sideways when zoomed in.
    const float delta = w.deltaX != 0.0f ? w.deltaX : w.deltaY;
    const double span = viewEnd - viewStart;
    if (span < clipLengthMs)
        setView (viewStart - (double) delta * span * 0.5, viewEnd - (double) delta * span * 0.5);
    else
        Component::mouseWheelMove (e, w);
}
