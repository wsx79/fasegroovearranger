cmake_minimum_required(VERSION 3.22)
project(FaseGrooveArranger VERSION 0.23.2 LANGUAGES C CXX)
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
add_library(fga_core STATIC Source/core/Project.cpp Source/core/Timeline.cpp Source/core/Midi.cpp
    ThirdParty/fasegroovepiano/Source/engine/Theory.cpp
    ThirdParty/fasegroovepiano/Source/engine/Generators.cpp
    ThirdParty/fasegroovepiano/Source/engine/GrooveEngine.cpp
    ThirdParty/fasegroovepiano/Source/engine/HarmonyAnalyzer.cpp
    ThirdParty/fasegroovepiano/Source/engine/Comping.cpp
    ThirdParty/fasegroovedrums/Source/engine/Drums.cpp
    ThirdParty/fasegroovedrums/Source/engine/Generators.cpp
    ThirdParty/fasegroovedrums/Source/engine/GrooveEngine.cpp
    ThirdParty/fasegrooveguitar/Source/engine/Guitar.cpp
    ThirdParty/fasegrooveguitar/Source/engine/Generators.cpp
    ThirdParty/fasegrooveguitar/Source/engine/GrooveEngine.cpp
    ThirdParty/fasegroovelead/Source/engine/Lead.cpp
    ThirdParty/fasegroovelead/Source/engine/Generators.cpp
    ThirdParty/fasegroovelead/Source/engine/GrooveEngine.cpp)
target_include_directories(fga_core PUBLIC Source/core)
if(FGA_BUILD_TESTS)
    enable_testing()
    add_executable(fga_core_tests tests/CoreTests.cpp)
    target_link_libraries(fga_core_tests PRIVATE fga_core)
    # The core tests also write the MIDI files that scripts/verify-midi.py checks independently.
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/core-midi")
    add_test(NAME arranger_core COMMAND fga_core_tests "${CMAKE_BINARY_DIR}/core-midi")
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME verify_midi COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify-midi.py" "${CMAKE_BINARY_DIR}/core-midi")
        set_tests_properties(verify_midi PROPERTIES DEPENDS arranger_core)
    endif()
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
        MICROPHONE_PERMISSION_ENABLED TRUE
        MICROPHONE_PERMISSION_TEXT "Record audio only when you choose audio input recording in Studio."
        PLIST_TO_MERGE "<plist><dict><key>NSAppTransportSecurity</key><dict><key>NSAllowsLocalNetworking</key><true/></dict><key>NSLocalNetworkUsageDescription</key><string>Connect to your selected Ollama server to compose MIDI.</string></dict></plist>")
    juce_add_binary_data(ArrangerResources SOURCES Resources/Inter-Regular.otf Resources/Inter-SemiBold.otf
        Resources/fs_pitch_logo.png Resources/arranger.svg Resources/arranger.png docs/GUIDA.md docs/GUIDE.md docs/options.png docs/images/guide_arrangement.png docs/images/guide_piano.png docs/images/guide_guitar.png docs/images/guide_lead.png docs/images/guide_mixer.png docs/images/guide_ai.png docs/images/guide_ai_it.png docs/images/guide_sampler.png docs/images/guide_recording.png docs/images/guide_buses.png PRIVACY.it.md PRIVACY.md THIRD_PARTY_NOTICES.md)
    target_sources(FaseGrooveArranger PRIVATE Source/Main.cpp Source/StudioWindow.cpp Source/MixerRoutingWindow.cpp Source/AiComposer.cpp Source/AiComposerWindow.cpp Source/PluginCatalog.cpp Source/PluginBrowser.cpp Source/PluginSettings.cpp Source/Product.cpp Source/FamilyControls.cpp Source/FamilyOptions.cpp ThirdParty/fasegroove-common/Updater.cpp Source/MainComponent.cpp Source/DawWorkspace.cpp Source/TimelineEditing.cpp Source/DawNavigation.cpp Source/InstrumentPanel.cpp Source/UiText.cpp ThirdParty/fasegroove-common/Localisation.cpp Source/PianoTools.cpp Source/GuidePage.cpp Source/Menus.cpp Source/AudioEngine.cpp Source/Sampler.cpp Source/MixerBuses.cpp Source/MidiRecording.cpp Source/AudioInput.cpp Source/ProjectIO.cpp Source/StudioIO.cpp Source/SampleLibrary.cpp Source/SampleBrowser.cpp
        # The family's shared module, vendored by fasegroove/scripts/sync.sh (do not edit here).
        ThirdParty/fasegroove-common/Branding.cpp ThirdParty/fasegroove-common/CircleOfFifths.cpp)
    target_include_directories(FaseGrooveArranger PRIVATE ThirdParty/fasegroove-common ThirdParty/signalsmith-stretch ThirdParty/signalsmith-linear/include)
    target_compile_definitions(FaseGrooveArranger PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=1 JUCE_LOAD_CURL_SYMBOLS_LAZILY=1 JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_PLUGINHOST_VST3=1 JUCE_PLUGINHOST_AU=$<BOOL:${APPLE}> JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:FaseGrooveArranger,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}")
    target_link_libraries(FaseGrooveArranger PRIVATE fga_core fga_mp3 ArrangerResources juce::juce_audio_utils juce::juce_dsp juce::juce_cryptography
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    if(FGA_BUILD_TESTS)
        juce_add_console_app(fga_io_tests PRODUCT_NAME "Arranger project tests")
        target_sources(fga_io_tests PRIVATE tests/IOTests.cpp tests/StudioIOTests.cpp tests/AiTests.cpp Source/AiComposer.cpp Source/ProjectIO.cpp Source/StudioIO.cpp)
        target_link_libraries(fga_io_tests PRIVATE fga_core juce::juce_audio_basics juce::juce_core juce::juce_cryptography)
        target_compile_definitions(fga_io_tests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=1 JUCE_LOAD_CURL_SYMBOLS_LAZILY=1)
        add_test(NAME project_io COMMAND fga_io_tests)
        juce_add_console_app(fga_update_tests PRODUCT_NAME "Arranger update tests")
        target_sources(fga_update_tests PRIVATE tests/UpdaterTests.cpp ThirdParty/fasegroove-common/Updater.cpp ThirdParty/fasegroove-common/Localisation.cpp Source/Product.cpp)
        target_include_directories(fga_update_tests PRIVATE Source ThirdParty/fasegroove-common)
        target_link_libraries(fga_update_tests PRIVATE ArrangerResources juce::juce_gui_basics juce::juce_cryptography)
        target_compile_definitions(fga_update_tests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}")
        add_test(NAME family_updater COMMAND fga_update_tests)
        juce_add_console_app(fga_engine_tests PRODUCT_NAME "Arranger engine tests")
        target_sources(fga_engine_tests PRIVATE tests/EngineTests.cpp tests/StudioEngineTests.cpp tests/EngineAuditTests.cpp Source/AudioEngine.cpp Source/Sampler.cpp Source/MixerBuses.cpp Source/MidiRecording.cpp Source/AudioInput.cpp)
        target_include_directories(fga_engine_tests PRIVATE ThirdParty/signalsmith-stretch ThirdParty/signalsmith-linear/include)
        target_link_libraries(fga_engine_tests PRIVATE fga_core fga_mp3 juce::juce_audio_utils juce::juce_dsp juce::juce_cryptography)
        target_compile_definitions(fga_engine_tests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_PLUGINHOST_VST3=0)
        add_test(NAME audio_engine COMMAND fga_engine_tests)
        juce_add_console_app(fga_browser_tests PRODUCT_NAME "Arranger sample browser tests")
        target_sources(fga_browser_tests PRIVATE tests/SampleLibraryTests.cpp Source/SampleLibrary.cpp)
        target_link_libraries(fga_browser_tests PRIVATE juce::juce_audio_formats)
        target_compile_definitions(fga_browser_tests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
        add_test(NAME sample_browser COMMAND fga_browser_tests)
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
