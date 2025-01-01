/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include "zephyr/sys/util_macro.h"
#include <zephyr/logging/log.h>
#ifdef CONFIG_MYFUNCTION
#include "myFunc.h"
#endif

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS 500

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE    DT_ALIAS(led0)
#define LED1_NODE    DT_ALIAS(led1)
#define BUTTON0_NODE DT_ALIAS(sw0)
// #define BUTTON0_NODE DT_NODELABEL(button0)

LOG_MODULE_REGISTER(blinking, LOG_LEVEL_INF);

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led0    = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led1    = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static const struct gpio_dt_spec button0 = GPIO_DT_SPEC_GET(BUTTON0_NODE, gpios);

static struct gpio_callback button0_cb_data;

void button0_isr(const struct device* port, struct gpio_callback* cb, gpio_port_pins_t pins);

int main(void) {
    int ret;
    bool led_state = true;

    if (!gpio_is_ready_dt(&led0)) {
        return 0;
    }
    if (!gpio_is_ready_dt(&led1)) {
        return 0;
    }
    if (!gpio_is_ready_dt(&button0)) {
        return 0;
    }
    ret = gpio_pin_configure_dt(&led0, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        return 0;
    }
    ret = gpio_pin_configure_dt(&led1, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        return 0;
    }
    ret = gpio_pin_configure_dt(&button0, GPIO_INPUT);
    if (ret < 0) {
        return 0;
    }
    ret = gpio_pin_interrupt_configure_dt(&button0, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret < 0) {
        return 0;
    }
    gpio_init_callback(&button0_cb_data, button0_isr, BIT(button0.pin));
    gpio_add_callback_dt(&button0, &button0_cb_data);

#ifdef CONFIG_LOG
    int exercise_num = 2;
    uint8_t data[]   = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 'H', 'e', 'l', 'l', 'o'};
    //Printf-like messages
    LOG_INF("nRF Connect SDK Fundamentals");
    LOG_INF("Exercise %d", exercise_num);
    LOG_DBG("A log message in debug level");
    LOG_WRN("A log message in warning level!");
    LOG_ERR("A log message in Error level!");
    //Hexdump some data
    LOG_HEXDUMP_INF(data, sizeof(data), "Sample Data!");
#endif
    while (1) {
        ret = gpio_pin_toggle_dt(&led0);
        if (ret < 0) {
            return 0;
        }
        // if (gpio_pin_get_dt(&button0) == 1) {
        //     gpio_pin_toggle_dt(&led1);
        // }
        led_state = !led_state;
        // printf("LED state: %s\n", led_state ? "ON" : "OFF");
        LOG_INF("LED state: %s\n", led_state ? "ON" : "OFF");
#ifdef CONFIG_MYFUNCTION
        int a = 3, b = 4;
        // printf("The sum of %3d and %3d is %3d\n", a, b, sum(a, b));
        LOG_INF("The sum of %2d and %2d is %2d\n", a, b, sum(a, b));
#else
        // printf("MYFUNCTION not enabled\n");
        LOG_INF("MYFUNCTION not enabled\n");

        // return 0;
#endif
        k_msleep(SLEEP_TIME_MS);
    }
    return 0;
}

void button0_isr(const struct device* port, struct gpio_callback* cb, gpio_port_pins_t pins) {
    gpio_pin_toggle_dt(&led1);
}