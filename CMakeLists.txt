cmake_minimum_required(VERSION 3.22)

# macOS : binaire universel (Apple Silicon + Intel), macOS 10.13 minimum (a fixer AVANT project())
if(APPLE)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)
endif()

project(Omi VERSION 1.0.8)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Windows : runtime C++ statique (pas de redistribuable Visual C++ a installer)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# JUCE : dossier local si JUCE_DIR est fourni, sinon telechargement automatique
set(JUCE_DIR "" CACHE PATH "Copie locale de JUCE (optionnel)")
if(JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
  add_subdirectory(${JUCE_DIR} JUCE)
else()
  include(FetchContent)
  FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW ON)
  FetchContent_MakeAvailable(JUCE)
endif()

# visuel integre au plugin (aucun fichier externe a l'execution)
juce_add_binary_data(OmiAssets SOURCES
  background.png glow_blue.png led_on.png fs_refl.png knob_marker.png knob_shadow.png toggle_strip.png Poppins-Medium.ttf Poppins-Bold.ttf Poppins-Light.ttf NineHandsLogoWhite.svg)
set_target_properties(OmiAssets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

juce_add_plugin(Omi
  COMPANY_NAME "NINE HANDS | DSP"
  PLUGIN_MANUFACTURER_CODE Wiga
  PLUGIN_CODE WgOm
  BUNDLE_ID com.ninehandsdsp.omi
  FORMATS VST3
  VST3_CATEGORIES Fx Analyzer
  PRODUCT_NAME "Omi"
  VERSION 1.0.8
  DESCRIPTION "Guitar to MIDI converter (monophonic, very low tunings) - single pedal format"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT TRUE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Omi PRIVATE PluginProcessor.cpp PluginEditor.cpp ReaperBridge.cpp)
# en-tetes du SDK VST3 fournis par JUCE (interface IReaperHostApplication de REAPER, ReaperBridge.cpp)
if(JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
  set(OMI_JUCE_SRC "${JUCE_DIR}")
else()
  set(OMI_JUCE_SRC "${juce_SOURCE_DIR}")
endif()
target_include_directories(Omi PRIVATE "${OMI_JUCE_SRC}/modules/juce_audio_processors_headless/format_types/VST3_SDK")


# verrou de taille de la fenetre du DAW (jamais sous 60 %, jamais au-dela de 150 %)
if(APPLE)
  target_sources(Omi PRIVATE WindowMinSize.mm)
  target_link_libraries(Omi PRIVATE "-framework Cocoa")
else()
  target_sources(Omi PRIVATE WindowMinSize.cpp)
endif()
if(WIN32)
  target_link_libraries(Omi PRIVATE comctl32 user32)
endif()
if(MSVC)
  target_compile_options(Omi PRIVATE /utf-8)
endif()

target_compile_definitions(Omi PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Omi
  PRIVATE
    OmiAssets
    juce::juce_audio_utils
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
