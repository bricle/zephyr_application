#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/addr.h>
#include <dk_buttons_and_leds.h>
#include <zephyr/drivers/display.h>
#include <zephyr/display/cfb.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/drivers/sensor.h>
#include "my_lbs.h"
#include "bh1750_sensor.h"
LOG_MODULE_REGISTER(BH1750_SENSOR, LOG_LEVEL_INF);
#define STACKSIZE 1024
#define PRIORITY  7
static uint32_t app_sensor_value = 100;

static const struct device* get_bh1750_device(void) {
    const struct device* const dev = DEVICE_DT_GET_ANY(rohm_bh1750);

    if (dev == NULL) {
        // No such node, or the node does not have status "okay".
        LOG_ERR("no device found");
        return NULL;
    }

    if (!device_is_ready(dev)) {
        LOG_ERR("Device \"%s\" is not ready; "
                "check the driver initialization logs for errors.",
                dev->name);
        return NULL;
    }

    LOG_INF("Found device \"%s\", getting sensor data\n", dev->name);
    return dev;
}

void bh1750_thread(void) {
    const struct device* bh1750_dev = get_bh1750_device();
    struct sensor_value light;

    while (1) {

        LOG_INF("Fetching sensor data");

        sensor_sample_fetch(bh1750_dev);

        LOG_INF("Fetching sensor data done");

        sensor_channel_get(bh1750_dev, SENSOR_CHAN_ALL, &light);
        printk("Light: %d.%06d\n", light.val1, light.val2);
        my_lbs_send_sensor_notify(light.val1);
        k_sleep(K_MSEC(2000));
    }
}

K_THREAD_DEFINE(bh1750_thread_id, STACKSIZE, bh1750_thread, NULL, NULL, NULL, PRIORITY, 0, 0);
