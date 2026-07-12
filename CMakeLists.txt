cmake_minimum_required(VERSION 3.22)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()
set(CMAKE_PROJECT_NAME tos)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
project(${CMAKE_PROJECT_NAME} LANGUAGES C CXX ASM)
message("Build type: ${CMAKE_BUILD_TYPE}")

set(TOS_IMAGE_VERSION "1" CACHE STRING "Secure boot image version")
set(TOS_SIGNING_KEY "$ENV{TOS_SIGNING_KEY}" CACHE FILEPATH
    "PKCS#8 P-256 signing key; project-local development key when empty")
set(TOS_DEVELOPMENT_SIGNING_KEY
    "${CMAKE_SOURCE_DIR}/sign/development-signing-key.pk8")
if(NOT TOS_SIGNING_KEY OR
   TOS_SIGNING_KEY STREQUAL
       "${CMAKE_BINARY_DIR}/secure-boot/development-signing-key.pk8")
  set(TOS_SIGNING_KEY "${TOS_DEVELOPMENT_SIGNING_KEY}" CACHE FILEPATH
      "PKCS#8 P-256 signing key" FORCE)
endif()
set(TOS_SECURE_BOOT_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated/secure_boot")
set(TOS_SECURE_BOOT_KEY_HEADER "${TOS_SECURE_BOOT_GENERATED_DIR}/tos_secure_boot_key.h")
set(TOS_SECURE_BOOT_KEY_PATH_FILE "${CMAKE_BINARY_DIR}/secure-boot/signing-key.path")
execute_process(
    COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File
            ${CMAKE_SOURCE_DIR}/scripts/keygen.ps1
            -OutputHeader ${TOS_SECURE_BOOT_KEY_HEADER}
            -KeyPath ${TOS_SIGNING_KEY}
            -KeyPathFile ${TOS_SECURE_BOOT_KEY_PATH_FILE}
    RESULT_VARIABLE TOS_SECURE_BOOT_KEY_RESULT)
if(NOT TOS_SECURE_BOOT_KEY_RESULT EQUAL 0)
  message(FATAL_ERROR "Secure boot key preparation failed")
endif()

include(cmake/PartitionTargets.cmake)

# SYSTEM remains the CubeMX-owned application target. Generated sources are
# not copied into the boot partitions and are left untouched.
add_executable(${CMAKE_PROJECT_NAME})
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/partitions/SBL/source/image_header.c)
add_subdirectory(cmake/stm32cubemx)
target_include_directories(stm32cubemx INTERFACE ${CMAKE_SOURCE_DIR}/partitions)
add_subdirectory(cmake/user)
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME system)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    USER_VECT_TAB_ADDRESS VECT_TAB_OFFSET=0x00040000U)
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    -T${CMAKE_SOURCE_DIR}/ld/SYSTEM.ld
    -Wl,-Map=system.map -Wl,--gc-sections -Wl,--print-memory-usage
    -Wl,-z,max-page-size=0x4000)
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ADDITIONAL_CLEAN_FILES system.map)
add_custom_target(system DEPENDS ${CMAKE_PROJECT_NAME})

add_subdirectory(cmake/partitions)

add_custom_target(export_partitions
    COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File
            ${CMAKE_SOURCE_DIR}/scripts/export.ps1
            -BuildDir ${CMAKE_BINARY_DIR}
    DEPENDS sbl rec ${CMAKE_PROJECT_NAME}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_custom_target(factory_images
    DEPENDS elf_stage export_partitions)

if(CMAKE_HOST_WIN32)
  add_custom_target(flash_system
      COMMAND cmd /c ${CMAKE_SOURCE_DIR}/scripts/flash.bat system
      DEPENDS factory_images)
  add_custom_target(flash_runtime
      COMMAND cmd /c ${CMAKE_SOURCE_DIR}/scripts/flash.bat runtime
      DEPENDS factory_images)
  add_custom_target(flash_factory
      COMMAND cmd /c ${CMAKE_SOURCE_DIR}/scripts/flash.bat factory
      DEPENDS factory_images)
endif()

list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)
