cmake_minimum_required(VERSION 3.22)
project(WallHub VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

set(HUB_VERSION "0" CACHE STRING "Build number of the hub")
set(HUB_MANIFEST_URL "https://github.com/zanewalls-coder/WallVox/releases/download/latest/manifest.json" CACHE STRING "")
option(WALL_SNAPSHOT "Save a UI screenshot and quit" OFF)

juce_add_gui_app(WallHub
    PRODUCT_NAME "Wall Hub"
    COMPANY_NAME "WallVox"
    BUNDLE_ID com.wallvox.hub
    VERSION 1.0.${HUB_VERSION})

target_sources(WallHub PRIVATE Source/Main.cpp Source/Hub.cpp)
target_include_directories(WallHub PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../Shared")
target_compile_definitions(WallHub PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:WallHub,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:WallHub,JUCE_VERSION>"
    HUB_VERSION=${HUB_VERSION}
    HUB_MANIFEST_URL="${HUB_MANIFEST_URL}"
    WALL_SNAPSHOT=$<BOOL:${WALL_SNAPSHOT}>)
target_link_libraries(WallHub PRIVATE juce::juce_gui_extra
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
