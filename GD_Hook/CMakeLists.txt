cmake_minimum_required(VERSION 3.15)
project(GD_Speedhack_Hook)

# GD - это 32-битное приложение (x86)
set(CMAKE_CXX_STANDARD 17)

# Автоматическое скачивание и подключение библиотеки MinHook
include(FetchContent)
FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(minhook)

# Собираем DLL
add_library(SpeedhackHook SHARED src/dllmain.cpp)

# Линкуем MinHook
target_link_libraries(SpeedhackHook PRIVATE minhook)
