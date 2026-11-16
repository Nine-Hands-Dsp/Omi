# Outil hors plugin : compile le processeur + la fenetre d'OMI sous Linux et exporte des captures PNG (apercu, notice)
cmake_minimum_required(VERSION 3.22)
project(OmiRender VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)
add_subdirectory(${JUCE_DIR} JUCE)
set(W ${CMAKE_CURRENT_SOURCE_DIR}/../..)
juce_add_binary_data(OmiAssets SOURCES
  ${W}/background.png ${W}/glow_blue.png ${W}/led_on.png ${W}/fs_refl.png ${W}/knob_marker.png ${W}/knob_shadow.png
  ${W}/Poppins-Medium.ttf ${W}/Poppins-Bold.ttf ${W}/Poppins-Light.ttf ${W}/NineHandsLogoWhite.svg)
juce_add_console_app(OmiRender PRODUCT_NAME "OmiRender")
target_sources(OmiRender PRIVATE main.cpp ${W}/PluginProcessor.cpp ${W}/PluginEditor.cpp ${W}/WindowMinSize.cpp ${W}/ReaperBridge.cpp)
target_include_directories(OmiRender PRIVATE ${W})
target_compile_definitions(OmiRender PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_ALSA=0 JUCE_JACK=0
  JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XSHM=0 JucePlugin_Name="Omi" JucePlugin_Build_VST3=0
  JucePlugin_VersionString="1.0.1" JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(OmiRender PRIVATE OmiAssets juce::juce_audio_utils juce::juce_recommended_config_flags)
