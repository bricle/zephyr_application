#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include "zephyr/sys/util_macro.h"
#include "zephyr/toolchain.h"
#include <zephyr/logging/log.h>
#include "button.h"
LOG_MODULE_REGISTER(button, LOG_LEVEL_INF);

static struct gpio_callback button0_cb_data;
void button0_isr(const struct device* port, struct gpio_callback* cb, gpio_port_pins_t pins);

static void cooldown_work_cb(struct k_work* work);

K_WORK_DELAYABLE_DEFINE(cooldown_work, cooldown_work_cb);

int button_init_dt(const struct gpio_dt_spec* spec, button_evt_cb_t cb) {
    int ret;
    if (!gpio_is_ready_dt(spec)) {
        return 0;
    };
    ret = gpio_pin_configure_dt(spec, GPIO_INPUT);
    if (ret < 0) {
        return 0;
    }
    ret = gpio_pin_interrupt_configure_dt(spec, GPIO_INT_EDGE_BOTH);
    if (ret < 0) {
        return 0;
    }
    gpio_init_callback(&button0_cb_data, button0_isr, BIT(spec->pin));
    gpio_add_callback_dt(spec, &button0_cb_data);
    k_work_init_delayable(&cooldown_work, cooldown_work_cb);
    return 0;
}

void button0_isr(const struct device* port, struct gpio_callback* cb, gpio_port_pins_t pins) {
    // gpio_pin_toggle_dt(&led1);
    k_work_reschedule(&cooldown_work, K_MSEC(15));
    LOG_INF("PORT: %p, PINS: %d", port, pins);
}

extern const struct gpio_dt_spec button0;

static void cooldown_work_cb(struct k_work* work) {
    ARG_UNUSED(work);
    int val             = gpio_pin_get_dt(&button0);
    enum button_evt evt = (val == 1 ? BUTTON_EVT_PRESSED : BUTTON_EVT_RELEASED);
    switch (evt) {
        case BUTTON_EVT_PRESSED:  LOG_INF("Button pressed"); break;
        case BUTTON_EVT_RELEASED: LOG_INF("Button released"); break;
        default:                  break;
    }
    LOG_INF("Cooldown callback executed");
}