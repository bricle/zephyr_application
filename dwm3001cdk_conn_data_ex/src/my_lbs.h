#ifndef MY_LBS_H_
#define MY_LBS_H_

#include <zephyr/types.h>
#define BT_UUID_LBS_VAL BT_UUID_128_ENCODE(0x00001523, 0x1212, 0xefde, 0x1523, 0x785feabcd123)

/** @brief Button Characteristic UUID. */
#define BT_UUID_LBS_BUTTON_VAL                                                                     \
	BT_UUID_128_ENCODE(0x00001524, 0x1212, 0xefde, 0x1523, 0x785feabcd123)

/** @brief LED Characteristic UUID. */
#define BT_UUID_LBS_LED_VAL BT_UUID_128_ENCODE(0x00001525, 0x1212, 0xefde, 0x1523, 0x785feabcd123)


#define  BT_UUID_LBS BT_UUID_DECLARE_128(BT_UUID_LBS_VAL)
#define  BT_UUID_LBS_BUTTON BT_UUID_DECLARE_128(BT_UUID_LBS_BUTTON_VAL)
#define BT_UUID_LBS_LED     BT_UUID_DECLARE_128(BT_UUID_LBS_LED_VAL)

struct bt_lbs_cb {
    bool (*button_read)(void);
    void (*led_write)(const bool led_state);
};

int my_lbs_init(const struct bt_lbs_cb* callbacks);
#endif // MY_LBS_H_