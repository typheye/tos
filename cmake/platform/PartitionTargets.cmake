set(TOS_ROOT ${CMAKE_SOURCE_DIR})
set(TOS_PART_ROOT ${TOS_ROOT}/partitions)
set(TOS_COMMON_INCLUDES
    ${TOS_PART_ROOT}/common/include
    ${TOS_ROOT}/Core/Inc
    ${TOS_ROOT}/FATFS/Target
    ${TOS_ROOT}/FATFS/App
    ${TOS_ROOT}/USB_DEVICE/App
    ${TOS_ROOT}/USB_DEVICE/Target
    ${TOS_ROOT}/Drivers/STM32F4xx_HAL_Driver/Inc
    ${TOS_ROOT}/Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
    ${TOS_ROOT}/Middlewares/Third_Party/FatFs/src
    ${TOS_ROOT}/Middlewares/ST/STM32_USB_Device_Library/Core/Inc
    ${TOS_ROOT}/Middlewares/ST/STM32_USB_Device_Library/Class/CustomHID/Inc
    ${TOS_ROOT}/Drivers/CMSIS/Device/ST/STM32F4xx/Include
    ${TOS_ROOT}/Drivers/CMSIS/Include
    ${TOS_PART_ROOT}/SBL/include
    ${TOS_PART_ROOT}/REC/include
    ${TOS_PART_ROOT}/SAH/include
    ${TOS_PART_ROOT}/ELF/include
    ${TOS_PART_ROOT}/TEE/include
)

function(tos_partition_target target linker map_name)
  target_include_directories(${target} PRIVATE ${TOS_COMMON_INCLUDES})
  target_compile_definitions(${target} PRIVATE USE_HAL_DRIVER STM32F407xx)
  # Boot partitions are size-constrained and favor predictable code size over
  # SYSTEM's throughput-oriented global -O3 setting.
  target_compile_options(${target} PRIVATE -Os -ffunction-sections -fdata-sections)
  target_link_options(${target} PRIVATE
      -T${TOS_ROOT}/${linker}
      -Wl,-Map=${map_name}.map
      -Wl,--gc-sections
      -Wl,--print-memory-usage)
  set_target_properties(${target} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
      OUTPUT_NAME ${target}
      ADDITIONAL_CLEAN_FILES ${map_name}.map)
endfunction()
