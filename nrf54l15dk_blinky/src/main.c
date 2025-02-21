/*
 *
 * gpio、按键、日志、自定义函数示例
 */

#include <stdbool.h>
#include <sys/_intsup.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "button.h"
#ifdef CONFIG_MYFUNCTION
#include "myFunc.h"
#endif

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS 1000

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE    DT_ALIAS(led0)
#define LED1_NODE    DT_ALIAS(led1)
#define BUTTON0_NODE DT_ALIAS(sw0)
// #define BUTTON0_NODE DT_NODELABEL(button0)

LOG_MODULE_REGISTER(blinking, LOG_LEVEL_INF);

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
const struct gpio_dt_spec button0     = GPIO_DT_SPEC_GET(BUTTON0_NODE, gpios);

int main(void) {
    int ret;
    bool led_state = true;
    int led1_state = 0;
    button_init_dt(&button0, NULL);
    if (!gpio_is_ready_dt(&led0)) {
        return 0;
    }
    if (!gpio_is_ready_dt(&led1)) {
        return 0;
    }

    ret = gpio_pin_configure_dt(&led0, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        return 0;
    }
    ret = gpio_pin_configure_dt(&led1, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        return 0;
    }

#ifdef CONFIG_LOG
    int exercise_num = 2;
    uint8_t data[]   = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 'H', 'e', 'l', 'l', 'o'};
    LOG_DBG("A log message in debug level");
    LOG_INF("Exercise %d", exercise_num);
    LOG_WRN("A log message in warning level!");
    LOG_ERR("A log message in Error level!");
    //Hexdump some data
    LOG_HEXDUMP_INF(data, sizeof(data), "Sample Data!");
#endif
    while (1) {
        ret = gpio_pin_toggle_dt(&led0);
        if (ret < 0) {
            return 0;
        }
        // 这里形参value是逻辑电平，即1表示灯亮，和设备树中的GPIO_ACTIVE_HIGH一致，物理上1为亮，0为灭；
        // 如果GPIO_ACTIVE_LOW，即物理上1为灭，0为亮；
        gpio_pin_set_dt(&led1, 1);
        led1_state = gpio_pin_get_dt(&led1);
        LOG_INF("LED1 state: %d\n", led1_state);
        k_msleep(1000);
        // 同样，读取到的也是逻辑电平
        gpio_pin_set_dt(&led1, 0);
        led1_state = gpio_pin_get_dt(&led1);
        LOG_INF("LED1 state: %d\n", led1_state);
        led_state = !led_state;
        LOG_INF("LED state: %s\n", led_state ? "ON" : "OFF");
#ifdef CONFIG_MYFUNCTION
        int a = 3, b = 4;
        LOG_INF("The sum of %2d and %2d is %2d\n", a, b, sum(a, b));
#else
        LOG_INF("MYFUNCTION not enabled\n");

#endif
        k_msleep(SLEEP_TIME_MS);
    }
    return 0;
}
