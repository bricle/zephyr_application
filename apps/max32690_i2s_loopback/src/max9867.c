/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include "max9867.h"

#define MAX9867_NODE DT_NODELABEL(max9867)

#define MAX9867_REG_INTERRUPT_ENABLE 0x04
#define MAX9867_REG_SYSTEM_CLOCK     0x05
#define MAX9867_REG_AUDIO_CLOCK_HIGH 0x06
#define MAX9867_REG_AUDIO_CLOCK_LOW  0x07
#define MAX9867_REG_DAI_FORMAT       0x08
#define MAX9867_REG_DAI_CLOCK        0x09
#define MAX9867_REG_FILTERS          0x0a
#define MAX9867_REG_ADC_LEVEL        0x0d
#define MAX9867_REG_LINE_LEFT        0x0e
#define MAX9867_REG_LINE_RIGHT       0x0f
#define MAX9867_REG_VOLUME_LEFT      0x10
#define MAX9867_REG_VOLUME_RIGHT     0x11
#define MAX9867_REG_ADC_INPUT        0x14
#define MAX9867_REG_MICROPHONE       0x15
#define MAX9867_REG_MODE             0x16
#define MAX9867_REG_POWER            0x17
#define MAX9867_REG_REVISION         0xff

#define MAX9867_DAI_MASTER     BIT(7)
#define MAX9867_DAI_I2S_DELAY  BIT(4)
#define MAX9867_DAI_HIZ_OFF    BIT(3)
#define MAX9867_BCLK_PCLK_DIV8 0x06

#define MAX9867_LINE_PLAYBACK_MUTE BIT(6)
#define MAX9867_LINE_GAIN_0_DB     0x0c

#define MAX9867_ADC_LEFT_LINE  (0x2 << 6)
#define MAX9867_ADC_RIGHT_LINE (0x2 << 4)
#define MAX9867_ADC_GAIN_0_DB  0x3

#define MAX9867_HEADPHONE_VOLUME_0_DB 0x09
#define MAX9867_HEADPHONE_STEREO_MODE 0x02

#define MAX9867_POWER_NORMAL     BIT(7)
#define MAX9867_POWER_LINE_LEFT  BIT(6)
#define MAX9867_POWER_LINE_RIGHT BIT(5)
#define MAX9867_POWER_DAC_LEFT   BIT(3)
#define MAX9867_POWER_DAC_RIGHT  BIT(2)
#define MAX9867_POWER_ADC_LEFT   BIT(1)
#define MAX9867_POWER_ADC_RIGHT  BIT(0)

#define MAX9867_SAMPLE_RATE_HZ 44100U

struct max9867_reg_value {
	uint8_t reg;
	uint8_t value;
};

static const struct i2c_dt_spec codec = I2C_DT_SPEC_GET(MAX9867_NODE);

static int max9867_write(uint8_t reg, uint8_t value)
{
	int ret;

	ret = i2c_reg_write_byte_dt(&codec, reg, value);
	if (ret < 0) {
		printk("MAX9867 write reg 0x%02x failed: %d\n", reg, ret);
	}

	return ret;
}

int max9867_init_line_in_monitor(uint32_t sample_rate_hz)
{
	static const struct max9867_reg_value init[] = {
		/* 12.288 MHz MCLK, 44.1 kHz LRCLK: NI = 0x5833. */
		{MAX9867_REG_SYSTEM_CLOCK, 0x10},
		{MAX9867_REG_AUDIO_CLOCK_HIGH, 0x58},
		{MAX9867_REG_AUDIO_CLOCK_LOW, 0x33},
		/* MAX9867 supplies BCLK and LRCLK; use the I2S data format. */
		{MAX9867_REG_DAI_CLOCK, MAX9867_BCLK_PCLK_DIV8},
		{MAX9867_REG_DAI_FORMAT,
		 MAX9867_DAI_MASTER | MAX9867_DAI_I2S_DELAY | MAX9867_DAI_HIZ_OFF},
		{MAX9867_REG_FILTERS, 0xa2},
		/* Select both analog line inputs for the two ADCs. */
		{MAX9867_REG_MICROPHONE, 0x00},
		{MAX9867_REG_ADC_INPUT, MAX9867_ADC_LEFT_LINE | MAX9867_ADC_RIGHT_LINE},
		{MAX9867_REG_ADC_LEVEL, (MAX9867_ADC_GAIN_0_DB << 4) | MAX9867_ADC_GAIN_0_DB},
		/*
		 * Set line gain to 0 dB and mute the direct analog path. Audio
		 * is available only through the ADC and I2S capture path.
		 */
		{MAX9867_REG_LINE_LEFT, MAX9867_LINE_PLAYBACK_MUTE | MAX9867_LINE_GAIN_0_DB},
		{MAX9867_REG_LINE_RIGHT, MAX9867_LINE_PLAYBACK_MUTE | MAX9867_LINE_GAIN_0_DB},
		/* Match the board's stereo capacitorless headphone connection. */
		{MAX9867_REG_MODE, MAX9867_HEADPHONE_STEREO_MODE},
		{MAX9867_REG_VOLUME_LEFT, MAX9867_HEADPHONE_VOLUME_0_DB},
		{MAX9867_REG_VOLUME_RIGHT, MAX9867_HEADPHONE_VOLUME_0_DB},
		/* Enable the ADC-to-I2S and I2S-to-DAC paths for digital monitoring. */
		{MAX9867_REG_POWER, MAX9867_POWER_NORMAL | MAX9867_POWER_LINE_LEFT |
					    MAX9867_POWER_LINE_RIGHT | MAX9867_POWER_DAC_LEFT |
					    MAX9867_POWER_DAC_RIGHT | MAX9867_POWER_ADC_LEFT |
					    MAX9867_POWER_ADC_RIGHT},
	};
	uint8_t revision;
	int ret;

	if (sample_rate_hz != MAX9867_SAMPLE_RATE_HZ) {
		printk("MAX9867 unsupported sample rate: %u Hz\n", sample_rate_hz);
		return -EINVAL;
	}

	if (!i2c_is_ready_dt(&codec)) {
		printk("MAX9867 I2C controller is not ready\n");
		return -ENODEV;
	}

	ret = i2c_reg_read_byte_dt(&codec, MAX9867_REG_REVISION, &revision);
	if (ret < 0) {
		printk("MAX9867 revision read failed: %d\n", ret);
		return ret;
	}

	printk("MAX9867 revision: 0x%02x\n", revision);

	/* The codec has no external reset input. Configure it while shut down. */
	ret = max9867_write(MAX9867_REG_POWER, 0x00);
	if (ret < 0) {
		return ret;
	}

	for (uint8_t reg = MAX9867_REG_INTERRUPT_ENABLE; reg <= MAX9867_REG_POWER; ++reg) {
		ret = max9867_write(reg, 0x00);
		if (ret < 0) {
			return ret;
		}
	}

	for (size_t i = 0; i < ARRAY_SIZE(init); ++i) {
		ret = max9867_write(init[i].reg, init[i].value);
		if (ret < 0) {
			return ret;
		}
	}

	return 0;
}
