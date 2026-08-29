cmake_minimum_required(VERSION 3.20)

project(MusicXMLReader VERSION 2.0 LANGUAGES CXX)

add_executable(MusicXMLReader
    main.cpp
)

target_compile_features(MusicXMLReader PRIVATE cxx_std_17)

enable_testing()

add_executable(MusicXMLReaderTests
    tests/test_musicxml_converter.cpp
)

target_compile_features(MusicXMLReaderTests PRIVATE cxx_std_17)

add_test(
    NAME MusicXMLReaderTests
    COMMAND MusicXMLReaderTests
)

set_tests_properties(MusicXMLReaderTests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
