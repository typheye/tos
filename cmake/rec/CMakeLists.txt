set(SRE_Include_Dirs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../REC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../REC/include
)

set(SRE_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../REC/init.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../REC/fat.c
)

target_include_directories(stm32cubemx INTERFACE ${SRE_Include_Dirs})
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${SRE_Src})
