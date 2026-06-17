  .syntax unified
  .cpu cortex-m4
  .fpu softvfp
  .thumb

  .global Reset_Handler
  .type Reset_Handler, %function
  .section .text.Reset_Handler,"ax",%progbits
Reset_Handler:
  ldr sp, =_estack
  bl SystemInit

  ldr r0, =_sdata
  ldr r1, =_edata
  ldr r2, =_sidata
  movs r3, #0
1:
  adds r4, r0, r3
  cmp r4, r1
  bcs 2f
  ldr r4, [r2, r3]
  str r4, [r0, r3]
  adds r3, r3, #4
  b 1b
2:
  ldr r2, =_sbss
  ldr r4, =_ebss
  movs r3, #0
3:
  cmp r2, r4
  bcs 4f
  str r3, [r2], #4
  b 3b
4:
  bl ELF_Main
5:
  b 5b
  .size Reset_Handler, .-Reset_Handler
