cmake_minimum_required(VERSION 3.22)
project(Seedling VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# WebView2 SDK lives in the repo (libs/nuget), not in the user's NuGet folder
set(JUCE_WEBVIEW2_PACKAGE_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/libs/nuget")

add_subdirectory(libs/JUCE)
add_subdirectory(libs/clap-juce-extensions EXCLUDE_FROM_ALL)

juce_add_plugin(Seedling
    PRODUCT_NAME "gensexuell"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png"
    COMPANY_NAME "beatsexuell"
    DESCRIPTION "Grow sounds from a seed, by text or from a sample. Fully local synth."
    BUNDLE_ID com.synthsexuell.seedling
    PLUGIN_MANUFACTURER_CODE Sxsl
    PLUGIN_CODE Sdlg
    FORMATS VST3 AU LV2 Standalone
    LV2URI "urn:synthsexuell:seedling"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    NEEDS_WEBVIEW2 TRUE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice)

clap_juce_extensions_plugin(TARGET Seedling
    CLAP_ID "com.synthsexuell.seedling"
    CLAP_FEATURES instrument synthesizer stereo)

# the web UI is the same index.html that also opens in a browser; net.bin is the trained sound -> seed net
set(binary_sources "${CMAKE_CURRENT_SOURCE_DIR}/../index.html"
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/fonts/inter.woff2"
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/fonts/jetbrainsmono.woff2")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/net.bin")
    list(APPEND binary_sources "${CMAKE_CURRENT_SOURCE_DIR}/Resources/net.bin")
    target_compile_definitions(Seedling PRIVATE SEEDLING_HAS_NET=1)
endif()
juce_add_binary_data(SeedlingData SOURCES ${binary_sources})

file(GLOB grain_sources CONFIGURE_DEPENDS Source/grain/*.cpp)
target_sources(Seedling PRIVATE Source/Plugin.cpp ${grain_sources})
target_include_directories(Seedling PRIVATE Source)
target_compile_definitions(Seedling PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(Seedling
    PRIVATE SeedlingData juce::juce_audio_utils juce::juce_gui_extra juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

add_executable(EngineTest test/EngineTest.cpp)
target_include_directories(EngineTest PRIVATE Source)

add_executable(DatasetGen tools/DatasetGen.cpp)
target_include_directories(DatasetGen PRIVATE Source)
