/* TEE factory data image; last 4KB intentionally remains erased. */
MEMORY { TEE (rx) : ORIGIN = 0x0800C000, LENGTH = 0x3000 }
SECTIONS {
  .tee : { . = ALIGN(4); __tee_start__ = .; KEEP(*(.tee.data .tee.data.*)) . = ALIGN(4); __tee_end__ = .; } > TEE
  /DISCARD/ : { *(.comment*) *(.note*) *(.ARM.attributes*) }
  ASSERT(__tee_end__ <= 0x0800F000, "TEE manifest exceeds protected 12KB")
}
