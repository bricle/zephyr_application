#include <sys/_intsup.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#define PWM_PERIOD_NS   100000
#define PWM_PULSE_WIDTH 15000
// clang-format off
#define PWM_LED0        DT_ALIAS(pwmtestled1)
#define PWM_LED1        DT_ALIAS(pwmtestled2)
// #define PWM_LED0        DT_ALIAS(pwm_led0)
// this alias is defined in the devicetree as "pwm-led0", but here you should use "pwm_led0"
// clang-format on

static const struct pwm_dt_spec pwm_led0 = PWM_DT_SPEC_GET(PWM_LED0);
static const struct pwm_dt_spec pwm_led1 = PWM_DT_SPEC_GET(PWM_LED1);

int main(void) {
    int err;
    if (!pwm_is_ready_dt(&pwm_led0)) {
        LOG_ERR("Error: PWM device %s is not ready", pwm_led0.dev->name);
        return 0;
    }
    if (!pwm_is_ready_dt(&pwm_led1)) {
        LOG_ERR("Error: PWM device %s is not ready", pwm_led0.dev->name);
        return 0;
    }
    err = pwm_set_dt(&pwm_led0, PWM_PERIOD_NS, PWM_PULSE_WIDTH);
    if (err) {
        LOG_ERR("Error in pwm_set_dt(), err: %d", err);
        return 0;
    }
    err = pwm_set_dt(&pwm_led1, PWM_PERIOD_NS, PWM_PULSE_WIDTH);
    if (err) {
        LOG_ERR("Error in pwm_set_dt(), err: %d", err);
        return 0;
    }

    return 0;
}