#pragma once
#include <vector>
#include <cmath>
#include <string>
#include <array>
#include "Telemetry.h"

namespace conductor {

class TunerDetector {
public:
    TunerDetector() = default;

    void prepare(double sampleRate) {
        mSampleRate = sampleRate;
        mBuffer.assign(static_cast<size_t>(sampleRate * 0.05), 0.0f); // ~50ms buffer
        mWritePos = 0;
    }

    void process(const float* channelData, int numSamples, AudioTelemetry& telemetry) {
        if (numSamples <= 0 || mBuffer.empty()) return;

        float maxSample = 0.0f;
        for (int i = 0; i < numSamples; ++i) {
            float s = channelData[i];
            float absS = std::fabs(s);
            if (absS > maxSample) maxSample = absS;

            mBuffer[mWritePos] = s;
            mWritePos = (mWritePos + 1) % mBuffer.size();
        }

        // Only detect pitch if signal is sufficiently above noise floor
        if (maxSample < 0.015f) {
            telemetry.isPitchLocked.store(false, std::memory_order_relaxed);
            return;
        }

        detectPitch(telemetry);
    }

    static std::string getNoteName(int midiNote) {
        static const std::array<const char*, 12> noteNames = {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };
        if (midiNote < 0 || midiNote > 127) return "--";
        int noteIndex = midiNote % 12;
        int octave = (midiNote / 12) - 1;
        return std::string(noteNames[noteIndex]) + std::to_string(octave);
    }

private:
    void detectPitch(AudioTelemetry& telemetry) {
        const int n = static_cast<int>(mBuffer.size());
        const int minLag = static_cast<int>(mSampleRate / 1000.0); // max ~1000Hz
        const int maxLag = static_cast<int>(mSampleRate / 70.0);   // min ~70Hz (Low E is ~82.4Hz)

        if (maxLag >= n) return;

        // Unroll circular buffer into contiguous array
        std::vector<float> linear(n);
        for (int i = 0; i < n; ++i) {
            linear[i] = mBuffer[(mWritePos + i) % n];
        }

        // Simplified Autocorrelation
        float bestCorr = 0.0f;
        int bestLag = -1;

        for (int lag = minLag; lag <= maxLag; ++lag) {
            float sum = 0.0f;
            float norm1 = 0.0f;
            float norm2 = 0.0f;
            for (int i = 0; i < n - lag; ++i) {
                sum += linear[i] * linear[i + lag];
                norm1 += linear[i] * linear[i];
                norm2 += linear[i + lag] * linear[i + lag];
            }
            float norm = std::sqrt(norm1 * norm2);
            if (norm > 0.0001f) {
                float r = sum / norm;
                if (r > bestCorr && r > 0.65f) {
                    bestCorr = r;
                    bestLag = lag;
                }
            }
        }

        if (bestLag > 0 && bestCorr > 0.70f) {
            float freq = static_cast<float>(mSampleRate) / static_cast<float>(bestLag);
            if (freq >= 70.0f && freq <= 1100.0f) {
                float midiExact = 69.0f + 12.0f * std::log2(freq / 440.0f);
                int nearestMidi = static_cast<int>(std::round(midiExact));
                float cents = (midiExact - static_cast<float>(nearestMidi)) * 100.0f;

                telemetry.detectedPitchHz.store(freq, std::memory_order_relaxed);
                telemetry.detectedMidiNote.store(nearestMidi, std::memory_order_relaxed);
                telemetry.pitchCentsOffset.store(cents, std::memory_order_relaxed);
                telemetry.isPitchLocked.store(true, std::memory_order_relaxed);
                return;
            }
        }

        telemetry.isPitchLocked.store(false, std::memory_order_relaxed);
    }

    double mSampleRate{48000.0};
    std::vector<float> mBuffer;
    size_t mWritePos{0};
};

} // namespace conductor
