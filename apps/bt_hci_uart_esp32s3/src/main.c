#include <string.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/logging/log.h>
#include "zephyr/posix/sys/stat.h"
#include "zephyr/sys/util.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define DEVICE_NAME     "PE"
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)
#define COMPANY_ID_CODE 0x0059

typedef struct adv_mfg_data {
    uint16_t company_code; /* Company Identifier Code. */
    uint16_t number_press; /* Number of times Button 1 is pressed*/
} adv_mfg_data_type;

static const struct bt_le_adv_param* ble_adv_param =
    // BT_LE_ADV_PARAM(BT_LE_ADV_OPT_NONE, 800, 801, NULL);
    BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONN | BT_LE_ADV_OPT_USE_IDENTITY, 800, 801, NULL);

static adv_mfg_data_type mfg_data = {
    .company_code = COMPANY_ID_CODE,
    .number_press = 0,
};
/*BT_LE_AD_NO_BREDR means classic Bluetooth (BR/EDR) is not supported.*/
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    // BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
    // BT_DATA(BT_DATA_MANUFACTURER_DATA, (uint8_t*)&mfg_data, sizeof(mfg_data)),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL,
                  BT_UUID_128_ENCODE(0x00001523, 0x1212, 0xefde, 0x1523, 0x785feabcd123)),
    // BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0x00, 0x18),
};
static unsigned char url_data[] = {0x17, '/', '/', 'a', 'c', 'a', 'd', 'e', 'm', 'y', '.', 'n', 'o',
                                   'r',  'd', 'i', 'c', 's', 'e', 'm', 'i', '.', 'c', 'o', 'm'};
static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_URI, url_data, sizeof(url_data)),
};

int main(void) {
    printf("Hello World! %s\n", CONFIG_BOARD_TARGET);
    int err;
    bt_addr_le_t addr;
    err = bt_addr_le_from_str("FF:EE:DD:CC:BB:AA", "random", &addr);
    if (err) {
        LOG_INF("bt_addr_le_from_str failed (err %d)\n", err);
        return 0;
    }
    err = bt_id_create(&addr, NULL);
    if (err < 0) {
        LOG_INF("bt_id_create failed (err %d)\n", err);
    }
    err = bt_enable(NULL);
    if (err) {
        LOG_INF("Bluetooth init failed (err %d)\n", err);
        return 0;
    }
    LOG_INF("Bluetooth initialized\n");
    err = bt_le_adv_start(ble_adv_param, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (err) {
        LOG_INF("Advertising failed to start (err %d)\n", err);
        return 0;
    }
    // while (1) {
    //     k_sleep(K_SECONDS(1));
    //     mfg_data.number_press++;
    //     bt_le_adv_update_data(ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    // }
}

// static void button_changed(uint32_t button_state, uint32_t has_changed) {
//     if (button_state & has_changed & USER_BUTTON) {
//         mfg_data.number_press++;
//         bt_le_adv_update_data(ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
//     }
// }