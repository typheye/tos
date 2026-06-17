add_executable(sbl
    ${CMAKE_SOURCE_DIR}/startup_stm32f407xx.s
    ${CMAKE_SOURCE_DIR}/partitions/SBL/boot.s
    ${CMAKE_SOURCE_DIR}/partitions/SBL/init.c
    ${CMAKE_SOURCE_DIR}/partitions/SBL/runtime.c
    ${CMAKE_SOURCE_DIR}/partitions/SBL/common.c
    ${CMAKE_SOURCE_DIR}/partitions/SBL/flash.c
    ${CMAKE_SOURCE_DIR}/partitions/SBL/hw.c
    ${CMAKE_SOURCE_DIR}/partitions/SBL/lcd.c
    ${CMAKE_SOURCE_DIR}/partitions/SBL/splash.c
    ${CMAKE_SOURCE_DIR}/partitions/SBL/state.c
    ${CMAKE_SOURCE_DIR}/partitions/SBL/trust.c
    ${CMAKE_SOURCE_DIR}/partitions/SBL/ui.c
    ${CMAKE_SOURCE_DIR}/partitions/SBL/usb.c
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
tos_partition_target(sbl ld/sbl.ld sbl)
target_compile_definitions(sbl PRIVATE USER_VECT_TAB_ADDRESS VECT_TAB_OFFSET=0x00004000U)
target_link_libraries(sbl PRIVATE stm32cubemx ${TOOLCHAIN_LINK_LIBRARIES})
target_link_options(sbl PRIVATE -nostartfiles)
