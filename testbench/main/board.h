#pragma once

#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Waveshare ESP32-S3-Touch-LCD-1.54 pin map.
 *
 * Cross-checked against two independent sources that agree: the xiaozhi-esp32
 * board config for `waveshare-s3-lcd-1.54`, and Waveshare's own Arduino demos.
 */

/* Display: ST7789, 4-wire SPI, mode 3, colors inverted. */
#define BSP_LCD_SPI_HOST    SPI2_HOST
#define BSP_LCD_SCLK        GPIO_NUM_38
#define BSP_LCD_MOSI        GPIO_NUM_39
#define BSP_LCD_CS          GPIO_NUM_21
#define BSP_LCD_DC          GPIO_NUM_45
#define BSP_LCD_RST         GPIO_NUM_40
#define BSP_LCD_BACKLIGHT   GPIO_NUM_46
#define BSP_LCD_H_RES       240
#define BSP_LCD_V_RES       240

/* Shared I2C bus: CST816 touch, QMI8658 IMU, ES8311 and ES7210 codecs. */
#define BSP_I2C_SDA         GPIO_NUM_42
#define BSP_I2C_SCL         GPIO_NUM_41
#define BSP_TOUCH_INT       GPIO_NUM_48
#define BSP_TOUCH_RST       GPIO_NUM_47

/* Buttons. GPIO 0 is also the BOOT strapping pin. */
#define BSP_BTN_PWR         GPIO_NUM_5
#define BSP_BTN_PLUS        GPIO_NUM_4
#define BSP_BTN_BOOT        GPIO_NUM_0

/**
 * Bring up the backlight, SPI display, I2C touch controller, and LVGL.
 *
 * On return LVGL is running on its own task; take `lvgl_port_lock()` before
 * touching any lv_* object from application code.
 */
esp_err_t board_init(void);

/** Set backlight duty as a percentage (0-100). */
esp_err_t board_backlight_set(int percent);

#ifdef __cplusplus
}
#endif
