cmake_minimum_required(VERSION 3.16)

project(ProjectEthos LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(ETHOS_CORE_SOURCES
    Src/emu/Console/Console.c
    Src/emu/Core/Emulator.c
    Src/emu/Core/Logger.c
    Src/emu/FIles/FIles.c
    Src/emu/Mem/Mem.c
)

add_library(ethos_core STATIC ${ETHOS_CORE_SOURCES})

target_include_directories(ethos_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Src/emu/Console
        ${CMAKE_CURRENT_SOURCE_DIR}/Src/emu/Core
        ${CMAKE_CURRENT_SOURCE_DIR}/Src/emu/FIles
        ${CMAKE_CURRENT_SOURCE_DIR}/Src/emu/Mem
        ${CMAKE_CURRENT_SOURCE_DIR}/Src/sys/Memory
        ${CMAKE_CURRENT_SOURCE_DIR}/Src/sys/Resouces
        ${CMAKE_CURRENT_SOURCE_DIR}/Src/sys/Scheduler
        ${CMAKE_CURRENT_SOURCE_DIR}/Src/sys/SysCalls
)

if(MSVC)
    target_compile_options(ethos_core PRIVATE /W4)
else()
    target_compile_options(ethos_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(project_ethos Src/main.c)
target_link_libraries(project_ethos PRIVATE ethos_core)