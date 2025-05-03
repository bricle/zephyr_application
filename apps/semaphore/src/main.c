/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include "zephyr/random/random.h"
#include "zephyr/sys/printk.h"
K_SEM_DEFINE(instance_sem, 10, 10);
volatile uint32_t counter = 10;

void get_acess(void) {
    k_sem_take(&instance_sem, K_FOREVER);
    counter--;
    printk("get access and counter is %d\n", counter);
}

void release_access(void) {
    counter++;
    printk("release access and counter is %d\n", counter);
    // 下面这个函数是用来释放信号量的，并且开始任务调度，counter的值可能会被其他线程修改
    // 所以必须在k_sem_give之前打印counter的值
    k_sem_give(&instance_sem);
}

void producer_fun(void) {
    printk("Producer thread started\n");
    while (1) {
        release_access();
        k_msleep(500 + sys_rand32_get() % 10);
    }
}

void consumer_fun(void) {
    printk("Consumer thread started\n");
    while (1) {
        get_acess();
        k_msleep(sys_rand32_get() % 10);
    }
}

K_THREAD_DEFINE(producer, 1024, producer_fun, NULL, NULL, NULL, 4, 0, 0);
K_THREAD_DEFINE(consumer, 1024, consumer_fun, NULL, NULL, NULL, 2, 0, 0);
