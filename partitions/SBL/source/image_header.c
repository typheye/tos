#include "tee_format.h"

/* The host signing step replaces this section in exported ELF and BIN files. */
__attribute__((section(".tos_image_header"), used, aligned(4)))
const uint8_t g_tos_image_header_placeholder[TOS_IMAGE_HEADER_SIZE] = {
    [0 ... TOS_IMAGE_HEADER_SIZE - 1] = 0xFFU,
};
