cmake_minimum_required(VERSION 3.22)

project(FaseGrooveGuitar VERSION 0.4.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Universal binary (Apple Silicon + Intel) on macOS
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

option(FGG_BUILD_PLUGIN "Build the JUCE plug-in (needs JUCE)" ON)
option(FGG_BUILD_TESTS  "Build the engine unit tests" ON)

#==============================================================================
# Guitar engine (pure C++, no JUCE)
add_library(fgg_engine STATIC
    Source/engine/Guitar.cpp
    Source/engine/Generators.cpp
    Source/engine/GrooveEngine.cpp)
target_include_directories(fgg_engine PUBLIC Source/engine)
set_target_properties(fgg_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(FGG_BUILD_TESTS)
    enable_testing()
    add_executable(fgg_engine_tests tests/EngineTests.cpp)
    target_link_libraries(fgg_engine_tests PRIVATE fgg_engine)
    add_test(NAME engine COMMAND fgg_engine_tests)
endif()

#==============================================================================
# Plug-in
if(FGG_BUILD_PLUGIN)
    # Use a local JUCE checkout with -DJUCE_DIR=/path/to/JUCE, otherwise download it.
    set(JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout")

    if(JUCE_DIR)
        add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)
    else()
        include(FetchContent)
        FetchContent_Declare(JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 8.0.9
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(JUCE)
    endif()

    set(FGG_FORMATS VST3 Standalone)
    if(APPLE)
        list(APPEND FGG_FORMATS AU)
    endif()

    juce_add_plugin(FaseGrooveGuitar
        PRODUCT_NAME "Fase Groove Guitar"
        COMPANY_NAME "FS Pitch"
        BUNDLE_ID "com.fspitch.fasegrooveguitar"
        PLUGIN_MANUFACTURER_CODE FsPt
        PLUGIN_CODE Fgg1
        ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon_512.png"
        FORMATS ${FGG_FORMATS}
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT TRUE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Instrument Generator
        AU_MAIN_TYPE kAudioUnitType_MusicDevice)

    juce_add_binary_data(FaseGrooveGuitarData SOURCES
        Resources/fgg_icon.svg
        Resources/fs_pitch_logo.png
        Resources/Inter-Regular.otf
        Resources/Inter-SemiBold.otf)

    target_sources(FaseGrooveGuitar PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/Branding.cpp
        Source/Localisation.cpp
        Source/Updater.cpp
        Source/CircleOfFifths.cpp)

    target_compile_definitions(FaseGrooveGuitar PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    target_link_libraries(FaseGrooveGuitar
        PUBLIC
            FaseGrooveGuitarData
        PRIVATE
            fgg_engine
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    # Plug-in level tests (state, grid, MIDI export, transport); run by ctest in CI.
    if(FGG_BUILD_TESTS)
        juce_add_console_app(fgg_plugin_tests PRODUCT_NAME "Fase Groove Guitar Tests")
        target_sources(fgg_plugin_tests PRIVATE tests/PluginTests.cpp)
        target_link_libraries(fgg_plugin_tests PRIVATE FaseGrooveGuitar juce::juce_audio_utils)
        add_test(NAME plugin COMMAND fgg_plugin_tests)
    endif()
endif()

