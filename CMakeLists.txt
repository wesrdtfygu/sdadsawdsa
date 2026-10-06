cmake_minimum_required(VERSION 3.25)
project(StrafeHelperRemake VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
    message(FATAL_ERROR "This project targets Windows.")
endif()

add_executable(StrafeHelper WIN32
    src/main.cpp
    src/App.cpp
    src/App.h
    src/Config.cpp
    src/Config.h
    src/MovementTrainer.cpp
    src/MovementTrainer.h
)

target_compile_definitions(StrafeHelper PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

target_link_libraries(StrafeHelper PRIVATE
    user32
    gdi32
    shell32
)

if(MSVC)
    target_compile_options(StrafeHelper PRIVATE /W4 /permissive-)
endif()

enable_testing()
add_executable(StrafeHelperTests
    tests/MovementTrainerTests.cpp
    src/MovementTrainer.cpp
)
target_include_directories(StrafeHelperTests PRIVATE src)
if(MSVC)
    target_compile_options(StrafeHelperTests PRIVATE /W4 /permissive-)
endif()
add_test(NAME MovementTrainerTests COMMAND StrafeHelperTests)
