/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/addr.h>
#include <dk_buttons_and_leds.h>
#include "zephyr/bluetooth/conn.h"
#include <zephyr/drivers/display.h>
#include <zephyr/display/cfb.h>
#include <stdio.h>
#include "my_lbs.h"
#include "logo.h"
LOG_MODULE_REGISTER(FUNPACK_DWM3001CDK, LOG_LEVEL_INF);
#define COMPANY_ID_CODE 0x0059

#define DEVICE_NAME     CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define RUN_STATUS_LED         DK_LED4
#define RUN_LED_BLINK_INTERVAL 1000
#define CONNECTION_STATUS_LED  DK_LED2
#define LBS_LED                DK_LED1
#define USER_BUTTON            DK_BTN1_MSK
#define BT_UUID_LBS_VAL        BT_UUID_128_ENCODE(0x00001523, 0x1212, 0xefde, 0x1523, 0x785feabcd123)
#define STACKSIZE              1024
#define PRIORITY               7

#define DISPLAY_BUFFER_PITCH 128
// static const struct device* display = DEVICE_DT_GET(DT_NODELABEL(ssd1306_ssd1306_128x64));
struct bt_conn* default_conn        = NULL;
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
}

void on_disconnected(struct bt_conn* conn, uint8_t reason) {
    dk_set_led_off(CONNECTION_STATUS_LED);
    if (default_conn) {
        bt_conn_unref(default_conn);
        // default_conn = NULL;
    }
    LOG_INF("Disconnected (reason %u)\n", reason);
}

struct bt_conn_cb conn_callbacks = {
    .connected    = on_connected,
    .disconnected = on_disconnected,

};
static uint32_t user_button_state;

static void button_changed(uint32_t button_state, uint32_t has_changed) {
    int err;
    bool btn_ched = (has_changed & USER_BUTTON) ? true : false;
    bool btn_pred = (button_state & USER_BUTTON) ? true : false;
    if (btn_ched) {
        LOG_INF("Button %s\n", btn_pred ? "pressed" : "released");
        user_button_state = button_state & USER_BUTTON;
        my_lbs_sent_btn_indi(user_button_state);
        // if (has_changed & button_state & USER_BUTTON) {
        //     adv_mfg_data.number_pressed += 1;
        //     bt_le_adv_update_data(ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
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
    .led_write   = led_cb,
    .button_read = btn_cb,
};
static uint32_t app_sensor_value = 100;

static void simulate_data(void) {
    app_sensor_value++;
    if (app_sensor_value == 200) {
        app_sensor_value = 100;
    }
}

void send_data_thread(void) {
    while (1) {
        /* Simulate data */
        simulate_data();
        /* Send notification, the function sends notifications only if a client is subscribed */
        my_lbs_send_sensor_notify(app_sensor_value);

        k_sleep(K_MSEC(1500));
    }
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
    my_lbs_init(&lbs_cb);

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

    const struct device* display;
    display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (display == NULL) {
        LOG_ERR("device pointer is NULL");
        return -1;
    }

    if (!device_is_ready(display)) {
        LOG_ERR("display device is not ready");
        return -1;
    }
    err = cfb_framebuffer_init(display);
    if (err < 0) {
        LOG_ERR("Framebuffer initialization failed");
        return -1;
    }
    // cfb_set_kerning(display, 3);
    err = cfb_print(display, "hello world", 0, 0);
    if (err < 0) {
        LOG_ERR("cfb_print failed");
        return -1;
    }
    err = cfb_framebuffer_invert(display);
    err = cfb_framebuffer_finalize(display);
    if (err < 0) {
        LOG_ERR("Framebuffer finalization failed");
        return -1;
    }
    struct display_capabilities capabilities;
    display_get_capabilities(display, &capabilities);

    const uint16_t x_res = capabilities.x_resolution;
    const uint16_t y_res = capabilities.y_resolution;

    LOG_INF("x_resolution: %d", x_res);
    LOG_INF("y_resolution: %d", y_res);
    LOG_INF("supported pixel formats: %d", capabilities.supported_pixel_formats);
    LOG_INF("screen_info: %d", capabilities.screen_info);
    LOG_INF("current_pixel_format: %d", capabilities.current_pixel_format);
    LOG_INF("current_orientation: %d", capabilities.current_orientation);

    const struct display_buffer_descriptor buf_desc = {.width    = x_res,
                                                       .height   = y_res,
                                                       .buf_size = x_res * y_res,
                                                       .pitch    = DISPLAY_BUFFER_PITCH};

    // if (display_write(display, 0, 0, &buf_desc, buf) != 0) {
    //     LOG_ERR("could not write to display");
    // }

    // if (display_set_contrast(display, 0) != 0) {
    //     LOG_ERR("could not set display contrast");
    // }
    size_t ms_sleep = 50;
    for (;;) {
        // dk_set_led(RUN_STATUS_LED, (++blink_status) % 2);

        k_sleep(K_MSEC(RUN_LED_BLINK_INTERVAL));
        // for (size_t i = 0; i < 255; i++) {
        //     display_set_contrast(display, i);
        //     k_sleep(K_MSEC(ms_sleep));
        // }

        // // Decrease brightness
        // for (size_t i = 255; i > 0; i--) {
        //     display_set_contrast(display, i);
        //     k_sleep(K_MSEC(ms_sleep));
        // }
    }
}

K_THREAD_DEFINE(send_data_thread_id, STACKSIZE, send_data_thread, NULL, NULL, NULL, PRIORITY, 0, 0);