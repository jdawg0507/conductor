#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../AI/LLMClient.h"

namespace conductor {

class AIDialogOverlay : public juce::Component, public juce::Timer {
public:
    AIDialogOverlay() {
        setInterceptsMouseClicks(false, false);
        setVisible(false);
    }

    void showProcessing(const juce::String& promptText, bool isEditMode) {
        mPromptText = promptText;
        mIsEditMode = isEditMode;
        mPhase = Phase::Processing;
        mResultText = "";
        mMatchedTags.clear();
        mCandidateGear.clear();
        mAnimProgress = 0.0f;
        mPulsePhase = 0.0f;
        setVisible(true);
        startTimerHz(30);
        repaint();
    }

    void showResult(const RigSetupResult& result) {
        mPhase = Phase::Result;
        mResultText = result.rationale;
        mMatchedTags = result.matchedTags;
        mCandidateGear = result.candidateGear;
        mSummary = result.summary;
        mIsFallback = result.isFallback;
        mFallbackReason = result.fallbackReason;
        mAnimProgress = 0.0f;
        repaint();

        // Auto-dismiss after 5 seconds
        mDismissCountdown = 150; // 150 frames at 30fps = 5s
    }

    void dismiss() {
        stopTimer();
        setVisible(false);
        repaint();
    }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();

        // Semi-transparent backdrop
        g.setColour(juce::Colour(0xff0a0e14).withAlpha(0.88f));
        g.fillRoundedRectangle(bounds.reduced(24.0f, 4.0f), 16.0f);

        // Glowing border
        juce::Colour borderColour = mPhase == Phase::Processing
            ? juce::Colour(0xff00f0ff).withAlpha(0.6f)
            : juce::Colour(0xff22c55e).withAlpha(0.6f);
        g.setColour(borderColour);
        g.drawRoundedRectangle(bounds.reduced(24.0f, 4.0f), 16.0f, 1.5f);

        auto content = bounds.reduced(40.0f, 16.0f);

        if (mPhase == Phase::Processing) {
            paintProcessing(g, content);
        } else {
            paintResult(g, content);
        }
    }

    void timerCallback() override {
        mPulsePhase += 0.08f;
        if (mPulsePhase > 6.28f) mPulsePhase -= 6.28f;

        mAnimProgress = std::min(1.0f, mAnimProgress + 0.03f);

        if (mPhase == Phase::Result) {
            mDismissCountdown--;
            if (mDismissCountdown <= 0) {
                dismiss();
                return;
            }
        }

        repaint();
    }

    void resized() override {}

private:
    enum class Phase { Processing, Result };

    void paintProcessing(juce::Graphics& g, juce::Rectangle<float> area) {
        // Pulsing AI orb
        float orbX = area.getCentreX();
        float orbY = area.getY() + 30.0f;
        float pulse = 0.5f + 0.5f * std::sin(mPulsePhase);

        // Outer glow
        g.setColour(juce::Colour(0xff00f0ff).withAlpha(0.15f * pulse));
        g.fillEllipse(orbX - 28.0f, orbY - 28.0f, 56.0f, 56.0f);
        g.setColour(juce::Colour(0xff00f0ff).withAlpha(0.35f * pulse));
        g.fillEllipse(orbX - 18.0f, orbY - 18.0f, 36.0f, 36.0f);

        // Core orb
        juce::ColourGradient orbGrad(juce::Colour(0xff00f0ff).withAlpha(0.9f), orbX - 5.0f, orbY - 5.0f,
                                     juce::Colour(0xff6366f1).withAlpha(0.7f), orbX + 12.0f, orbY + 12.0f, true);
        g.setGradientFill(orbGrad);
        g.fillEllipse(orbX - 12.0f, orbY - 12.0f, 24.0f, 24.0f);

        // Ring
        g.setColour(juce::Colour(0xff00f0ff).withAlpha(0.5f));
        g.drawEllipse(orbX - 14.0f, orbY - 14.0f, 28.0f, 28.0f, 1.5f);

        // Status text
        g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
        g.setColour(juce::Colour(0xff00f0ff));
        juce::String statusLine = mIsEditMode ? "TWEAKING ACTIVE RIG..." : "SYNTHESIZING TONE...";
        g.drawText(statusLine, area.getX(), orbY + 24.0f, area.getWidth(), 18.0f, juce::Justification::centred);

        // Prompt echo
        g.setFont(juce::FontOptions(10.0f));
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.drawText("\"" + mPromptText + "\"", area.getX(), orbY + 44.0f, area.getWidth(), 16.0f, juce::Justification::centred);
    }

    void paintResult(juce::Graphics& g, juce::Rectangle<float> area) {
        float y = area.getY() + 6.0f;

        // Success indicator
        juce::Colour successColour = mIsFallback ? juce::Colour(0xfff59e0b) : juce::Colour(0xff22c55e);
        g.setColour(successColour);
        g.fillEllipse(area.getX(), y + 2.0f, 10.0f, 10.0f);

        // Summary
        g.setFont(juce::FontOptions(11.5f).withStyle("Bold"));
        g.setColour(juce::Colours::white);
        g.drawText(mSummary, area.getX() + 16.0f, y, area.getWidth() - 16.0f, 16.0f, juce::Justification::left);
        y += 20.0f;

        // Fallback warning
        if (mIsFallback && !mFallbackReason.empty()) {
            g.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
            g.setColour(juce::Colour(0xfff59e0b));
            g.drawText("FALLBACK: " + juce::String(mFallbackReason), area.getX(), y, area.getWidth(), 14.0f, juce::Justification::left);
            y += 16.0f;
        }

        // Matched tags as pills
        if (!mMatchedTags.empty()) {
            g.setFont(juce::FontOptions(8.5f).withStyle("Bold"));
            float tagX = area.getX();
            float maxWidth = area.getRight();
            for (const auto& tag : mMatchedTags) {
                juce::String tagStr(tag);
                float tagW = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), tagStr) + 12.0f;
                if (tagX + tagW > maxWidth) {
                    tagX = area.getX();
                    y += 18.0f;
                }
                // Pill background
                g.setColour(juce::Colour(0xff00f0ff).withAlpha(0.15f));
                g.fillRoundedRectangle(tagX, y, tagW, 16.0f, 8.0f);
                g.setColour(juce::Colour(0xff00f0ff).withAlpha(0.5f));
                g.drawRoundedRectangle(tagX, y, tagW, 16.0f, 8.0f, 0.8f);
                g.setColour(juce::Colour(0xff00f0ff));
                g.drawText(tagStr, tagX + 6.0f, y, tagW - 12.0f, 16.0f, juce::Justification::centred);
                tagX += tagW + 5.0f;
            }
            y += 22.0f;
        }

        // Rationale
        if (!mResultText.empty()) {
            g.setFont(juce::FontOptions(9.5f));
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            juce::Rectangle<float> rationaleArea(area.getX(), y, area.getWidth(), area.getBottom() - y - 4.0f);
            g.drawFittedText(juce::String(mResultText), rationaleArea.toNearestInt(), juce::Justification::topLeft, 5);
        }
    }

    Phase mPhase{Phase::Processing};
    juce::String mPromptText;
    bool mIsEditMode{false};
    std::string mResultText;
    std::string mSummary;
    std::vector<std::string> mMatchedTags;
    std::vector<std::string> mCandidateGear;
    bool mIsFallback{false};
    std::string mFallbackReason;
    float mAnimProgress{0.0f};
    float mPulsePhase{0.0f};
    int mDismissCountdown{0};
};

} // namespace conductor
