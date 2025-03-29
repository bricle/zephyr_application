#ifndef MY_LBS_H_
#define MY_LBS_H_

#include <zephyr/types.h>
#include "src/led_strip.h"
#define BT_UUID_LBS_VAL BT_UUID_128_ENCODE(0x00001532, 0x1212, 0xefde, 0x1523, 0x785feabcd123)

/** @brief Button Characteristic UUID. */
#define BT_UUID_LBS_BUTTON_VAL                                                                     \
	BT_UUID_128_ENCODE(0x00001533, 0x1212, 0xefde, 0x1523, 0x785feabcd123)

/** @brief LED Characteristic UUID. */
#define BT_UUID_LBS_LED_VAL BT_UUID_128_ENCODE(0x00001534, 0x1212, 0xefde, 0x1523, 0x785feabcd123)
#define BT_UUID_LBS_MYSENSOR_VAL \
	BT_UUID_128_ENCODE(0x00001535, 0x1212, 0xefde, 0x1523, 0x785feabcd123)
#define BT_UUID_LBS_LED_STRIP_VAL                                                                  \
    BT_UUID_128_ENCODE(0x00001536, 0x1212, 0xefde, 0x1523, 0x785feabcd123)
#define  BT_UUID_LBS BT_UUID_DECLARE_128(BT_UUID_LBS_VAL)
#define  BT_UUID_LBS_BUTTON BT_UUID_DECLARE_128(BT_UUID_LBS_BUTTON_VAL)
#define BT_UUID_LBS_LED     BT_UUID_DECLARE_128(BT_UUID_LBS_LED_VAL)
#define BT_UUID_LBS_MYSENSOR     BT_UUID_DECLARE_128(BT_UUID_LBS_MYSENSOR_VAL)
#define BT_UUID_LBS_LED_STRIP    BT_UUID_DECLARE_128(BT_UUID_LBS_LED_STRIP_VAL)

struct bt_lbs_cb {
    bool (*button_read)(void);
    void (*led_write)(const bool led_state);
    void (*led_strip_display_number)(uint8_t number);
};

int my_lbs_init(const struct bt_lbs_cb* callbacks);
int my_lbs_sent_btn_indi(bool btn_state);
int my_lbs_send_sensor_notify(uint32_t sensor_value);
#endif // MY_LBS_H_