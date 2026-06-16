set(TRUST_Include_Dirs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../TRUST
    ${CMAKE_CURRENT_SOURCE_DIR}/../../TRUST/include
)

set(TRUST_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../TRUST/trust.c
)

target_include_directories(stm32cubemx INTERFACE ${TRUST_Include_Dirs})
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${TRUST_Src})
