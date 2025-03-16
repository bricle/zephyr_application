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
#include "zephyr/bluetooth/conn.h"
#include "zephyr/bluetooth/gatt.h"
#include <bluetooth/services/lbs.h>
LOG_MODULE_REGISTER(Lesson2_Exercise1, LOG_LEVEL_INF);
#define COMPANY_ID_CODE 0x0059

#define DEVICE_NAME     CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define RUN_STATUS_LED         DK_LED4
#define RUN_LED_BLINK_INTERVAL 1000
#define CONNECTION_STATUS_LED  DK_LED2
#define LBS_LED                DK_LED1
#define USER_BUTTON            DK_BTN1_MSK
struct bt_conn* default_conn                   = NULL;
static const struct bt_le_adv_param* adv_param = BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONN
                                                                     | BT_LE_ADV_OPT_USE_IDENTITY,
                                                                 BT_GAP_ADV_FAST_INT_MIN_1,
                                                                 BT_GAP_ADV_FAST_INT_MIN_1,
                                                                 NULL);

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

static void update_data_len(struct bt_conn* conn) {
    int err;
    struct bt_conn_le_data_len_param my_param = {
        .tx_max_len  = BT_GAP_DATA_LEN_MAX,
        .tx_max_time = BT_GAP_DATA_TIME_MAX,
    };
    err = bt_conn_le_data_len_update(conn, &my_param);
    if (err) {
        LOG_ERR("data len update failed (err %d)", err);
    }
};

static void ex_func(struct bt_conn* conn, uint8_t att_err, struct bt_gatt_exchange_params* params) {
    LOG_INF("MTU exchange %s", att_err == 0 ? "successful" : "failed");
    if (!att_err) {
        uint16_t payload_mtu = bt_gatt_get_mtu(conn) - 3; // 3 bytes used for Attribute headers.
        LOG_INF("New MTU: %d bytes", payload_mtu);
    }
}
static struct bt_gatt_exchange_params exchange_params;

static void update_mtu(struct bt_conn* conn) {
    int err;
    exchange_params.func = ex_func;
    err                  = bt_gatt_exchange_mtu(conn, &exchange_params);
    if (err) {
        LOG_ERR("bt gatt ex mtu failed (err %d)", err);
    }
}

void on_le_data_len_updated(struct bt_conn* conn, struct bt_conn_le_data_len_info* info) {
    uint16_t tx_len  = info->tx_max_len;
    uint16_t tx_time = info->tx_max_time;
    uint16_t rx_len  = info->rx_max_len;
    uint16_t rx_time = info->rx_max_time;
    LOG_INF("Data length updated. Length %d/%d bytes, time %d/%d us",
            tx_len,
            rx_len,
            tx_time,
            rx_time);
}
void on_connected(struct bt_conn* conn, uint8_t err) {
    if (err) {
        LOG_ERR("Failed to connect (err %d)\n", err);
        return;
    }
    LOG_INF("...Connected\n");
    default_conn = bt_conn_ref(conn);
    dk_set_led_on(CONNECTION_STATUS_LED);
    struct bt_conn_info info;
    bt_conn_get_info(default_conn, &info);
    LOG_INF("Connected to %02x:%02x:%02x:%02x:%02x:%02x\n",
            info.le.dst->a.val[5],
            info.le.dst->a.val[4],
            info.le.dst->a.val[3],
            info.le.dst->a.val[2],
            info.le.dst->a.val[1],
            info.le.dst->a.val[0]);
    double coon_intvl = info.le.interval * 1.25;
    uint16_t spv_tmo  = info.le.timeout * 10;
    LOG_INF("coon parameter: interval: %.2f ms; latency %d intervals, timeout %d ms",
            coon_intvl,
            info.le.latency,
            spv_tmo);
    update_data_len(conn);
    update_mtu(conn);
}

void on_disconnected(struct bt_conn* conn, uint8_t reason) {
    dk_set_led_off(CONNECTION_STATUS_LED);
    if (default_conn) {
        bt_conn_unref(default_conn);
        // default_conn = NULL;
    }
    LOG_INF("Disconnected (reason %u)\n", reason);
}

void on_le_param_updated(struct bt_conn* conn,
                         uint16_t interval,
                         uint16_t latency,
                         uint16_t timeout) {
    double coon_intvl = interval * 1.25;
    uint16_t spv_tmo  = timeout * 10;
    LOG_INF("connection parameter updated: interval: %.2f ms; latency %d intervals, timeout %d ms",
            coon_intvl,
            latency,
            spv_tmo);
};

struct bt_conn_cb conn_callbacks = {
    .connected           = on_connected,
    .disconnected        = on_disconnected,
    .le_param_updated    = on_le_param_updated,
    .le_data_len_updated = on_le_data_len_updated,
};

static void button_changed(uint32_t button_state, uint32_t has_changed) {
    int err;
    bool btn_ched = (has_changed & USER_BUTTON) ? true : false;
    bool btn_pred = (button_state & USER_BUTTON) ? true : false;
    if (btn_ched) {
        LOG_INF("Button %s\n", btn_pred ? "pressed" : "released");
        err = bt_lbs_send_button_state(btn_pred);
        if (err) {
            LOG_ERR("Failed to send button state, err %d\n", err);
        }
    }
    // if (has_changed & button_state & USER_BUTTON) {
    //     adv_mfg_data.number_pressed += 1;
    //     bt_le_adv_update_data(ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    // }
}

static int init_button(void) {
    int err;

    err = dk_buttons_init(button_changed);
    if (err) {
        printk("Cannot init buttons (err: %d)\n", err);
    }

    return err;
}

bool btn_cb(void) {
    return dk_get_buttons() & USER_BUTTON;
}

void led_cb(const bool led_state) {
    if (led_state) {
        dk_set_led_on(LBS_LED);
    } else {
        dk_set_led_off(LBS_LED);
    }
}

struct bt_lbs_cb lbs_cb = {
    .led_cb    = led_cb,
    .button_cb = btn_cb,
};

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
    err = bt_lbs_init(&lbs_cb);
    if (err) {
        LOG_ERR("Failed to init LBS (err %d)\n", err);
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
    err = bt_conn_cb_register(&conn_callbacks);
    if (err < 0) {
        LOG_ERR("Failed to register connection callbacks (err %d)\n", err);
        return -1;
    }
    err = bt_enable(NULL);
    if (err < 0) {
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