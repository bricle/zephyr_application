#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/led_strip.h>
#include <string.h>
#include "numbers.h"
#include "led_strip.h"

LOG_MODULE_REGISTER(LED_STRIP, LOG_LEVEL_INF);

#define STRIP_NODE DT_ALIAS(led_strip)

#if DT_NODE_HAS_PROP(DT_ALIAS(led_strip), chain_length)
#define STRIP_NUM_PIXELS DT_PROP(DT_ALIAS(led_strip), chain_length)
#else
#error Unable to determine length of LED strip
#endif

#define LED_MATRIX_SIZE   8
#define NUM_PIXELS        (LED_MATRIX_SIZE * LED_MATRIX_SIZE)
#define MAX_STRING_LENGTH 100

// LED像素缓冲区
static struct led_rgb pixels[STRIP_NUM_PIXELS];
static const struct device* const strip = DEVICE_DT_GET(STRIP_NODE);

// 滚动显示相关变量
static uint8_t scroll_buffer[MAX_STRING_LENGTH * LED_MATRIX_SIZE * 2][LED_MATRIX_SIZE];
static int scroll_buffer_width              = 0;
static int original_width                   = 0;
static int scroll_position                  = 0;
static bool scroll_running                  = false;
static scroll_direction_t current_direction = SCROLL_LEFT;

// 获取LED矩阵中LED的索引
#define LED_INDEX(row, col) ((row) * LED_MATRIX_SIZE + (col))

static void clear_display(struct led_rgb* pixels) {
    const struct led_rgb color_off = LED_COLOR_OFF;
    for (int i = 0; i < STRIP_NUM_PIXELS; i++) {
        pixels[i] = color_off;
    }
}

static void update_display() {
    int ret = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
    if (ret) {
        printk("Failed to update strip: %d\n", ret);
    }
}

// 添加字符间距配置
static uint8_t char_spacing = 2; // 默认间距为2个像素

// 设置字符间距的函数
void led_strip_set_char_spacing(uint8_t spacing) {
    char_spacing = spacing;
}
// 准备字符串的滚动缓冲区
#define TEXT_GAP 4 // 两遍文本之间的间距

static void prepare_scroll_buffer(const char* str) {
    int str_len = strlen(str);
    memset(scroll_buffer, 0, sizeof(scroll_buffer));
    scroll_buffer_width = 0;
    original_width      = 0;

    // 第一遍复制
    for (int i = 0; i < str_len; i++) {
        char c = str[i];
        if (c < 0 || c > 127) {
            continue;
        }

        uint8_t char_width = CHAR_WIDTHS[c];

        // 复制字符到缓冲区
        for (int row = 0; row < 7; row++) {
            for (int col = 0; col < char_width; col++) {
                scroll_buffer[scroll_buffer_width + col][row + 1] = ASCII_CHARS[c][row][col];
            }
        }
        scroll_buffer_width += char_width;

        // 在每个字符后添加间距
        if (i < str_len - 1) {
            // 添加空白间距
            for (int space = 0; space < char_spacing; space++) {
                for (int row = 0; row < LED_MATRIX_SIZE; row++) {
                    scroll_buffer[scroll_buffer_width + space][row] = 0;
                }
            }
            scroll_buffer_width += char_spacing;
        }
    }

    // 保存原始宽度（包括将要添加的间距）
    original_width = scroll_buffer_width + TEXT_GAP;

    // 添加文本间的间距
    scroll_buffer_width += TEXT_GAP;

    // 第二遍复制
    for (int i = 0; i < str_len; i++) {
        char c = str[i];
        if (c < 0 || c > 127) {
            continue;
        }

        uint8_t char_width = CHAR_WIDTHS[c];

        for (int row = 0; row < 7; row++) {
            for (int col = 0; col < char_width; col++) {
                scroll_buffer[scroll_buffer_width + col][row + 1] = ASCII_CHARS[c][row][col];
            }
        }
        scroll_buffer_width += char_width;

        if (i < str_len - 1) {
            scroll_buffer_width += char_spacing;
        }
    }
}

void led_strip_scroll_text(const char* str, scroll_direction_t direction) {
    if (!device_is_ready(strip)) {
        LOG_ERR("LED strip device is not ready");
        return;
    }

    current_direction = direction;
    scroll_running    = true;
    scroll_position   = 0; // 总是从0开始，因为我们有两份完整的文本

    prepare_scroll_buffer(str);
}

void led_strip_scroll_update() {
    if (!scroll_running) {
        return;
    }

    const struct led_rgb color_on  = LED_COLOR_WHITE;
    const struct led_rgb color_off = LED_COLOR_OFF;

    // 清除显示
    clear_display(pixels);

    // 更新显示内容
    for (int row = 0; row < LED_MATRIX_SIZE; row++) {
        for (int col = 0; col < LED_MATRIX_SIZE; col++) {
            int buffer_x = scroll_position + col;
            if (buffer_x >= 0 && buffer_x < scroll_buffer_width) {
                int idx     = LED_INDEX(row, col);
                pixels[idx] = scroll_buffer[buffer_x][row] ? color_on : color_off;
            }
        }
    }

    // 更新LED显示
    update_display();

    // 更新滚动位置
    if (current_direction == SCROLL_LEFT) {
        scroll_position++;
        // 当第一遍文本滚动完成后，回到第二遍文本的开始位置
        if (scroll_position >= original_width) {
            scroll_position = 0;
        }
    } else {
        scroll_position--;
        // 当滚动到开始位置时，跳转到第一遍文本的末尾
        if (scroll_position < 0) {
            scroll_position = original_width - 1;
        }
    }
}

void led_strip_stop_scroll() {
    scroll_running = false;
    clear_display(pixels);
    update_display();
}

int led_strip_init() {
    if (!device_is_ready(strip)) {
        LOG_ERR("LED strip device %s is not ready", strip->name);
        return -1;
    }

    LOG_INF("LED strip initialized successfully");
    clear_display(pixels);
    update_display();
    return 0;
}

void led_strip_display_number(uint8_t number) {
    if (number > 9) {
        return;
    }

    char str[2] = {number + '0', '\0'};
    led_strip_scroll_text(str, SCROLL_LEFT);
}