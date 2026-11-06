cmake_minimum_required(VERSION 3.22)

# macOS: binario universale (Apple Silicon + Intel). Va impostato PRIMA di project().
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Architetture macOS")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Versione minima macOS")

project(DaniRig VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(DANIRIG_BUILD_TESTS "Compila i test DSP (console)" OFF)
set(ASIO_SDK_DIR "" CACHE PATH "Cartella dell'SDK ASIO di Steinberg (solo Windows, opzionale)")

# ASIO: se l'SDK Steinberg è estratto in third_party/ (es. third_party/asiosdk), viene trovato da solo.
if(WIN32 AND NOT ASIO_SDK_DIR)
    file(GLOB _asio_headers "${CMAKE_CURRENT_SOURCE_DIR}/third_party/*/common/iasiodrv.h")
    if(_asio_headers)
        list(GET _asio_headers 0 _asio_h)
        get_filename_component(_asio_common "${_asio_h}" DIRECTORY)
        get_filename_component(_asio_root "${_asio_common}" DIRECTORY)
        set(ASIO_SDK_DIR "${_asio_root}" CACHE PATH "" FORCE)
    endif()
endif()

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.5
    GIT_SHALLOW    TRUE)

# Core NAM ufficiale (supporta A2). Commit fissato per build riproducibili.
# SOURCE_SUBDIR inesistente: usiamo solo i sorgenti, senza add_subdirectory del suo CMake.
FetchContent_Declare(nam_src
    GIT_REPOSITORY https://github.com/sdatkinson/NeuralAmpModelerCore.git
    GIT_TAG        0b3d3c97b0859a3a8c92a8628c4dd89a25eb5842
    GIT_SUBMODULES "Dependencies/eigen"
    SOURCE_SUBDIR  _usa_solo_i_sorgenti)
FetchContent_MakeAvailable(JUCE nam_src)

# ---------------------------------------------------------------------------------------------
# Libreria statica con il core NAM
file(GLOB NAM_SOURCES "${nam_src_SOURCE_DIR}/NAM/*.cpp" "${nam_src_SOURCE_DIR}/NAM/*/*.cpp")
# OBJECT (non STATIC): il core NAM registra le architetture con inizializzatori statici,
# che il linker scarterebbe da una libreria statica.
option(DANIRIG_NAM_AS_STATIC "Solo diagnostica: core NAM come libreria statica (riproduce il caso dei plugin)" OFF)
if(DANIRIG_NAM_AS_STATIC)
    add_library(nam_core STATIC ${NAM_SOURCES})
else()
    add_library(nam_core OBJECT ${NAM_SOURCES})
endif()
target_include_directories(nam_core PUBLIC
    "${nam_src_SOURCE_DIR}"
    "${nam_src_SOURCE_DIR}/Dependencies/nlohmann")
if(EXISTS "${nam_src_SOURCE_DIR}/Dependencies/eigen/Eigen/Dense")
    target_include_directories(nam_core SYSTEM PUBLIC "${nam_src_SOURCE_DIR}/Dependencies/eigen")
else()
    find_package(Eigen3 3.3 REQUIRED NO_MODULE)
    target_link_libraries(nam_core PUBLIC Eigen3::Eigen)
endif()
target_compile_features(nam_core PUBLIC cxx_std_20)
target_compile_definitions(nam_core PUBLIC NAM_SAMPLE_FLOAT NAM_ENABLE_A2_FAST)
if(WIN32)
    target_compile_definitions(nam_core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
set_target_properties(nam_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------------------------
set(DR_DSP_SOURCES
    Source/RigEngine.cpp
    Source/dsp/NamAmp.cpp)
set(DR_UI_SOURCES
    Source/ui/RigUI.cpp)

juce_add_gui_app(DaniRig
    PRODUCT_NAME "DaniRig"
    COMPANY_NAME "DaniArieta"
    BUNDLE_ID "com.daniarieta.danirig"
    VERSION ${PROJECT_VERSION}
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "DaniRig usa l'ingresso audio (la tua chitarra) per elaborare il suono in tempo reale.")

target_sources(DaniRig PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    ${DR_UI_SOURCES}
    ${DR_DSP_SOURCES})

target_compile_definitions(DaniRig PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:DaniRig,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:DaniRig,JUCE_VERSION>"
    JUCE_MODAL_LOOPS_PERMITTED=0)

if(WIN32 AND ASIO_SDK_DIR)
    target_compile_definitions(DaniRig PRIVATE JUCE_ASIO=1)
    target_include_directories(DaniRig PRIVATE "${ASIO_SDK_DIR}/common")
    message(STATUS "ASIO abilitato (SDK: ${ASIO_SDK_DIR})")
elseif(WIN32)
    message(STATUS "ASIO NON abilitato: mancano gli header dell'SDK Steinberg (vedi README). Resta WASAPI.")
endif()

target_link_libraries(DaniRig PRIVATE
    nam_core
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags)

# ---------------------------------------------------------------------------------------------
# Plugin: VST3 (Windows e Mac) + AU (solo Mac). VST2 non è più disponibile (SDK ritirato da Steinberg).
if(APPLE)
    set(DR_PLUGIN_FORMATS VST3 AU)
else()
    set(DR_PLUGIN_FORMATS VST3)
endif()

juce_add_plugin(DaniRigPlugin
    PRODUCT_NAME "DaniRig"
    COMPANY_NAME "DaniArieta"
    PLUGIN_MANUFACTURER_CODE Dnar
    PLUGIN_CODE Drig
    FORMATS ${DR_PLUGIN_FORMATS}
    VERSION ${PROJECT_VERSION}
    BUNDLE_ID "com.daniarieta.danirig.plugin"
    DESCRIPTION "NAM amp + IR cabinet + FX rig"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Distortion
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(DaniRigPlugin PRIVATE
    Source/plugin/PluginProcessor.cpp
    Source/plugin/PluginEditor.cpp
    ${DR_UI_SOURCES}
    ${DR_DSP_SOURCES})

target_compile_definitions(DaniRigPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_MODAL_LOOPS_PERMITTED=0)

target_link_libraries(DaniRigPlugin PRIVATE
    nam_core
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags)

# ---------------------------------------------------------------------------------------------
if(DANIRIG_BUILD_TESTS)
    juce_add_console_app(DaniRigTests PRODUCT_NAME "DaniRigTests")
    target_sources(DaniRigTests PRIVATE tests/Tests.cpp ${DR_DSP_SOURCES})
    target_include_directories(DaniRigTests PRIVATE Source)
    target_compile_definitions(DaniRigTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(DaniRigTests PRIVATE nam_core juce::juce_audio_devices juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
