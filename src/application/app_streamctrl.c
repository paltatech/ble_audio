#include <zephyr/logging/log.h>

#include "app_streamctrl.h"
#include "audio_defs.h"
#include "led_handler.h"
#include "button_handler.h"
#include "audio_handler.h"
#include "codec_handler.h"
#include "ble_audio_handler.h"

LOG_MODULE_REGISTER(app_streamctrl, LOG_LEVEL_INF);

static int16_t pcm_buf[AUDIO_MAX_SAMPLES_PER_FRAME];

static void on_connected(void)
{
	LOG_INF("Headset connected");
	led_handler_set(true);
}

static void on_disconnected(void)
{
	LOG_INF("Headset disconnected");
	led_handler_set(false);
}

static void on_stream_configured(const struct ble_audio_handler_stream_info *info)
{
	int err;

	LOG_INF("Stream configured: %d Hz, %d us frames", info->freq_hz, info->frame_duration_us);

	err = codec_handler_configure(info->freq_hz, info->frame_duration_us);
	if (err != 0) {
		LOG_ERR("Failed to configure codec: %d", err);
	}
}

static void on_stream_recv(const uint8_t *data, size_t len)
{
	int num_samples;
	int err;

	num_samples = codec_handler_decode(data, len, pcm_buf);
	if (num_samples < 0) {
		LOG_WRN("Decode failed: %d", num_samples);
		return;
	}

	err = audio_handler_write(pcm_buf, (size_t)num_samples);
	if (err != 0) {
		LOG_WRN("Audio write failed: %d", err);
	}
}

static void on_stream_stopped(void)
{
	LOG_INF("Stream stopped");
	codec_handler_reset();
}

static void on_button_pressed(void)
{
	LOG_INF("Button pressed");
}

static const struct ble_audio_handler_cb ble_audio_cb = {
	.connected = on_connected,
	.disconnected = on_disconnected,
	.stream_configured = on_stream_configured,
	.stream_recv = on_stream_recv,
	.stream_stopped = on_stream_stopped,
};

int app_streamctrl_start(void)
{
	int err;

	err = led_handler_init();
	if (err != 0) {
		LOG_ERR("Failed to init LED handler: %d", err);
		return err;
	}

	err = button_handler_init(on_button_pressed);
	if (err != 0) {
		LOG_ERR("Failed to init button handler: %d", err);
		return err;
	}

	err = audio_handler_init();
	if (err != 0) {
		LOG_ERR("Failed to init audio handler: %d", err);
		return err;
	}

	err = ble_audio_handler_start(&ble_audio_cb);
	if (err != 0) {
		LOG_ERR("Failed to start BLE audio handler: %d", err);
		return err;
	}

	return 0;
}
