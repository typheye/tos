set(SAH_Include_Dirs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../SAH
    ${CMAKE_CURRENT_SOURCE_DIR}/../../SAH/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../TRUST
    ${CMAKE_CURRENT_SOURCE_DIR}/../../TRUST/include
)

set(SAH_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../SAH/init.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../SAH/logo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../TRUST/trust.c
)

target_include_directories(stm32cubemx INTERFACE ${SAH_Include_Dirs})
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${SAH_Src})
