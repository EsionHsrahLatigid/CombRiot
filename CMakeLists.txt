cmake_minimum_required(VERSION 3.31)

if(APPLE)
    project(CombRiot VERSION 0.1.1 LANGUAGES C CXX OBJC OBJCXX)
else()
    project(CombRiot VERSION 0.1.1 LANGUAGES C CXX)
endif()

option(COMBRIOT_BUILD_PLUGIN "Build the YUP Standalone/VST3/AU targets" ON)
option(COMBRIOT_BUILD_TESTS "Build deterministic DSP regression tests" ON)
option(COMBRIOT_ADHOC_SIGN "Ad-hoc sign local macOS plugin and app bundles after building" ON)
set(COMBRIOT_YUP_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../yup" CACHE PATH
    "Path to a local YUP checkout; fetched from upstream when unavailable")
set(COMBRIOT_CREATE_AU OFF)
if(APPLE)
    set(COMBRIOT_CREATE_AU ON)
endif()

add_library(combriot_engine STATIC
    source/violent/CombRiotEngine.cpp)
target_include_directories(combriot_engine PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/include")
target_compile_features(combriot_engine PUBLIC cxx_std_20)
set_target_properties(combriot_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(combriot_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(COMBRIOT_BUILD_TESTS)
    include(CTest)

    add_executable(combriot_engine_tests
        tests/CombRiotEngineTests.cpp)
    target_link_libraries(combriot_engine_tests PRIVATE combriot_engine)
    target_compile_features(combriot_engine_tests PRIVATE cxx_std_20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(combriot_engine_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    elseif(MSVC)
        target_compile_options(combriot_engine_tests PRIVATE /UNDEBUG)
    endif()
    add_test(NAME combriot_engine_tests COMMAND combriot_engine_tests)
endif()

if(COMBRIOT_BUILD_PLUGIN)
    set(YUP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(YUP_BUILD_PLUGINS OFF CACHE BOOL "" FORCE)
    set(YUP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_PLUGINVAL OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_VST3_VALIDATOR OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_CLAP_VALIDATOR OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_AUVAL_VALIDATOR OFF CACHE BOOL "" FORCE)

    if(EXISTS "${COMBRIOT_YUP_SOURCE_DIR}/CMakeLists.txt")
        add_subdirectory("${COMBRIOT_YUP_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/yup")
    else()
        include(FetchContent)
        FetchContent_Declare(yup
            GIT_REPOSITORY https://github.com/kunitoki/yup.git
            GIT_TAG 9a1c9bc699b6a714f6f52486462d98a140c8bf95)
        FetchContent_MakeAvailable(yup)
    endif()

    if(APPLE)
        include(cmake/YupMacOSIconWorkaround.cmake)
    endif()

    yup_audio_plugin(
        TARGET_NAME combriot
        TARGET_VERSION ${PROJECT_VERSION}
        TARGET_IDE_GROUP "CombRiot"
        TARGET_APP_ID "audio.2bit.combriot"
        TARGET_APP_NAMESPACE "audio.2bit"
        TARGET_CXX_STANDARD 20
        PLUGIN_ID "audio.2bit.CombRiot"
        PLUGIN_NAME "CombRiot"
        PLUGIN_VENDOR "2bit"
        PLUGIN_EMAIL ""
        PLUGIN_VERSION "${PROJECT_VERSION}"
        PLUGIN_DESCRIPTION "Noise-excited fractional comb resonator swarm"
        PLUGIN_URL "https://github.com/2bbb/CombRiot"
        PLUGIN_IS_SYNTH ON
        PLUGIN_IS_MONO OFF
        PLUGIN_CREATE_CLAP OFF
        PLUGIN_CREATE_VST3 ON
        PLUGIN_CREATE_AU ${COMBRIOT_CREATE_AU}
        PLUGIN_AU_SUBTYPE "CmRt"
        PLUGIN_AU_MANUFACTURER "2Bit"
        PLUGIN_AU_SANDBOX_SAFE ON
        PLUGIN_CREATE_AUv3 OFF
        PLUGIN_CREATE_LV2 OFF
        PLUGIN_CREATE_AAX OFF
        PLUGIN_CREATE_STANDALONE ON
        PLUGIN_COPY_AFTER_BUILD OFF
        MODULES
            yup::yup_gui
            yup::yup_audio_processors)

    target_sources(combriot_shared INTERFACE
        source/violent/ParameterGridEditor.cpp
        source/violent/ParameterGridEditor.h
        source/violent/ProductState.h
        source/violent/plugins/CombRiotPlugin.cpp
        source/violent/plugins/CombRiotPlugin.h)
    target_link_libraries(combriot_shared INTERFACE combriot_engine)
    target_include_directories(combriot_shared INTERFACE
        "${CMAKE_CURRENT_LIST_DIR}/include"
        "${CMAKE_CURRENT_LIST_DIR}/source")

    if(APPLE AND COMBRIOT_ADHOC_SIGN)
        find_program(COMBRIOT_CODESIGN_EXECUTABLE codesign REQUIRED)
        foreach(combriot_target IN ITEMS combriot_standalone_plugin combriot_vst3_plugin combriot_au_plugin)
            add_custom_command(TARGET ${combriot_target} POST_BUILD
                COMMAND "${COMBRIOT_CODESIGN_EXECUTABLE}" --force --deep --sign -
                        "$<TARGET_BUNDLE_DIR:${combriot_target}>"
                COMMENT "Ad-hoc signing ${combriot_target} for local loading"
                VERBATIM)
        endforeach()
    endif()
endif()
