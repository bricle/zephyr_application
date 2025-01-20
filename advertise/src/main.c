/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

struct scan_device_info {
    char addr[BT_ADDR_LE_STR_LEN];
    char name[64];
    bool has_name;
    uint8_t manuf_data[256]; // 存储制造商数据
    size_t manuf_data_len;   // 制造商数据长度
    bool has_manuf_data;     // 标记是否有制造商数据
};

static void
scan_cb(const bt_addr_le_t* addr, int8_t rssi, uint8_t adv_type, struct net_buf_simple* buf);

static struct bt_le_scan_param scan_param = {
    .type     = BT_LE_SCAN_TYPE_ACTIVE,
    .options  = BT_LE_SCAN_OPT_NONE,
    .interval = 3200,
    .window   = 1600,
};

static bool data_cb(struct bt_data* data, void* user_data) {
    struct scan_device_info* dev_info = user_data;

    switch (data->type) {
        case BT_DATA_NAME_COMPLETE:
            // 提取设备名称
            memcpy(dev_info->name, data->data, data->data_len);
            dev_info->name[data->data_len] = '\0';
            dev_info->has_name             = true;
            break;
        case BT_DATA_MANUFACTURER_DATA:
            // 提取制造商数据
            memcpy(dev_info->manuf_data, data->data, data->data_len);
            dev_info->manuf_data_len = data->data_len;
            dev_info->has_manuf_data = true;
            break;
        default: break;
    }

    return true; // 继续解析下一个字段
}

static void
scan_cb(const bt_addr_le_t* addr, int8_t rssi, uint8_t adv_type, struct net_buf_simple* buf) {
    struct scan_device_info dev_info = {0};

    // Get address
    bt_addr_le_to_str(addr, dev_info.addr, sizeof(dev_info.addr));

    // Parse advertisement data for name
    bt_data_parse(buf, data_cb, &dev_info);

    // LOG_INF("Device: [%s] (RSSI %d), length: %u", dev_info.addr, rssi, buf->len);
    // LOG_INF("length: %u", buf->len);
    if (memcmp(dev_info.name, "oket", 4) == 0 && dev_info.manuf_data_len > 0) {
        LOG_HEXDUMP_INF(dev_info.manuf_data, dev_info.manuf_data_len, "manuf_data");
    }
    if (dev_info.has_name) {
        LOG_HEXDUMP_INF(dev_info.name, strlen(dev_info.name), "name");
    }
    // LOG_HEXDUMP_INF(dev_info.name, strlen(dev_info.name), "name");
    LOG_INF("nothing");
    return;

    // LOG_DBG("Advertisement data length: %u", buf->len);
    // LOG_INF("Device: [%s] (RSSI %d)", dev_info.addr, rssi);
}

int main(void)
{
    printf("Hello World! %s\n", CONFIG_BOARD_TARGET);
    int err;
    err = bt_enable(NULL);
    if (err) {
        LOG_INF("Bluetooth init failed (err %d)\n", err);
        return 0;
    }
    LOG_INF("Bluetooth initialized\n");
    err = bt_le_scan_start(&scan_param, scan_cb);
    if (err) {
        printk("Starting scanning failed (err %d)\n", err);
        return 0;
    }
    return 0;
}
