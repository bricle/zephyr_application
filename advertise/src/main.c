/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>
#include "zephyr/posix/sys/stat.h"
#include "zephyr/sys/util.h"
#include <dk_buttons_and_leds.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define DEVICE_NAME     CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)
#define COMPANY_ID_CODE 0x0059
#define USER_BUTTON     DK_BTN1_MSK

typedef struct adv_mfg_data {
    uint16_t company_code; /* Company Identifier Code. */
    uint16_t number_press; /* Number of times Button 1 is pressed*/
} adv_mfg_data_type;

static const struct bt_le_adv_param* ble_adv_param =
    BT_LE_ADV_PARAM(BT_LE_ADV_OPT_NONE, 800, 801, NULL);

static adv_mfg_data_type mfg_data = {
    .company_code = COMPANY_ID_CODE,
    .number_press = 0,
};
/*BT_LE_AD_NO_BREDR means classic Bluetooth (BR/EDR) is not supported.*/
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
    BT_DATA(BT_DATA_MANUFACTURER_DATA, (uint8_t*)&mfg_data, sizeof(mfg_data)),
    // BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    // BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0x00, 0x18),
};
static unsigned char url_data[] = {0x17, '/', '/', 'a', 'c', 'a', 'd', 'e', 'm', 'y', '.', 'n', 'o',
                                   'r',  'd', 'i', 'c', 's', 'e', 'm', 'i', '.', 'c', 'o', 'm'};
static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_URI, url_data, sizeof(url_data)),
};

static int init_button(void);
static void button_changed(uint32_t button_state, uint32_t has_changed);

int main(void) {
    printf("Hello World! %s\n", CONFIG_BOARD_TARGET);
    int err;
    err = bt_enable(NULL);
    if (err) {
        LOG_INF("Bluetooth init failed (err %d)\n", err);
        return 0;
    }
    LOG_INF("Bluetooth initialized\n");
    // bt_id_create(bt_addr_le_t *addr, uint8_t *irk)
    err = bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (err) {
        LOG_INF("Advertising failed to start (err %d)\n", err);
        return 0;
    }
    err = init_button();
    if (err) {
        LOG_INF("Button init failed (err %d)\n", err);
        return 0;
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

static void button_changed(uint32_t button_state, uint32_t has_changed) {
    if (button_state & has_changed & USER_BUTTON) {
        mfg_data.number_press++;
        bt_le_adv_update_data(ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    }
}