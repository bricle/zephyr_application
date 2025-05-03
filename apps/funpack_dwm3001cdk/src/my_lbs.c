#include <stdint.h>
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
#include <zephyr/drivers/display.h>
#include <zephyr/display/cfb.h>
#include "my_lbs.h"
#include "led_strip.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(MY_LBS, LOG_LEVEL_DBG);

static struct bt_lbs_cb lbs_cb;
char uart_display_buffer[100];
static ssize_t read_button(struct bt_conn* conn,
                           const struct bt_gatt_attr* attr,
                           void* buf,
                           uint16_t len,
                           uint16_t offset) {
    LOG_DBG("Attribute read, handle: %u, conn: %p", attr->handle, (void*)conn);

    if (lbs_cb.button_read) {
        // Get button state directly from callback
        uint8_t value = lbs_cb.button_read();
        LOG_DBG("buf: %p, len: %u, offset: %u, value: %u", buf, len, offset, value);
        // Pass the value directly to bt_gatt_attr_read
        return bt_gatt_attr_read(conn, attr, buf, len, offset, &value, sizeof(value));
    }

    return 0;
}

static ssize_t write_led(struct bt_conn* conn,
                         const struct bt_gatt_attr* attr,
                         const void* buf,
                         uint16_t len,
                         uint16_t offset,
                         uint8_t flags) {
    LOG_DBG("attribute write, handle: %u, conn: %p", attr->handle, (void*)conn);
    LOG_DBG("... len: %u, offset: %u, flags: %u", len, offset, flags);
    if (lbs_cb.led_write) {
        // Get the value directly from the buffer
        bool led_state = *((bool*)buf);
        LOG_DBG("led_state: %u", led_state);
        // Pass the value directly to the callback
        lbs_cb.led_write(led_state);
    }
    return len;
};

static ssize_t strip_display_number(struct bt_conn* conn,
                                    const struct bt_gatt_attr* attr,
                                    const void* buf,
                                    uint16_t len,
                                    uint16_t offset,
                                    uint8_t flags) {
    LOG_DBG("attribute write, handle: %u, conn: %p", attr->handle, (void*)conn);
    LOG_DBG("... len: %u, offset: %u, flags: %u", len, offset, flags);
    if (lbs_cb.led_strip_display_number) {
        // Get the value directly from the buffer
        uint8_t number = *((uint8_t*)buf);
        LOG_DBG("number: %u", number);
        // Pass the value directly to the callback
        lbs_cb.led_strip_display_number(number);
    }
    return len;
};
extern const struct device* display;
static ssize_t on_receive(struct bt_conn* conn,
                          const struct bt_gatt_attr* attr,
                          const void* buf,
                          uint16_t len,
                          uint16_t offset,
                          uint8_t flags) {
    LOG_DBG("Received data, handle %d, conn %p", attr->handle, (void*)conn);
    memcpy(uart_display_buffer, buf, len);
    uart_display_buffer[len] = '\0'; // Null-terminate the string
    LOG_DBG("Received data: %s", uart_display_buffer);
    led_strip_scroll_text(uart_display_buffer, SCROLL_LEFT);
    int err = 0;
    cfb_framebuffer_clear(display, 0);
    err = cfb_print(display, uart_display_buffer, 0, 0);
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
    // if (lbs_cb.received) {
    //     lbs_cb.received(conn, buf, len);
    // }
    // return len;
}
bool indicate_enabled;
bool notify_mysensor_enabled;
static struct bt_gatt_indicate_params indi_params;

static void my_lbs_changed_cb(const struct bt_gatt_attr* attr, uint16_t value) {
    LOG_DBG("Attribute changed: %u", value);
    indicate_enabled = (value == BT_GATT_CCC_INDICATE) ? true : false;
}

static void mylbsbc_ccc_mysensor_cfg_changed(const struct bt_gatt_attr* attr, uint16_t value) {
    LOG_DBG("... CCC MYSENSOR cfg changed: %u", value);
    notify_mysensor_enabled = (value == BT_GATT_CCC_NOTIFY);
}

BT_GATT_SERVICE_DEFINE(my_lbs,
                       BT_GATT_PRIMARY_SERVICE(BT_UUID_LBS),
                       BT_GATT_CHARACTERISTIC(BT_UUID_LBS_BUTTON,
                                              BT_GATT_CHRC_READ | BT_GATT_CHRC_INDICATE,
                                              BT_GATT_PERM_READ,
                                              read_button,
                                              NULL,
                                              NULL), // No user data needed
                       BT_GATT_CCC(my_lbs_changed_cb, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
                       BT_GATT_CHARACTERISTIC(BT_UUID_LBS_LED,
                                              BT_GATT_CHRC_WRITE,
                                              BT_GATT_PERM_WRITE,
                                              NULL,
                                              write_led,
                                              NULL),
                       BT_GATT_CHARACTERISTIC(BT_UUID_LBS_MYSENSOR,
                                              BT_GATT_CHRC_NOTIFY,
                                              BT_GATT_PERM_NONE,
                                              NULL,
                                              NULL,
                                              NULL),
                       BT_GATT_CCC(mylbsbc_ccc_mysensor_cfg_changed,
                                   BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
                       BT_GATT_CHARACTERISTIC(BT_UUID_LBS_LED_STRIP,
                                              BT_GATT_CHRC_WRITE,
                                              BT_GATT_PERM_WRITE,
                                              NULL,
                                              strip_display_number,
                                              NULL),
                       BT_GATT_CHARACTERISTIC(BT_UUID_NUS_RX,
                                              BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                                              BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
                                              NULL,
                                              on_receive,
                                              NULL));

void indi_cb(struct bt_conn* conn, struct bt_gatt_indicate_params* params, uint8_t err) {
    if (err != 0) {
        LOG_ERR("Indication failed: %d", err);
    } else {
        LOG_DBG("Indication sent successfully");
    }
    // Reset the indication parameters
    // indicate_enabled = false;
    // memset(&indi_params, 0, sizeof(indi_params));
}

int my_lbs_sent_btn_indi(bool btn_state) {
    if (!indicate_enabled) {
        return -EACCES;
    }
    indi_params.attr    = &my_lbs.attrs[2];
    indi_params.data    = &btn_state;
    indi_params.len     = sizeof(btn_state);
    indi_params.func    = indi_cb;
    indi_params.destroy = NULL; // No need to destroy the params
    return bt_gatt_indicate(NULL, &indi_params);
}

int my_lbs_send_sensor_notify(uint32_t sensor_value) {
    if (!notify_mysensor_enabled) {
        return -EACCES;
    }

    return bt_gatt_notify(NULL, &my_lbs.attrs[7], &sensor_value, sizeof(sensor_value));
}

int my_lbs_init(const struct bt_lbs_cb* callbacks) {
    if (callbacks) {
        lbs_cb.led_write   = callbacks->led_write;
        lbs_cb.button_read = callbacks->button_read;
        lbs_cb.led_strip_display_number = callbacks->led_strip_display_number;
    }

    return 0;
}