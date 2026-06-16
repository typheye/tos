#include "sah_common.h"

#include "dma.h"
#include "gpio.h"
#include "sah_logo.h"
#include "spi.h"
#include "tim.h"

#define SAH_LCD_CS_PIN  11U
#define SAH_LCD_DC_PIN  12U
#define SAH_LCD_RST_PIN 14U
#define SAH_LCD_BL_PIN  13U

#define SAH_BOARD_LED_PIN 13U
#define SAH_WARN_LED_PIN  8U
#define SAH_ERROR_LED_PIN 9U

#define SAH_UNLOCK_ICON_W      20U
#define SAH_UNLOCK_ICON_H      20U
#define SAH_UNLOCK_ICON_STRIDE  3U
#define SAH_UNLOCK_ICON_Y      23U
#define SAH_UNLOCK_ICON_COLOR 0x4208U

#define SAH_SBL_STATE_MAGIC   0x53424C55UL
#define SAH_SBL_STATE_VERSION 1UL
#define SAH_SBL_STATE_OLD_ADDR 0x08007C00UL
#define SAH_SBL_STATE_AREA_SIZE 1024UL

extern SPI_HandleTypeDef hspi1;
extern TIM_HandleTypeDef htim4;
extern uint32_t HAL_GetTick(void);
extern const uint32_t __sbl_state_start__[];

typedef struct {
  uint32_t magic;
  uint32_t version;
  uint32_t unlocked;
  uint32_t crc;
} SAH_SblStateRecord;

static const uint8_t sah_unlock_20x20[60] SAH_CONST = {
    0x00, 0x00, 0x00,
    0x00, 0x7C, 0x00,
    0x01, 0x86, 0x00,
    0x03, 0x03, 0x00,
    0x03, 0x01, 0x80,
    0x03, 0x01, 0x80,
    0x03, 0x00, 0x00,
    0x03, 0x00, 0x00,
    0x03, 0x00, 0x00,
    0x07, 0xFE, 0x00,
    0x0F, 0xFF, 0x00,
    0x0F, 0xFF, 0x00,
    0x0F, 0x9F, 0x00,
    0x0F, 0x0F, 0x00,
    0x0F, 0x9F, 0x00,
    0x0F, 0x9F, 0x00,
    0x0F, 0xFF, 0x00,
    0x07, 0xFE, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
};

SAH_CODE void SAH_DelayMs(uint32_t ms) {
  uint32_t start = HAL_GetTick();
  while ((uint32_t)(HAL_GetTick() - start) < ms) {
    __NOP();
  }
}

static SAH_CODE void sah_gpio_set(GPIO_TypeDef *port, uint32_t pin) {
  port->BSRR = (1UL << pin);
}

static SAH_CODE void sah_gpio_reset(GPIO_TypeDef *port, uint32_t pin) {
  port->BSRR = (1UL << (pin + 16U));
}

static SAH_CODE void sah_leds_off(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIODEN;
  (void)RCC->AHB1ENR;

  GPIOC->MODER &= ~(3UL << (SAH_BOARD_LED_PIN * 2U));
  GPIOC->MODER |=  (1UL << (SAH_BOARD_LED_PIN * 2U));
  GPIOD->MODER &= ~((3UL << (SAH_WARN_LED_PIN * 2U)) |
                    (3UL << (SAH_ERROR_LED_PIN * 2U)));
  GPIOD->MODER |=  ((1UL << (SAH_WARN_LED_PIN * 2U)) |
                    (1UL << (SAH_ERROR_LED_PIN * 2U)));

  sah_gpio_set(GPIOC, SAH_BOARD_LED_PIN);
  sah_gpio_reset(GPIOD, SAH_WARN_LED_PIN);
  sah_gpio_reset(GPIOD, SAH_ERROR_LED_PIN);
}

static SAH_CODE void sah_backlight_off(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
  (void)RCC->AHB1ENR;

  GPIOD->MODER &= ~(3UL << (SAH_LCD_BL_PIN * 2U));
  GPIOD->MODER |=  (1UL << (SAH_LCD_BL_PIN * 2U));
  GPIOD->OTYPER &= ~(1UL << SAH_LCD_BL_PIN);
  GPIOD->OSPEEDR |= (3UL << (SAH_LCD_BL_PIN * 2U));
  GPIOD->PUPDR &= ~(3UL << (SAH_LCD_BL_PIN * 2U));
  sah_gpio_reset(GPIOD, SAH_LCD_BL_PIN);
}

static SAH_CODE void sah_backlight_full(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
  (void)RCC->AHB1ENR;

  GPIOD->MODER &= ~(3UL << (SAH_LCD_BL_PIN * 2U));
  GPIOD->MODER |=  (1UL << (SAH_LCD_BL_PIN * 2U));
  GPIOD->OTYPER &= ~(1UL << SAH_LCD_BL_PIN);
  GPIOD->OSPEEDR |= (3UL << (SAH_LCD_BL_PIN * 2U));
  GPIOD->PUPDR &= ~(3UL << (SAH_LCD_BL_PIN * 2U));
  sah_gpio_set(GPIOD, SAH_LCD_BL_PIN);
}

static SAH_CODE void sah_spi_write(uint8_t v) {
  (void)HAL_SPI_Transmit(&hspi1, &v, 1U, 100U);
}

static SAH_CODE void sah_spi_write_bytes(const uint8_t *data, uint32_t len) {
  while (len > 0U) {
    uint16_t chunk = len > 0xFFFFU ? 0xFFFFU : (uint16_t)len;
    (void)HAL_SPI_Transmit(&hspi1, (uint8_t *)data, chunk, 1000U);
    data += chunk;
    len -= chunk;
  }
}

static SAH_CODE void sah_lcd_cmd(uint8_t cmd) {
  sah_gpio_reset(GPIOD, SAH_LCD_DC_PIN);
  sah_gpio_reset(GPIOD, SAH_LCD_CS_PIN);
  sah_spi_write(cmd);
  sah_gpio_set(GPIOD, SAH_LCD_CS_PIN);
}

static SAH_CODE void sah_lcd_data_bytes(const uint8_t *data, uint32_t len) {
  sah_gpio_set(GPIOD, SAH_LCD_DC_PIN);
  sah_gpio_reset(GPIOD, SAH_LCD_CS_PIN);
  sah_spi_write_bytes(data, len);
  sah_gpio_set(GPIOD, SAH_LCD_CS_PIN);
}

static SAH_CODE void sah_lcd_data(uint8_t data) {
  sah_lcd_data_bytes(&data, 1U);
}

static SAH_CODE void sah_lcd_data16(uint16_t data) {
  uint8_t bytes[2] = {(uint8_t)(data >> 8), (uint8_t)data};
  sah_lcd_data_bytes(bytes, sizeof(bytes));
}

static SAH_CODE void sah_lcd_addr(uint16_t x0, uint16_t y0,
                                  uint16_t x1, uint16_t y1) {
  sah_lcd_cmd(0x2AU);
  sah_lcd_data16(x0);
  sah_lcd_data16(x1);
  sah_lcd_cmd(0x2BU);
  sah_lcd_data16(y0);
  sah_lcd_data16(y1);
  sah_lcd_cmd(0x2CU);
}

static SAH_CODE void sah_lcd_fill(uint16_t color) {
  sah_lcd_addr(0U, 0U, SAH_LCD_W - 1U, SAH_LCD_H - 1U);
  sah_gpio_set(GPIOD, SAH_LCD_DC_PIN);
  sah_gpio_reset(GPIOD, SAH_LCD_CS_PIN);
  for (uint32_t i = 0; i < (uint32_t)SAH_LCD_W * SAH_LCD_H; ++i) {
    sah_spi_write((uint8_t)(color >> 8));
    sah_spi_write((uint8_t)color);
  }
  sah_gpio_set(GPIOD, SAH_LCD_CS_PIN);
}

static SAH_CODE uint32_t sah_sbl_state_crc(const SAH_SblStateRecord *r) {
  return r->magic ^ r->version ^ r->unlocked ^ 0xA5A55A5AUL;
}

static SAH_CODE uint8_t sah_sbl_state_valid(const SAH_SblStateRecord *r) {
  if (r->magic != SAH_SBL_STATE_MAGIC ||
      r->version != SAH_SBL_STATE_VERSION) {
    return 0U;
  }
  if (r->crc != sah_sbl_state_crc(r)) {
    return 0U;
  }
  return 1U;
}

static SAH_CODE uint8_t sah_sbl_state_erased(const SAH_SblStateRecord *r) {
  const uint32_t *w = (const uint32_t *)r;
  for (uint32_t i = 0U; i < sizeof(SAH_SblStateRecord) / sizeof(uint32_t); i++) {
    if (w[i] != 0xFFFFFFFFUL) {
      return 0U;
    }
  }
  return 1U;
}

static SAH_CODE uint8_t sah_bootloader_unlocked(void) {
  const SAH_SblStateRecord *latest = 0;
  for (uint32_t off = 0U; off + sizeof(SAH_SblStateRecord) <= SAH_SBL_STATE_AREA_SIZE;
       off += sizeof(SAH_SblStateRecord)) {
    const SAH_SblStateRecord *r =
        (const SAH_SblStateRecord *)((uint32_t)__sbl_state_start__ + off);
    if (sah_sbl_state_erased(r)) {
      break;
    }
    if (sah_sbl_state_valid(r)) {
      latest = r;
    }
  }
  if (!latest) {
    const SAH_SblStateRecord *old =
        (const SAH_SblStateRecord *)SAH_SBL_STATE_OLD_ADDR;
    latest = sah_sbl_state_valid(old) ? old : 0;
  }
  return (latest && latest->unlocked) ? 1U : 0U;
}

static SAH_CODE void sah_lcd_pixel(uint16_t x, uint16_t y, uint16_t color) {
  sah_lcd_addr(x, y, x, y);
  sah_lcd_data16(color);
}

static SAH_CODE void sah_draw_unlock_icon(void) {
  const uint16_t x0 = (uint16_t)((SAH_LCD_W - SAH_UNLOCK_ICON_W) / 2U);
  const uint16_t y0 = SAH_UNLOCK_ICON_Y;

  for (uint16_t y = 0U; y < SAH_UNLOCK_ICON_H; y++) {
    const uint8_t *row =
        &sah_unlock_20x20[(uint32_t)y * SAH_UNLOCK_ICON_STRIDE];
    for (uint16_t x = 0U; x < SAH_UNLOCK_ICON_W; x++) {
      uint8_t bit = (uint8_t)(0x80U >> (x & 7U));
      if (row[x >> 3U] & bit) {
        sah_lcd_pixel((uint16_t)(x0 + x), (uint16_t)(y0 + y),
                      SAH_UNLOCK_ICON_COLOR);
      }
    }
  }
}

static SAH_CODE void sah_lcd_init(void) {
  sah_backlight_off();
  sah_gpio_set(GPIOD, SAH_LCD_CS_PIN);
  sah_gpio_reset(GPIOD, SAH_LCD_DC_PIN);
  sah_gpio_set(GPIOD, SAH_LCD_RST_PIN);
  SAH_DelayMs(10U);
  sah_gpio_reset(GPIOD, SAH_LCD_RST_PIN);
  SAH_DelayMs(10U);
  sah_gpio_set(GPIOD, SAH_LCD_RST_PIN);
  SAH_DelayMs(120U);

  sah_lcd_cmd(0x01U);
  SAH_DelayMs(150U);
  sah_lcd_cmd(0x11U);
  SAH_DelayMs(120U);
  sah_lcd_cmd(0x3AU);
  sah_lcd_data(0x55U);
  sah_lcd_cmd(0x36U);
  sah_lcd_data(0x10U);
  sah_lcd_cmd(0xB2U);
  { uint8_t d[] = {0x0C, 0x0C, 0x00, 0x33, 0x33}; sah_lcd_data_bytes(d, sizeof(d)); }
  sah_lcd_cmd(0xB7U);
  sah_lcd_data(0x35U);
  sah_lcd_cmd(0xBBU);
  sah_lcd_data(0x19U);
  sah_lcd_cmd(0xC0U);
  sah_lcd_data(0x2CU);
  sah_lcd_cmd(0xC2U);
  sah_lcd_data(0x01U);
  sah_lcd_cmd(0xC3U);
  sah_lcd_data(0x12U);
  sah_lcd_cmd(0xC4U);
  sah_lcd_data(0x20U);
  sah_lcd_cmd(0xC6U);
  sah_lcd_data(0x0FU);
  sah_lcd_cmd(0xD0U);
  { uint8_t d[] = {0xA4, 0xA1}; sah_lcd_data_bytes(d, sizeof(d)); }
  sah_lcd_cmd(0x21U);
  sah_lcd_cmd(0x29U);
  SAH_DelayMs(100U);
  __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, 0U);
  (void)HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2);
  sah_lcd_fill(0x0000U);
  SAH_DelayMs(100U);
  __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, 0U);
}

static SAH_CODE void sah_draw_logo(void) {
  sah_lcd_addr(0U, 0U, SAH_LCD_W - 1U, SAH_LCD_H - 1U);
  sah_gpio_set(GPIOD, SAH_LCD_DC_PIN);
  sah_gpio_reset(GPIOD, SAH_LCD_CS_PIN);
  for (uint32_t i = 0; i < (uint32_t)SAH_LCD_W * SAH_LCD_H; ++i) {
    uint16_t color = sah_logo_data[i];
    sah_spi_write((uint8_t)(color >> 8));
    sah_spi_write((uint8_t)color);
  }
  sah_gpio_set(GPIOD, SAH_LCD_CS_PIN);
  if (sah_bootloader_unlocked()) {
    sah_draw_unlock_icon();
  }
}

SAH_CODE void SAH_Run(void) {
  sah_leds_off();
  sah_backlight_off();

  MX_GPIO_Init();
  sah_backlight_off();
  MX_DMA_Init();
  MX_SPI1_Init();
  MX_TIM4_Init();

  sah_leds_off();
  sah_lcd_init();
  sah_draw_logo();
  sah_backlight_full();
  SAH_DelayMs(1600U);
}
