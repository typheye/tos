# 告诉CMake自定义头文件的搜索路径
set(USER_Include_Dirs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/hardware/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/library/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/include/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/manager/include/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/sys/include/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/sdk/include/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/library/include/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gui/demo/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gui/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gui/settings/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gui/components/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gui/miniapp/file_manager/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gui/miniapp/hid_tools/
)

# 列出所有需要编译的自定义源文件
set(USER_Application_Src
    # 核心系统文件
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/tos.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/sys/sysfonts.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/sys/syscalls_override.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/sys/syshandle.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/sys/syswatchdog.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/manager/file_manager.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/manager/settings_manager.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/manager/network_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/manager/emotion_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/manager/hid_manager.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/sys/syslog.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/sys/systime.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/sdk/tos_api.cpp
    
    # 硬件驱动文件
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/hardware/led.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/hardware/usart.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/hardware/buzzer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/hardware/lcd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/hardware/key.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/hardware/trtc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/hardware/tsdio.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/hardware/jy901s.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/hardware/bmp180.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/hardware/esp8266.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/hardware/tcs3472.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/hardware/sn74hc00n.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/hardware/pot.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/hardware/sfhd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/hardware/thid.cpp
    
    # 库文件
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/library/lib3dox.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/library/lib3dgyro.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/library/libfs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/library/libdly.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/library/libvan.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/library/libpd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/library/libemo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/library/libui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/library/libehw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/library/libjson.c

    # Demo 活动文件
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gui/demo/jyro_activity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gui/demo/key_activity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gui/demo/i2c_activity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gui/demo/bmp_activity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gui/demo/3dox_activity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gui/demo/tcs3472_activity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gui/demo/sn74hc00n_activity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gui/demo/demo_activity.cpp

    # GUI文件
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gui/sysui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gui/launcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gui/settings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gui/settings/wlan_activity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gui/settings/hotspot_activity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gui/settings/storage_activity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gui/settings/display_activity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gui/settings/sound_activity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gui/settings/time_activity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gui/settings/about_activity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gui/components/keyboard.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gui/components/alert.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gui/components/confirm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gui/components/status_icons.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gui/miniapp/file_manager/app.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gui/miniapp/hid_tools/app.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gui/miniapp/hid_tools/pages.cpp

)


# FatFs long-file-name codepage helper is generated by CubeMX.
# Do not add src/hardware/ffunicode.c here, otherwise it duplicates
# ff_convert()/ff_wtoupper() from Middlewares/Third_Party/FatFs/src/option/ccsbcs.c.


# 将头文件路径添加到全局包含目录中
target_include_directories(stm32cubemx INTERFACE ${USER_Include_Dirs})

# 将源文件添加到主项目目标中，让它们被编译和链接
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${USER_Application_Src})

# 编译器优化选项
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -ffast-math -fsingle-precision-constant")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -ffast-math -fsingle-precision-constant")

# FPU 设置
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfloat-abi=hard -mfpu=fpv4-sp-d16")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfloat-abi=hard -mfpu=fpv4-sp-d16")

# 可选：添加调试符号（开发时使用）
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

# 可选：启用更多警告
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

# 可选：禁用特定的警告（根据需要）
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-parameter")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter")
