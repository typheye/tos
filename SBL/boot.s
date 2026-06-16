  .syntax unified
  .cpu cortex-m4
  .fpu softvfp
  .thumb

  .global Reset_Handler
  .type Reset_Handler, %function

  .section .sbl.text.Reset_Handler,"ax",%progbits
Reset_Handler:
  ldr sp, =_estack

  bl SystemInit

  ldr r2, =_sbss
  ldr r4, =_ebss
  movs r3, #0
  b SBL_PreLoopFillZerobss

SBL_PreFillZerobss:
  str r3, [r2]
  adds r2, r2, #4

SBL_PreLoopFillZerobss:
  cmp r2, r4
  bcc SBL_PreFillZerobss

  bl SBL_PreMain

  ldr r0, =_sdata
  ldr r1, =_edata
  ldr r2, =_sidata
  movs r3, #0
  b LoopCopyDataInit

CopyDataInit:
  ldr r4, [r2, r3]
  str r4, [r0, r3]
  adds r3, r3, #4

LoopCopyDataInit:
  adds r4, r0, r3
  cmp r4, r1
  bcc CopyDataInit

  ldr r2, =_sbss
  ldr r4, =_ebss
  movs r3, #0
  b LoopFillZerobss

FillZerobss:
  str r3, [r2]
  adds r2, r2, #4

LoopFillZerobss:
  cmp r2, r4
  bcc FillZerobss

  bl __libc_init_array
  bl main
  bx lr

  .size Reset_Handler, .-Reset_Handler
