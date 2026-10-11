#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include "PluginProcessor.h"
#include "UI/ConductorLookAndFeel.h"
#include "UI/TunerComponent.h"
#include "UI/LevelMeterComponent.h"
#include "UI/PedalCardComponent.h"
#include "UI/DebugConsoleComponent.h"
#include "UI/AIDialogOverlay.h"

namespace conductor {

class ConductorAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      public juce::Timer
{
public:
    explicit ConductorAudioProcessorEditor(ConductorAudioProcessor&);
    ~ConductorAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    void submitPrompt();
    void openAudioSettings();

    ConductorAudioProcessor& mProcessor;
    ConductorLookAndFeel mLookAndFeel;

    // Header Controls
    juce::TextEditor mPromptEditor;
    juce::TextButton mConductButton;
    juce::TextButton mToneModeButton;  // NEW TONE / EDIT TONE toggle
    juce::TextButton mInputChannelButton;
    juce::TextButton mAudioSettingsButton;
    juce::TextButton mConsoleToggleButton;
    juce::Label mStatusLabel;

    bool mHasActiveRig{false};  // False on startup, true after first successful rig
    bool mIsEditMode{false};    // Toggleable after first rig is set

    void updateInputChannelButton();

    // Audio Telemetry
    TunerComponent mTunerComponent;
    LevelMeterComponent mInputMeter;
    LevelMeterComponent mOutputMeter;

    // Gear Rack
    std::array<std::unique_ptr<PedalCardComponent>, NamChain::NUM_SLOTS> mSlotCards;

    // Live Debug Console
    DebugConsoleComponent mDebugConsole;
    bool mShowConsole{true};

    // Siri-style AI Dialog Overlay
    AIDialogOverlay mAIDialog;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConductorAudioProcessorEditor)
};

} // namespace conductor
