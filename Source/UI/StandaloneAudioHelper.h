#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#include "DebugLog.h"

namespace conductor {

class StandaloneAudioHelper {
public:
    static void initializeStandaloneInput() {
        if (auto* holder = juce::StandalonePluginHolder::getInstance()) {
            // JUCE defaults shouldMuteInput to true on startup. Unmute it for live guitar input!
            if (holder->getMuteInputValue().getValue()) {
                Log("AUDIO", "Unmuting Standalone audio input (JUCE default muted -> now unmuted for guitar input).");
                holder->getMuteInputValue().setValue(false);
            }
        }
    }

    static void openAudioSettingsDialog() {
        if (auto* holder = juce::StandalonePluginHolder::getInstance()) {
            Log("UI", "Opening native JUCE Audio/MIDI device configuration dialog...");
            holder->showAudioSettingsDialog();
        } else {
            Log("UI", "Conductor running as DAW plugin - showing DAW audio guide.");
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::InfoIcon,
                "DAW Audio Routing",
                "Conductor is running as a DAW Plugin (VST3 / AU).\n\n"
                "To configure your guitar input:\n"
                "1. Arm your audio track for recording/input monitoring in your DAW.\n"
                "2. Set the track input to the channel your guitar is plugged into (Input 1 or 2).\n"
                "3. Ensure Conductor is inserted as an audio effect.",
                "OK"
            );
        }
    }
};

} // namespace conductor
