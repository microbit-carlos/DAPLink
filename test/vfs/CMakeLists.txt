cmake_minimum_required(VERSION 3.15)

project(vfs_test VERSION 1.0 LANGUAGES C)

# Command line arguments with default values for interface mcu and target mcu
set(BUILD_NAME "unamed" CACHE STRING "Unique name for the build, e.g. nrf52820_microbit")
set(INTERFACE_MCU "kl27z" CACHE STRING "DAPLink Interface name, e.g. nrf52820, kl27z, kl26z, etc.")
set(EXTRA_INCLUDE_DIRS "" CACHE STRING "Additional include directories (semicolon-separated)")
set(EXTRA_SOURCES "" CACHE STRING "Additional source files (semicolon-separated)")
message(STATUS "Building BL & IF for ${INTERFACE_MCU}")
message(STATUS "\tExtra includes: ${EXTRA_INCLUDE_DIRS}")
message(STATUS "\tExtra sources: ${EXTRA_SOURCES}")

add_compile_definitions(DAPLINK_DEBUG DAPLINK_DEBUG_STDOUT
    DEBUG_FLASH_MANAGER=1 DEBUG_INTELHEX=1 DEBUG_IAP_FLASH=1
    DAPLINK_DISABLE_STATIC_CHECKS DAPLINK_VFS_TESTS)

set(IF_DEFS DAPLINK_IF DAPLINK_BUILD_KEY=DAPLINK_BUILD_KEY_IF)
set(BL_DEFS DAPLINK_BL DAPLINK_BUILD_KEY=DAPLINK_BUILD_KEY_BL)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/daplink
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/daplink/drag-n-drop
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/daplink/settings
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/target
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/cmsis-core
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/rtos_none
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/hic_hal)
if(EXTRA_INCLUDE_DIRS)
    foreach(extra_dir ${EXTRA_INCLUDE_DIRS})
        if(NOT IS_DIRECTORY "${extra_dir}")
            message(FATAL_ERROR "EXTRA_INCLUDE_DIRS: Directory '${extra_dir}' does not exist!")
        endif()
        include_directories(${extra_dir})
    endforeach()
endif()

add_compile_options(-Wall)

set(SOURCES main.c intf_flash.c util.c
    ../../source/daplink/crc32.c
    ../../source/daplink/daplink_debug.c
    ../../source/daplink/drag-n-drop/iap_flash_intf.c
    ../../source/daplink/drag-n-drop/intelhex.c
    ../../source/daplink/drag-n-drop/file_stream.c
    ../../source/daplink/drag-n-drop/flash_decoder.c
    ../../source/daplink/drag-n-drop/flash_intf.c
    ../../source/daplink/drag-n-drop/flash_manager.c
    ../../source/daplink/drag-n-drop/virtual_fs.c
    ../../source/daplink/error.c
    ../../source/daplink/validation.c
    ../../source/target/target_default.c)
if(EXTRA_SOURCES)
    foreach(extra_src ${EXTRA_SOURCES})
        if(NOT EXISTS "${extra_src}")
            message(FATAL_ERROR "EXTRA_SOURCES: File '${extra_src}' does not exist!")
        endif()
        list(APPEND SOURCES ${extra_src})
    endforeach()
endif()

string(TOUPPER ${INTERFACE_MCU} INTERFACE_MCU_UPPER)
string(TOUPPER ${BUILD_NAME} BUILD_NAME_UPPER)

add_executable(vfs_test_${BUILD_NAME}_bl ${SOURCES})
target_compile_definitions(vfs_test_${BUILD_NAME}_bl PUBLIC -DINTERFACE_${INTERFACE_MCU_UPPER} -DBUILD_NAME_${BUILD_NAME_UPPER} ${BL_DEFS})
add_executable(vfs_test_${BUILD_NAME}_if ${SOURCES} target_flash.c)
target_compile_definitions(vfs_test_${BUILD_NAME}_if PUBLIC -DINTERFACE_${INTERFACE_MCU_UPPER} -DBUILD_NAME_${BUILD_NAME_UPPER} ${IF_DEFS})
