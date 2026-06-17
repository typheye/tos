add_executable(tee ${CMAKE_SOURCE_DIR}/part/TEE/init.c)
target_include_directories(tee PRIVATE ${TOS_COMMON_INCLUDES})
target_compile_definitions(tee PRIVATE USE_HAL_DRIVER STM32F407xx)
target_compile_options(tee PRIVATE -ffunction-sections -fdata-sections)
target_link_options(tee PRIVATE -nostdlib -nostartfiles -Wl,-e,0
    -T${CMAKE_SOURCE_DIR}/ld/tee.ld -Wl,-Map=tee.map -Wl,--gc-sections)
set_target_properties(tee PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    OUTPUT_NAME tee ADDITIONAL_CLEAN_FILES tee.map)
