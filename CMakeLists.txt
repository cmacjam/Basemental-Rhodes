cmake_minimum_required(VERSION 3.22)

# ---- Product identity (change the name here in one place) -------------------
set(PRODUCT_NAME "Basemental Rhodes")
set(PRODUCT_VERSION 1.0.0)

project(BasementalRhodes VERSION ${PRODUCT_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

# ---- JUCE: local checkout via -DJUCE_DIR=..., otherwise downloaded ----------
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# Optional Pro Tools build: pass -DAAX_SDK_PATH=/path/to/aax-sdk
set(PLUGIN_FORMATS AU VST3 Standalone)
if(DEFINED AAX_SDK_PATH)
    juce_set_aax_sdk_path(${AAX_SDK_PATH})
    list(APPEND PLUGIN_FORMATS AAX)
endif()

juce_add_plugin(BasementalRhodes
    PRODUCT_NAME "${PRODUCT_NAME}"
    COMPANY_NAME "Basemental"
    BUNDLE_ID com.basemental.rhodes
    PLUGIN_MANUFACTURER_CODE Bsml
    PLUGIN_CODE BRho
    FORMATS ${PLUGIN_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    VST3_CATEGORIES Instrument Piano
    AAX_CATEGORY AAX_ePlugInCategory_SWGenerators
    MICROPHONE_PERMISSION_ENABLED FALSE)

target_sources(BasementalRhodes PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(BasementalRhodes PRIVATE Source)

target_compile_definitions(BasementalRhodes PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(BasementalRhodes
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---- Offline DSP test (no JUCE): renders demo WAVs -------------------------
option(BUILD_DSP_TESTS "Build the offline DSP render test" OFF)
if(BUILD_DSP_TESTS)
    add_executable(render_demo tests/render_demo.cpp)
endif()
