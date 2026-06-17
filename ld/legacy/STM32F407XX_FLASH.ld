/*
******************************************************************************
**

**  File        : LinkerScript.ld
**
**  Author		: STM32CubeMX
**
**  Abstract    : Linker script for STM32F407ZGTx series
**                1024Kbytes FLASH and 192Kbytes RAM
**
**                Set heap size, stack size and stack location according
**                to application requirements.
**
**                Set memory bank area and size if external memory is used.
**
**  Target      : STMicroelectronics STM32
**
**  Distribution: The file is distributed "as is," without any warranty
**                of any kind.
**
*****************************************************************************
** @attention
**
** <h2><center>&copy; COPYRIGHT(c) 2025 STMicroelectronics</center></h2>
**
** Redistribution and use in source and binary forms, with or without modification,
** are permitted provided that the following conditions are met:
**   1. Redistributions of source code must retain the above copyright notice,
**      this list of conditions and the following disclaimer.
**   2. Redistributions in binary form must reproduce the above copyright notice,
**      this list of conditions and the following disclaimer in the documentation
**      and/or other materials provided with the distribution.
**   3. Neither the name of STMicroelectronics nor the names of its contributors
**      may be used to endorse or promote products derived from this software
**      without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
** AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
** SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
** CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
** OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
*****************************************************************************
*/

/* Entry Point */
ENTRY(Reset_Handler)

/* Specify the memory areas */
MEMORY
{
RAM (rw)      : ORIGIN = 0x20000000, LENGTH = 128K
CCMRAM (rw)      : ORIGIN = 0x10000000, LENGTH = 64K
FLASH (rx)      : ORIGIN = 0x8000000, LENGTH = 1024K
}

/*
 * Split ELF program headers so GNU ld 14+ does not merge executable FLASH
 * sections and writable init-data load images into one RWX LOAD segment.
 * FLAGS: 5 = R|X, 6 = R|W. This changes ELF metadata only; section VMAs/LMAs
 * used by startup code and objcopy remain the same.
 */
PHDRS
{
  flash   PT_LOAD FLAGS(5);
  ccmload PT_LOAD FLAGS(6);
  ramload PT_LOAD FLAGS(6);
}

/* Highest address of the user mode stack */
_estack = ORIGIN(RAM) + LENGTH(RAM);    /* end of RAM */
/* Generate a link error if heap and stack don't fit into RAM */
_Min_Heap_Size = 0x200;      /* required amount of heap  */
_Min_Stack_Size = 0x1000; /* required amount of stack */

/* Define output sections */
SECTIONS
{
  /* The startup code goes first into FLASH */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector)) /* Startup code */
    . = ALIGN(4);
  } >FLASH :flash

  .sbl :
  {
    . = ALIGN(4);
    __sbl_start__ = .;
    KEEP(*(.sbl.text))
    KEEP(*(.sbl.text*))
    KEEP(*(.sbl.rodata))
    KEEP(*(.sbl.rodata*))
    */SBL/*.c.obj(.rodata .rodata*)
    */SBL/*.s.obj(.rodata .rodata*)
    *startup_stm32f407xx.s.obj(.text.Default_Handler)
    *system_stm32f4xx.c.obj(.text .text.* .rodata .rodata*)
    *stm32f4xx_hal.c.obj(.text .text.* .rodata .rodata*)
    *stm32f4xx_hal_msp.c.obj(.text .text.* .rodata .rodata*)
    *stm32f4xx_hal_cortex.c.obj(.text .text.* .rodata .rodata*)
    *stm32f4xx_hal_gpio.c.obj(.text .text.* .rodata .rodata*)
    *stm32f4xx_hal_pwr.c.obj(.text .text.* .rodata .rodata*)
    *stm32f4xx_hal_pwr_ex.c.obj(.text .text.* .rodata .rodata*)
    *stm32f4xx_hal_rcc.c.obj(.text .text.* .rodata .rodata*)
    *stm32f4xx_hal_rcc_ex.c.obj(.text .text.* .rodata .rodata*)
    *stm32f4xx_it.c.obj(.text .text.* .rodata .rodata*)
    *main.c.obj(.text.Error_Handler)
    *usbd_conf.c.obj(.text .text.* .rodata .rodata*)
    *usbd_core.c.obj(.text .text.* .rodata .rodata*)
    *usbd_ctlreq.c.obj(.text .text.* .rodata .rodata*)
    *usbd_ioreq.c.obj(.text .text.* .rodata .rodata*)
    *stm32f4xx_hal_pcd.c.obj(.text .text.* .rodata .rodata*)
    *stm32f4xx_hal_pcd_ex.c.obj(.text .text.* .rodata .rodata*)
    *stm32f4xx_ll_usb.c.obj(.text .text.* .rodata .rodata*)
    . = ALIGN(4);
    __sbl_end__ = .;
  } >FLASH :flash

  __sbl_reserved_end__ = ORIGIN(FLASH) + 64K;
  __sbl_state_start__ = ORIGIN(FLASH) + 63K;
  __sbl_state_end__ = ORIGIN(FLASH) + 64K;
  ASSERT(__sbl_end__ <= (ORIGIN(FLASH) + 48K),
         "SBL code must stay below sector 3 reserved for state")

  __rec_start__ = ORIGIN(FLASH) + 64K;

  .rec __rec_start__ :
  {
    . = ALIGN(4);
    __rec_image_start__ = .;
    KEEP(*(.rec.text))
    KEEP(*(.rec.text*))
    */REC/*.c.obj(.text .text*)
    KEEP(*(.rec.rodata))
    KEEP(*(.rec.rodata*))
    */REC/*.c.obj(.rodata .rodata*)
    *sdio.c.obj(.text .text.* .rodata .rodata*)
    *stm32f4xx_hal_sd.c.obj(.text .text.* .rodata .rodata*)
    *stm32f4xx_ll_sdmmc.c.obj(.text .text.* .rodata .rodata*)
    *stm32f4xx_hal_dma.c.obj(.text .text.* .rodata .rodata*)
    *stm32f4xx_hal_dma_ex.c.obj(.text .text.* .rodata .rodata*)
    *fatfs.c.obj(.text .text.* .rodata .rodata*)
    *diskio.c.obj(.text .text.* .rodata .rodata*)
    *ff.c.obj(.text .text.* .rodata .rodata*)
    *ff_gen_drv.c.obj(.text .text.* .rodata .rodata*)
    . = ALIGN(4);
    __rec_image_end__ = .;
  } >FLASH :flash

  __rec_reserved_end__ = ORIGIN(FLASH) + 128K;
  __rec_command_start__ = __rec_reserved_end__ - 1K;
  ASSERT(__rec_image_end__ <= __rec_command_start__,
         "REC code overlaps reserved command area")

  __sah_data_start__ = ORIGIN(FLASH) + 128K;

  .sah __sah_data_start__ :
  {
    . = ALIGN(4);
    __sah_start__ = .;
    KEEP(*(.sah.rodata))
    KEEP(*(.sah.rodata*))
    . = ALIGN(4);
    __sah_end__ = .;
  } >FLASH :flash

  __sah_reserved_end__ = ORIGIN(FLASH) + 256K;
  ASSERT(__sah_end__ <= __sah_reserved_end__,
         "SAH data exceeds reserved 128KB partition")
  __tos_app_start__ = __sah_reserved_end__;

  /* The program code and other data goes into FLASH */
  .text __tos_app_start__ :
  {
    . = ALIGN(4);
    *(.text)           /* .text sections (code) */
    *(.text*)          /* .text* sections (code) */
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(4);
    _etext = .;        /* define a global symbols at end of code */
  } >FLASH :flash

  /* Constant data goes into FLASH */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
    . = ALIGN(4);
  } >FLASH :flash

  .ARM.extab : /* The "READONLY" keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(4);
    *(.ARM.extab* .gnu.linkonce.armextab.*)
    . = ALIGN(4);
   } >FLASH :flash

  .ARM : /* The "READONLY" keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(4);
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
    . = ALIGN(4);
  } >FLASH :flash

  .preinit_array : /* The "READONLY" keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
    . = ALIGN(4);
  } >FLASH :flash

  .init_array : /* The "READONLY" keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
    . = ALIGN(4);
  } >FLASH :flash

  .fini_array : /* The "READONLY" keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
    . = ALIGN(4);
  } >FLASH :flash

  _siccmram = LOADADDR(.ccmram);

  /* CCM-RAM section
  *
  * IMPORTANT NOTE!
  * If initialized variables will be placed in this section,
  * the startup code needs to be modified to copy the init-values.
  */
  .ccmram :
  {
    . = ALIGN(4);
    _sccmram = .;       /* create a global symbol at ccmram start */
    *(.ccmram)
    *(.ccmram*)

    . = ALIGN(4);
    _eccmram = .;       /* create a global symbol at ccmram end */
  } >CCMRAM AT> FLASH :ccmload

  .sysdram_ccm_heap (NOLOAD) :
  {
    . = ALIGN(8);
    __sysdram_ccm_heap_start__ = .;
    KEEP(*(.sysdram_ccm_heap))
    KEEP(*(.sysdram_ccm_heap*))
    . = ALIGN(8);
    __sysdram_ccm_heap_end__ = .;
  } >CCMRAM :NONE

  /* used by the startup to initialize data */
  _sidata = LOADADDR(.data);

  /* Initialized data sections goes into RAM, load LMA copy after code */
  .data :
  {
    . = ALIGN(4);
    _sdata = .;        /* create a global symbol at data start */
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */
    *(.RamFunc)        /* .RamFunc sections */
    *(.RamFunc*)       /* .RamFunc* sections */

    . = ALIGN(4);
  } >RAM AT> FLASH :ramload

 /* Initialized TLS data section */
  .tdata : ALIGN(4)
  {
    *(.tdata .tdata.* .gnu.linkonce.td.*)
    . = ALIGN(4);
    _edata = .;        /* define a global symbol at data end */
    PROVIDE(__data_end = .);
    PROVIDE(__tdata_end = .);
  } >RAM AT> FLASH :ramload

  PROVIDE( __tdata_start = ADDR(.tdata) );
  PROVIDE( __tdata_size = __tdata_end - __tdata_start );

  PROVIDE( __data_start = ADDR(.data) );
  PROVIDE( __data_size = __data_end - __data_start );

  PROVIDE( __tdata_source = LOADADDR(.tdata) );
  PROVIDE( __tdata_source_end = LOADADDR(.tdata) + SIZEOF(.tdata) );
  PROVIDE( __tdata_source_size = __tdata_source_end - __tdata_source );

  PROVIDE( __data_source = LOADADDR(.data) );
  PROVIDE( __data_source_end = __tdata_source_end );
  PROVIDE( __data_source_size = __data_source_end - __data_source );
  __tos_flash_image_end__ = __data_source_end;
  /* Uninitialized data section */
  .tbss (NOLOAD) : ALIGN(4)
  {
     /* This is used by the startup in order to initialize the .bss secion */
    _sbss = .;         /* define a global symbol at bss start */
    __bss_start__ = _sbss;
    *(.tbss .tbss.*)
    . = ALIGN(4);
    PROVIDE( __tbss_end = . );
  } >RAM :NONE

  PROVIDE( __tbss_start = ADDR(.tbss) );
  PROVIDE( __tbss_size = __tbss_end - __tbss_start );
  PROVIDE( __tbss_offset = ADDR(.tbss) - ADDR(.tdata) );

  PROVIDE( __tls_base = __tdata_start );
  PROVIDE( __tls_end = __tbss_end );
  PROVIDE( __tls_size = __tls_end - __tls_base );
  PROVIDE( __tls_align = MAX(ALIGNOF(.tdata), ALIGNOF(.tbss)) );
  PROVIDE( __tls_size_align = (__tls_size + __tls_align - 1) & ~(__tls_align - 1) );
  PROVIDE( __arm32_tls_tcb_offset = MAX(8, __tls_align) );
  PROVIDE( __arm64_tls_tcb_offset = MAX(16, __tls_align) );

  .bss (NOLOAD) : ALIGN(4)
  {
    *(.bss)
    *(.bss*)
    *(COMMON)

      . = ALIGN(4);
    _ebss = .;         /* define a global symbol at bss end */
    __bss_end__ = _ebss;
      PROVIDE( __bss_end = .);
  } >RAM :NONE
  PROVIDE( __non_tls_bss_start = ADDR(.bss) );

  PROVIDE( __bss_start = __tbss_start );
  PROVIDE( __bss_size = __bss_end - __bss_start );

  .sysdram_ram_fixed (NOLOAD) :
  {
    . = ALIGN(8);
    __sysdram_ram_fixed_start__ = .;
    KEEP(*(.sysdram_lcd_fixed))
    KEEP(*(.sysdram_lcd_fixed*))
    KEEP(*(.sysdram_core_fixed))
    KEEP(*(.sysdram_core_fixed*))
    . = ALIGN(8);
    __sysdram_ram_fixed_end__ = .;
  } >RAM :NONE

  .sysdram_ram_heap (NOLOAD) :
  {
    . = ALIGN(8);
    __sysdram_ram_heap_start__ = .;
    KEEP(*(.sysdram_ram_heap))
    KEEP(*(.sysdram_ram_heap*))
    . = ALIGN(8);
    __sysdram_ram_heap_end__ = .;
  } >RAM :NONE

  /* User_heap_stack section, used to check that there is enough RAM left */
  ._user_heap_stack (NOLOAD) :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM :NONE



  /* Remove information from the standard libraries */
  /DISCARD/ :
  {
    libc.a:* ( * )
    libm.a:* ( * )
    libgcc.a:* ( * )
  }

  ASSERT(__tos_flash_image_end__ <= (ORIGIN(FLASH) + 0x000C0000),
         "TOS flash image exceeds 512KB system partition")

}
