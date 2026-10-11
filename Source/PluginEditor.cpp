#include "PluginEditor.h"
#include "UI/DebugLog.h"
#include "UI/StandaloneAudioHelper.h"

namespace conductor {

ConductorAudioProcessorEditor::ConductorAudioProcessorEditor(ConductorAudioProcessor& p)
    : AudioProcessorEditor(&p),
      mProcessor(p),
      mTunerComponent(p.getTelemetry()),
      mInputMeter(p.getTelemetry(), true),
      mOutputMeter(p.getTelemetry(), false)
{
    setLookAndFeel(&mLookAndFeel);

    // Ensure audio input is unmuted in JUCE Standalone mode
    StandaloneAudioHelper::initializeStandaloneInput();

    // Cybernetic Prompt Editor
    mPromptEditor.setTextToShowWhenEmpty("Describe any tone (e.g. '80s stadium metal with tight delay' or 'glassy funk clean')...", juce::Colour(0xff475569));
    mPromptEditor.setReturnKeyStartsNewLine(false);
    mPromptEditor.setFont(juce::FontOptions(12.5f));
    mPromptEditor.onReturnKey = [this]() { submitPrompt(); };
    addAndMakeVisible(mPromptEditor);

    // Conduct Tone Button with Neon Glow
    mConductButton.setButtonText("CONDUCT TONE");
    mConductButton.onClick = [this]() { submitPrompt(); };
    addAndMakeVisible(mConductButton);

    // NEW / EDIT Tone Mode Toggle
    mToneModeButton.setButtonText("NEW TONE");
    mToneModeButton.onClick = [this]() {
        if (!mHasActiveRig) {
            Log("UI", "Cannot switch to Edit Mode: no active rig loaded yet.");
            return;
        }
        mIsEditMode = !mIsEditMode;
        mToneModeButton.setButtonText(mIsEditMode ? "EDIT TONE" : "NEW TONE");
        mPromptEditor.setTextToShowWhenEmpty(
            mIsEditMode ? "Tweak active tone (e.g. 'more gain', 'swap pedal for fuzz', 'less reverb')..."
                        : "Describe any tone (e.g. '80s stadium metal with tight delay')...",
            juce::Colour(0xff475569));
        Log("UI", "Tone mode switched to: " + std::string(mIsEditMode ? "EDIT TONE" : "NEW TONE"));
    };
    addAndMakeVisible(mToneModeButton);

    // Input Routing Button (Auto / Ch 1 / Ch 2 / Sum)
    mInputChannelButton.setButtonText("IN: AUTO");
    mInputChannelButton.onClick = [this]() {
        auto& tel = mProcessor.getTelemetry();
        int cur = tel.inputModeSetting.load(std::memory_order_relaxed);
        int nxt = (cur + 1) % 4;
        tel.inputModeSetting.store(nxt, std::memory_order_relaxed);
        updateInputChannelButton();
        static const char* modeNames[] = { "AUTO (smart detect)", "CH 1 (Input 1)", "CH 2 (Input 2)", "CH 1+2 SUM" };
        Log("UI", "Guitar Input switched to: " + std::string(modeNames[nxt]));
    };
    addAndMakeVisible(mInputChannelButton);

    // Audio Settings Button
    mAudioSettingsButton.setButtonText("AUDIO SETUP");
    mAudioSettingsButton.onClick = [this]() { openAudioSettings(); };
    addAndMakeVisible(mAudioSettingsButton);

    // Console Toggle Button
    mConsoleToggleButton.setButtonText("CONSOLE");
    mConsoleToggleButton.onClick = [this]() {
        mShowConsole = !mShowConsole;
        mDebugConsole.setVisible(mShowConsole);
        setSize(getWidth(), mShowConsole ? 680 : 500);
        resized();
    };
    addAndMakeVisible(mConsoleToggleButton);

    // Status Label HUD
    mStatusLabel.setText("Ready: Describe a tone to begin // Tone3000 + Nebius AI", juce::dontSendNotification);
    mStatusLabel.setFont(juce::FontOptions(10.5f).withStyle("Bold"));
    mStatusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff38bdf8));
    addAndMakeVisible(mStatusLabel);

    // Telemetry & Pitch
    addAndMakeVisible(mTunerComponent);
    addAndMakeVisible(mInputMeter);
    addAndMakeVisible(mOutputMeter);

    // Gear Slot Cards
    auto& chain = mProcessor.getNamChain();
    for (int i = 0; i < NamChain::NUM_SLOTS; ++i) {
        mSlotCards[i] = std::make_unique<PedalCardComponent>(chain.getSlotConfig(i));
        addAndMakeVisible(*mSlotCards[i]);
    }

    // Live Debug Console
    addAndMakeVisible(mDebugConsole);

    // AI Dialog Overlay (drawn on top of gear rack)
    addAndMakeVisible(mAIDialog);

    setSize(960, 680);
    startTimerHz(30); // 30 FPS ultra-smooth metering & tuner

    Log("UI", "Conductor GUI initialized. Size: 960x680");
}

ConductorAudioProcessorEditor::~ConductorAudioProcessorEditor() {
    setLookAndFeel(nullptr);
    stopTimer();
}

void ConductorAudioProcessorEditor::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();

    // 1. Deep Aerospace Titanium Background
    juce::ColourGradient bgGrad(juce::Colour(0xff11151c), 0, 0,
                                juce::Colour(0xff080a0e), 0, bounds.getBottom(), false);
    g.setGradientFill(bgGrad);
    g.fillRect(bounds);

    // Subtle brushed metallic horizontal lines
    g.setColour(juce::Colours::white.withAlpha(0.015f));
    for (int y = 0; y < getHeight(); y += 4) {
        g.drawHorizontalLine(y, 0.0f, static_cast<float>(getWidth()));
    }

    // 2. Top Header OLED Console Plate
    auto headerArea = bounds.removeFromTop(68.0f);
    juce::ColourGradient headerGrad(juce::Colour(0xff181e28), 0, 0,
                                    juce::Colour(0xff0d1117), 0, 68.0f, false);
    g.setGradientFill(headerGrad);
    g.fillRect(headerArea);

    g.setColour(juce::Colour(0xff212936));
    g.drawHorizontalLine(68, 0.0f, static_cast<float>(getWidth()));

    // Brand Title & Hologram Logo
    float logoX = 18.0f;
    float logoY = 22.0f;

    // Glowing Cyan Dot Indicator
    g.setColour(juce::Colour(0xff00f0ff).withAlpha(0.35f));
    g.fillEllipse(logoX - 2.0f, logoY - 2.0f, 16.0f, 16.0f);
    g.setColour(juce::Colour(0xff00f0ff));
    g.fillEllipse(logoX + 2.0f, logoY + 2.0f, 8.0f, 8.0f);

    g.setFont(juce::FontOptions(18.0f).withStyle("Bold"));
    g.setColour(juce::Colours::white);
    g.drawText("CONDUCTOR", logoX + 20.0f, 14.0f, 130.0f, 22.0f, juce::Justification::left);

    g.setFont(juce::FontOptions(8.5f).withStyle("Bold"));
    g.setColour(juce::Colour(0xff00f0ff));
    g.drawText("NEURAL GUITAR INTELLIGENCE", logoX + 20.0f, 36.0f, 160.0f, 14.0f, juce::Justification::left);

    // 3. Glowing Audio Signal Flow Patch Traces (Connecting Modules)
    float inPeak = mProcessor.getTelemetry().inputPeakDbfs.load(std::memory_order_relaxed);
    bool signalActive = inPeak > -50.0f;

    juce::Colour traceGlow = signalActive ? juce::Colour(0xff00f0ff) : juce::Colour(0xff334155);
    float cableY = 160.0f;

    // Draw horizontal trace bar behind gear slots
    if (mSlotCards[0]) {
        float startX = mInputMeter.getRight() + 4.0f;
        float endX = mOutputMeter.getX() - 4.0f;

        g.setColour(traceGlow.withAlpha(signalActive ? 0.35f : 0.15f));
        g.drawLine(startX, cableY, endX, cableY, 4.0f);

        g.setColour(traceGlow.withAlpha(signalActive ? 0.85f : 0.40f));
        g.drawLine(startX, cableY, endX, cableY, 1.5f);

        // Jack Nodes at each module intersection
        for (int i = 0; i < NamChain::NUM_SLOTS; ++i) {
            float nodeX = mSlotCards[i]->getX() - 5.0f;
            g.setColour(juce::Colour(0xff090c10));
            g.fillEllipse(nodeX - 5.0f, cableY - 5.0f, 10.0f, 10.0f);
            g.setColour(traceGlow);
            g.drawEllipse(nodeX - 5.0f, cableY - 5.0f, 10.0f, 10.0f, 1.5f);
        }
    }
}

void ConductorAudioProcessorEditor::resized() {
    auto bounds = getLocalBounds();

    // 1. Top Header Bar
    auto topBar = bounds.removeFromTop(68);
    int rightX = getWidth() - 12;

    rightX -= 80;
    mConsoleToggleButton.setBounds(rightX, 18, 76, 32);

    rightX -= 104;
    mAudioSettingsButton.setBounds(rightX, 18, 98, 32);

    rightX -= 112;
    mInputChannelButton.setBounds(rightX, 18, 106, 32);

    rightX -= 130;
    mConductButton.setBounds(rightX, 18, 124, 32);

    rightX -= 96;
    mToneModeButton.setBounds(rightX, 18, 90, 32);

    int promptX = 205;
    int promptW = rightX - promptX - 12;
    mPromptEditor.setBounds(promptX, 18, std::max(140, promptW), 32);

    // 2. Live Debug Console at bottom
    if (mShowConsole) {
        auto consoleArea = bounds.removeFromBottom(180).reduced(12, 4);
        mDebugConsole.setBounds(consoleArea);
    }

    // 3. Status HUD Bar above console
    auto bottomBar = bounds.removeFromBottom(26).reduced(14, 2);
    mStatusLabel.setBounds(bottomBar);

    // 4. Left Telemetry Column (Input Meter & Precision Tuner)
    auto leftArea = bounds.removeFromLeft(145).reduced(10, 8);
    mInputMeter.setBounds(leftArea.removeFromTop(88));
    leftArea.removeFromTop(8);
    mTunerComponent.setBounds(leftArea);

    // 5. Right Telemetry Column (Output Meter)
    auto rightArea = bounds.removeFromRight(145).reduced(10, 8);
    mOutputMeter.setBounds(rightArea.removeFromTop(88));

    // 6. Center Gear Rack (4 Hardware Slots)
    auto centerArea = bounds.reduced(6, 8);
    int slotW = (centerArea.getWidth() - 24) / 4;
    for (int i = 0; i < NamChain::NUM_SLOTS; ++i) {
        mSlotCards[i]->setBounds(centerArea.getX() + i * (slotW + 8), centerArea.getY(), slotW, centerArea.getHeight());
    }

    // 7. AI Dialog Overlay (positioned over the gear rack, slides down from header)
    mAIDialog.setBounds(centerArea.getX() - 10, 68, centerArea.getWidth() + 20, 160);
}

void ConductorAudioProcessorEditor::timerCallback() {
    mInputMeter.update();
    mOutputMeter.update();
    mTunerComponent.update();
    updateInputChannelButton();
}

void ConductorAudioProcessorEditor::updateInputChannelButton() {
    auto& tel = mProcessor.getTelemetry();
    int mode = tel.inputModeSetting.load(std::memory_order_relaxed);
    if (mode == 0) {
        int act = tel.activeInputChannel.load(std::memory_order_relaxed);
        if (act == 1) {
            mInputChannelButton.setButtonText("IN: AUTO [2]");
        } else if (act == 2) {
            mInputChannelButton.setButtonText("IN: AUTO [1+2]");
        } else {
            mInputChannelButton.setButtonText("IN: AUTO [1]");
        }
    } else if (mode == 1) {
        mInputChannelButton.setButtonText("IN: CH 1");
    } else if (mode == 2) {
        mInputChannelButton.setButtonText("IN: CH 2");
    } else {
        mInputChannelButton.setButtonText("IN: SUM");
    }
}

void ConductorAudioProcessorEditor::submitPrompt() {
    juce::String promptText = mPromptEditor.getText();
    if (promptText.isEmpty()) return;

    bool editMode = mIsEditMode && mHasActiveRig;

    mConductButton.setEnabled(false);
    mConductButton.setButtonText(editMode ? "TWEAKING..." : "CURATING...");
    mStatusLabel.setText(
        (editMode ? "AI Tweaking Active Rig: \"" : "AI Tone Architect Synthesizing: \"") + promptText + "\"...",
        juce::dontSendNotification);

    Log("UI", "Submitting prompt (" + std::string(editMode ? "EDIT" : "NEW") + "): \"" + promptText.toStdString() + "\"");

    // Show the Siri-style AI dialog overlay
    mAIDialog.showProcessing(promptText, editMode);

    mProcessor.triggerPrompt(promptText.toStdString(), editMode, [this](const RigSetupResult& res) {
        juce::MessageManager::callAsync([this, res]() {
            mConductButton.setEnabled(true);
            mConductButton.setButtonText("CONDUCT TONE");

            if (res.success) {
                mHasActiveRig = true;
                // After first rig, default subsequent prompts to edit mode
                if (!mIsEditMode) {
                    mIsEditMode = true;
                    mToneModeButton.setButtonText("EDIT TONE");
                    mPromptEditor.setTextToShowWhenEmpty(
                        "Tweak active tone (e.g. 'more gain', 'swap pedal for fuzz', 'less reverb')...",
                        juce::Colour(0xff475569));
                }
            }

            for (int i = 0; i < NamChain::NUM_SLOTS; ++i) {
                if (mSlotCards[i]) {
                    mSlotCards[i]->refreshFromConfig();
                }
            }

            // Show result in the AI dialog overlay
            mAIDialog.showResult(res);

            std::string labelText = res.summary;
            if (res.isFallback && !res.fallbackReason.empty()) {
                labelText += "  [FALLBACK: " + res.fallbackReason + "]";
            }
            if (!res.rationale.empty()) {
                labelText += "  //  " + res.rationale;
            }
            mStatusLabel.setText(juce::String(labelText), juce::dontSendNotification);
            Log("UI", "Rig updated: " + res.summary + (res.isFallback ? " [Fallback: " + res.fallbackReason + "]" : ""));
            mPromptEditor.clear();
            repaint();
        });
    });
}

void ConductorAudioProcessorEditor::openAudioSettings() {
    Log("UI", "Audio Settings button clicked.");
    StandaloneAudioHelper::openAudioSettingsDialog();
}

} // namespace conductor
