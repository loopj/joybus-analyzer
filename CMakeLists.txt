cmake_minimum_required(VERSION 3.13)

project(JoybusAnalyzer)

add_definitions(-DLOGIC2)

# enable generation of compile_commands.json, helpful for IDEs to locate include files.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# custom CMake Modules are located in the cmake directory.
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

include(ExternalAnalyzerSDK)

set(SOURCES
    src/JoybusAnalyzer.cpp
    src/JoybusAnalyzer.h
    src/JoybusAnalyzerResults.cpp
    src/JoybusAnalyzerResults.h
    src/JoybusAnalyzerSettings.cpp
    src/JoybusAnalyzerSettings.h
    src/JoybusSimulationDataGenerator.cpp
    src/JoybusSimulationDataGenerator.h
)

add_analyzer_plugin(${PROJECT_NAME} SOURCES ${SOURCES})

# Build the decoder against the SDK test harness so it can be driven from synthetic waveforms
enable_testing()

set(TEST_HARNESS_DIR ${analyzersdk_SOURCE_DIR}/testlib)

set(TEST_HARNESS_SOURCES
    ${TEST_HARNESS_DIR}/AnalyzerStubs.cpp
    ${TEST_HARNESS_DIR}/HelperStubs.cpp
    ${TEST_HARNESS_DIR}/MockChannelData.cpp
    ${TEST_HARNESS_DIR}/MockResults.cpp
    ${TEST_HARNESS_DIR}/MockSimulatedChannelDescriptor.cpp
    ${TEST_HARNESS_DIR}/SettingsStubs.cpp
    ${TEST_HARNESS_DIR}/StreamHelpers.cpp
    ${TEST_HARNESS_DIR}/TestInstance.cpp
)

# Force <stdexcept> into the harness, SettingsStubs.cpp throws std::runtime_error
# without including it and only libc++ pulls the header in on its own
if(MSVC)
    set(TEST_HARNESS_FORCE_INCLUDE /FIstdexcept)
else()
    set(TEST_HARNESS_FORCE_INCLUDE -include stdexcept)
endif()

set_source_files_properties(
    ${TEST_HARNESS_SOURCES}
    PROPERTIES COMPILE_OPTIONS "${TEST_HARNESS_FORCE_INCLUDE}"
)

add_executable(
    JoybusAnalyzerTests
    tests/TestJoybusAnalyzer.cpp
    ${SOURCES}
    ${TEST_HARNESS_SOURCES}
)

target_include_directories(
    JoybusAnalyzerTests
    PRIVATE src ${TEST_HARNESS_DIR} ${analyzersdk_SOURCE_DIR}/include
)

add_test(NAME JoybusAnalyzerTests COMMAND JoybusAnalyzerTests)
