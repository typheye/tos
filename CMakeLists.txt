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

include(cmake/platform/PartitionTargets.cmake)

# SYSTEM remains the CubeMX-owned application target. Generated sources are
# not copied into the boot partitions and are left untouched.
add_executable(${CMAKE_PROJECT_NAME})
add_subdirectory(cmake/stm32cubemx)
target_include_directories(stm32cubemx INTERFACE ${CMAKE_SOURCE_DIR}/partitions/common/include)
add_subdirectory(cmake/user)
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME system)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    USER_VECT_TAB_ADDRESS VECT_TAB_OFFSET=0x00040000U)
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    -T${CMAKE_SOURCE_DIR}/ld/system.ld
    -Wl,-Map=system.map -Wl,--gc-sections -Wl,--print-memory-usage)
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ADDITIONAL_CLEAN_FILES system.map)
add_custom_target(system DEPENDS ${CMAKE_PROJECT_NAME})

add_subdirectory(cmake/elf)
add_subdirectory(cmake/tee)
add_subdirectory(cmake/sbl)
add_subdirectory(cmake/rec)
add_subdirectory(cmake/sah)

add_custom_target(export_partitions
    COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File
            ${CMAKE_SOURCE_DIR}/scripts/export_partitions.ps1
            -BuildDir ${CMAKE_BINARY_DIR}
    DEPENDS sbl tee rec sah ${CMAKE_PROJECT_NAME}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_custom_target(factory_images
    DEPENDS elf_stage export_partitions)

if(CMAKE_HOST_WIN32)
  add_custom_target(flash_system
      COMMAND cmd /c ${CMAKE_SOURCE_DIR}/scripts/upload.bat system
      DEPENDS factory_images)
  add_custom_target(flash_runtime
      COMMAND cmd /c ${CMAKE_SOURCE_DIR}/scripts/upload.bat runtime
      DEPENDS factory_images)
  add_custom_target(flash_factory
      COMMAND cmd /c ${CMAKE_SOURCE_DIR}/scripts/upload.bat factory
      DEPENDS factory_images)
endif()

list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)
