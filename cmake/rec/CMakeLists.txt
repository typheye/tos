add_executable(rec
    ${CMAKE_SOURCE_DIR}/startup_stm32f407xx.s
    ${CMAKE_SOURCE_DIR}/partitions/REC/boot.s
    ${CMAKE_SOURCE_DIR}/partitions/REC/runtime.c
    ${CMAKE_SOURCE_DIR}/partitions/REC/init.c
    ${CMAKE_SOURCE_DIR}/partitions/REC/fat.c
    ${CMAKE_SOURCE_DIR}/partitions/REC/msc.c
    ${CMAKE_SOURCE_DIR}/partitions/SBL/common.c
    ${CMAKE_SOURCE_DIR}/partitions/SBL/flash.c
    ${CMAKE_SOURCE_DIR}/partitions/SBL/hw.c
    ${CMAKE_SOURCE_DIR}/partitions/SBL/lcd.c
    ${CMAKE_SOURCE_DIR}/partitions/SBL/state.c
    ${CMAKE_SOURCE_DIR}/Core/Src/sdio.c
    ${CMAKE_SOURCE_DIR}/Core/Src/dma.c
    ${CMAKE_SOURCE_DIR}/Core/Src/stm32f4xx_hal_msp.c
    ${CMAKE_SOURCE_DIR}/USB_DEVICE/Target/usbd_conf.c
    ${CMAKE_SOURCE_DIR}/Core/Src/system_stm32f4xx.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma_ex.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pcd.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pcd_ex.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_sd.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_sdmmc.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_usb.c
    ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_core.c
    ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ctlreq.c
    ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ioreq.c
    ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FatFs/src/diskio.c
    ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FatFs/src/ff.c
    ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FatFs/src/ff_gen_drv.c
    ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FatFs/src/option/syscall.c
    ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FatFs/src/option/ccsbcs.c)
tos_partition_target(rec ld/rec.ld rec)
target_compile_definitions(rec PRIVATE REC_IMAGE USER_VECT_TAB_ADDRESS VECT_TAB_OFFSET=0x00010000U)

# Keep the vendor HAL source unchanged while bounding only REC's ACMD41 loop.
# The forced header is attached to this source in the REC directory scope and
# is not applied to SYSTEM or to any CubeMX-generated FatFs source.
set_property(SOURCE
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_sd.c
    APPEND PROPERTY COMPILE_OPTIONS
    -include${CMAKE_SOURCE_DIR}/partitions/REC/include/rec_hal_sd_limits.h)
target_link_libraries(rec PRIVATE stm32cubemx ${TOOLCHAIN_LINK_LIBRARIES})
target_link_options(rec PRIVATE -nostartfiles)
