cmake_minimum_required(VERSION 3.12)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(picoluainit C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(LUA_SOURCE_DIR lua-5.4.6)
set(LITTLEFS_SOURCE_DIR littlefs-2.6.1)


set(FS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/files/)
set(FS_ELF ${CMAKE_CURRENT_BINARY_DIR}/files.elf)
set(FS_BLOCKS 16)

include_directories(${LUA_SOURCE_DIR}/src/)
include_directories(${LITTLEFS_SOURCE_DIR}/)
include_directories(${CMAKE_C_STANDARD_INCLUDE_DIRECTORIES}/)

FILE(GLOB Files ${FS_DIR}/*)

add_custom_command(OUTPUT ${FS_ELF} COMMAND "../scripts/files2elf.sh" ${FS_BLOCKS} ${FS_DIR} ${FS_ELF} DEPENDS ${Files} "../scripts/files2elf.sh")
add_custom_target(fs_elf ALL DEPENDS ${FS_ELF})

pico_sdk_init()

if (TARGET tinyusb_device)
    FILE(GLOB LuaSources ${LUA_SOURCE_DIR}/src/*.c)
    FILE(GLOB LittleFSSources ${LITTLEFS_SOURCE_DIR}/*.c)

    add_executable(picoluainit
            init.c
            ${LuaSources}
            ${LittleFSSources}
            )
    # add_definitions(-DREENTRANT_SYSCALLS_PROVIDED)
    add_definitions(-DLFS_YES_TRACE)
    add_dependencies(picoluainit fs_elf)

    target_link_directories(picoluainit PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    pico_set_linker_script(picoluainit ${CMAKE_SOURCE_DIR}/memmap_default.ld)
    pico_add_link_depend(picoluainit ${FS_ELF} ${CMAKE_SOURCE_DIR}/memmap_default.ld)
    target_link_libraries(picoluainit pico_stdlib hardware_dma ${FS_ELF})

    # enable usb output, disable uart output
    pico_enable_stdio_usb(picoluainit 1)
    pico_enable_stdio_uart(picoluainit 0)

    # create map/bin/hex/uf2 file etc.
    pico_add_extra_outputs(picoluainit)
elseif(PICO_ON_DEVICE)
    message(WARNING "not building picoluainit because TinyUSB submodule is not initialized in the SDK")
endif()
