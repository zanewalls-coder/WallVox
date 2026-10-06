#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <cmath>

// Shared visual style for WallVox, Wall Chop and Wall Chords.
// Each plugin passes its own two-colour accent gradient.
namespace wl
{
struct Theme { juce::Colour a, b; };

namespace c
{
    const juce::Colour bg0     (0xff0b0c13);
    const juce::Colour bg1     (0xff131522);
    const juce::Colour panel   (0xff191c2b);
    const juce::Colour panelHi (0xff222639);
    const juce::Colour line    (0xff2d3249);
    const juce::Colour text    (0xffe8e9f4);
    const juce::Colour dim     (0xff858aa8);
}

inline juce::ColourGradient grad (const Theme& t, juce::Point<float> p0, juce::Point<float> p1)
{
    return juce::ColourGradient (t.a, p0, t.b, p1, false);
}

// ---------------------------------------------------------------- drawing helpers
inline void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, bool on, const Theme& t,
                       float headerH = 30.0f, float titleIndent = 44.0f)
{
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (r.translated (0, 3), 12.0f);
    g.setGradientFill (juce::ColourGradient (c::panelHi, r.getX(), r.getY(), c::panel, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (c::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);

    auto head = r.withHeight (headerH);
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.fillRoundedRectangle (head, 12.0f);
    g.fillRect (head.withTrimmedTop (12.0f));
    g.setColour (c::line);
    g.drawHorizontalLine ((int) head.getBottom(), r.getX() + 1, r.getRight() - 1);

    if (title.isNotEmpty())
    {
        g.setColour (on ? c::text : c::dim);
        g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
        g.drawText (title, head.withTrimmedLeft (titleIndent).withTrimmedRight (8), juce::Justification::centredLeft);
    }
    if (on)
    {
        g.setGradientFill (grad (t, { r.getX() + 14, head.getBottom() }, { r.getRight() - 14, head.getBottom() }));
        g.setOpacity (0.55f);
        g.fillRect (juce::Rectangle<float> (r.getX() + 14, head.getBottom() - 1.0f, r.getWidth() - 28, 2.0f));
        g.setOpacity (1.0f);
    }
}

inline void drawBackground (juce::Graphics& g, juce::Rectangle<int> bounds, float headerH, const Theme& t,
                            const juce::String& word1, const juce::String& word2)
{
    auto r = bounds.toFloat();
    g.setGradientFill (juce::ColourGradient (c::bg1, r.getX(), r.getY(), c::bg0, r.getX(), r.getBottom(), false));
    g.fillAll();

    // soft accent glow in the corner, like a lit faceplate
    juce::ColourGradient glow (t.a.withAlpha (0.10f), r.getRight() - 120, r.getY() + 40,
                               t.a.withAlpha (0.0f), r.getRight() - 520, r.getY() + 400, true);
    g.setGradientFill (glow);
    g.fillRect (r);

    auto head = r.withHeight (headerH);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRect (head);
    g.setColour (c::line);
    g.drawHorizontalLine ((int) head.getBottom(), r.getX(), r.getRight());

    // logo mark: stacked bars
    const float lx = 18, ly = head.getCentreY();
    for (int i = 0; i < 4; ++i)
    {
        const float w = 22.0f - std::abs (1.5f - (float) i) * 6.0f;
        g.setGradientFill (grad (t, { lx, ly - 10 }, { lx + 22, ly + 10 }));
        g.fillRoundedRectangle (lx + (22 - w) * 0.5f, ly - 10 + i * 5.5f, w, 3.0f, 1.5f);
    }
    g.setColour (c::text);
    g.setFont (juce::FontOptions (21.0f, juce::Font::bold));
    const float w1 = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), word1);
    g.drawText (word1, juce::Rectangle<float> (50, head.getY(), w1 + 4, headerH), juce::Justification::centredLeft);
    g.setGradientFill (grad (t, { 50 + w1, 0 }, { 50 + w1 + 90, 0 }));
    g.drawText (word2, juce::Rectangle<float> (50 + w1 + 4, head.getY(), 160, headerH), juce::Justification::centredLeft);
}

// glowing stroke (several passes)
inline void glowStroke (juce::Graphics& g, const juce::Path& p, juce::Colour col, float width)
{
    for (int i = 3; i >= 1; --i)
    {
        g.setColour (col.withAlpha (0.07f * (float) (4 - i)));
        g.strokePath (p, juce::PathStrokeType (width + i * 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour (col);
    g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// ---------------------------------------------------------------- look and feel
class Look : public juce::LookAndFeel_V4
{
public:
    explicit Look (Theme th) : theme (th)
    {
        setColour (juce::ResizableWindow::backgroundColourId, c::bg0);
        setColour (juce::ComboBox::backgroundColourId, c::bg0.withAlpha (0.7f));
        setColour (juce::ComboBox::outlineColourId, c::line);
        setColour (juce::ComboBox::textColourId, c::text);
        setColour (juce::ComboBox::arrowColourId, theme.a);
        setColour (juce::PopupMenu::backgroundColourId, c::panel);
        setColour (juce::PopupMenu::textColourId, c::text);
        setColour (juce::PopupMenu::headerTextColourId, theme.a);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, theme.a.withAlpha (0.85f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
        setColour (juce::TextButton::buttonColourId, c::panelHi);
        setColour (juce::TextButton::buttonOnColourId, theme.a);
        setColour (juce::TextButton::textColourOffId, c::text);
        setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        setColour (juce::Label::textColourId, c::text);
        setColour (juce::Slider::textBoxTextColourId, c::dim);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::TextEditor::backgroundColourId, c::bg0);
        setColour (juce::TextEditor::outlineColourId, c::line);
        setColour (juce::AlertWindow::backgroundColourId, c::panel);
        setColour (juce::AlertWindow::textColourId, c::text);
        setColour (juce::TooltipWindow::backgroundColourId, c::panelHi);
        setColour (juce::TooltipWindow::textColourId, c::text);
    }

    Theme theme;

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
        const float size = juce::jmin (r.getWidth(), r.getHeight());
        r = r.withSizeKeepingCentre (size, size);
        const auto ctr = r.getCentre();
        const float rad = size * 0.5f, ringR = rad - 4.0f, ang = a0 + pos * (a1 - a0);
        const float alpha = s.isEnabled() ? 1.0f : 0.4f;

        // ring track
        juce::Path track;
        track.addCentredArc (ctr.x, ctr.y, ringR, ringR, 0, a0, a1, true);
        g.setColour (juce::Colours::black.withAlpha (0.55f * alpha));
        g.strokePath (track, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // glowing value arc
        if (pos > 0.001f)
        {
            juce::Path val;
            val.addCentredArc (ctr.x, ctr.y, ringR, ringR, 0, a0, ang, true);
            const auto gr = grad (theme, { r.getX(), r.getBottom() }, { r.getRight(), r.getY() });
            for (int i = 3; i >= 1; --i)
            {
                g.setGradientFill (gr);
                g.setOpacity (0.08f * (float) (4 - i) * alpha);
                g.strokePath (val, juce::PathStrokeType (5.0f + i * 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            g.setGradientFill (gr);
            g.setOpacity (alpha);
            g.strokePath (val, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setOpacity (1.0f);
        }

        // knob body: soft 3D dome
        auto body = r.reduced (size * 0.2f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (body.translated (0, 2.5f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3f58), body.getX(), body.getY(),
                                                 juce::Colour (0xff15172a), body.getRight(), body.getBottom(), false));
        g.fillEllipse (body);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawEllipse (body.reduced (0.5f), 1.0f);

        // indicator dot
        const float dr = body.getWidth() * 0.5f - 6.0f;
        const juce::Point<float> dot (ctr.x + dr * std::sin (ang), ctr.y - dr * std::cos (ang));
        g.setColour (theme.a.withAlpha (0.35f * alpha));
        g.fillEllipse (juce::Rectangle<float> (9, 9).withCentre (dot));
        g.setColour (juce::Colours::white.withAlpha (alpha));
        g.fillEllipse (juce::Rectangle<float> (4.5f, 4.5f).withCentre (dot));
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hover, bool) override
    {
        const bool on = b.getToggleState();
        auto r = b.getLocalBounds().toFloat();
        if (b.getButtonText().isEmpty())
        {
            // power button
            auto circle = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()) - 2, juce::jmin (r.getWidth(), r.getHeight()) - 2);
            if (on)
            {
                g.setColour (theme.a.withAlpha (0.25f));
                g.fillEllipse (circle.expanded (2.0f));
            }
            g.setColour (on ? juce::Colour (0xff111320) : c::bg0);
            g.fillEllipse (circle);
            g.setColour (on ? theme.a : c::line.brighter (hover ? 0.4f : 0.15f));
            g.drawEllipse (circle.reduced (0.5f), 1.2f);
            const auto ct = circle.getCentre();
            const float pr = circle.getWidth() * 0.26f;
            juce::Path p;
            p.addCentredArc (ct.x, ct.y, pr, pr, 0, 0.75f, juce::MathConstants<float>::twoPi - 0.75f, true);
            p.startNewSubPath (ct.x, ct.y - pr - 1.5f);
            p.lineTo (ct.x, ct.y - 1.0f);
            g.setColour (on ? theme.a : c::dim);
            g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            return;
        }
        // labelled pill switch
        auto sw = r.removeFromLeft (40).withSizeKeepingCentre (36, 20);
        g.setColour (on ? theme.a.withAlpha (0.85f) : c::bg0);
        g.fillRoundedRectangle (sw, 10.0f);
        g.setColour (on ? theme.a : c::line.brighter (hover ? 0.4f : 0.2f));
        g.drawRoundedRectangle (sw.reduced (0.5f), 10.0f, 1.0f);
        const auto knob = juce::Rectangle<float> (16, 16).withCentre ({ on ? sw.getRight() - 10 : sw.getX() + 10, sw.getCentreY() });
        g.setColour (on ? juce::Colours::white : c::dim);
        g.fillEllipse (knob);
        g.setColour (on ? c::text : c::dim);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (b.getButtonText(), r.withTrimmedLeft (6), juce::Justification::centredLeft);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hover, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        if (on)
        {
            g.setGradientFill (grad (theme, r.getTopLeft(), r.getBottomRight()));
            g.fillRoundedRectangle (r, 8.0f);
            return;
        }
        g.setGradientFill (juce::ColourGradient (c::panelHi.brighter (hover ? 0.12f : 0.0f), r.getX(), r.getY(),
                                                 c::panel.darker (down ? 0.3f : 0.0f), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (hover ? theme.a.withAlpha (0.6f) : c::line.brighter (0.2f));
        g.drawRoundedRectangle (r, 8.0f, 1.0f);
    }

    juce::Label* createSliderTextBox (juce::Slider& sl) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (sl);
        l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::textColourId, c::text.withAlpha (0.8f));
        l->setColour (juce::Label::outlineWhenEditingColourId, theme.a);
        l->setFont (juce::FontOptions (12.0f));
        return l;
    }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return juce::FontOptions (juce::jmin (14.0f, h * 0.45f), juce::Font::bold); }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (c::bg0.withAlpha (0.8f));
        g.fillRoundedRectangle (r, 7.0f);
        g.setColour (box.hasKeyboardFocus (true) || box.isMouseOver (true) ? theme.a.withAlpha (0.7f) : c::line.brighter (0.15f));
        g.drawRoundedRectangle (r, 7.0f, 1.0f);
        juce::Path arrow;
        const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
        arrow.startNewSubPath (ax - 4, ay - 2);
        arrow.lineTo (ax, ay + 2);
        arrow.lineTo (ax + 4, ay - 2);
        g.setColour (theme.a);
        g.strokePath (arrow, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::FontOptions (13.5f); }
    juce::Font getPopupMenuFont() override { return juce::FontOptions (14.0f); }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (8, 1, box.getWidth() - 30, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (c::panel);
        g.setColour (c::line);
        g.drawRect (0, 0, w, h);
    }
};

// ---------------------------------------------------------------- live scope
// Audio thread pushes one peak pair per block; the editor draws a glowing waveform.
struct ScopeData
{
    static constexpr int N = 400;
    float in[N] {}, out[N] {};
    std::atomic<int> pos { 0 };
    std::atomic<float> hz { 0.0f }, bpm { 120.0f };
    std::atomic<int> note { -1 };

    void push (float inPeak, float outPeak)
    {
        const int p = pos.load (std::memory_order_relaxed);
        in[p] = inPeak;
        out[p] = outPeak;
        pos.store ((p + 1) % N, std::memory_order_release);
    }
};

inline float peakOf (const juce::AudioBuffer<float>& b, int numCh)
{
    float m = 0.0f;
    for (int ch = 0; ch < juce::jmin (numCh, b.getNumChannels()); ++ch)
        m = juce::jmax (m, b.getMagnitude (ch, 0, b.getNumSamples()));
    return m;
}

class Scope : public juce::Component, private juce::Timer
{
public:
    Scope (ScopeData& d, Theme t, juce::String title) : data (d), theme (t), name (std::move (title)) { startTimerHz (40); }

    std::function<juce::String()> readout;   // optional big text on the right (e.g. detected note)
    juce::String readoutCaption;

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0d0f1c), r.getX(), r.getY(),
                                                 juce::Colour (0xff171a2e), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 14.0f);
        g.setColour (c::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 14.0f, 1.0f);

        auto plot = r.reduced (18.0f, 16.0f);
        const float readW = readout ? 150.0f : 0.0f;
        plot.removeFromRight (readW);
        const float mid = plot.getCentreY(), half = plot.getHeight() * 0.42f;

        g.setColour (c::line.withAlpha (0.5f));
        for (int i = 1; i < 8; ++i) g.drawVerticalLine ((int) (plot.getX() + plot.getWidth() * i / 8.0f), plot.getY() + 14, plot.getBottom());
        g.setColour (c::line);
        g.drawHorizontalLine ((int) mid, plot.getX(), plot.getRight());

        const int N = ScopeData::N, start = data.pos.load (std::memory_order_acquire);
        auto shape = [&] (const float* src, float scale)
        {
            juce::Path top;
            for (int i = 0; i < N; ++i)
            {
                const float v = std::pow (juce::jlimit (0.0f, 1.0f, src[(start + i) % N] * scale), 0.6f);
                const float x = plot.getX() + plot.getWidth() * i / (float) (N - 1);
                const float y = mid - v * half;
                if (i == 0) top.startNewSubPath (x, y); else top.lineTo (x, y);
            }
            return top;
        };
        auto mirrored = [&] (const float* src)
        {
            juce::Path p;
            for (int i = 0; i < N; ++i)
            {
                const float v = std::pow (juce::jlimit (0.0f, 1.0f, src[(start + i) % N]), 0.6f);
                const float x = plot.getX() + plot.getWidth() * i / (float) (N - 1);
                if (i == 0) p.startNewSubPath (x, mid - v * half); else p.lineTo (x, mid - v * half);
            }
            for (int i = N - 1; i >= 0; --i)
            {
                const float v = std::pow (juce::jlimit (0.0f, 1.0f, src[(start + i) % N]), 0.6f);
                const float x = plot.getX() + plot.getWidth() * i / (float) (N - 1);
                p.lineTo (x, mid + v * half);
            }
            p.closeSubPath();
            return p;
        };

        // input: faint outline
        g.setColour (c::dim.withAlpha (0.35f));
        g.strokePath (shape (data.in, 1.0f), juce::PathStrokeType (1.0f));

        // output: glowing gradient body
        const auto body = mirrored (data.out);
        auto gr = grad (theme, { plot.getX(), mid }, { plot.getRight(), mid });
        gr.multiplyOpacity (0.35f);
        g.setGradientFill (gr);
        g.fillPath (body);
        g.setGradientFill (grad (theme, { plot.getX(), mid }, { plot.getRight(), mid }));
        for (int i = 3; i >= 1; --i)
        {
            g.setOpacity (0.08f * (float) (4 - i));
            g.strokePath (body, juce::PathStrokeType (1.5f + i * 3.0f));
        }
        g.setOpacity (1.0f);
        g.strokePath (body, juce::PathStrokeType (1.4f));

        g.setColour (c::dim);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (name, r.reduced (18, 10).removeFromTop (18), juce::Justification::topLeft);

        if (readout)
        {
            auto rr = r.reduced (16).removeFromRight (readW);
            g.setColour (c::line);
            g.drawVerticalLine ((int) rr.getX(), rr.getY() + 6, rr.getBottom() - 6);
            g.setColour (c::dim);
            g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            g.drawText (readoutCaption, rr.removeFromTop (20), juce::Justification::centred);
            g.setGradientFill (grad (theme, rr.getTopLeft(), rr.getBottomRight()));
            g.setFont (juce::FontOptions (40.0f, juce::Font::bold));
            g.drawText (readout(), rr, juce::Justification::centred);
        }
    }

private:
    void timerCallback() override { repaint(); }
    ScopeData& data;
    Theme theme;
    juce::String name;
};

// Friendly value text for knobs: "50%", "-3.0 dB", "1.2 kHz", "80 ms"...
inline std::function<juce::String (float, int)> valueToText (juce::String unit, float lo, float hi)
{
    return [unit, lo, hi] (float v, int) -> juce::String
    {
        if (unit == "dB") return juce::String (v, 1) + " dB";
        if (unit == "st") return (v > 0 ? "+" : "") + juce::String (v, 1) + " st";
        if (unit == "Hz") return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
        if (unit == "ms") return v < 10.0f ? juce::String (v, 1) + " ms" : juce::String (juce::roundToInt (v)) + " ms";
        if (unit == ":1") return juce::String (v, 1) + ":1";
        if (lo >= 0.0f && hi <= 1.0f) return juce::String (juce::roundToInt (v * 100.0f)) + "%";
        return juce::String (v, 2);
    };
}

inline std::function<float (const juce::String&)> textToValue (juce::String unit, float lo, float hi)
{
    return [unit, lo, hi] (const juce::String& t) -> float
    {
        float v = t.retainCharacters ("-0123456789.").getFloatValue();
        if (unit == "Hz" && t.containsIgnoreCase ("k")) v *= 1000.0f;
        else if (unit.isEmpty() && lo >= 0.0f && hi <= 1.0f) v = v / 100.0f;
        return juce::jlimit (lo, hi, v);
    };
}

inline juce::String midiNoteName (int n)
{
    static const char* names[12] { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    return n < 0 ? juce::String ("--") : juce::String (names[n % 12]) + juce::String (n / 12 - 1);
}

} // namespace wl
