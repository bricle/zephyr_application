/*
 * Copyright (c) 2016 Linaro Limited
 *               2016 Intel Corporation.
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include "zephyr/sys/printk.h"

#define TEST_PARTITION        storage_partition
#define TEST_PARTITION_OFFSET FIXED_PARTITION_OFFSET(TEST_PARTITION)
#define TEST_PARTITION_DEVICE FIXED_PARTITION_DEVICE(TEST_PARTITION)

#define FLASH_TEST_SIZE        512
#define FLASH_TEST_OFFSET      1024
#define FLASH_BOOT_TIME_OFFSET 0
uint8_t write_buf[FLASH_TEST_SIZE];
uint8_t read_buf[FLASH_TEST_SIZE];

int main(void) {
    k_msleep(1000);
    const struct flash_area* my_area;

    int err;

    // Prepare test data
    for (int i = 0; i < FLASH_TEST_SIZE; i++) {
        write_buf[i] = i & 0xFF;
    }

    // Open flash area
    err = flash_area_open(FIXED_PARTITION_ID(storage_partition), &my_area);
    if (err != 0) {
        printk("Error opening flash area: %d\n", err);
        return err;
    }

    printk("Flash area opened successfully:\n");
    printk("Area ID: %d\n", my_area->fa_id);
    printk("Area offset: 0x%lx\n", my_area->fa_off);
    printk("Area size: 0x%x\n", my_area->fa_size);

    // Erase flash sector
    err = flash_area_erase(my_area, FLASH_TEST_OFFSET, FLASH_TEST_SIZE);
    if (err != 0) {
        printk("Error erasing flash: %d\n", err);
        flash_area_close(my_area);
        return err;
    }
    printk("Flash erased successfully\n");

    // Write test pattern
    err = flash_area_write(my_area, FLASH_TEST_OFFSET, write_buf, FLASH_TEST_SIZE);
    if (err != 0) {
        printk("Error writing to flash: %d\n", err);
        // flash_area_close(my_area);
        return err;
    }
    printk("Flash written successfully\n");

    // Read back and verify
    err = flash_area_read(my_area, FLASH_TEST_OFFSET, read_buf, FLASH_TEST_SIZE);
    if (err != 0) {
        printk("Error reading flash: %d\n", err);
        flash_area_close(my_area);
        return err;
    }

    // Verify data
    bool verify_ok = true;
    for (int i = 0; i < FLASH_TEST_SIZE; i++) {
        if (read_buf[i] != write_buf[i]) {
            printk("Verification failed at offset %d: expected 0x%02x, got 0x%02x\n",
                   i,
                   write_buf[i],
                   read_buf[i]);
            verify_ok = false;
            break;
        }
    }

    if (verify_ok) {
        printk("Flash verification successful!\n");
    }

    // Clean up
    // flash_area_close(my_area);

    uint32_t up_time = 0;
    err              = flash_area_read(my_area, FLASH_BOOT_TIME_OFFSET, &up_time, 4);
    if (err != 0) {
        printk("Error reading flash: %d\n", err);
        flash_area_close(my_area);
        return err;
    }
    if (up_time == 0xFFFFFFFF) {
        up_time = 0;
        printk("first boot after erase and flash\n");
    }
    up_time += 1;
    err = flash_area_write(my_area, FLASH_BOOT_TIME_OFFSET, &up_time, 4);
    if (err != 0) {
        printk("Error writing to flash: %d\n", err);
    }
    printk("up_time: %d\n", up_time);
    return verify_ok ? 0 : -1;
}