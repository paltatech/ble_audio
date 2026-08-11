#include <errno.h>

#include <lc3.h>
#include <zephyr/logging/log.h>

#include "codec_handler.h"
#include "audio_defs.h"

LOG_MODULE_REGISTER(codec_handler, LOG_LEVEL_INF);

static lc3_decoder_t decoder;
static lc3_decoder_mem_48k_t decoder_mem;
static int configured_freq_hz;
static int configured_frame_us;

int codec_handler_configure(int freq_hz, int frame_duration_us)
{
	if (decoder != NULL && freq_hz == configured_freq_hz &&
	    frame_duration_us == configured_frame_us) {
		return 0;
	}

	decoder = lc3_setup_decoder(frame_duration_us, freq_hz, 0, &decoder_mem);
	if (decoder == NULL) {
		LOG_ERR("Failed to set up LC3 decoder (freq=%d dur=%d)", freq_hz,
			frame_duration_us);
		return -EINVAL;
	}

	configured_freq_hz = freq_hz;
	configured_frame_us = frame_duration_us;

	return 0;
}

int codec_handler_decode(const uint8_t *data, size_t len, int16_t *pcm_out)
{
	int err;

	if (decoder == NULL) {
		return -ENODEV;
	}

	err = lc3_decode(decoder, data, (int)len, LC3_PCM_FORMAT_S16, pcm_out, 1);
	if (err < 0) {
		return err;
	}

	return AUDIO_MAX_SAMPLES_PER_FRAME;
}
