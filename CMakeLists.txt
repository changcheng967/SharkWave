cmake_minimum_required(VERSION 3.20)
project(SharkWave VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_COMPILER_CLANG ON)

# Release optimizations
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -flto")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-flto")

# Source files
set(COMMON_SOURCES
    src/card.cpp
    src/deck.cpp
    src/hand_evaluator.cpp
    src/game_session.cpp
    src/decision_engine.cpp
    src/gto_charts.cpp
    src/simulation.cpp
)

set(COMMON_HEADERS
    src/card.h
    src/deck.h
    src/hand_evaluator.h
    src/game_session.h
    src/decision_engine.h
    src/gto_charts.h
    src/simulation.h
)

# Main CLI executable
add_executable(sharkwave src/main.cpp ${COMMON_SOURCES} ${COMMON_HEADERS})

target_compile_options(sharkwave PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

# Simulation executable
add_executable(sharkwave_sim src/main_sim.cpp ${COMMON_SOURCES} ${COMMON_HEADERS})

target_compile_options(sharkwave_sim PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

# GUI executable (Windows only)
if(WIN32)
    set(GUI_SOURCES src/gui.cpp src/main_gui.cpp)
    add_executable(sharkwave_gui WIN32 ${GUI_SOURCES} ${COMMON_SOURCES} ${COMMON_HEADERS})

    target_compile_definitions(sharkwave_gui PRIVATE
        UNICODE
        _UNICODE
    )

    target_link_libraries(sharkwave_gui
        comctl32
        gdi32
        user32
    )

    target_compile_options(sharkwave_gui PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
endif()

# Windows subsystem:console
if(WIN32)
    set_target_properties(sharkwave PROPERTIES
        WIN32_EXECUTABLE FALSE
    )
    set_target_properties(sharkwave_sim PROPERTIES
        WIN32_EXECUTABLE FALSE
    )
endif()
