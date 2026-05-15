cmake_minimum_required(VERSION 3.10)
project(Josephus_Simple)

find_package(SFML 2.5 COMPONENTS graphics window system audio REQUIRED)

add_executable(Josephus_Simple main.cpp)

target_link_libraries(Josephus_Simple sfml-graphics sfml-window sfml-system sfml-audio)
