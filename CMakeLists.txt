cmake_minimum_required(VERSION 3.21)
project(JUCE_EQ_Plugin VERSION 1.0.0)

#add_subdirectory(Ext/JUCE)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

include(FetchContent)
set(FETCH_CONTENT_QUIET OFF)
FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git 
        GIT_TAG origin/master
        GIT_SHALLOW ON
        [[FIND_PACKAGE_ARGS 7.0.3 GLOBAL]] <-- Uncomment this funny little character if you don't wanna redownload juce every project but you gotta do some shennanigans which the juce cmake api docs talk you thru 
)
FetchContent_MakeAvailable(JUCE)

set(FORMATS "VST3" "Standalone")

juce_add_plugin(Delay
        VERSION 1.0.0
        COMPANY_NAME lachesis
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        #PLUGIN_MANUFACTURER_CODE 0000
        #PLUGIN_CODE Delay 0000
        FORMATS ${FORMATS}
        VST3_CATEGORIES "Fx"
        #BUNDLE_ID ""
        PLUGIN_NAME "Delay Plugin"
        PRODUCT_NAME "Delay Plugin"
        ICON_BIG "Resources/icon.png"
        ICON_SMALL "Resources/icon.png"
        )

target_compile_features(Delay PRIVATE cxx_std_17)

juce_generate_juce_header(Delay)

juce_add_binary_data(BinaryData SOURCES
        Resources/Orbitron.ttf
        Resources/Monomaniac.ttf
)

target_sources(Delay
    PRIVATE
        Source/PluginEditor.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.h
        Source/PluginProcessor.h
        Resources/resources.rc
        )

#add_subdirectory(Sources)

target_compile_definitions(Delay PUBLIC
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_DISPLAY_SPLASH_SCREEN=0) # very naughty

target_link_libraries(Delay
        PRIVATE
            # AudioPluginData           # If we'd created a binary data target, we'd link to it here
            BinaryData
            juce::juce_audio_utils
            juce::juce_core
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)