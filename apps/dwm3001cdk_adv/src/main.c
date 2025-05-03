/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
/* STEP 3 - Include the header file of the Bluetooth LE stack */
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/addr.h>
#include <dk_buttons_and_leds.h>

LOG_MODULE_REGISTER(Lesson2_Exercise1, LOG_LEVEL_INF);
#define COMPANY_ID_CODE 0x0059

#define DEVICE_NAME     CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define RUN_STATUS_LED         DK_LED4
#define RUN_LED_BLINK_INTERVAL 1000
#define USER_BUTTON            DK_BTN1_MSK
static const struct bt_le_adv_param* adv_param =
    BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONN | BT_LE_ADV_OPT_USE_IDENTITY, 800, 801, NULL);

typedef struct {
    uint16_t company_id;
    uint16_t number_pressed;
} adv_mfg_data_t;

static adv_mfg_data_t adv_mfg_data = {
    .company_id     = COMPANY_ID_CODE,
    .number_pressed = 0,
};
/* STEP 4.1.1 - Declare the advertising packet */
static const struct bt_data ad[] = {
    /* STEP 4.1.2 - Set the advertising flags */
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
    /* STEP 4.1.3 - Set the advertising packet data  */
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
    BT_DATA(BT_DATA_MANUFACTURER_DATA, (uint8_t*)&adv_mfg_data, sizeof(adv_mfg_data)),

};

/* STEP 4.2.2 - Declare the URL data to include in the scan response */
static unsigned char url_data[] = {0x17, '/', '/', 'a', 'c', 'a', 'd', 'e', 'm', 'y', '.', 'n', 'o',
                                   'r',  'd', 'i', 'c', 's', 'e', 'm', 'i', '.', 'c', 'o', 'm'};

/* STEP 4.2.1 - Declare the scan response packet */
static const struct bt_data sd[] = {
    /* 4.2.3 Include the URL data in the scan response packet */
    // BT_DATA(BT_DATA_URI, url_data, sizeof(url_data)),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL,
                  BT_UUID_128_ENCODE(0x00001523, 0x1212, 0xefde, 0x1523, 0x785feabcd123)),
};

static void button_changed(uint32_t button_state, uint32_t has_changed) {
    if (has_changed & button_state & USER_BUTTON) {
        adv_mfg_data.number_pressed += 1;
        bt_le_adv_update_data(ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    }
}

static int init_button(void) {
    int err;

    err = dk_buttons_init(button_changed);
    if (err) {
        printk("Cannot init buttons (err: %d)\n", err);
    }

    return err;
}

int main(void) {
    int blink_status = 0;
    int err;

    LOG_INF("Starting Lesson 2 - Exercise 1 \n");

    err = dk_leds_init();
    if (err) {
        LOG_ERR("LEDs init failed (err %d)\n", err);
        return -1;
    }

    err = init_button();
    if (err) {

        LOG_ERR("Button init failed (err %d)\n", err);
        return -1;
    }
    bt_addr_le_t addr;
    err = bt_addr_le_from_str("FF:EE:DD:CC:BB:AA", "random", &addr);
    if (err < 0) {
        LOG_ERR("Failed to parse address (err %d)\n", err);
        return -1;
    }
    err = bt_id_create(&addr, NULL);
    if (err < 0) {
        LOG_ERR("Failed to create identity (err %d)\n", err);
        return -1;
    }
    /* STEP 5 - Enable the Bluetooth LE stack */
    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)\n", err);
        return -1;
    }

    LOG_INF("Bluetooth initialized\n");

    /* STEP 6 - Start advertising */
    err = bt_le_adv_start(adv_param, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (err) {
        LOG_ERR("Advertising failed to start (err %d)\n", err);
        return -1;
    }

    LOG_INF("Advertising successfully started\n");

    for (;;) {
        dk_set_led(RUN_STATUS_LED, (++blink_status) % 2);
        k_sleep(K_MSEC(RUN_LED_BLINK_INTERVAL));
    }
}