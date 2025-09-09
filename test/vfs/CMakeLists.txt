cmake_minimum_required(VERSION 3.15)

project(vfs_test VERSION 1.0 LANGUAGES C)

# Command line arguments with default values for interface mcu and target mcu
set(BUILD_NAME "unamed" CACHE STRING "Unique name for the build, e.g. nrf52820_microbit")
set(INTERFACE_MCU "kl27z" CACHE STRING "DAPLink Interface name, e.g. nrf52820, kl27z, kl26z, etc.")
set(TARGET_FILE "target_nrf52833.c" CACHE STRING "C file with the target DAPLink config, e.g. target_nrf52833.c.")
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${TARGET_FILE}")
    message(FATAL_ERROR "Target file ${TARGET_FILE} does not exist for INTERFACE_MCU: ${INTERFACE_MCU}")
endif()
message(STATUS "Building BL & IF for ${INTERFACE_MCU} with target ${TARGET_FILE}")

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

string(TOUPPER ${INTERFACE_MCU} INTERFACE_MCU_UPPER)
string(TOUPPER ${BUILD_NAME} BUILD_NAME_UPPER)

add_executable(vfs_test_${BUILD_NAME}_bl ${SOURCES} ${TARGET_FILE})
target_compile_definitions(vfs_test_${BUILD_NAME}_bl PUBLIC -DINTERFACE_${INTERFACE_MCU_UPPER} -DBUILD_NAME_${BUILD_NAME_UPPER} ${BL_DEFS})
add_executable(vfs_test_${BUILD_NAME}_if ${SOURCES} target_flash.c ${TARGET_FILE})
target_compile_definitions(vfs_test_${BUILD_NAME}_if PUBLIC -DINTERFACE_${INTERFACE_MCU_UPPER} -DBUILD_NAME_${BUILD_NAME_UPPER} ${IF_DEFS})
