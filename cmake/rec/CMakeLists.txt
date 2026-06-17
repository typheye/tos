set(REC_Include_Dirs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../REC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../REC/include
)

set(REC_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../REC/init.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../REC/fat.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../REC/msc.c
)

target_include_directories(stm32cubemx INTERFACE ${REC_Include_Dirs})
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${REC_Src})
