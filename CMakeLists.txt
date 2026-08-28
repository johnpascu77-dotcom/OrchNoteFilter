cmake_minimum_required(VERSION 3.22)

project(OrchNoteFilter VERSION 0.1.0)

# Path to your JUCE folder.
add_subdirectory("C:/JUCE/JUCE" JUCE)

juce_add_plugin(OrchNoteFilter
    COMPANY_NAME "JohnPascu"
    PLUGIN_MANUFACTURER_CODE Jpas
    PLUGIN_CODE Onft

    FORMATS VST3

    PRODUCT_NAME "OrchNoteFilter"
    PLUGIN_NAME "OrchNoteFilter"
    BUNDLE_ID "com.johnpascu.OrchNoteFilter"

    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_REPLACES_VST2 FALSE

    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(OrchNoteFilter)

target_sources(OrchNoteFilter
    PRIVATE
        Source/OrchNoteFilterProcessor.cpp
        Source/OrchNoteFilterEditor.cpp
)

target_compile_definitions(OrchNoteFilter
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(OrchNoteFilter
    PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_gui_extra

    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if (TARGET OrchNoteFilter_VST3)
    target_compile_definitions(OrchNoteFilter_VST3
        PRIVATE
            JUCE_VST3_CAN_REPLACE_VST2=0
    )
endif()

# Developer-only verification target for the field / foreign-note / shift logic.
juce_add_console_app(OrchNoteFilterFieldLogicCheck
    PRODUCT_NAME "OrchNoteFilterFieldLogicCheck"
)

juce_generate_juce_header(OrchNoteFilterFieldLogicCheck)

target_sources(OrchNoteFilterFieldLogicCheck
    PRIVATE
        Tools/OrchNoteFilterFieldLogicCheck.cpp
        Source/OrchNoteFilterFieldLogic.cpp
)

target_link_libraries(OrchNoteFilterFieldLogicCheck
    PRIVATE
        juce::juce_core

    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

target_include_directories(OrchNoteFilterFieldLogicCheck
    PRIVATE
        Source
)
