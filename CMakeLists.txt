cmake_minimum_required(VERSION 3.22)
project(WallVox VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

# Filled in automatically by the GitHub build so "Get More Presets" knows where to look
set(WALLVOX_PRESET_INDEX_URL "" CACHE STRING "URL of the online presets/index.json")

if(APPLE)
    set(WALLVOX_FORMATS VST3 AU)
else()
    set(WALLVOX_FORMATS VST3)
endif()

option(WALL_SNAPSHOT "Build a standalone app that saves a UI screenshot" OFF)
if(WALL_SNAPSHOT)
    list(APPEND WALLVOX_FORMATS Standalone)
endif()

juce_add_plugin(WallVox
    COMPANY_NAME "WallVox"
    PLUGIN_MANUFACTURER_CODE Wlvx
    PLUGIN_CODE Wvx1
    FORMATS ${WALLVOX_FORMATS}
    PRODUCT_NAME "WallVox"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect)

set(WALL_SHARED "${CMAKE_CURRENT_SOURCE_DIR}/Shared")
target_sources(WallVox PRIVATE
    ${WALL_SHARED}/Vox/Modules.cpp
    ${WALL_SHARED}/Vox/PluginProcessor.cpp
    ${WALL_SHARED}/Vox/PluginEditor.cpp
    ${WALL_SHARED}/Vox/PresetManager.cpp)

juce_add_binary_data(WallVoxPresets
    HEADER_NAME FactoryPresets.h
    NAMESPACE FactoryPresets
    SOURCES ${WALL_SHARED}/Vox/factory/vox/factory_presets.json)
set_target_properties(WallVoxPresets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_include_directories(WallVox PRIVATE "${WALL_SHARED}" "${WALL_SHARED}/Vox")

target_compile_definitions(WallVox PUBLIC
    WALL_CHOP=0
    WALL_SNAPSHOT=$<BOOL:${WALL_SNAPSHOT}>
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    WALLVOX_PRESET_INDEX_URL="${WALLVOX_PRESET_INDEX_URL}")

target_link_libraries(WallVox
    PRIVATE
        WallVoxPresets
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
