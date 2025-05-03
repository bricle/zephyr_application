#include <stdint.h>
#include <string.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

static const uint8_t manuf_header[4] = {0x6f, 0x6b, 0x64, 0x05};

struct scan_device_info {
    char addr[BT_ADDR_LE_STR_LEN];
    char name[64];
    // bool has_name;
    uint8_t manuf_data[256]; // 存储制造商数据
    uint8_t manuf_data_len;  // 制造商数据长度
    int8_t rssi;
    // bool has_manuf_data;     // 标记是否有制造商数据
};
struct scan_device_info dev_info = {0};
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
            break;
        case BT_DATA_MANUFACTURER_DATA:
            memcpy(dev_info->manuf_data, data->data, data->data_len);
            dev_info->manuf_data_len = data->data_len;
            break;
        default: break;
    }

    return true; // 继续解析下一个字段
}

static void
scan_cb(const bt_addr_le_t* addr, int8_t rssi, uint8_t adv_type, struct net_buf_simple* buf) {
    // struct scan_device_info dev_info = {0};
    if (adv_type == BT_GAP_ADV_TYPE_ADV_IND) {
        memset(&dev_info, 0, sizeof(dev_info));
        dev_info.rssi = rssi;
        // Get address
        bt_addr_le_to_str(addr, dev_info.addr, sizeof(dev_info.addr));

        // Parse advertisement data for name
        bt_data_parse(buf, data_cb, &dev_info);
        if (dev_info.manuf_data_len <= sizeof(manuf_header)) {
            return;
        }
        if (memcmp(dev_info.manuf_data, manuf_header, sizeof(manuf_header)) != 0) {
            return;
        }
        LOG_INF("............................................");
        LOG_INF("oket beacon found");
        LOG_INF("Device: [%s] (RSSI %d), ", dev_info.addr, rssi);
        LOG_INF("oket beacon addr:[%d,%d]",
                dev_info.manuf_data[2] << 8 | dev_info.manuf_data[3],
                dev_info.manuf_data[4] << 8 | dev_info.manuf_data[5]);
        LOG_HEXDUMP_INF(dev_info.manuf_data, dev_info.manuf_data_len, "manuf_data");
        LOG_INF("............................................");

    } else if (adv_type == BT_GAP_ADV_TYPE_SCAN_RSP) {
        memset(&dev_info, 0, sizeof(dev_info));

        // Get address
        bt_addr_le_to_str(addr, dev_info.addr, sizeof(dev_info.addr));
        // Parse advertisement data for name
        bt_data_parse(buf, data_cb, &dev_info);

        if (memcmp(dev_info.name, "oket", 4) == 0) {
            // LOG_HEXDUMP_INF(dev_info.manuf_data, dev_info.manuf_data_len, "manuf_data");
            // LOG_INF("manuf_data_len: %u", dev_info.manuf_data_len);
        }
        return;
    }
}

int main(void) {
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
