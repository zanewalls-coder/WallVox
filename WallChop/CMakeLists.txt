cmake_minimum_required(VERSION 3.22)
project(WallChop VERSION 1.0.0 LANGUAGES C CXX)

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

juce_add_plugin(WallChop
    COMPANY_NAME "WallVox"
    PLUGIN_MANUFACTURER_CODE Wlvx
    PLUGIN_CODE Wch1
    FORMATS ${WALLVOX_FORMATS}
    PRODUCT_NAME "Wall Chop"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect)

set(WALL_SHARED "${CMAKE_CURRENT_SOURCE_DIR}/../Shared")
target_sources(WallChop PRIVATE
    ${WALL_SHARED}/Vox/Modules.cpp
    ${WALL_SHARED}/Vox/PluginProcessor.cpp
    ${WALL_SHARED}/Vox/PluginEditor.cpp
    ${WALL_SHARED}/Vox/PresetManager.cpp)

juce_add_binary_data(WallChopPresets
    HEADER_NAME FactoryPresets.h
    NAMESPACE FactoryPresets
    SOURCES ${WALL_SHARED}/Vox/factory/chop/factory_presets.json)
set_target_properties(WallChopPresets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_include_directories(WallChop PRIVATE "${WALL_SHARED}" "${WALL_SHARED}/Vox" "${WALL_SHARED}/ThirdParty")

target_compile_definitions(WallChop PUBLIC
    WALL_CHOP=1
    WALL_SNAPSHOT=$<BOOL:${WALL_SNAPSHOT}>
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    WALLVOX_PRESET_INDEX_URL="${WALLVOX_PRESET_INDEX_URL}")

target_link_libraries(WallChop
    PRIVATE
        WallChopPresets
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
