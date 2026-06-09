cmake_minimum_required(VERSION 3.24)

project(GeodeMacroBot)

# Подключаем Geode SDK
include(${CMAKE_CURRENT_SOURCE_DIR}/build/geode-deps/Geode/Geode.cmake)
find_package(Geode REQUIRED)

setup_geode_mod(${PROJECT_NAME})
