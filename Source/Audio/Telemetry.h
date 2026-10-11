#pragma once
#include <atomic>
#include <cmath>
#include <algorithm>

namespace conductor {

struct AudioTelemetry {
    std::atomic<float> inputPeakDbfs{-100.0f};
    std::atomic<float> inputRmsDbfs{-100.0f};
    std::atomic<float> outputPeakDbfs{-100.0f};
    std::atomic<float> outputRmsDbfs{-100.0f};
    std::atomic<bool> inputClipped{false};
    std::atomic<bool> outputClipped{false};

    // Tuner
    std::atomic<float> detectedPitchHz{0.0f};
    std::atomic<float> pitchCentsOffset{0.0f};
    std::atomic<int> detectedMidiNote{0};
    std::atomic<bool> isPitchLocked{false};

    // Input Routing (0: Auto, 1: Ch 1, 2: Ch 2, 3: Sum)
    std::atomic<int> inputModeSetting{0};
    std::atomic<int> activeInputChannel{0}; // 0 = Ch 1, 1 = Ch 2, 2 = Sum

    void reset() {
        inputPeakDbfs.store(-100.0f, std::memory_order_relaxed);
        inputRmsDbfs.store(-100.0f, std::memory_order_relaxed);
        outputPeakDbfs.store(-100.0f, std::memory_order_relaxed);
        outputRmsDbfs.store(-100.0f, std::memory_order_relaxed);
        inputClipped.store(false, std::memory_order_relaxed);
        outputClipped.store(false, std::memory_order_relaxed);
        detectedPitchHz.store(0.0f, std::memory_order_relaxed);
        pitchCentsOffset.store(0.0f, std::memory_order_relaxed);
        detectedMidiNote.store(0, std::memory_order_relaxed);
        isPitchLocked.store(false, std::memory_order_relaxed);
    }
};

inline float linearToDbfs(float linear) {
    if (linear <= 0.00001f) return -100.0f;
    float db = 20.0f * std::log10(linear);
    return std::max(-100.0f, std::min(6.0f, db));
}

} // namespace conductor
