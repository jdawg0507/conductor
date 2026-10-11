cmake_minimum_required(VERSION 3.22)

project(Conductor VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler optimization options
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
    add_compile_options(-O3)
    if (APPLE)
        add_compile_options(-stdlib=libc++)
    endif()
endif()

# Neural Amp Modeler Core Sources & Includes
set(NAM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Libs/NeuralAmpModelerCore")
file(GLOB NAM_SOURCES_TOP "${NAM_ROOT}/NAM/*.cpp" "${NAM_ROOT}/NAM/*.c")
file(GLOB NAM_SOURCES_SUB "${NAM_ROOT}/NAM/*/*.cpp")
set(AUDIO_DSP_TOOLS_DIR "${NAM_ROOT}/Dependencies/AudioDSPTools")
set(AUDIO_DSP_TOOLS_WAV "${AUDIO_DSP_TOOLS_DIR}/dsp/wav.cpp")

set(NAM_CORE_SOURCES
    ${NAM_SOURCES_TOP}
    ${NAM_SOURCES_SUB}
    ${AUDIO_DSP_TOOLS_WAV}
)

# Conductor Plugin Source Files
set(CONDUCTOR_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Audio/NamChain.h
    Source/Audio/TunerDetector.h
    Source/Audio/Telemetry.h
    Source/AI/ModelCache.h
    Source/AI/Tone3000Catalog.h
    Source/AI/LLMClient.h
    Source/UI/ConductorLookAndFeel.h
    Source/UI/TunerComponent.h
    Source/UI/LevelMeterComponent.h
    Source/UI/PedalCardComponent.h
    Source/UI/DebugLog.h
    Source/UI/DebugConsoleComponent.h
)

# Include JUCE Framework
add_subdirectory(Libs/JUCE EXCLUDE_FROM_ALL)

# Add Conductor Plugin Target (AU, VST3, Standalone)
juce_add_plugin(Conductor
    COMPANY_NAME "ConductorAudio"
    BUNDLE_ID "com.conductoraudio.conductor"
    MICROPHONE_PERMISSION_ENABLED TRUE
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Cond
    PLUGIN_CODE Cnd1
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Conductor"
)

target_sources(Conductor PRIVATE
    ${CONDUCTOR_SOURCES}
    ${NAM_CORE_SOURCES}
)

target_include_directories(Conductor PRIVATE
    Source
    ${NAM_ROOT}
    ${NAM_ROOT}/Dependencies/eigen
    ${NAM_ROOT}/Dependencies/nlohmann
    ${AUDIO_DSP_TOOLS_DIR}/dsp
)

target_compile_definitions(Conductor PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    NAM_ENABLE_A2_FAST=1
    NAM_SAMPLE_FLOAT=1
)

target_link_libraries(Conductor PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
