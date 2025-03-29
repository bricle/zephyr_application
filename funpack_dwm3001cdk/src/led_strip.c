#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/addr.h>
#include <dk_buttons_and_leds.h>
#include "zephyr/bluetooth/conn.h"
#include <zephyr/drivers/display.h>
#include <zephyr/display/cfb.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/drivers/sensor.h>
#include <stdio.h>
#include "my_lbs.h"
#include "numbers.h"
#include "led_strip.h"
LOG_MODULE_REGISTER(LED_STRIP, LOG_LEVEL_INF);

#define STRIP_NODE DT_ALIAS(led_strip)

#if DT_NODE_HAS_PROP(DT_ALIAS(led_strip), chain_length)
#define STRIP_NUM_PIXELS DT_PROP(DT_ALIAS(led_strip), chain_length)
#else
#error Unable to determine length of LED strip
#endif

#define DELAY_TIME K_MSEC(CONFIG_SAMPLE_LED_UPDATE_DELAY)

#define RGB(_r, _g, _b) {.r = (_r), .g = (_g), .b = (_b)}

static const struct led_rgb colors[] = {
    RGB(CONFIG_SAMPLE_LED_BRIGHTNESS, 0x00, 0x00), /* red */
    RGB(0x00, CONFIG_SAMPLE_LED_BRIGHTNESS, 0x00), /* green */
    RGB(0x00, 0x00, CONFIG_SAMPLE_LED_BRIGHTNESS), /* blue */
};

static struct led_rgb pixels[STRIP_NUM_PIXELS];

static const struct device* const strip = DEVICE_DT_GET(STRIP_NODE);
#define LED_MATRIX_SIZE 8
#define NUM_PIXELS      (LED_MATRIX_SIZE * LED_MATRIX_SIZE)
#define DISPLAY_DELAY   2000 // 每个数字显示2秒

// 获取LED矩阵中LED的索引
#define LED_INDEX(row, col) ((row) * LED_MATRIX_SIZE + (col))

// LED像素缓冲区
static struct led_rgb pixels[STRIP_NUM_PIXELS];

static void clear_display(struct led_rgb* pixels) {
    const struct led_rgb color_off = LED_COLOR_OFF;
    for (int i = 0; i < STRIP_NUM_PIXELS; i++) {
        pixels[i] = color_off;
    }
}

void led_strip_display_number(uint8_t number) {
    const struct led_rgb color_on  = LED_COLOR_WHITE;
    const struct led_rgb color_off = LED_COLOR_OFF;

    if (number > 9) {
        return;
    }

    // 更新像素缓冲区
    for (int row = 0; row < LED_MATRIX_SIZE; row++) {
        for (int col = 0; col < LED_MATRIX_SIZE; col++) {
            int idx     = LED_INDEX(row, col);
            pixels[idx] = NUMBERS[number][row][col] ? color_on : color_off;
        }
    }

    // 更新LED条
    int ret = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
    if (ret) {
        printk("Failed to update strip: %d\n", ret);
    }
}

int led_strip_init() {
    if (device_is_ready(strip)) {
        LOG_INF("Found LED strip device %s", strip->name);
    } else {
        LOG_ERR("LED strip device %s is not ready", strip->name);
        return 0;
    }

    LOG_INF("Displaying pattern on strip");
}