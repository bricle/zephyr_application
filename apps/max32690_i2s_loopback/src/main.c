/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "max9867.h"
#include "audio_display.h"

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include <arm_math.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#define AUDIO_I2S_NODE DT_ALIAS(audio_i2s)

#define SAMPLE_RATE_HZ    44100U
#define SAMPLE_BIT_WIDTH  16U
#define CHANNEL_COUNT     2U
#define ANALYSIS_CHANNEL  0U
#define FRAMES_PER_BLOCK  1024U
#define SAMPLES_PER_BLOCK (FRAMES_PER_BLOCK * CHANNEL_COUNT)
#define BLOCK_SIZE        (SAMPLES_PER_BLOCK * sizeof(int16_t))
#define BLOCK_COUNT       8U
#define INITIAL_TX_BLOCKS 2U
#define I2S_TIMEOUT_MS    1000

#define FFT_SIZE               1024U
#define FFT_BIN_COUNT          (FFT_SIZE / 2U)
#define ANALYSIS_BUFFER_COUNT  2U
#define INVALID_BUFFER_INDEX   UINT8_MAX
#define PCM_FULL_SCALE         32768.0f
#define MIN_POWER_DBFS         (-120.0f)
#define MIN_NORMALIZED_POWER   1.0e-12f
#define SILENCE_LEVEL_DBFS     (-80.0f)
#define DSP_THREAD_STACK_SIZE  4096
#define DSP_THREAD_PRIORITY    5
#define SPECTRUM_LOG_PERIOD_MS 500U

BUILD_ASSERT(INITIAL_TX_BLOCKS < BLOCK_COUNT);

K_MEM_SLAB_DEFINE_STATIC(audio_slab, BLOCK_SIZE, BLOCK_COUNT, 4);
K_MSGQ_DEFINE(free_analysis_buffers, sizeof(uint8_t), ANALYSIS_BUFFER_COUNT, 1);
K_MSGQ_DEFINE(ready_analysis_buffers, sizeof(uint8_t), ANALYSIS_BUFFER_COUNT, 1);
K_SEM_DEFINE(dsp_start, 0, 1);

static int16_t analysis_buffers[ANALYSIS_BUFFER_COUNT][FFT_SIZE];
static float32_t fft_input[FFT_SIZE];
static float32_t fft_output[FFT_SIZE];
static float32_t power_spectrum[FFT_BIN_COUNT];
static float32_t hann_window[FFT_SIZE];
static arm_rfft_fast_instance_f32 fft;
static float32_t window_sum;
static atomic_t dropped_analysis_blocks;

static int init_dsp(void)
{
	arm_status status;

	status = arm_rfft_fast_init_1024_f32(&fft);
	if (status != ARM_MATH_SUCCESS) {
		printk("Failed to initialize 1024-point RFFT: %d\n", status);
		return -EINVAL;
	}

	arm_hanning_f32(hann_window, FFT_SIZE);
	for (size_t i = 0; i < FFT_SIZE; ++i) {
		window_sum += hann_window[i];
	}

	for (uint8_t i = 0; i < ANALYSIS_BUFFER_COUNT; ++i) {
		if (k_msgq_put(&free_analysis_buffers, &i, K_NO_WAIT) < 0) {
			printk("Failed to initialize analysis buffer queue\n");
			return -ENOMEM;
		}
	}

	k_sem_give(&dsp_start);
	return 0;
}

static void print_spectrum_result(float32_t frequency_hz, float32_t level_dbfs,
				  uint32_t processing_us)
{
	static uint32_t last_log_ms;
	uint32_t now_ms = k_uptime_get_32();
	uint32_t frequency_tenths = (uint32_t)(frequency_hz * 10.0f + 0.5f);
	int32_t level_tenths =
		(int32_t)(level_dbfs * 10.0f + ((level_dbfs >= 0.0f) ? 0.5f : -0.5f));
	uint32_t level_abs =
		(level_tenths < 0) ? (uint32_t)(-(int64_t)level_tenths) : (uint32_t)level_tenths;
	const char *level_sign = (level_tenths < 0) ? "-" : "";
	const char *state = (level_dbfs < SILENCE_LEVEL_DBFS) ? " (silence/noise)" : "";

	if ((uint32_t)(now_ms - last_log_ms) < SPECTRUM_LOG_PERIOD_MS) {
		return;
	}
	last_log_ms = now_ms;

	printk("Peak: %u.%u Hz, level: %s%u.%u dBFS%s, DSP: %u us, dropped blocks: %u\n",
	       frequency_tenths / 10U, frequency_tenths % 10U, level_sign, level_abs / 10U,
	       level_abs % 10U, state, processing_us,
	       (uint32_t)atomic_get(&dropped_analysis_blocks));
}

static void process_spectrum(const int16_t *samples)
{
	uint32_t start_cycles = k_cycle_get_32();
	float32_t mean = 0.0f;
	float32_t peak_power;
	float32_t peak_bin;
	float32_t normalized_power;
	float32_t level_dbfs;
	uint32_t peak_index;

	for (size_t i = 0; i < FFT_SIZE; ++i) {
		fft_input[i] = (float32_t)samples[i] / PCM_FULL_SCALE;
		mean += fft_input[i];
	}
	mean /= FFT_SIZE;

	for (size_t i = 0; i < FFT_SIZE; ++i) {
		fft_input[i] = (fft_input[i] - mean) * hann_window[i];
	}

	arm_rfft_fast_f32(&fft, fft_input, fft_output, 0);

	/*
	 * The fast RFFT packs DC and Nyquist into fft_output[0] and
	 * fft_output[1]. Compute bins 1 through N/2 - 1 as normal complex
	 * values; the Nyquist bin is not needed for the audio display.
	 */
	power_spectrum[0] = fft_output[0] * fft_output[0];
	arm_cmplx_mag_squared_f32(&fft_output[2], &power_spectrum[1], FFT_BIN_COUNT - 1U);

	arm_max_f32(&power_spectrum[1], FFT_BIN_COUNT - 1U, &peak_power, &peak_index);
	peak_index += 1U;
	peak_bin = (float32_t)peak_index;

	/* Refine the dominant frequency with three-point parabolic interpolation. */
	if ((peak_index > 1U) && (peak_index < (FFT_BIN_COUNT - 1U))) {
		float32_t left = power_spectrum[peak_index - 1U];
		float32_t center = power_spectrum[peak_index];
		float32_t right = power_spectrum[peak_index + 1U];
		float32_t denominator = left - (2.0f * center) + right;

		if (denominator != 0.0f) {
			float32_t offset = 0.5f * (left - right) / denominator;

			if ((offset >= -0.5f) && (offset <= 0.5f)) {
				float32_t interpolated_power =
					center - (0.25f * (left - right) * offset);

				peak_bin += offset;
				if (interpolated_power > peak_power) {
					peak_power = interpolated_power;
				}
			}
		}
	}

	/*
	 * A real sinusoid contributes N/2 times its peak amplitude to one
	 * positive-frequency bin. window_sum corrects the coherent gain of
	 * the Hann window, yielding a peak-amplitude value relative to full
	 * scale. Use power so only one logarithm is needed.
	 */
	normalized_power = (4.0f * peak_power) / (window_sum * window_sum);
	if (normalized_power < MIN_NORMALIZED_POWER) {
		level_dbfs = MIN_POWER_DBFS;
	} else {
		level_dbfs = 10.0f * log10f(normalized_power);
	}

	audio_display_update(samples, FFT_SIZE, power_spectrum, FFT_BIN_COUNT, window_sum);
	print_spectrum_result(peak_bin * (float32_t)SAMPLE_RATE_HZ / (float32_t)FFT_SIZE,
			      level_dbfs, k_cyc_to_us_floor32(k_cycle_get_32() - start_cycles));
}

static void dsp_thread(void *unused1, void *unused2, void *unused3)
{
	uint8_t buffer_index;

	ARG_UNUSED(unused1);
	ARG_UNUSED(unused2);
	ARG_UNUSED(unused3);

	k_sem_take(&dsp_start, K_FOREVER);

	for (;;) {
		k_msgq_get(&ready_analysis_buffers, &buffer_index, K_FOREVER);
		process_spectrum(analysis_buffers[buffer_index]);

		if (k_msgq_put(&free_analysis_buffers, &buffer_index, K_NO_WAIT) < 0) {
			printk("Failed to recycle analysis buffer %u\n", buffer_index);
		}
	}
}

K_THREAD_DEFINE(dsp_thread_id, DSP_THREAD_STACK_SIZE, dsp_thread, NULL, NULL, NULL,
		DSP_THREAD_PRIORITY, 0, 0);

static void collect_analysis_samples(const int16_t *samples, size_t frame_count)
{
	static uint8_t active_buffer = INVALID_BUFFER_INDEX;
	static size_t sample_count;
	size_t frame = 0;

	while (frame < frame_count) {
		size_t copy_count;

		if (active_buffer == INVALID_BUFFER_INDEX) {
			if (k_msgq_get(&free_analysis_buffers, &active_buffer, K_NO_WAIT) < 0) {
				atomic_inc(&dropped_analysis_blocks);
				return;
			}
			sample_count = 0;
		}

		copy_count = MIN(frame_count - frame, FFT_SIZE - sample_count);
		for (size_t i = 0; i < copy_count; ++i) {
			analysis_buffers[active_buffer][sample_count + i] =
				samples[(frame + i) * CHANNEL_COUNT + ANALYSIS_CHANNEL];
		}

		frame += copy_count;
		sample_count += copy_count;

		if (sample_count == FFT_SIZE) {
			if (k_msgq_put(&ready_analysis_buffers, &active_buffer, K_NO_WAIT) < 0) {
				(void)k_msgq_put(&free_analysis_buffers, &active_buffer, K_NO_WAIT);
				atomic_inc(&dropped_analysis_blocks);
			}
			active_buffer = INVALID_BUFFER_INDEX;
		}
	}
}

static int prepare_tx(const struct device *i2s)
{
	for (size_t i = 0U; i < INITIAL_TX_BLOCKS; ++i) {
		void *block;
		int ret;

		ret = k_mem_slab_alloc(&audio_slab, &block, K_NO_WAIT);
		if (ret < 0) {
			printk("Failed to allocate initial TX block %u: %d\n", (uint32_t)i, ret);
			return ret;
		}

		memset(block, 0, BLOCK_SIZE);
		ret = i2s_write(i2s, block, BLOCK_SIZE);
		if (ret < 0) {
			printk("Failed to queue initial TX block %u: %d\n", (uint32_t)i, ret);
			k_mem_slab_free(&audio_slab, block);
			return ret;
		}
	}

	return 0;
}

int main(void)
{
	const struct device *const i2s = DEVICE_DT_GET(AUDIO_I2S_NODE);
	const struct i2s_config config = {
		.word_size = SAMPLE_BIT_WIDTH,
		.channels = CHANNEL_COUNT,
		.format = I2S_FMT_DATA_FORMAT_I2S,
		.options = I2S_OPT_BIT_CLK_TARGET | I2S_OPT_FRAME_CLK_TARGET,
		.frame_clk_freq = SAMPLE_RATE_HZ,
		.mem_slab = &audio_slab,
		.block_size = BLOCK_SIZE,
		.timeout = I2S_TIMEOUT_MS,
	};
	int ret;

	printk("MAX32690EVKIT 44.1 kHz line-in spectrum analyzer with digital monitor\n");

	if (!device_is_ready(i2s)) {
		printk("%s is not ready\n", i2s->name);
		return 0;
	}

	ret = max9867_init_line_in_monitor(SAMPLE_RATE_HZ);
	if (ret < 0) {
		printk("MAX9867 initialization failed: %d\n", ret);
		return 0;
	}

	ret = init_dsp();
	if (ret < 0) {
		return 0;
	}

	ret = audio_display_init(SAMPLE_RATE_HZ, FFT_SIZE);
	if (ret < 0) {
		printk("Display initialization failed: %d\n", ret);
		return 0;
	}

	ret = i2s_configure(i2s, I2S_DIR_BOTH, &config);
	if (ret < 0) {
		printk("Failed to configure I2S RX/TX: %d\n", ret);
		return 0;
	}

	ret = prepare_tx(i2s);
	if (ret < 0) {
		return 0;
	}

	ret = i2s_trigger(i2s, I2S_DIR_BOTH, I2S_TRIGGER_START);
	if (ret < 0) {
		printk("Failed to start I2S RX/TX: %d\n", ret);
		return 0;
	}

	printk("Capturing 44.1 kHz stereo LINE_IN; analyzing left channel, 1024-point FFT\n");
	printk("Digital monitor: J5 LINE_IN -> MAX9867 ADC -> I2S RX/TX -> "
	       "MAX9867 DAC -> J6 HD_PHONE\n");

	for (;;) {
		void *block;
		size_t block_size;
		size_t frame_count;

		ret = i2s_read(i2s, &block, &block_size);
		if (ret < 0) {
			printk("I2S read failed: %d\n", ret);
			break;
		}

		if ((block_size % (CHANNEL_COUNT * sizeof(int16_t))) != 0U) {
			printk("Unexpected I2S block size: %u\n", (uint32_t)block_size);
			k_mem_slab_free(&audio_slab, block);
			break;
		}

		frame_count = block_size / (CHANNEL_COUNT * sizeof(int16_t));
		collect_analysis_samples(block, frame_count);

		/* i2s_write() takes ownership and releases the block after transmission. */
		ret = i2s_write(i2s, block, block_size);
		if (ret < 0) {
			printk("I2S write failed: %d\n", ret);
			k_mem_slab_free(&audio_slab, block);
			break;
		}
	}

	(void)i2s_trigger(i2s, I2S_DIR_BOTH, I2S_TRIGGER_DROP);
	return 0;
}
