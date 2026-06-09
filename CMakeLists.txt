cmake_minimum_required(VERSION 3.24)

# Создаем проект мода
project(GeodeMacroBot)

# Находим Geode SDK
find_package(Geode REQUIRED)

# Указываем, какие файлы компилировать
add_geode_mod(
    ${PROJECT_NAME}
    VERSION v1.0.0
    CONTAINS src/main.cpp
)
