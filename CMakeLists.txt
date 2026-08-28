cmake_minimum_required(VERSION 3.20)

project(MusicXMLReader LANGUAGES CXX)

add_executable(MusicXMLReader
    main.cpp
)

target_compile_features(MusicXMLReader PRIVATE cxx_std_17)
