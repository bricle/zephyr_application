/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include "zephyr/sys/printk.h"

int main(void) {
    printk("Hello World! %s\n", CONFIG_BOARD);
    printk("Hello World! %s\n", CONFIG_SOC);
    printk("Hello World! %s\n", CONFIG_ARCH);
}