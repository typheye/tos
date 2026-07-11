# Shared runtime library (common, flash, hw, lcd, state) — also used by REC
add_library(sbl_runtime OBJECT
    ${CMAKE_SOURCE_DIR}/partitions/SBL/source/common.c
    ${CMAKE_SOURCE_DIR}/partitions/SBL/source/flash.c
    ${CMAKE_SOURCE_DIR}/partitions/SBL/source/hw.c
    ${CMAKE_SOURCE_DIR}/partitions/SBL/source/lcd.c
    ${CMAKE_SOURCE_DIR}/partitions/SBL/source/state.c)
target_include_directories(sbl_runtime PUBLIC ${TOS_COMMON_INCLUDES})
target_compile_definitions(sbl_runtime PRIVATE USE_HAL_DRIVER STM32F407xx)
target_compile_options(sbl_runtime PRIVATE -Os -ffunction-sections -fdata-sections)

add_executable(sbl
    ${CMAKE_SOURCE_DIR}/startup_stm32f407xx.s
    ${CMAKE_SOURCE_DIR}/partitions/SBL/boot.s
    ${CMAKE_SOURCE_DIR}/partitions/SBL/source/init.c
    ${CMAKE_SOURCE_DIR}/partitions/SBL/source/runtime.c
    ${CMAKE_SOURCE_DIR}/partitions/SBL/source/splash.c
    ${CMAKE_SOURCE_DIR}/partitions/SBL/source/trust.c
    ${CMAKE_SOURCE_DIR}/partitions/SBL/source/ui.c
    ${CMAKE_SOURCE_DIR}/partitions/SBL/source/usb.c
    ${CMAKE_SOURCE_DIR}/partitions/SBL/source/image_header.c
    ${CMAKE_SOURCE_DIR}/partitions/ELF/source/sha256.c
    ${CMAKE_SOURCE_DIR}/partitions/ELF/source/verify.c
    ${CMAKE_SOURCE_DIR}/partitions/ELF/source/p256-m.c
    ${CMAKE_SOURCE_DIR}/USB_DEVICE/Target/usbd_conf.c
    ${CMAKE_SOURCE_DIR}/Core/Src/system_stm32f4xx.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pcd.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pcd_ex.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_usb.c
    ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_core.c
    ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ctlreq.c
    ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ioreq.c)
target_link_libraries(sbl PRIVATE sbl_runtime stm32cubemx ${TOOLCHAIN_LINK_LIBRARIES})
tos_partition_target(sbl ld/SBL.ld sbl)
target_compile_definitions(sbl PRIVATE USER_VECT_TAB_ADDRESS VECT_TAB_OFFSET=0x00004000U)
target_link_options(sbl PRIVATE -nostartfiles -Wl,-z,max-page-size=0x4000)
