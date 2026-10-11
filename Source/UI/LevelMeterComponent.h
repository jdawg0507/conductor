#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Audio/Telemetry.h"

namespace conductor {

class LevelMeterComponent : public juce::Component {
public:
    LevelMeterComponent(AudioTelemetry& telemetry, bool isInputMeter)
        : mTelemetry(telemetry), mIsInputMeter(isInputMeter) {}

    void update() {
        float newPeak = 0.0f;
        float newRms = 0.0f;
        bool clip = false;

        if (mIsInputMeter) {
            newPeak = mTelemetry.inputPeakDbfs.load(std::memory_order_relaxed);
            newRms = mTelemetry.inputRmsDbfs.load(std::memory_order_relaxed);
            clip = mTelemetry.inputClipped.load(std::memory_order_relaxed);
        } else {
            newPeak = mTelemetry.outputPeakDbfs.load(std::memory_order_relaxed);
            newRms = mTelemetry.outputRmsDbfs.load(std::memory_order_relaxed);
            clip = mTelemetry.outputClipped.load(std::memory_order_relaxed);
        }

        mPeakDb = newPeak;
        mRmsDb = newRms;
        mClipped = clip;

        if (newPeak > mPeakHoldDb) {
            mPeakHoldDb = newPeak;
            mHoldCounter = 30; // ~1 second hold at 30 FPS
        } else if (mHoldCounter > 0) {
            --mHoldCounter;
        } else {
            mPeakHoldDb = std::max(-100.0f, mPeakHoldDb - 1.2f); // Smooth decay
        }

        repaint();
    }

    void resetClip() {
        if (mIsInputMeter) mTelemetry.inputClipped.store(false);
        else mTelemetry.outputClipped.store(false);
        mClipped = false;
    }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();

        // 1. Dark Brushed Obsidian Chassis
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.fillRoundedRectangle(bounds.translated(0.0f, 2.0f), 8.0f);

        juce::ColourGradient bgGrad(juce::Colour(0xff161b24), bounds.getX(), bounds.getY(),
                                    juce::Colour(0xff090c10), bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill(bgGrad);
        g.fillRoundedRectangle(bounds, 8.0f);

        g.setColour(juce::Colour(0xff2d3748));
        g.drawRoundedRectangle(bounds, 8.0f, 1.0f);

        // 2. Header Title with Neon Accent Line
        auto header = bounds.removeFromTop(24.0f).reduced(8.0f, 4.0f);
        g.setFont(juce::FontOptions(9.5f).withStyle("Bold"));
        g.setColour(mIsInputMeter ? juce::Colour(0xff00f0ff) : juce::Colour(0xffa855f7));
        g.drawText(mIsInputMeter ? "INPUT METER" : "OUTPUT METER", header.removeFromLeft(80.0f), juce::Justification::left);

        // Clip LED indicator in header right
        auto clipArea = header.removeFromRight(32.0f);
        g.setFont(juce::FontOptions(8.0f).withStyle("Bold"));
        if (mClipped) {
            g.setColour(juce::Colour(0xffef4444).withAlpha(0.35f));
            g.fillRoundedRectangle(clipArea, 3.0f);
            g.setColour(juce::Colour(0xffef4444));
            g.drawRoundedRectangle(clipArea, 3.0f, 1.0f);
            g.drawText("CLIP", clipArea, juce::Justification::centred);
        } else {
            g.setColour(juce::Colour(0xff1e293b));
            g.fillRoundedRectangle(clipArea, 3.0f);
            g.setColour(juce::Colour(0xff475569));
            g.drawText("OK", clipArea, juce::Justification::centred);
        }

        // 3. Segmented Multi-LED Ladder Track
        auto meterArea = bounds.reduced(8.0f, 6.0f);
        auto barArea = meterArea.removeFromTop(18.0f);

        g.setColour(juce::Colour(0xff06080b));
        g.fillRoundedRectangle(barArea, 3.0f);
        g.setColour(juce::Colour(0xff1e293b));
        g.drawRoundedRectangle(barArea, 3.0f, 0.8f);

        // 20 Discrete Segment Blocks
        const int numSegments = 20;
        float segW = (barArea.getWidth() - (numSegments + 1)) / static_cast<float>(numSegments);
        float normPeak = juce::jlimit(0.0f, 1.0f, (mPeakDb + 54.0f) / 54.0f);
        float normHold = juce::jlimit(0.0f, 1.0f, (mPeakHoldDb + 54.0f) / 54.0f);
        int activeSegments = static_cast<int>(normPeak * numSegments);
        int holdSegment = static_cast<int>(normHold * (numSegments - 1));

        for (int i = 0; i < numSegments; ++i) {
            float sx = barArea.getX() + 1.0f + i * (segW + 1.0f);
            auto segRect = juce::Rectangle<float>(sx, barArea.getY() + 1.5f, segW, barArea.getHeight() - 3.0f);

            juce::Colour segCol(0xff10b981); // Green (-54 to -18 dB)
            if (i >= 13 && i < 17) segCol = juce::Colour(0xffeab308); // Yellow (-18 to -6 dB)
            else if (i >= 17) segCol = juce::Colour(0xffef4444); // Red (-6 to 0 dB)

            if (i < activeSegments) {
                // Active glowing segment
                g.setColour(segCol);
                g.fillRect(segRect);
            } else if (i == holdSegment && mPeakHoldDb > -50.0f) {
                // Peak hold tick
                g.setColour(juce::Colours::white);
                g.fillRect(segRect);
            } else {
                // Inactive dim segment
                g.setColour(segCol.withAlpha(0.12f));
                g.fillRect(segRect);
            }
        }

        // 4. Value Readout & Scale labels
        auto readoutArea = bounds.reduced(8.0f, 2.0f);
        g.setFont(juce::FontOptions(9.5f).withStyle("Bold"));
        g.setColour(juce::Colour(0xfff8fafc));
        juce::String valStr = (mPeakDb > -90.0f ? juce::String(mPeakDb, 1) + " dBFS" : "-inf dBFS");
        g.drawText(valStr, readoutArea.removeFromLeft(80.0f), juce::Justification::left);

        g.setFont(juce::FontOptions(8.0f));
        g.setColour(juce::Colour(0xff64748b));
        g.drawText("-48      -24      -12     -6     0", readoutArea, juce::Justification::right);
    }

    void mouseDown(const juce::MouseEvent&) override {
        resetClip();
        repaint();
    }

private:
    AudioTelemetry& mTelemetry;
    bool mIsInputMeter{true};
    float mPeakDb{-100.0f};
    float mRmsDb{-100.0f};
    float mPeakHoldDb{-100.0f};
    int mHoldCounter{0};
    bool mClipped{false};
};

} // namespace conductor
