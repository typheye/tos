# 告诉CMake自定义头文件的搜索路径
set(USER_Include_Dirs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/hardware/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/library/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/demo/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gui/
)

# 列出所有需要编译的自定义源文件
set(USER_Application_Src
    # 核心系统文件
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/tos.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/sysfonts.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/syscalls_override.c
    
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
    
    # 库文件
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/library/lib3dox.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/library/lib3dgyro.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/library/libfs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/library/libdly.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/library/libvan.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/library/libpd.c
    
    # Demo 活动文件
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/demo/jyro_activity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/demo/key_activity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/demo/sd_activity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/demo/i2c_activity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/demo/display_activity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/demo/bmp_activity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/demo/3dox_activity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/demo/esp8266_activity.cpp
    
    # GUI文件
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gui/sysui.cpp

)

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