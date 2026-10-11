#pragma once

#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <filesystem>
#include <cmath>
#include <algorithm>

#include "NAM/dsp.h"
#include "NAM/get_dsp.h"
#include "NAM/model_config.h"
#include "NAM/wavenet/model.h"
#include "NAM/lstm.h"
#include "NAM/linear.h"
#include "NAM/convnet.h"
#include "NAM/container.h"
#include "Telemetry.h"
#include "../UI/DebugLog.h"
#include <juce_dsp/juce_dsp.h>

namespace conductor {

inline void ensureNamRegistrations() {
    auto& reg = nam::ConfigParserRegistry::instance();
    if (!reg.has("WaveNet")) {
        reg.registerParser("WaveNet", nam::wavenet::create_config);
    }
    if (!reg.has("LSTM")) {
        reg.registerParser("LSTM", nam::lstm::create_config);
    }
    if (!reg.has("Linear")) {
        reg.registerParser("Linear", nam::linear::create_config);
    }
    if (!reg.has("ConvNet")) {
        reg.registerParser("ConvNet", nam::convnet::create_config);
    }
    if (!reg.has("SlimmableContainer")) {
        reg.registerParser("SlimmableContainer", nam::container::create_config);
    }
    Log("NAM", "Ensured NAM architecture parsers: WaveNet, LSTM, Linear, ConvNet, SlimmableContainer registered.");
}

enum class GearType {
    Stompbox,
    Amp,
    Space
};

struct GearSlotConfig {
    int slotIndex{0};
    GearType type{GearType::Stompbox};
    std::string name{"Empty"};
    std::string modelPath{""};
    std::atomic<bool> isBypassed{false};
    std::atomic<float> gainDb{0.0f};
    std::atomic<float> outputLevelDb{0.0f};
    std::atomic<float> mix{1.0f}; // 1.0 = 100% wet
    std::atomic<float> trebleDb{0.0f};
    std::atomic<float> midDb{0.0f};
    std::atomic<float> bassDb{0.0f};
};

class NamChain {
public:
    static constexpr int NUM_SLOTS = 4;
    static constexpr int CROSSFADE_SAMPLES = 512;

    NamChain() {
        ensureNamRegistrations();
        for (int i = 0; i < NUM_SLOTS; ++i) {
            auto cfg = std::make_unique<GearSlotConfig>();
            cfg->slotIndex = i;
            if (i == 0 || i == 1) cfg->type = GearType::Stompbox;
            else if (i == 2) cfg->type = GearType::Amp;
            else cfg->type = GearType::Space;
            mSlots[i] = std::move(cfg);
            mActiveDsps[i] = nullptr;
            mPendingDsps[i] = nullptr;
        }
    }

    void prepare(double sampleRate, int maxBlockSize) {
        mSampleRate = sampleRate;
        mMaxBlockSize = std::max(512, maxBlockSize);
        mChannelBuffer.assign(mMaxBlockSize, 0.0f);
        mSlotScratch.assign(mMaxBlockSize, 0.0f);
        mCrossfadeBuffer.assign(mMaxBlockSize, 0.0f);
        mDryInputBuffer.assign(mMaxBlockSize, 0.0f);

        for (int i = 0; i < NUM_SLOTS; ++i) {
            if (mActiveDsps[i]) {
                mActiveDsps[i]->Reset(mSampleRate, std::max(4096, mMaxBlockSize));
            }
            if (mPendingDsps[i]) {
                mPendingDsps[i]->Reset(mSampleRate, std::max(4096, mMaxBlockSize));
            }
        }

        mSpaceReverb.reset();
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32>(mMaxBlockSize);
        spec.numChannels = 2;
        mSpaceReverb.prepare(spec);
    }

    const std::vector<float>& getDryInputBuffer() const { return mDryInputBuffer; }

    // Called on Audio Thread: Real-Time, Zero Allocation, Hard Deadlines
    void process(float* const* channels, int numInputChannels, int numOutputChannels, int numSamples, AudioTelemetry& telemetry) {
        if (numSamples <= 0 || numInputChannels <= 0 || numOutputChannels <= 0) return;

        // Ensure buffer sizing
        if (static_cast<int>(mDryInputBuffer.size()) < numSamples) {
            mDryInputBuffer.resize(numSamples, 0.0f);
            mSlotScratch.resize(numSamples, 0.0f);
            mChannelBuffer.resize(numSamples, 0.0f);
        }

        // 1. Stable, glitch-free input signal routing
        int mode = telemetry.inputModeSetting.load(std::memory_order_relaxed);
        int selectedChannel = 0;

        float ch0Peak = 0.0f;
        for (int i = 0; i < numSamples; ++i) {
            float a = std::fabs(channels[0][i]);
            if (a > ch0Peak) ch0Peak = a;
        }

        float ch1Peak = 0.0f;
        if (numInputChannels > 1) {
            for (int i = 0; i < numSamples; ++i) {
                float a = std::fabs(channels[1][i]);
                if (a > ch1Peak) ch1Peak = a;
            }
        }

        if (mode == 1) { // Forced Ch 1
            selectedChannel = 0;
            mAutoLatchedChannel = 0;
            mCh1DetectCount = 0;
        } else if (mode == 2 && numInputChannels > 1) { // Forced Ch 2
            selectedChannel = 1;
            mAutoLatchedChannel = 1;
            mCh1DetectCount = 0;
        } else if (mode == 3 && numInputChannels > 1) { // Forced Sum
            selectedChannel = 2;
            mAutoLatchedChannel = 2;
            mCh1DetectCount = 0;
        } else {
            // Mode 0 = Auto:
            // Sticky channel selection with hysteresis to eliminate block-by-block switching static
            if (numInputChannels > 1 && ch1Peak > 0.02f && ch1Peak > ch0Peak * 3.0f) {
                if (++mCh1DetectCount > 15) { // Must be consistently louder on Ch 2 for 15+ blocks
                    mAutoLatchedChannel = 1;
                }
            } else if (ch0Peak > 0.015f && ch0Peak > ch1Peak * 1.8f) {
                mCh1DetectCount = 0;
                mAutoLatchedChannel = 0;
            }
            selectedChannel = mAutoLatchedChannel;
        }
        telemetry.activeInputChannel.store(selectedChannel, std::memory_order_relaxed);

        if (selectedChannel == 1 && numInputChannels > 1) {
            for (int i = 0; i < numSamples; ++i) mDryInputBuffer[i] = channels[1][i];
        } else if (selectedChannel == 2 && numInputChannels > 1) {
            for (int i = 0; i < numSamples; ++i) mDryInputBuffer[i] = 0.5f * (channels[0][i] + channels[1][i]);
        } else {
            for (int i = 0; i < numSamples; ++i) mDryInputBuffer[i] = channels[0][i];
        }

        // 2. DC Blocker Filter (~15 Hz high-pass): y[n] = x[n] - x[n-1] + R * y[n-1]
        constexpr float dcR = 0.997f;
        for (int i = 0; i < numSamples; ++i) {
            float x = mDryInputBuffer[i];
            float y = x - mDcPrevX + dcR * mDcPrevY;
            mDcPrevX = x;
            mDcPrevY = y;
            mDryInputBuffer[i] = y;
        }

        // 3. Measure raw input telemetry before gate
        float inPeak = 0.0f;
        float inSumSq = 0.0f;
        for (int i = 0; i < numSamples; ++i) {
            float s = mDryInputBuffer[i];
            float absS = std::fabs(s);
            if (absS > inPeak) inPeak = absS;
            inSumSq += s * s;
        }
        float inRms = std::sqrt(inSumSq / numSamples);
        telemetry.inputPeakDbfs.store(linearToDbfs(inPeak), std::memory_order_relaxed);
        telemetry.inputRmsDbfs.store(linearToDbfs(inRms), std::memory_order_relaxed);
        if (inPeak >= 0.999f) telemetry.inputClipped.store(true, std::memory_order_relaxed);

        // 4. Studio Noise Gate (Downward Expander)
        // Silences preamp noise & pickup hum when guitar is idle
        constexpr float gateThreshold = 0.0018f; // ~ -55 dBFS
        float targetGain = (inRms > gateThreshold || inPeak > gateThreshold * 2.2f) ? 1.0f : 0.0f;
        constexpr float attackCoeff = 0.90f;   // ~1ms fast open
        constexpr float releaseCoeff = 0.9988f; // ~60ms smooth natural close

        for (int i = 0; i < numSamples; ++i) {
            if (targetGain > mGateGain) {
                mGateGain = attackCoeff * mGateGain + (1.0f - attackCoeff) * targetGain;
            } else {
                mGateGain = releaseCoeff * mGateGain + (1.0f - releaseCoeff) * targetGain;
            }
            float effectiveGate = 0.015f + 0.985f * mGateGain;
            mDryInputBuffer[i] *= effectiveGate;
        }

        // 5. Stage clean dry signal into channels[0]
        std::copy(mDryInputBuffer.begin(), mDryInputBuffer.begin() + numSamples, channels[0]);

        // 6. Process through active slots (0: Pedal 1, 1: Pedal 2, 2: Amp)
        for (int s = 0; s < 3; ++s) {
            checkAndApplyPendingModel(s);

            if (mSlots[s]->isBypassed.load(std::memory_order_relaxed) || !mActiveDsps[s]) {
                continue;
            }

            float gainLinear = std::pow(10.0f, mSlots[s]->gainDb.load(std::memory_order_relaxed) / 20.0f);
            float mixVal = mSlots[s]->mix.load(std::memory_order_relaxed);

            // Apply gain pre-DSP
            for (int i = 0; i < numSamples; ++i) {
                mSlotScratch[i] = channels[0][i] * gainLinear;
            }

            float* inPtr = mSlotScratch.data();
            float* outPtr = mChannelBuffer.data();

            mActiveDsps[s]->process(&inPtr, &outPtr, numSamples);

            // Mix back
            float outGainLinear = std::pow(10.0f, mSlots[s]->outputLevelDb.load(std::memory_order_relaxed) / 20.0f);
            for (int i = 0; i < numSamples; ++i) {
                float wet = mChannelBuffer[i] * outGainLinear;
                channels[0][i] = (channels[0][i] * (1.0f - mixVal)) + (wet * mixVal);
            }
        }

        // 7. Duplicate mono guitar tone across all output channels (Left & Right)
        for (int ch = 1; ch < numOutputChannels; ++ch) {
            std::copy(channels[0], channels[0] + numSamples, channels[ch]);
        }

        // 8. Process Slot 3 (Space & Ambience) with cached reverb parameters (no zipper noise)
        checkAndApplyPendingModel(3);
        if (!mSlots[3]->isBypassed.load(std::memory_order_relaxed) && mSlots[3]->name != "Empty") {
            std::string sName = mSlots[3]->name;
            float spaceMix = mSlots[3]->mix.load(std::memory_order_relaxed);

            // ONLY update parameters if settings actually changed (eliminates filter reset static)
            if (sName != mLastReverbName || std::fabs(spaceMix - mLastReverbMix) > 0.005f) {
                mLastReverbName = sName;
                mLastReverbMix = spaceMix;

                juce::dsp::Reverb::Parameters rParams;
                if (sName.find("Room") != std::string::npos) {
                    rParams.roomSize = 0.38f;
                    rParams.damping = 0.60f;
                } else if (sName.find("Plate") != std::string::npos || sName.find("Lexicon") != std::string::npos) {
                    rParams.roomSize = 0.70f;
                    rParams.damping = 0.32f;
                } else if (sName.find("Spring") != std::string::npos) {
                    rParams.roomSize = 0.45f;
                    rParams.damping = 0.20f;
                } else {
                    rParams.roomSize = 0.35f;
                    rParams.damping = 0.60f;
                }
                rParams.wetLevel = spaceMix * 0.40f;
                rParams.dryLevel = 1.0f;
                rParams.width = 1.0f;
                rParams.freezeMode = 0.0f;
                mSpaceReverb.setParameters(rParams);
            }

            if (numOutputChannels >= 2) {
                juce::dsp::AudioBlock<float> block(channels, 2, numSamples);
                juce::dsp::ProcessContextReplacing<float> context(block);
                mSpaceReverb.process(context);
            }
        }

        // 9. Master Output Stage: Clean transparent soft limiter only at ceiling (>0.98)
        for (int ch = 0; ch < numOutputChannels; ++ch) {
            for (int i = 0; i < numSamples; ++i) {
                float s = channels[ch][i];
                if (s > 0.98f) s = 0.98f + 0.02f * std::tanh((s - 0.98f) / 0.02f);
                else if (s < -0.98f) s = -0.98f + 0.02f * std::tanh((s + 0.98f) / 0.02f);
                channels[ch][i] = s;
            }
        }

        // 10. Measure output telemetry across all output channels
        float outPeak = 0.0f;
        float outSumSq = 0.0f;
        for (int ch = 0; ch < numOutputChannels; ++ch) {
            for (int i = 0; i < numSamples; ++i) {
                float s = channels[ch][i];
                float absS = std::fabs(s);
                if (absS > outPeak) outPeak = absS;
                outSumSq += s * s;
            }
        }
        telemetry.outputPeakDbfs.store(linearToDbfs(outPeak), std::memory_order_relaxed);
        telemetry.outputRmsDbfs.store(linearToDbfs(std::sqrt(outSumSq / (numSamples * numOutputChannels))), std::memory_order_relaxed);
        if (outPeak >= 0.999f) telemetry.outputClipped.store(true, std::memory_order_relaxed);
    }

    // Called on Background Worker Thread: Safe for disk I/O & allocation
    bool stageModel(int slotIndex, const std::string& modelPath, const std::string& displayName) {
        if (slotIndex < 0 || slotIndex >= NUM_SLOTS) return false;

        try {
            if (!std::filesystem::exists(modelPath)) {
                Log("NAM", "Model file does not exist on disk: " + modelPath);
                return false;
            }

            auto dsp = nam::get_dsp(std::filesystem::path(modelPath));
            if (!dsp) {
                Log("NAM", "Failed to instantiate NAM DSP from: " + modelPath);
                return false;
            }

            // Reset DSP unit with current sample rate & buffer size (allocates matrices & prewarms)
            dsp->Reset(mSampleRate, std::max(4096, mMaxBlockSize));

            {
                std::lock_guard<std::mutex> lock(mSwapMutex);
                mPendingDsps[slotIndex] = std::move(dsp);
                mSlots[slotIndex]->name = displayName;
                mSlots[slotIndex]->modelPath = modelPath;
                mHasPendingSwap[slotIndex].store(true, std::memory_order_release);
            }
            Log("NAM", "Staged model into slot " + std::to_string(slotIndex) + " (" + displayName + ")");
            return true;
        } catch (const std::exception& e) {
            Log("NAM", "Exception while loading model [" + modelPath + "]: " + e.what());
            return false;
        } catch (...) {
            Log("NAM", "Unknown exception loading model [" + modelPath + "]");
            return false;
        }
    }

    GearSlotConfig& getSlotConfig(int slotIndex) {
        return *mSlots[std::max(0, std::min(NUM_SLOTS - 1, slotIndex))];
    }

    void clearSlot(int slotIndex) {
        if (slotIndex < 0 || slotIndex >= NUM_SLOTS) return;
        std::lock_guard<std::mutex> lock(mSwapMutex);
        mActiveDsps[slotIndex] = nullptr;
        mPendingDsps[slotIndex] = nullptr;
        mSlots[slotIndex]->name = "Empty";
        mSlots[slotIndex]->modelPath = "";
    }

private:
    void checkAndApplyPendingModel(int slotIndex) {
        if (mHasPendingSwap[slotIndex].load(std::memory_order_acquire)) {
            std::lock_guard<std::mutex> lock(mSwapMutex);
            if (mPendingDsps[slotIndex]) {
                mActiveDsps[slotIndex] = std::move(mPendingDsps[slotIndex]);
                mPendingDsps[slotIndex] = nullptr;
            }
            mHasPendingSwap[slotIndex].store(false, std::memory_order_release);
        }
    }

    double mSampleRate{48000.0};
    int mMaxBlockSize{512};
    std::array<std::unique_ptr<GearSlotConfig>, NUM_SLOTS> mSlots;
    std::array<std::unique_ptr<nam::DSP>, NUM_SLOTS> mActiveDsps;
    std::array<std::unique_ptr<nam::DSP>, NUM_SLOTS> mPendingDsps;
    std::array<std::atomic<bool>, NUM_SLOTS> mHasPendingSwap{false, false, false, false};

    std::mutex mSwapMutex;
    std::vector<float> mChannelBuffer;
    std::vector<float> mSlotScratch;
    std::vector<float> mCrossfadeBuffer;
    std::vector<float> mDryInputBuffer;
    juce::dsp::Reverb mSpaceReverb;

    int mAutoLatchedChannel{0};
    int mCh1DetectCount{0};
    float mDcPrevX{0.0f};
    float mDcPrevY{0.0f};
    float mGateGain{1.0f};
    std::string mLastReverbName{""};
    float mLastReverbMix{-1.0f};
};

} // namespace conductor
