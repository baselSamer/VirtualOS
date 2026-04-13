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
        ${CMAKE_CURRENT_SOURCE_DIR}/Src/sys/Memory Manger
        ${CMAKE_CURRENT_SOURCE_DIR}/Src/sys/Resouces
        ${CMAKE_CURRENT_SOURCE_DIR}/Src/sys/Schduler
        ${CMAKE_CURRENT_SOURCE_DIR}/Src/sys/SysCalls
)

if(MSVC)
    target_compile_options(ethos_core PRIVATE /W4)
else()
    target_compile_options(ethos_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

set(ETHOS_MAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/Src/main.c)
if(EXISTS ${ETHOS_MAIN_FILE})
    file(READ ${ETHOS_MAIN_FILE} ETHOS_MAIN_CONTENTS)
    string(FIND "${ETHOS_MAIN_CONTENTS}" "main(" ETHOS_MAIN_FN_POS)

    if(NOT ETHOS_MAIN_FN_POS EQUAL -1)
        add_executable(project_ethos Src/main.c)
        target_link_libraries(project_ethos PRIVATE ethos_core)
    endif()
endif()