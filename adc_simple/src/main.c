#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);
const struct adc_dt_spec adc_channel = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0);

int main(void) {
    int err;
    uint32_t count = 0;
    int16_t buffer;
    struct adc_sequence sequence = {
        .buffer      = &buffer,
        .buffer_size = sizeof(buffer),

    };
    if (!adc_is_ready_dt(&adc_channel)) {
        LOG_ERR("ADC controller devivce %s not ready", adc_channel.dev->name);
        return 0;
    }
    err = adc_channel_setup_dt(&adc_channel);
    if (err < 0) {
        LOG_ERR("ADC channel setup error %d", err);
        return 0;
    }
    err = adc_sequence_init_dt(&adc_channel, &sequence);
    if (err < 0) {
        LOG_ERR("ADC sequence init error %d", err);
        return 0;
    }
    while (1) {
        int val_mv;
        err = adc_read(adc_channel.dev, &sequence);
        if (err < 0) {
            LOG_ERR("ADC read error %d", err);
            continue;
        }
        val_mv = buffer;
        err = adc_raw_to_millivolts_dt(&adc_channel, &val_mv);
        if (err < 0) {
            LOG_ERR("ADC raw to millivolts error %d", err);
            continue;
        }
        LOG_INF(" = %d mV", val_mv);
        k_msleep(1000);
    }
}