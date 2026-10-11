#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include "Audio/NamChain.h"
#include "Audio/TunerDetector.h"
#include "Audio/Telemetry.h"
#include "AI/LLMClient.h"

namespace conductor {

class ConductorAudioProcessor : public juce::AudioProcessor {
public:
    ConductorAudioProcessor();
    ~ConductorAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Conductor"; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Conductor Engine Accessors
    NamChain& getNamChain() { return mNamChain; }
    AudioTelemetry& getTelemetry() { return mTelemetry; }
    LLMClient& getLLMClient() { return *mLLMClient; }

    void triggerPrompt(const std::string& prompt, bool isEditMode, std::function<void(const RigSetupResult&)> callback);

private:
    NamChain mNamChain;
    TunerDetector mTunerDetector;
    AudioTelemetry mTelemetry;
    std::unique_ptr<LLMClient> mLLMClient;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConductorAudioProcessor)
};

} // namespace conductor
