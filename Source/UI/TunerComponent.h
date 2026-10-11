#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Audio/Telemetry.h"
#include "../Audio/TunerDetector.h"

namespace conductor {

class TunerComponent : public juce::Component {
public:
    TunerComponent(AudioTelemetry& telemetry) : mTelemetry(telemetry) {}

    void update() {
        bool locked = mTelemetry.isPitchLocked.load(std::memory_order_relaxed);
        float cents = mTelemetry.pitchCentsOffset.load(std::memory_order_relaxed);
        int note = mTelemetry.detectedMidiNote.load(std::memory_order_relaxed);
        float freq = mTelemetry.detectedPitchHz.load(std::memory_order_relaxed);

        mIsLocked = locked;
        mCents = cents;
        mNoteName = locked ? TunerDetector::getNoteName(note) : "--";
        mFreq = freq;

        repaint();
    }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();

        // 1. Dark Brushed Obsidian Bezel
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.fillRoundedRectangle(bounds.translated(0.0f, 2.0f), 8.0f);

        juce::ColourGradient bgGrad(juce::Colour(0xff161b24), bounds.getX(), bounds.getY(),
                                    juce::Colour(0xff090c10), bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill(bgGrad);
        g.fillRoundedRectangle(bounds, 8.0f);

        g.setColour(juce::Colour(0xff2d3748));
        g.drawRoundedRectangle(bounds, 8.0f, 1.0f);

        // 2. Header
        auto header = bounds.removeFromTop(20.0f).reduced(8.0f, 3.0f);
        g.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
        g.setColour(juce::Colour(0xff10b981));
        g.drawText("PITCH ANALYZER", header, juce::Justification::left);

        // 3. Central Note Badge
        bool inTune = mIsLocked && std::fabs(mCents) <= 3.5f;
        auto noteArea = bounds.removeFromTop(36.0f);
        float noteBoxW = 44.0f;
        auto noteBox = juce::Rectangle<float>(noteArea.getCentreX() - noteBoxW * 0.5f, noteArea.getY(), noteBoxW, noteArea.getHeight());

        // Note Badge Background with Bloom
        juce::Colour noteBg = inTune ? juce::Colour(0xff10b981) : (mIsLocked ? juce::Colour(0xff0284c7) : juce::Colour(0xff1e293b));
        if (mIsLocked) {
            g.setColour(noteBg.withAlpha(0.20f));
            g.fillRoundedRectangle(noteBox.expanded(4.0f), 8.0f);
        }
        g.setColour(juce::Colour(0xff06080b));
        g.fillRoundedRectangle(noteBox, 6.0f);
        g.setColour(noteBg.withAlpha(0.85f));
        g.drawRoundedRectangle(noteBox, 6.0f, 1.2f);

        g.setFont(juce::FontOptions(20.0f).withStyle("Bold"));
        g.setColour(inTune ? juce::Colour(0xff10b981) : (mIsLocked ? juce::Colour(0xff38bdf8) : juce::Colour(0xff64748b)));
        g.drawText(mNoteName, noteBox, juce::Justification::centred);

        // 4. Frequency & Cents Counter
        auto subArea = bounds.removeFromTop(16.0f);
        g.setFont(juce::FontOptions(8.5f));
        if (mIsLocked) {
            g.setColour(juce::Colour(0xffcbd5e1));
            juce::String centsStr = (mCents >= 0.0f ? "+" : "") + juce::String(mCents, 1) + " ct";
            g.drawText(juce::String(mFreq, 1) + " Hz   [" + centsStr + "]", subArea, juce::Justification::centred);
        } else {
            g.setColour(juce::Colour(0xff475569));
            g.drawText("AWAITING SIGNAL", subArea, juce::Justification::centred);
        }

        // 5. Strobe / Gauge Scale Arc
        auto gaugeArea = bounds.reduced(10.0f, 4.0f);
        float cx = gaugeArea.getCentreX();
        float cy = gaugeArea.getCentreY();
        float gw = gaugeArea.getWidth();

        // Scale track
        g.setColour(juce::Colour(0xff1e293b));
        g.fillRect(gaugeArea.getX(), cy - 1.0f, gw, 2.0f);

        // Tick marks (-50, -25, 0, +25, +50)
        auto drawTick = [&g, cy, cx, gw](float centVal, float height, juce::Colour col) {
            float tx = cx + (centVal / 50.0f) * (gw * 0.45f);
            g.setColour(col);
            g.fillRect(tx - 0.5f, cy - height * 0.5f, 1.0f, height);
        };
        drawTick(-50.0f, 6.0f, juce::Colour(0xff475569));
        drawTick(-25.0f, 6.0f, juce::Colour(0xff475569));
        drawTick(25.0f, 6.0f, juce::Colour(0xff475569));
        drawTick(50.0f, 6.0f, juce::Colour(0xff475569));

        // Center Perfect Zero Target
        g.setColour(juce::Colour(0xff10b981));
        g.fillRect(cx - 1.0f, cy - 8.0f, 2.0f, 16.0f);

        // 6. Holographic Indicator Pointer
        if (mIsLocked) {
            float clampedCents = std::max(-50.0f, std::min(50.0f, mCents));
            float needleX = cx + (clampedCents / 50.0f) * (gw * 0.45f);

            juce::Colour needleCol = inTune ? juce::Colour(0xff10b981) : (mCents < 0 ? juce::Colour(0xfff59e0b) : juce::Colour(0xffef4444));

            // Ambient Needle Glow
            g.setColour(needleCol.withAlpha(0.35f));
            g.fillEllipse(needleX - 5.0f, cy - 6.0f, 10.0f, 12.0f);

            // Razor-sharp Needle
            g.setColour(needleCol);
            g.fillEllipse(needleX - 2.5f, cy - 4.5f, 5.0f, 9.0f);
        }
    }

private:
    AudioTelemetry& mTelemetry;
    bool mIsLocked{false};
    float mCents{0.0f};
    std::string mNoteName{"--"};
    float mFreq{0.0f};
};

} // namespace conductor
