/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AUDIO_DISPLAY_H_
#define AUDIO_DISPLAY_H_

#include <stddef.h>
#include <stdint.h>

int audio_display_init(uint32_t sample_rate_hz, size_t fft_size);

void audio_display_update(const int16_t *samples, size_t sample_count, const float *power_spectrum,
			  size_t spectrum_bin_count, float window_sum);

#endif /* AUDIO_DISPLAY_H_ */
