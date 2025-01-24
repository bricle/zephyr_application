/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdio.h>
#include <zephyr/kernel.h>
#include "zephyr/sys/printk.h"

static inline void emulate_work();

static void offload_work_handler(struct k_work* work) {
    emulate_work();
}

K_WORK_DEFINE(offload_work, offload_work_handler);

static inline void emulate_work() {
    for (volatile int count_out = 0; count_out < 300000; count_out++)
        ;
}

void task1_fun(void) {
    uint64_t time_stamp;
    int64_t delta_time;
    k_work_init(&offload_work, offload_work_handler);
    while (1) {
        time_stamp = k_uptime_get();
        // emulate_work();
        k_work_submit(&offload_work);
        delta_time = k_uptime_delta(&time_stamp);
        printk("thread1 yielding this round in %lld ms\n", delta_time);
        k_msleep(20);
    }
}

void task2_fun(void) {
    uint64_t time_stamp;
    int64_t delta_time;
    while (1) {
        time_stamp = k_uptime_get();
        emulate_work();
        delta_time = k_uptime_delta(&time_stamp);
        printk("thread2 yielding this round in %lld ms\n", delta_time);
        k_msleep(20);
    }
}

K_THREAD_DEFINE(task1, 512, task1_fun, NULL, NULL, NULL, 2, 0, 0);
K_THREAD_DEFINE(task2, 512, task2_fun, NULL, NULL, NULL, 5, 0, 0);
