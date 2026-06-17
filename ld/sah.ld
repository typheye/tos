/* SAH data-only image. */
MEMORY { SAH (rx) : ORIGIN = 0x08020000, LENGTH = 0x20000 }
SECTIONS {
  .sah : { . = ALIGN(4); __sah_start__ = .; KEEP(*(.sah.rodata .sah.rodata.*)) . = ALIGN(4); __sah_end__ = .; } > SAH
  /DISCARD/ : { *(.comment*) *(.note*) *(.ARM.attributes*) }
  ASSERT(__sah_end__ <= 0x08040000, "SAH exceeds 128KB")
}
