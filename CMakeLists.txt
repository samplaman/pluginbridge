cmake_minimum_required(VERSION 3.22)
project(PluginBridge VERSION 1.0.5 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE submodule
add_subdirectory(JUCE)

juce_add_plugin(PluginBridge
    COMPANY_NAME "PluginBridge"
    PLUGIN_MANUFACTURER_CODE Plgb
    PLUGIN_CODE Pb01
    FORMATS Standalone VST3 AU
    PRODUCT_NAME "PluginBridge"
    VERSION "1.0.5"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(PluginBridge PRIVATE
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp

    Source/Network/NetworkProtocol.h
    Source/Network/BeaconService.h
    Source/Network/BeaconService.cpp
    Source/Network/AudioSender.h
    Source/Network/AudioSender.cpp
    Source/Network/AudioReceiver.h
    Source/Network/AudioReceiver.cpp

    Source/DSP/JitterBuffer.h
    Source/DSP/JitterBuffer.cpp
    Source/DSP/DriftCompensator.h
    Source/DSP/DriftCompensator.cpp
    Source/DSP/RoutingMatrix.h
    Source/DSP/RoutingMatrix.cpp

    Source/UI/OmniLookAndFeel.h
    Source/UI/OmniLookAndFeel.cpp
    Source/UI/MeterComponent.h
    Source/UI/MeterComponent.cpp
    Source/UI/PatchbayComponent.h
    Source/UI/PatchbayComponent.cpp
    Source/UI/PeerListComponent.h
    Source/UI/PeerListComponent.cpp
    Source/UI/ChannelMappingComponent.h
    Source/UI/ChannelMappingComponent.cpp
)

target_include_directories(PluginBridge PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_compile_definitions(PluginBridge PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(PluginBridge PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_basics
    juce::juce_gui_extra
    juce::juce_gui_basics
    juce::juce_graphics
    juce::juce_events
    juce::juce_core
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# Test harness target
find_package(Threads REQUIRED)

add_executable(test_harness
    Tests/TestHarness.cpp
    Source/Network/BeaconService.cpp
    Source/Network/AudioSender.cpp
    Source/Network/AudioReceiver.cpp
    Source/DSP/JitterBuffer.cpp
    Source/DSP/DriftCompensator.cpp
    Source/DSP/RoutingMatrix.cpp
)

target_include_directories(test_harness PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_link_libraries(test_harness PRIVATE
    Threads::Threads
)

