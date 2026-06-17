add_executable(sah ${CMAKE_SOURCE_DIR}/part/SAH/logo.c)
target_include_directories(sah PRIVATE ${TOS_COMMON_INCLUDES})
target_compile_definitions(sah PRIVATE USE_HAL_DRIVER STM32F407xx)
target_compile_options(sah PRIVATE -ffunction-sections -fdata-sections)
target_link_options(sah PRIVATE -nostdlib -nostartfiles -Wl,-e,0
    -T${CMAKE_SOURCE_DIR}/ld/sah.ld -Wl,-Map=sah.map -Wl,--gc-sections)
set_target_properties(sah PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    OUTPUT_NAME sah ADDITIONAL_CLEAN_FILES sah.map)
