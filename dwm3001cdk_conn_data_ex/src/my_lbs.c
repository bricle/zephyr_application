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

#include "my_lbs.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(MY_LBS, LOG_LEVEL_DBG);

static struct bt_lbs_cb lbs_cb;

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

BT_GATT_SERVICE_DEFINE(my_lbs,
                       BT_GATT_PRIMARY_SERVICE(BT_UUID_LBS),
                       BT_GATT_CHARACTERISTIC(BT_UUID_LBS_BUTTON,
                                              BT_GATT_CHRC_READ,
                                              BT_GATT_PERM_READ,
                                              read_button,
                                              NULL,
                                              NULL), // No user data needed
);

int my_lbs_init(const struct bt_lbs_cb* callbacks) {
    if (callbacks) {
        lbs_cb.led_write   = callbacks->led_write;
        lbs_cb.button_read = callbacks->button_read;
    }

    return 0;
}