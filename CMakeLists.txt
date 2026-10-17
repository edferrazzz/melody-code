cmake_minimum_required(VERSION 3.22)
project(MelodyCode VERSION 1.6.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
 set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum supported macOS version")
endif()
if(MSVC)
 set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")  # static CRT: the .exe runs without installing anything
endif()

option(MC_FETCH_JUCE "Fetch JUCE when it is not installed" ON)
find_package(JUCE CONFIG QUIET)
if(NOT JUCE_FOUND AND MC_FETCH_JUCE)
 include(FetchContent)
 FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.12)
 FetchContent_MakeAvailable(JUCE)
endif()

add_library(melodycode_core STATIC
 Source/Core/Music.cpp Source/Core/Music.h
 Source/Core/MelodyEngine.cpp Source/Core/MelodyEngine.h)
target_include_directories(melodycode_core PUBLIC Source)
target_compile_features(melodycode_core PUBLIC cxx_std_20)
set_target_properties(melodycode_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(TARGET juce::juce_audio_utils)
 if(WIN32 AND NOT MSVC AND juce_SOURCE_DIR)
  include(scripts/patch-juce-mingw.cmake)
 endif()
 set(MC_FORMATS Standalone VST3)
 if(APPLE)
  list(APPEND MC_FORMATS AU)
 endif()
 juce_add_plugin(MelodyCode
  COMPANY_NAME "Audio Makers"
  BUNDLE_ID "com.audiomakers.melodycode"
  PLUGIN_MANUFACTURER_CODE AuMk
  PLUGIN_CODE McDe
  ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Assets/melody-code-icon-1024.png"
  FORMATS ${MC_FORMATS}
  PRODUCT_NAME "Melody Code"
  VST3_CATEGORIES Instrument Synth
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT TRUE
  VST3_AUTO_MANIFEST FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE)
 target_sources(MelodyCode PRIVATE
  Source/PluginProcessor.cpp Source/PluginProcessor.h
  Source/PluginEditor.cpp Source/PluginEditor.h
  Source/Synth.cpp Source/Synth.h
  Source/EmbeddedFonts.cpp Source/EmbeddedFonts.h)
 target_link_libraries(MelodyCode PRIVATE melodycode_core juce::juce_audio_utils juce::juce_audio_processors juce::juce_gui_basics
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
 target_compile_definitions(MelodyCode PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_DIRECT2D=0 JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)
 if(MINGW OR (WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT MSVC))
  # zig / mingw cross build: link everything statically so the .exe needs no runtime DLLs
  target_compile_options(MelodyCode PRIVATE -Wno-error=date-time)
  target_include_directories(MelodyCode SYSTEM PRIVATE scripts/mingw-case)  # case-only header name differences
  target_link_options(MelodyCode_Standalone PRIVATE -Wl,--subsystem,windows)               # GUI app, no console window
  # comctl32 v6 (TaskDialogIndirect, used by JUCE message boxes) only resolves with this manifest
  target_sources(MelodyCode_Standalone PRIVATE scripts/win/exe-manifest.rc)
  target_sources(MelodyCode_VST3 PRIVATE scripts/win/dll-manifest.rc)
  target_compile_definitions(MelodyCode PRIVATE D2D1_SATURATION_PROP_SATURATION=0 CaretPosition_Unknown=0 CaretPosition_EndOfLine=1 CaretPosition_BeginningOfLine=2 NS_E_NO_MORE_SAMPLES=0xC00D0BCF)
  target_link_libraries(MelodyCode PRIVATE kernel32 user32 gdi32 winspool shell32 ole32 oleaut32 uuid comdlg32 advapi32 ws2_32 wininet version shlwapi winmm imm32 comctl32 dwmapi dwrite d2d1 dxgi d3d11 dcomp dxguid usp10 dbghelp)
 endif()
endif()

enable_testing()
add_executable(melodycode_tests Tests/CoreTests.cpp)
target_link_libraries(melodycode_tests PRIVATE melodycode_core)
add_test(NAME melodycode_core_tests COMMAND melodycode_tests)
if(TARGET MelodyCode)
 juce_add_console_app(melodycode_audio_tests PRODUCT_NAME "Melody Code Audio Tests")
 target_sources(melodycode_audio_tests PRIVATE Tests/AudioTests.cpp)
 target_include_directories(melodycode_audio_tests PRIVATE Source $<TARGET_PROPERTY:MelodyCode,INCLUDE_DIRECTORIES>)
 target_compile_definitions(melodycode_audio_tests PRIVATE $<TARGET_PROPERTY:MelodyCode,COMPILE_DEFINITIONS>)
 target_link_libraries(melodycode_audio_tests PRIVATE MelodyCode juce::juce_audio_utils)
 add_test(NAME melodycode_audio_tests COMMAND melodycode_audio_tests)
endif()
