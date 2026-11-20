cmake_minimum_required(VERSION 3.22)

project(MusicCreativeLab VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ---------------------------------------------------------------------------
# JUCE (AGPLv3 / commercial). Downloaded automatically.
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# Optional ASIO support: pass -DMCL_ASIO_SDK_DIR=path/to/asiosdk (folder that contains common/iasiodrv.h)
set(MCL_ASIO_SDK_DIR "" CACHE PATH "Steinberg ASIO SDK folder (optional)")

juce_add_gui_app(MusicCreativeLab
    PRODUCT_NAME "Music Creative Lab"
    COMPANY_NAME "Music Creative Lab"
    VERSION ${PROJECT_VERSION}
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.png"
    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE)

juce_generate_juce_header(MusicCreativeLab)

file(GLOB_RECURSE MCL_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")
target_sources(MusicCreativeLab PRIVATE ${MCL_SOURCES})
target_include_directories(MusicCreativeLab PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

set(MCL_ASIO 0)
if(MCL_ASIO_SDK_DIR AND EXISTS "${MCL_ASIO_SDK_DIR}/common/iasiodrv.h")
    message(STATUS "ASIO SDK found: ${MCL_ASIO_SDK_DIR}")
    set(MCL_ASIO 1)
    target_include_directories(MusicCreativeLab PRIVATE "${MCL_ASIO_SDK_DIR}/common")
endif()

target_compile_definitions(MusicCreativeLab PRIVATE
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:MusicCreativeLab,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:MusicCreativeLab,JUCE_VERSION>"
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_VST=0
    JUCE_PLUGINHOST_LADSPA=0
    JUCE_PLUGINHOST_LV2=0
    JUCE_WASAPI=1
    JUCE_DIRECTSOUND=1
    JUCE_ASIO=${MCL_ASIO}
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries(MusicCreativeLab
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_osc
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
