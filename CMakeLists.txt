cmake_minimum_required(VERSION 3.20)

project(qutilite VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(qutilite
    src/main.cpp
    src/LocalSimulationEngine.cpp
    src/TerminalVisualizer.cpp
)

target_include_directories(qutilite PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

find_package(CLI11 CONFIG QUIET)

if(CLI11_FOUND)
    target_link_libraries(qutilite PRIVATE CLI11::CLI11)
else()
    add_library(qutilite_cli11 INTERFACE)
    target_include_directories(qutilite_cli11 INTERFACE
        ${PROJECT_SOURCE_DIR}/third_party/CLI11/include
    )
    add_library(CLI11::CLI11 ALIAS qutilite_cli11)
    target_link_libraries(qutilite PRIVATE CLI11::CLI11)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(qutilite PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -O3
        -march=native
        -ffast-math
    )
elseif(MSVC)
    target_compile_options(qutilite PRIVATE
        /W4
        /O2
    )
endif()
