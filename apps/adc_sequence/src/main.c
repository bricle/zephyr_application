#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include "zephyr/sys/printk.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);
const struct adc_dt_spec adc_channel = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0);

int main(void) {
    int err;
    uint32_t count = 0;
    int16_t buffer[5];
    const struct adc_sequence_options options = {
        .extra_samplings = 5 - 1,
        // crucial, extra_samplings field must be set to the total number of samplings - 1
        .interval_us = 0,
    };
    struct adc_sequence sequence = {
        .buffer      = buffer,
        .buffer_size = sizeof(buffer),
        .channels    = BIT(0),
        .options     = &options,

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
        // val_mv = *(int*)sequence.buffer;
        for (uint32_t i = 0; i < 5; i++) {
            val_mv = buffer[i];
            err    = adc_raw_to_millivolts_dt(&adc_channel, &val_mv);
            if (err < 0) {
                LOG_ERR("ADC raw to millivolts error %d", err);
                continue;
            }
            printk(" = %d mV", val_mv);
        }
        printk("\n\r**********************\n\r");
        LOG_HEXDUMP_INF(buffer, sizeof(buffer), "adc buffer");
        k_msleep(1000);
    }
}