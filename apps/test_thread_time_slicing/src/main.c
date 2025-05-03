/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "zephyr/logging/log.h"
#include "zephyr/sys/printk.h"

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS 1000

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
LOG_MODULE_REGISTER(blinking, LOG_LEVEL_INF);

void task1_fun(void) {
    while (1) {
        // LOG_INF("Task1\n");
        printk("Task1\n");
        k_busy_wait(1000000);
    }
}

void task2_fun(void) {
    while (1) {
        // LOG_INF("Task2\n");
        printk("Task2\n");
        k_busy_wait(1000000);
    }
}

/* TIMESLICE_PRIORITY =0
* time slicing only affect threads with the same priority level.*/
K_THREAD_DEFINE(task1, 512, task1_fun, NULL, NULL, NULL, 2, 0, 0);
K_THREAD_DEFINE(task2, 512, task2_fun, NULL, NULL, NULL, 5, 0, 0);
