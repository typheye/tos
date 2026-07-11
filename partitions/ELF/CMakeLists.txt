add_executable(elf_stage
    ${CMAKE_SOURCE_DIR}/startup_stm32f407xx.s
    ${CMAKE_SOURCE_DIR}/partitions/ELF/boot.s
    ${CMAKE_SOURCE_DIR}/partitions/ELF/source/init.c
    ${CMAKE_SOURCE_DIR}/partitions/ELF/source/cust.c
    ${CMAKE_SOURCE_DIR}/partitions/ELF/source/sha256.c
    ${CMAKE_SOURCE_DIR}/partitions/ELF/source/verify.c
    ${CMAKE_SOURCE_DIR}/partitions/ELF/source/p256-m.c
    ${CMAKE_SOURCE_DIR}/Core/Src/system_stm32f4xx.c)
tos_partition_target(elf_stage ld/ELF.ld elf_stage)
target_include_directories(elf_stage PRIVATE ${TOS_PART_ROOT}/ELF/include)
target_compile_definitions(elf_stage PRIVATE USER_VECT_TAB_ADDRESS VECT_TAB_OFFSET=0x00000000U)
target_link_options(elf_stage PRIVATE -nostartfiles)
