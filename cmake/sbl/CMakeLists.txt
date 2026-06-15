set(SBL_Include_Dirs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../SBL
    ${CMAKE_CURRENT_SOURCE_DIR}/../../SBL/include
)

set(SBL_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../SBL/init.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../SBL/common.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../SBL/hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../SBL/lcd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../SBL/mem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../SBL/state.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../SBL/ui.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/sys/sysdram.c
)

target_include_directories(stm32cubemx INTERFACE ${SBL_Include_Dirs})
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${SBL_Src})
