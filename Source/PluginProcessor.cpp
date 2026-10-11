#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "UI/DebugLog.h"

namespace conductor {

ConductorAudioProcessor::ConductorAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    Log("SYSTEM", "Conductor Audio Processor initializing...");

    juce::File currentExecutable = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    juce::File projectRoot = currentExecutable.getParentDirectory();
    while (projectRoot.exists() && (!projectRoot.getChildFile("Libs").exists() || projectRoot.getFileName() == "build") && projectRoot != projectRoot.getParentDirectory()) {
        projectRoot = projectRoot.getParentDirectory();
    }
    if (!projectRoot.getChildFile("Libs").getChildFile("NeuralAmpModelerCore").exists()) {
        projectRoot = juce::File("/Users/jdawg/Conductor");
    }
    std::string rootStr = projectRoot.exists() ? projectRoot.getFullPathName().toStdString() : "/Users/jdawg/Conductor";
    Log("SYSTEM", "Project root resolved to: " + rootStr);

    mLLMClient = std::make_unique<LLMClient>(mNamChain, rootStr);

    // No auto-rig on startup. User must submit a prompt to build first rig.
    Log("AI", "Conductor ready. Awaiting user prompt to build initial rig (NEW TONE mode forced).");
}

ConductorAudioProcessor::~ConductorAudioProcessor() {
    Log("SYSTEM", "Conductor Audio Processor shutting down.");
}

void ConductorAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    Log("AUDIO", "prepareToPlay: sampleRate=" + std::to_string(sampleRate) + ", bufferSize=" + std::to_string(samplesPerBlock));
    mNamChain.prepare(sampleRate, samplesPerBlock);
    mTunerDetector.prepare(sampleRate);
    mTelemetry.reset();
}

void ConductorAudioProcessor::releaseResources() {
    Log("AUDIO", "releaseResources called.");
    mTelemetry.reset();
}

bool ConductorAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto& mainIn = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainIn != juce::AudioChannelSet::mono() && mainIn != juce::AudioChannelSet::stereo())
        return false;
    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void ConductorAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/) {
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    static int noInputChannelCounter = 0;
    if (totalNumInputChannels <= 0) {
        if (noInputChannelCounter++ % 1000 == 0) {
            Log("AUDIO", "WARNING: 0 input channels available! Please click 'AUDIO SETTINGS' to select your audio interface & input channel.");
        }
        return;
    }

    if (numSamples <= 0) return;

    // Clear any extra output channels beyond totalNumInputChannels
    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i) {
        buffer.clear(i, 0, numSamples);
    }

    // Real-Time Audio Processing through NAM Series Chain (smart input routing & stereo output duplication)
    mNamChain.process(buffer.getArrayOfWritePointers(), totalNumInputChannels, totalNumOutputChannels, numSamples, mTelemetry);

    // Process guitar tuner autocorrelation on routed dry input signal
    if (!mNamChain.getDryInputBuffer().empty()) {
        mTunerDetector.process(mNamChain.getDryInputBuffer().data(), numSamples, mTelemetry);
    }

    // Periodic input signal detection logging
    static int signalLogThrottle = 0;
    float inPeak = mTelemetry.inputPeakDbfs.load(std::memory_order_relaxed);
    if (inPeak > -45.0f && (++signalLogThrottle % 600 == 0)) {
        int actChan = mTelemetry.activeInputChannel.load(std::memory_order_relaxed);
        std::string chanStr = (actChan == 0 ? "Ch 1" : (actChan == 1 ? "Ch 2" : "Ch 1+2 Sum"));
        Log("AUDIO", "Guitar signal detected on [" + chanStr + "]! Peak: " + juce::String(inPeak, 1).toStdString() + " dBFS" +
                     (mTelemetry.isPitchLocked.load() ? " | Tuner: " + TunerDetector::getNoteName(mTelemetry.detectedMidiNote.load()) : ""));
    }
}

juce::AudioProcessorEditor* ConductorAudioProcessor::createEditor() {
    return new ConductorAudioProcessorEditor(*this);
}

void ConductorAudioProcessor::getStateInformation(juce::MemoryBlock& /*destData*/) {
}

void ConductorAudioProcessor::setStateInformation(const void* /*data*/, int /*sizeInBytes*/) {
}

void ConductorAudioProcessor::triggerPrompt(const std::string& prompt, bool isEditMode, std::function<void(const RigSetupResult&)> callback) {
    if (mLLMClient) {
        mLLMClient->submitPromptAsync(prompt, isEditMode, callback);
    }
}

} // namespace conductor

// Standalone & Plugin Entry point export
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new conductor::ConductorAudioProcessor();
}
