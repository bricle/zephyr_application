/* main.c - Application main entry point */

/*
 * Copyright (c) 2019 Aaron Tsui <aaron.tsui@outlook.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/services/bas.h>

#include "hts.h"
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#define TEMPSEN_ADD  (0x48)
#define I2C_DEV_NODE DT_ALIAS(i2c_0)
const struct device* const i2c_dev = DEVICE_DT_GET_ANY(nxp_p3t1755);
uint32_t i2c_cfg                   = I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_CONTROLLER;
// extern double temperature;
void configure_sensor(void);
void update_temperature(void);
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID16_ALL,
                  BT_UUID_16_ENCODE(BT_UUID_HTS_VAL),
                  BT_UUID_16_ENCODE(BT_UUID_DIS_VAL),
                  BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
};

static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static void connected(struct bt_conn* conn, uint8_t err) {
    if (err) {
        printk("Connection failed, err 0x%02x %s\n", err, bt_hci_err_to_str(err));
    } else {
        printk("Connected\n");
    }
}

static void disconnected(struct bt_conn* conn, uint8_t reason) {
    printk("Disconnected, reason 0x%02x %s\n", reason, bt_hci_err_to_str(reason));
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected    = connected,
    .disconnected = disconnected,
};

static void bt_ready(void) {
    int err;

    printk("Bluetooth initialized\n");

    hts_init();

    err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (err) {
        printk("Advertising failed to start (err %d)\n", err);
        return;
    }

    printk("Advertising successfully started\n");
}

static void auth_cancel(struct bt_conn* conn) {
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    printk("Pairing cancelled: %s\n", addr);
}

static struct bt_conn_auth_cb auth_cb_display = {
    .cancel = auth_cancel,
};

static void bas_notify(void) {
    uint8_t battery_level = bt_bas_get_battery_level();

    battery_level--;

    if (!battery_level) {
        battery_level = 100U;
    }

    bt_bas_set_battery_level(battery_level);
}

int main(void) {
    int err;

    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return 0;
    }

    bt_ready();

    bt_conn_auth_cb_register(&auth_cb_display);
    configure_sensor();
    /* Implement indicate. At the moment there is no suitable way
	 * of starting delayed work so we do it here
	 */
    while (1) {
        k_sleep(K_SECONDS(1));

        /* Temperature measurements simulation */
        hts_indicate();
        update_temperature();
        /* Battery level simulation */
        bas_notify();
    }
    return 0;
}

void update_temperature(void) {
    unsigned char datas[2];
    if (!device_is_ready(i2c_dev)) {
        printk("I2C device is not ready\n");
    }
    (void)memset(datas, 0, sizeof(datas));
    /*2c_read() */
    // if (i2c_read(i2c_dev, datas, 2, 0x48)) {
    //     printk("Fail to fetch sample from sensor \n");
    // }
    struct sensor_value temp_value;
    int r = sensor_sample_fetch(i2c_dev);
    if (r) {
        printk("sensor_sample_fetch failed return: %d\n", r);
    }
    r = sensor_channel_get(i2c_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp_value);
    if (r) {
        printk("sensor_channel_get failed return: %d\n", r);
    }
    temperature = sensor_value_to_double(&temp_value);
    // temperature = (double)((((uint16_t)datas[0] << 8U) | (uint16_t)datas[1]) >> 4U);
    // temperature = temperature * 0.0625;
    printk("temperature = %f\n", temperature);
    k_sleep(K_MSEC(1));
}

void configure_sensor(void) {
    if (!device_is_ready(i2c_dev)) {
        printk("I2C device is not ready\n");
    }
    /*Verify i2c_configure() */
    // if (i2c_configure(i2c_dev, i2c_cfg)) {
    //     printk("I2C config failed\n");
    // }

    k_sleep(K_MSEC(1));
}