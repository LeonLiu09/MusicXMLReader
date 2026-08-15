cmake_minimum_required(VERSION 3.20)

project(MusicXMLReader LANGUAGES CXX)

add_executable(MusicXMLReader
    main.cpp
    lib/pugixml/pugixml.cpp
)

target_include_directories(MusicXMLReader PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/pugixml"
)

target_compile_features(MusicXMLReader PRIVATE cxx_std_17)