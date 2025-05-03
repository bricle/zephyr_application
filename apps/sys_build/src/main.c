/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/linker/linker-defs.h>
const uint16_t rom_val = 0x1234; // This is a sample variable
uint16_t ram_val       = 0x1234; // This is a sample variable
int main() {
    printk("Address of sample %p\n", (void*)__rom_region_start);
    printk("Address of sample %p\n", (void*)&rom_val);
    printk("Address of sample %p\n", (void*)&ram_val);
    printk("Hello from DevAcademy Intermediate, Lesson 8, Exercise 1\n");

    return 0;
}