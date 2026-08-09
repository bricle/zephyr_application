/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "audio_display.h"

#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <mxc_device.h>
#include <zephyr/arch/cpu.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/mipi_dbi.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#define DISPLAY_NODE     DT_CHOSEN(zephyr_display)
#define DISPLAY_SPI_NODE DT_PHANDLE(DT_PARENT(DISPLAY_NODE), spi_dev)
#define DISPLAY_CS_INDEX DT_REG_ADDR_RAW(DISPLAY_NODE)

#define DISPLAY_CLK_PIN  DT_GPIO_PIN(DISPLAY_SPI_NODE, clk_gpios)
#define DISPLAY_MOSI_PIN DT_GPIO_PIN(DISPLAY_SPI_NODE, mosi_gpios)
#define DISPLAY_CS_PIN   DT_GPIO_PIN_BY_IDX(DISPLAY_SPI_NODE, cs_gpios, DISPLAY_CS_INDEX)

#define DISPLAY_CLK_CTLR  DT_GPIO_CTLR(DISPLAY_SPI_NODE, clk_gpios)
#define DISPLAY_MOSI_CTLR DT_GPIO_CTLR(DISPLAY_SPI_NODE, mosi_gpios)
#define DISPLAY_CS_CTLR   DT_GPIO_CTLR_BY_IDX(DISPLAY_SPI_NODE, cs_gpios, DISPLAY_CS_INDEX)

#if !DT_NODE_HAS_STATUS(DISPLAY_NODE, okay)
#error "A ready zephyr,display devicetree node is required"
#endif

#define DISPLAY_WIDTH    DT_PROP(DISPLAY_NODE, width)
#define DISPLAY_HEIGHT   DT_PROP(DISPLAY_NODE, height)
#define DISPLAY_X_OFFSET DT_PROP(DISPLAY_NODE, x_offset)
#define DISPLAY_Y_OFFSET DT_PROP(DISPLAY_NODE, y_offset)

#define WAVEFORM_TOP            1U
#define WAVEFORM_BOTTOM         ((DISPLAY_HEIGHT / 2U) - 2U)
#define WAVEFORM_CENTER         ((WAVEFORM_TOP + WAVEFORM_BOTTOM) / 2U)
#define WAVEFORM_HEIGHT         (WAVEFORM_BOTTOM - WAVEFORM_TOP + 1U)
#define WAVEFORM_PCM_FULL_SCALE ((int32_t)INT16_MAX + 1)

#define DISPLAY_DIVIDER_Y         (DISPLAY_HEIGHT / 2U)
#define SPECTRUM_TOP              (DISPLAY_DIVIDER_Y + 2U)
#define SPECTRUM_BOTTOM           (DISPLAY_HEIGHT - 2U)
#define SPECTRUM_HEIGHT           (SPECTRUM_BOTTOM - SPECTRUM_TOP + 1U)
#define SPECTRUM_BAR_COUNT        32U
#define SPECTRUM_FLOOR_DBFS       (-80.0f)
#define SPECTRUM_CEILING_DBFS     0.0f
#define SPECTRUM_MAX_FREQUENCY_HZ 20000.0f

#define SNAPSHOT_MIN_INTERVAL_MS  40U
#define DISPLAY_REFRESH_PERIOD_MS 80U
#define DISPLAY_STATS_PERIOD_MS   2000U
#define DISPLAY_THREAD_STACK_SIZE 2048
#define DISPLAY_THREAD_PRIORITY   7

#define SPECTRUM_RISE_STEP            8U
#define SPECTRUM_FALL_STEP            4U
#define FAST_TX_WORD_CAPACITY         17600U
#define FAST_FRAME_SETUP_WORDS        2U
#define FAST_RECTANGLE_OVERHEAD_WORDS 11U
#define FAST_PIXEL_WORDS              2U
#define MIPI_DBI_DATA_FLAG            BIT(8)

#define ST7735_CMD_COLMOD 0x3aU
#define ST7735_CMD_CASET  0x2aU
#define ST7735_CMD_RASET  0x2bU
#define ST7735_CMD_RAMWR  0x2cU
#define ST7735_COLMOD     DT_PROP(DISPLAY_NODE, colmod)

#define RGB565_GRID    0x18e3U
#define RGB565_DIVIDER 0x4208U
#define RGB565_CYAN    0x07ffU
#define RGB565_GREEN   0x07e0U
#define RGB565_YELLOW  0xffe0U
#define RGB565_RED     0xf800U

BUILD_ASSERT(DISPLAY_WIDTH > SPECTRUM_BAR_COUNT);
BUILD_ASSERT(DISPLAY_HEIGHT >= 64U);
BUILD_ASSERT(DISPLAY_HEIGHT <= UINT8_MAX);
BUILD_ASSERT(DT_NODE_HAS_COMPAT(DISPLAY_NODE, sitronix_st7735r));
BUILD_ASSERT(DT_STRING_UPPER_TOKEN(DISPLAY_NODE, mipi_mode) == MIPI_DBI_MODE_SPI_3WIRE);
BUILD_ASSERT(DT_PROP(DISPLAY_NODE, mipi_cpol) == 0);
BUILD_ASSERT(DT_PROP(DISPLAY_NODE, mipi_cpha) == 0);
BUILD_ASSERT(ST7735_COLMOD == 0x05U);
BUILD_ASSERT(IS_ENABLED(CONFIG_SOC_MAX32690_M4));
BUILD_ASSERT(CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC == MHZ(120));
BUILD_ASSERT(DT_PROP(DISPLAY_NODE, mipi_max_frequency) == MHZ(6));
BUILD_ASSERT(DISPLAY_CLK_PIN < 32U);
BUILD_ASSERT(DISPLAY_MOSI_PIN < 32U);
BUILD_ASSERT(DISPLAY_CS_PIN < 32U);
BUILD_ASSERT(DT_REG_ADDR(DT_NODELABEL(gpio2)) == MXC_BASE_GPIO2);
BUILD_ASSERT(DT_SAME_NODE(DISPLAY_CLK_CTLR, DT_NODELABEL(gpio2)));
BUILD_ASSERT(DT_SAME_NODE(DISPLAY_MOSI_CTLR, DT_NODELABEL(gpio2)));
BUILD_ASSERT(DT_SAME_NODE(DISPLAY_CS_CTLR, DT_NODELABEL(gpio2)));
BUILD_ASSERT((DT_GPIO_FLAGS(DISPLAY_SPI_NODE, clk_gpios) & GPIO_ACTIVE_LOW) == 0U);
BUILD_ASSERT((DT_GPIO_FLAGS(DISPLAY_SPI_NODE, mosi_gpios) & GPIO_ACTIVE_LOW) == 0U);
BUILD_ASSERT((DT_GPIO_FLAGS_BY_IDX(DISPLAY_SPI_NODE, cs_gpios, DISPLAY_CS_INDEX) &
	      GPIO_ACTIVE_LOW) != 0U);
BUILD_ASSERT(
	FAST_TX_WORD_CAPACITY >=
	(FAST_FRAME_SETUP_WORDS +
	 (SPECTRUM_BAR_COUNT * (FAST_RECTANGLE_OVERHEAD_WORDS +
				(MAX(SPECTRUM_RISE_STEP, SPECTRUM_FALL_STEP) * FAST_PIXEL_WORDS))) +
	 FAST_RECTANGLE_OVERHEAD_WORDS + (DISPLAY_WIDTH * WAVEFORM_HEIGHT * FAST_PIXEL_WORDS)));

struct display_snapshot {
	uint8_t waveform_top[DISPLAY_WIDTH];
	uint8_t waveform_bottom[DISPLAY_WIDTH];
	uint8_t spectrum_height[SPECTRUM_BAR_COUNT];
	uint32_t sequence;
};

struct display_render_state {
	uint8_t spectrum_height[SPECTRUM_BAR_COUNT];
};

static const struct device *const display = DEVICE_DT_GET(DISPLAY_NODE);
static const struct device *const display_spi = DEVICE_DT_GET(DISPLAY_SPI_NODE);
static uint16_t framebuffer[DISPLAY_HEIGHT][DISPLAY_WIDTH];
static uint16_t fast_tx_words[FAST_TX_WORD_CAPACITY];
static uint16_t spectrum_bin_edges[SPECTRUM_BAR_COUNT + 1U];
static struct display_snapshot latest_snapshot;
static struct display_render_state displayed_state;
static struct display_render_state prepared_state;
static struct k_spinlock snapshot_lock;
static enum display_pixel_format pixel_format;
static size_t fast_tx_word_count;
static uint32_t last_snapshot_ms;
static atomic_t display_initialized;

K_SEM_DEFINE(display_start, 0, 1);

static uint16_t encode_color(uint16_t rgb565)
{
	if (pixel_format == PIXEL_FORMAT_RGB_565X) {
		return sys_cpu_to_be16(rgb565);
	}

	return rgb565;
}

static void put_pixel(uint16_t x, uint16_t y, uint16_t color)
{
	if ((x < DISPLAY_WIDTH) && (y < DISPLAY_HEIGHT)) {
		framebuffer[y][x] = encode_color(color);
	}
}

static void draw_vertical_line(uint16_t x, uint16_t y_start, uint16_t y_end, uint16_t color)
{
	if (y_start > y_end) {
		uint16_t temporary = y_start;

		y_start = y_end;
		y_end = temporary;
	}

	for (uint16_t y = y_start; y <= y_end; ++y) {
		put_pixel(x, y, color);
	}
}

static void draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
	int32_t x = x0;
	int32_t y = y0;
	int32_t delta_x = (x1 >= x0) ? (int32_t)(x1 - x0) : (int32_t)(x0 - x1);
	int32_t delta_y = -((y1 >= y0) ? (int32_t)(y1 - y0) : (int32_t)(y0 - y1));
	int32_t step_x = (x0 < x1) ? 1 : -1;
	int32_t step_y = (y0 < y1) ? 1 : -1;
	int32_t error = delta_x + delta_y;

	for (;;) {
		put_pixel((uint16_t)x, (uint16_t)y, color);
		if ((x == x1) && (y == y1)) {
			break;
		}

		int32_t doubled_error = 2 * error;

		if (doubled_error >= delta_y) {
			error += delta_y;
			x += step_x;
		}
		if (doubled_error <= delta_x) {
			error += delta_x;
			y += step_y;
		}
	}
}

static uint8_t sample_to_y(int16_t sample)
{
	const int32_t half_height = (WAVEFORM_BOTTOM - WAVEFORM_TOP) / 2U;
	int32_t y = (int32_t)WAVEFORM_CENTER -
		    ((int32_t)sample * half_height) / WAVEFORM_PCM_FULL_SCALE;

	if (y < WAVEFORM_TOP) {
		y = WAVEFORM_TOP;
	} else if (y > WAVEFORM_BOTTOM) {
		y = WAVEFORM_BOTTOM;
	}

	return (uint8_t)y;
}

static void update_waveform(struct display_snapshot *snapshot, const int16_t *samples,
			    size_t sample_count)
{
	for (size_t x = 0U; x < DISPLAY_WIDTH; ++x) {
		size_t start = (x * sample_count) / DISPLAY_WIDTH;
		size_t end = ((x + 1U) * sample_count) / DISPLAY_WIDTH;
		int16_t minimum;
		int16_t maximum;

		if (end <= start) {
			end = start + 1U;
		}
		if (end > sample_count) {
			end = sample_count;
		}

		minimum = samples[start];
		maximum = samples[start];
		for (size_t i = start + 1U; i < end; ++i) {
			minimum = MIN(minimum, samples[i]);
			maximum = MAX(maximum, samples[i]);
		}

		snapshot->waveform_top[x] = sample_to_y(maximum);
		snapshot->waveform_bottom[x] = sample_to_y(minimum);
	}
}

static void update_spectrum(struct display_snapshot *snapshot, const float *power_spectrum,
			    size_t spectrum_bin_count, float window_sum)
{
	const float power_scale = 4.0f / (window_sum * window_sum);

	for (size_t bar = 0U; bar < SPECTRUM_BAR_COUNT; ++bar) {
		uint16_t start = spectrum_bin_edges[bar];
		uint16_t end = spectrum_bin_edges[bar + 1U];
		float peak_power = 0.0f;
		float level_dbfs;
		float normalized_height;
		uint32_t height;

		if ((start >= spectrum_bin_count) || (end > spectrum_bin_count)) {
			snapshot->spectrum_height[bar] = 0U;
			continue;
		}

		for (uint16_t bin = start; bin < end; ++bin) {
			peak_power = MAX(peak_power, power_spectrum[bin]);
		}

		peak_power *= power_scale;
		if (peak_power <= 1.0e-12f) {
			level_dbfs = SPECTRUM_FLOOR_DBFS;
		} else {
			level_dbfs = 10.0f * log10f(peak_power);
		}

		level_dbfs = CLAMP(level_dbfs, SPECTRUM_FLOOR_DBFS, SPECTRUM_CEILING_DBFS);
		normalized_height = (level_dbfs - SPECTRUM_FLOOR_DBFS) /
				    (SPECTRUM_CEILING_DBFS - SPECTRUM_FLOOR_DBFS);
		height = (uint32_t)(normalized_height * SPECTRUM_HEIGHT + 0.5f);
		snapshot->spectrum_height[bar] = (uint8_t)MIN(height, (uint32_t)SPECTRUM_HEIGHT);
	}
}

static uint16_t spectrum_color(uint16_t y)
{
	uint16_t height = SPECTRUM_BOTTOM - y + 1U;

	if (height > (SPECTRUM_HEIGHT * 3U) / 4U) {
		return RGB565_RED;
	}
	if (height > SPECTRUM_HEIGHT / 2U) {
		return RGB565_YELLOW;
	}

	return RGB565_GREEN;
}

static bool is_vertical_grid_line(uint16_t x)
{
	for (uint16_t division = 1U; division < 4U; ++division) {
		if (x == (division * DISPLAY_WIDTH) / 4U) {
			return true;
		}
	}

	return false;
}

static uint16_t background_color(uint16_t x, uint16_t y)
{
	if (y == DISPLAY_DIVIDER_Y) {
		return RGB565_DIVIDER;
	}

	if (y == WAVEFORM_CENTER) {
		return RGB565_GRID;
	}

	if (is_vertical_grid_line(x) && ((y & 1U) == 0U) &&
	    (((y >= WAVEFORM_TOP) && (y <= WAVEFORM_BOTTOM)) ||
	     ((y >= SPECTRUM_TOP) && (y <= SPECTRUM_BOTTOM)))) {
		return RGB565_GRID;
	}

	if ((y >= SPECTRUM_TOP) && (y <= SPECTRUM_BOTTOM) && ((x & 1U) == 0U)) {
		for (uint16_t division = 1U; division < 4U; ++division) {
			uint16_t grid_y = SPECTRUM_BOTTOM - (division * SPECTRUM_HEIGHT) / 4U;

			if (y == grid_y) {
				return RGB565_GRID;
			}
		}
	}

	return 0U;
}

static void restore_area(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
	for (uint16_t row = y; row < (y + height); ++row) {
		for (uint16_t column = x; column < (x + width); ++column) {
			put_pixel(column, row, background_color(column, row));
		}
	}
}

static void render_grid(void)
{
	restore_area(0U, 0U, DISPLAY_WIDTH, DISPLAY_HEIGHT);
}

static void fast_append_command(uint8_t command)
{
	fast_tx_words[fast_tx_word_count++] = command;
}

static void fast_append_data(uint8_t data)
{
	fast_tx_words[fast_tx_word_count++] = MIPI_DBI_DATA_FLAG | data;
}

static int fast_append_rectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
	size_t required_words;
	uint16_t x_start;
	uint16_t x_end;
	uint16_t y_start;
	uint16_t y_end;

	if ((width == 0U) || (height == 0U) || ((x + width) > DISPLAY_WIDTH) ||
	    ((y + height) > DISPLAY_HEIGHT)) {
		return -EINVAL;
	}

	required_words =
		FAST_RECTANGLE_OVERHEAD_WORDS + ((size_t)width * height * FAST_PIXEL_WORDS);
	if ((fast_tx_word_count + required_words) > ARRAY_SIZE(fast_tx_words)) {
		return -ENOMEM;
	}

	x_start = x + DISPLAY_X_OFFSET;
	x_end = x_start + width - 1U;
	y_start = y + DISPLAY_Y_OFFSET;
	y_end = y_start + height - 1U;

	fast_append_command(ST7735_CMD_CASET);
	fast_append_data((uint8_t)(x_start >> 8));
	fast_append_data((uint8_t)x_start);
	fast_append_data((uint8_t)(x_end >> 8));
	fast_append_data((uint8_t)x_end);

	fast_append_command(ST7735_CMD_RASET);
	fast_append_data((uint8_t)(y_start >> 8));
	fast_append_data((uint8_t)y_start);
	fast_append_data((uint8_t)(y_end >> 8));
	fast_append_data((uint8_t)y_end);

	fast_append_command(ST7735_CMD_RAMWR);
	for (uint16_t row = y; row < (y + height); ++row) {
		for (uint16_t column = x; column < (x + width); ++column) {
			const uint8_t *pixel = (const uint8_t *)&framebuffer[row][column];

			fast_append_data(pixel[0]);
			fast_append_data(pixel[1]);
		}
	}

	return 0;
}

static ALWAYS_INLINE void fast_gpio_half_period(void)
{
	/*
	 * Ten cycles at 120 MHz are 83.3 ns. GPIO stores and loop overhead
	 * make the resulting clock no faster than the panel's 6 MHz limit.
	 */
	arch_nop();
	arch_nop();
	arch_nop();
	arch_nop();
	arch_nop();
	arch_nop();
	arch_nop();
	arch_nop();
	arch_nop();
	arch_nop();
}

static int fast_flush(void)
{
	/*
	 * After panel initialization, the display thread exclusively owns these
	 * pins. Bit-band writes update one GPIO2 output atomically, so the I2S pins
	 * on the same port are unaffected. Keep interrupts and preemption enabled:
	 * an interruption only stretches the display clock.
	 */
	volatile uint32_t *const clock =
		(volatile uint32_t *)BITBAND(&MXC_GPIO2->out, DISPLAY_CLK_PIN);
	volatile uint32_t *const mosi =
		(volatile uint32_t *)BITBAND(&MXC_GPIO2->out, DISPLAY_MOSI_PIN);
	volatile uint32_t *const chip_select =
		(volatile uint32_t *)BITBAND(&MXC_GPIO2->out, DISPLAY_CS_PIN);

	if (fast_tx_word_count == 0U) {
		return 0;
	}

	*clock = 0U;
	*chip_select = 0U;
	fast_gpio_half_period();

	for (size_t word = 0U; word < fast_tx_word_count; ++word) {
		for (uint16_t mask = MIPI_DBI_DATA_FLAG; mask != 0U; mask >>= 1U) {
			*mosi = (fast_tx_words[word] & mask) != 0U;
			fast_gpio_half_period();
			*clock = 1U;
			fast_gpio_half_period();
			*clock = 0U;
		}
	}

	fast_gpio_half_period();
	*chip_select = 1U;
	__DSB();

	return 0;
}

static int append_waveform_frame(const struct display_snapshot *snapshot)
{
	uint16_t previous_y = WAVEFORM_CENTER;

	restore_area(0U, WAVEFORM_TOP, DISPLAY_WIDTH, WAVEFORM_HEIGHT);
	for (uint16_t x = 0U; x < DISPLAY_WIDTH; ++x) {
		uint16_t top = snapshot->waveform_top[x];
		uint16_t bottom = snapshot->waveform_bottom[x];
		uint16_t middle = (top + bottom) / 2U;

		draw_vertical_line(x, top, bottom, RGB565_CYAN);
		if (x > 0U) {
			draw_line(x - 1U, previous_y, x, middle, RGB565_CYAN);
		}
		previous_y = middle;
	}

	return fast_append_rectangle(0U, WAVEFORM_TOP, DISPLAY_WIDTH, WAVEFORM_HEIGHT);
}

static int append_spectrum_changes(const struct display_snapshot *snapshot,
				   struct display_render_state *state)
{
	for (uint16_t bar = 0U; bar < SPECTRUM_BAR_COUNT; ++bar) {
		uint8_t old_height = state->spectrum_height[bar];
		uint8_t target_height = snapshot->spectrum_height[bar];
		uint8_t new_height;
		uint16_t x_start = (bar * DISPLAY_WIDTH) / SPECTRUM_BAR_COUNT;
		uint16_t x_end = ((bar + 1U) * DISPLAY_WIDTH) / SPECTRUM_BAR_COUNT;
		uint16_t x = (x_start + x_end) / 2U;
		uint16_t y_start;
		uint16_t y_end;
		int ret;

		if (target_height > old_height) {
			new_height = MIN(target_height, (uint8_t)(old_height + SPECTRUM_RISE_STEP));
		} else if ((old_height - target_height) > SPECTRUM_FALL_STEP) {
			new_height = old_height - SPECTRUM_FALL_STEP;
		} else {
			new_height = target_height;
		}

		if (new_height == old_height) {
			continue;
		}

		if (new_height > old_height) {
			y_start = SPECTRUM_BOTTOM - new_height + 1U;
			y_end = SPECTRUM_BOTTOM - old_height;
			for (uint16_t y = y_start; y <= y_end; ++y) {
				put_pixel(x, y, spectrum_color(y));
			}
		} else {
			y_start = SPECTRUM_BOTTOM - old_height + 1U;
			y_end = SPECTRUM_BOTTOM - new_height;
			for (uint16_t y = y_start; y <= y_end; ++y) {
				put_pixel(x, y, background_color(x, y));
			}
		}

		ret = fast_append_rectangle(x, y_start, 1U, y_end - y_start + 1U);
		if (ret < 0) {
			return ret;
		}
		state->spectrum_height[bar] = new_height;
	}

	return 0;
}

static int prepare_partial_frame(const struct display_snapshot *snapshot, size_t *word_count)
{
	int ret;

	fast_tx_word_count = 0U;
	prepared_state = displayed_state;
	fast_append_command(ST7735_CMD_COLMOD);
	fast_append_data(ST7735_COLMOD);

	ret = append_spectrum_changes(snapshot, &prepared_state);
	if (ret < 0) {
		return ret;
	}

	ret = append_waveform_frame(snapshot);
	if (ret < 0) {
		return ret;
	}

	*word_count = fast_tx_word_count;
	return 0;
}

static int write_initial_frame(void)
{
	const struct display_buffer_descriptor descriptor = {
		.buf_size = sizeof(framebuffer),
		.width = DISPLAY_WIDTH,
		.height = DISPLAY_HEIGHT,
		.pitch = DISPLAY_WIDTH,
		.frame_incomplete = false,
	};

	memset(framebuffer, 0, sizeof(framebuffer));
	render_grid();

	return display_write(display, 0U, 0U, &descriptor, framebuffer);
}

static void display_thread(void *unused1, void *unused2, void *unused3)
{
	struct display_snapshot snapshot;
	uint32_t displayed_sequence = 0U;
	uint32_t frame_count = 0U;
	uint32_t write_us_sum = 0U;
	uint32_t write_us_max = 0U;
	uint32_t word_count_sum = 0U;
	size_t word_count_max = 0U;
	uint32_t pending_sequence = 0U;
	size_t pending_word_count = 0U;
	bool frame_pending = false;
	int64_t stats_start_ms;
	int64_t next_refresh_ms;

	ARG_UNUSED(unused1);
	ARG_UNUSED(unused2);
	ARG_UNUSED(unused3);

	k_sem_take(&display_start, K_FOREVER);
	stats_start_ms = k_uptime_get();
	next_refresh_ms = stats_start_ms;

	for (;;) {
		int64_t now_ms;

		if (!frame_pending) {
			k_spinlock_key_t key;
			bool new_snapshot = false;

			key = k_spin_lock(&snapshot_lock);
			if (latest_snapshot.sequence != displayed_sequence) {
				snapshot = latest_snapshot;
				pending_sequence = latest_snapshot.sequence;
				new_snapshot = true;
			}
			k_spin_unlock(&snapshot_lock, key);

			if (new_snapshot) {
				int ret = prepare_partial_frame(&snapshot, &pending_word_count);

				if (ret < 0) {
					printk("Display update preparation failed: %d\n", ret);
				} else {
					frame_pending = true;
				}
			}
		}

		if (frame_pending) {
			uint32_t start_cycles = k_cycle_get_32();
			uint32_t write_us;
			int ret = fast_flush();

			write_us = k_cyc_to_us_floor32(k_cycle_get_32() - start_cycles);
			if (ret < 0) {
				printk("Display write failed, retrying frame: %d\n", ret);
			} else {
				displayed_state = prepared_state;
				displayed_sequence = pending_sequence;
				write_us_sum += write_us;
				write_us_max = MAX(write_us_max, write_us);
				word_count_sum += (uint32_t)pending_word_count;
				word_count_max = MAX(word_count_max, pending_word_count);
				++frame_count;
				frame_pending = false;
			}
		}

		now_ms = k_uptime_get();
		if ((now_ms - stats_start_ms) >= DISPLAY_STATS_PERIOD_MS) {
			uint32_t elapsed_ms = (uint32_t)(now_ms - stats_start_ms);
			uint32_t fps_tenths = (frame_count * 10000U) / elapsed_ms;
			uint32_t write_us_average = frame_count ? (write_us_sum / frame_count) : 0U;
			uint32_t word_count_average =
				frame_count ? (word_count_sum / frame_count) : 0U;
			uint32_t effective_wire_kbps =
				write_us_average
					? (word_count_average * 9U * 1000U) / write_us_average
					: 0U;

			printk("Display: %u.%u fps, write avg/max: %u/%u us, "
			       "tx avg/max: %u/%u words, wire: %u kbps\n",
			       fps_tenths / 10U, fps_tenths % 10U, write_us_average, write_us_max,
			       word_count_average, (uint32_t)word_count_max, effective_wire_kbps);
			stats_start_ms = now_ms;
			frame_count = 0U;
			write_us_sum = 0U;
			write_us_max = 0U;
			word_count_sum = 0U;
			word_count_max = 0U;
		}

		next_refresh_ms += DISPLAY_REFRESH_PERIOD_MS;
		now_ms = k_uptime_get();
		if (next_refresh_ms > now_ms) {
			k_sleep(K_TIMEOUT_ABS_MS(next_refresh_ms));
		} else {
			next_refresh_ms = now_ms;
		}
	}
}

K_THREAD_DEFINE(display_thread_id, DISPLAY_THREAD_STACK_SIZE, display_thread, NULL, NULL, NULL,
		DISPLAY_THREAD_PRIORITY, 0, 0);

static int init_spectrum_bins(uint32_t sample_rate_hz, size_t fft_size)
{
	size_t bin_count;
	float nyquist_hz;
	float maximum_hz;
	uint16_t maximum_edge;

	if ((sample_rate_hz == 0U) || (fft_size < 2U) || ((fft_size / 2U) > UINT16_MAX)) {
		return -EINVAL;
	}

	bin_count = fft_size / 2U;
	nyquist_hz = (float)sample_rate_hz / 2.0f;
	maximum_hz = MIN(nyquist_hz, SPECTRUM_MAX_FREQUENCY_HZ);
	maximum_edge = (uint16_t)MIN(
		(size_t)(maximum_hz * (float)fft_size / (float)sample_rate_hz) + 1U, bin_count);

	if (maximum_edge <= SPECTRUM_BAR_COUNT) {
		return -EINVAL;
	}

	spectrum_bin_edges[0] = 1U;
	for (size_t edge = 1U; edge < SPECTRUM_BAR_COUNT; ++edge) {
		float fraction = (float)edge / (float)SPECTRUM_BAR_COUNT;
		uint16_t bin = (uint16_t)(powf((float)maximum_edge, fraction) + 0.5f);
		uint16_t largest_allowed = maximum_edge - (SPECTRUM_BAR_COUNT - edge);

		bin = MAX(bin, (uint16_t)(spectrum_bin_edges[edge - 1U] + 1U));
		spectrum_bin_edges[edge] = MIN(bin, largest_allowed);
	}
	spectrum_bin_edges[SPECTRUM_BAR_COUNT] = maximum_edge;

	return 0;
}

int audio_display_init(uint32_t sample_rate_hz, size_t fft_size)
{
	struct display_capabilities capabilities;
	uint32_t start_cycles;
	uint32_t initial_write_us;
	int ret;

	if (!device_is_ready(display)) {
		printk("%s is not ready\n", display->name);
		return -ENODEV;
	}
	if (!device_is_ready(display_spi)) {
		printk("%s is not ready\n", display_spi->name);
		return -ENODEV;
	}

	display_get_capabilities(display, &capabilities);
	if ((capabilities.x_resolution != DISPLAY_WIDTH) ||
	    (capabilities.y_resolution != DISPLAY_HEIGHT)) {
		printk("Unexpected display resolution: %ux%u\n",
		       (uint32_t)capabilities.x_resolution, (uint32_t)capabilities.y_resolution);
		return -EINVAL;
	}

	if ((capabilities.current_pixel_format != PIXEL_FORMAT_RGB_565) &&
	    (capabilities.current_pixel_format != PIXEL_FORMAT_RGB_565X)) {
		printk("Unsupported display pixel format: %u\n",
		       (uint32_t)capabilities.current_pixel_format);
		return -ENOTSUP;
	}
	pixel_format = capabilities.current_pixel_format;

	ret = init_spectrum_bins(sample_rate_hz, fft_size);
	if (ret < 0) {
		printk("Failed to configure spectrum display bins: %d\n", ret);
		return ret;
	}

	for (size_t point = 0U; point < DISPLAY_WIDTH; ++point) {
		latest_snapshot.waveform_top[point] = WAVEFORM_CENTER;
		latest_snapshot.waveform_bottom[point] = WAVEFORM_CENTER;
	}

	ret = display_blanking_off(display);
	if (ret < 0) {
		printk("Failed to enable display: %d\n", ret);
		return ret;
	}

	start_cycles = k_cycle_get_32();
	ret = write_initial_frame();
	initial_write_us = k_cyc_to_us_floor32(k_cycle_get_32() - start_cycles);
	if (ret < 0) {
		printk("Failed to draw initial display frame: %d\n", ret);
		return ret;
	}

	atomic_set(&display_initialized, 1);
	k_sem_give(&display_start);
	printk("Display: %ux%u, full %u-column waveform envelope + "
	       "%u logarithmic spectrum bars, "
	       "initial write: %u us\n",
	       (uint32_t)capabilities.x_resolution, (uint32_t)capabilities.y_resolution,
	       DISPLAY_WIDTH, SPECTRUM_BAR_COUNT, initial_write_us);

	return 0;
}

void audio_display_update(const int16_t *samples, size_t sample_count, const float *power_spectrum,
			  size_t spectrum_bin_count, float window_sum)
{
	struct display_snapshot snapshot;
	k_spinlock_key_t key;
	uint32_t now_ms;

	if (!atomic_get(&display_initialized) || (samples == NULL) ||
	    (sample_count < DISPLAY_WIDTH) || (power_spectrum == NULL) || (window_sum <= 0.0f)) {
		return;
	}

	now_ms = k_uptime_get_32();
	if ((uint32_t)(now_ms - last_snapshot_ms) < SNAPSHOT_MIN_INTERVAL_MS) {
		return;
	}
	last_snapshot_ms = now_ms;

	update_waveform(&snapshot, samples, sample_count);
	update_spectrum(&snapshot, power_spectrum, spectrum_bin_count, window_sum);

	key = k_spin_lock(&snapshot_lock);
	snapshot.sequence = latest_snapshot.sequence + 1U;
	latest_snapshot = snapshot;
	k_spin_unlock(&snapshot_lock, key);
}
