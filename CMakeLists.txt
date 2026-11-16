cmake_minimum_required(VERSION 3.24)
project(FreshLab VERSION 0.2.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(FRESHLAB_CORE_ONLY "Build dependency-free DSP tests only" OFF)
option(FRESHLAB_INTEGRATION_TESTS "Build JUCE processor integration tests" ON)
enable_testing()
add_executable(freshlab-dsp-tests Tests/DspTests.cpp)
target_include_directories(freshlab-dsp-tests PRIVATE Source)
add_test(NAME freshlab-dsp COMMAND freshlab-dsp-tests)
add_executable(freshlab-extension-tests Tests/ExtensionTests.cpp)
target_include_directories(freshlab-extension-tests PRIVATE Source)
add_test(NAME freshlab-extensions COMMAND freshlab-extension-tests)
add_executable(freshlab-render-voice Tools/RenderVoice.cpp)
target_include_directories(freshlab-render-voice PRIVATE Source)
if(FRESHLAB_CORE_ONLY)
    return()
endif()

# Pin an exact, reproducible framework version. A local checkout can be supplied offline.
set(JUCE_SOURCE_DIR "" CACHE PATH "Optional path to a JUCE 8.0.12 checkout")
if(JUCE_SOURCE_DIR)
    add_subdirectory("${JUCE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        URL https://codeload.github.com/juce-framework/JUCE/tar.gz/refs/tags/8.0.12
        URL_HASH SHA256=dec1a8baee5aaec4502b717a421b850e257ab7e9c2efd03766185ec1957cf102
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()
file(GLOB VOICE_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Voices/*.wav")
list(LENGTH VOICE_FILES VOICE_COUNT)
if(NOT VOICE_COUNT EQUAL 24)
    message(FATAL_ERROR "Missing voice bank. Run python scripts/make_voices.py first.")
endif()
juce_add_binary_data(FreshLabVoices SOURCES ${VOICE_FILES})
set(FRESH_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND FRESH_FORMATS AU)
endif()

function(add_freshlab target is_synth code product)
    if(is_synth)
        set(categories Instrument Synth)
    else()
        set(categories Fx Dynamics EQ Distortion)
    endif()
    juce_add_plugin(${target}
        COMPANY_NAME "FreshLab Audio"
        BUNDLE_ID "audio.freshlab.${target}"
        PLUGIN_MANUFACTURER_CODE Frsh
        PLUGIN_CODE ${code}
        PRODUCT_NAME "${product}"
        FORMATS ${FRESH_FORMATS}
        IS_SYNTH ${is_synth}
        NEEDS_MIDI_INPUT ${is_synth}
        NEEDS_MIDI_OUTPUT FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES ${categories})
    target_sources(${target} PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_include_directories(${target} PUBLIC Source)
    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0)
    target_link_libraries(${target} PRIVATE FreshLabVoices juce::juce_audio_utils
        PUBLIC juce::juce_recommended_config_flags)
endfunction()
add_freshlab(FreshLab TRUE Frsi "FreshLab")
add_freshlab(FreshLabFX FALSE Frfx "FreshLab FX")

if(FRESHLAB_INTEGRATION_TESTS)
    foreach(kind IN ITEMS instrument fx)
        if(kind STREQUAL "instrument")
            set(plugin_target FreshLab)
        else()
            set(plugin_target FreshLabFX)
        endif()
        add_executable(freshlab-${kind}-tests)
        target_sources(freshlab-${kind}-tests PRIVATE Tests/ProcessorTests.cpp)
        # Reuse the plugin's compiled JUCE modules instead of compiling them again.
        target_link_libraries(freshlab-${kind}-tests PRIVATE ${plugin_target})
        target_include_directories(freshlab-${kind}-tests PRIVATE
            $<TARGET_PROPERTY:${plugin_target},INCLUDE_DIRECTORIES>)
        target_compile_definitions(freshlab-${kind}-tests PRIVATE
            $<TARGET_PROPERTY:${plugin_target},COMPILE_DEFINITIONS>)
        add_test(NAME freshlab-${kind} COMMAND freshlab-${kind}-tests)
    endforeach()
endif()
