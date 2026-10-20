cmake_minimum_required(VERSION 3.22)
project(FaseGrooveArranger VERSION 0.3.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal macOS binary" FORCE)
endif()
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/utf-8)
endif()
option(FGA_BUILD_APP "Build the standalone JUCE application" ON)
option(FGA_BUILD_TESTS "Build tests" ON)
option(FGA_BUILD_HOST_TEST_PLUGIN "Build a test-only VST3 instrument for host integration checks" OFF)
file(GLOB SHINE_SOURCES CONFIGURE_DEPENDS ThirdParty/shine/src/lib/*.c)
add_library(shine STATIC ${SHINE_SOURCES})
if(MSVC)
    target_compile_options(shine PRIVATE "/FI${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/shine/msvc_compat.h")
endif()
if(NOT MSVC)
    target_link_libraries(shine PRIVATE m)
endif()
add_library(fga_mp3 STATIC Source/core/Mp3Encoder.cpp)
target_link_libraries(fga_mp3 PUBLIC shine)
add_library(fga_core STATIC Source/core/Project.cpp Source/core/PianoComping.cpp Source/core/Midi.cpp
    ThirdParty/fasegroovepiano/Source/engine/Theory.cpp
    ThirdParty/fasegroovepiano/Source/engine/Generators.cpp
    ThirdParty/fasegroovepiano/Source/engine/GrooveEngine.cpp
    ThirdParty/fasegroovepiano/Source/engine/HarmonyAnalyzer.cpp
    ThirdParty/fasegroovedrums/Source/engine/Drums.cpp
    ThirdParty/fasegroovedrums/Source/engine/Generators.cpp
    ThirdParty/fasegroovedrums/Source/engine/GrooveEngine.cpp
    ThirdParty/fasegrooveguitar/Source/engine/Guitar.cpp
    ThirdParty/fasegrooveguitar/Source/engine/Generators.cpp
    ThirdParty/fasegrooveguitar/Source/engine/GrooveEngine.cpp)
target_include_directories(fga_core PUBLIC Source/core)
if(FGA_BUILD_TESTS)
    enable_testing()
    add_executable(fga_core_tests tests/CoreTests.cpp)
    target_link_libraries(fga_core_tests PRIVATE fga_core)
    add_test(NAME arranger_core COMMAND fga_core_tests)
    add_executable(fga_mp3_tests tests/Mp3Tests.cpp)
    target_link_libraries(fga_mp3_tests PRIVATE fga_mp3)
    add_test(NAME mp3_encoder COMMAND fga_mp3_tests)
endif()
if(FGA_BUILD_APP)
    set(JUCE_DIR "" CACHE PATH "Local JUCE 8.0.9 checkout (optional)")
    if(JUCE_DIR)
        add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")
    else()
        include(FetchContent)
        FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.9 GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(JUCE)
    endif()
    juce_add_gui_app(FaseGrooveArranger PRODUCT_NAME "Fase Groove Arranger"
        COMPANY_NAME "FS Pitch" BUNDLE_ID "com.fspitch.fasegroovearranger"
        ICON_BIG Resources/arranger.png ICON_SMALL Resources/arranger.png
        MICROPHONE_PERMISSION_ENABLED FALSE)
    juce_add_binary_data(ArrangerResources SOURCES Resources/Inter-Regular.otf Resources/Inter-SemiBold.otf
        Resources/fs_pitch_logo.png Resources/arranger.svg Resources/arranger.png docs/GUIDA.md docs/GUIDE.md docs/images/guide_arrangement.png docs/images/guide_piano.png docs/images/guide_guitar.png docs/images/guide_mixer.png PRIVACY.it.md THIRD_PARTY_NOTICES.md)
    target_sources(FaseGrooveArranger PRIVATE Source/Main.cpp Source/MainComponent.cpp Source/InstrumentPanel.cpp Source/UiText.cpp Source/Localisation.cpp Source/PianoTools.cpp Source/CircleOfFifths.cpp Source/GuidePage.cpp Source/Menus.cpp Source/AudioEngine.cpp Source/ProjectIO.cpp)
    target_compile_definitions(FaseGrooveArranger PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_PLUGINHOST_VST3=1 JUCE_PLUGINHOST_AU=$<BOOL:${APPLE}> JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:FaseGrooveArranger,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}")
    target_link_libraries(FaseGrooveArranger PRIVATE fga_core fga_mp3 ArrangerResources juce::juce_audio_utils juce::juce_dsp
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    if(FGA_BUILD_TESTS)
        juce_add_console_app(fga_io_tests PRODUCT_NAME "Arranger project tests")
        target_sources(fga_io_tests PRIVATE tests/IOTests.cpp Source/ProjectIO.cpp)
        target_link_libraries(fga_io_tests PRIVATE fga_core juce::juce_audio_basics juce::juce_core)
        target_compile_definitions(fga_io_tests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
        add_test(NAME project_io COMMAND fga_io_tests)
    endif()
    if(FGA_BUILD_HOST_TEST_PLUGIN)
        juce_add_plugin(ArrangerHostProbe PRODUCT_NAME "Arranger Host Probe" COMPANY_NAME "FS Pitch"
            PLUGIN_MANUFACTURER_CODE FsPt PLUGIN_CODE FgTs FORMATS VST3
            IS_SYNTH TRUE NEEDS_MIDI_INPUT TRUE NEEDS_MIDI_OUTPUT FALSE COPY_PLUGIN_AFTER_BUILD FALSE)
        target_sources(ArrangerHostProbe PRIVATE tests/HostProbe.cpp)
        target_compile_definitions(ArrangerHostProbe PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
        target_link_libraries(ArrangerHostProbe PRIVATE juce::juce_audio_utils)
    endif()
endif()
