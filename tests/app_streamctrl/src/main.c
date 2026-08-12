#include <errno.h>

#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#include "app_streamctrl.h"
#include "audio_handler.h"
#include "ble_audio_handler.h"
#include "button_handler.h"
#include "codec_handler.h"
#include "led_handler.h"

DEFINE_FFF_GLOBALS;

FAKE_VALUE_FUNC(int, led_handler_init);
FAKE_VALUE_FUNC(int, led_handler_set, bool);

FAKE_VALUE_FUNC(int, button_handler_init, button_handler_pressed_cb_t);

FAKE_VALUE_FUNC(int, audio_handler_init);
FAKE_VALUE_FUNC(int, audio_handler_write, const int16_t *, size_t);

FAKE_VALUE_FUNC(int, codec_handler_configure, int, int);
FAKE_VALUE_FUNC(int, codec_handler_decode, const uint8_t *, size_t, int16_t *);
FAKE_VOID_FUNC(codec_handler_reset);

FAKE_VALUE_FUNC(int, ble_audio_handler_start, const struct ble_audio_handler_cb *);

/* app_streamctrl registers its callbacks with ble_audio_handler_start()
 * once, at app_streamctrl_start(). The real implementation would invoke
 * them later from BLE stack events; here the test invokes them itself
 * to drive app_streamctrl's logic directly, with everything below it
 * faked out.
 */
static const struct ble_audio_handler_cb *captured_cb;

static int fake_ble_audio_handler_start(const struct ble_audio_handler_cb *cb)
{
	captured_cb = cb;
	return 0;
}

static void app_streamctrl_test_before(void *fixture)
{
	ARG_UNUSED(fixture);

	RESET_FAKE(led_handler_init);
	RESET_FAKE(led_handler_set);
	RESET_FAKE(button_handler_init);
	RESET_FAKE(audio_handler_init);
	RESET_FAKE(audio_handler_write);
	RESET_FAKE(codec_handler_configure);
	RESET_FAKE(codec_handler_decode);
	RESET_FAKE(codec_handler_reset);
	RESET_FAKE(ble_audio_handler_start);
	FFF_RESET_HISTORY();

	captured_cb = NULL;
	ble_audio_handler_start_fake.custom_fake = fake_ble_audio_handler_start;
}

ZTEST(app_streamctrl, test_start_initializes_all_middlewares)
{
	zassert_ok(app_streamctrl_start());

	zassert_equal(led_handler_init_fake.call_count, 1);
	zassert_equal(button_handler_init_fake.call_count, 1);
	zassert_equal(audio_handler_init_fake.call_count, 1);
	zassert_equal(ble_audio_handler_start_fake.call_count, 1);
	zassert_not_null(captured_cb, "app_streamctrl should register BLE audio callbacks");
}

ZTEST(app_streamctrl, test_connected_turns_led_on)
{
	zassert_ok(app_streamctrl_start());

	captured_cb->connected();

	zassert_equal(led_handler_set_fake.call_count, 1);
	zassert_true(led_handler_set_fake.arg0_val, "LED should turn on when connected");
}

ZTEST(app_streamctrl, test_disconnected_turns_led_off)
{
	zassert_ok(app_streamctrl_start());

	captured_cb->disconnected();

	zassert_equal(led_handler_set_fake.call_count, 1);
	zassert_false(led_handler_set_fake.arg0_val, "LED should turn off when disconnected");
}

ZTEST(app_streamctrl, test_stream_configured_configures_codec)
{
	struct ble_audio_handler_stream_info info = {
		.freq_hz = 48000,
		.frame_duration_us = 10000,
	};

	zassert_ok(app_streamctrl_start());

	captured_cb->stream_configured(&info);

	zassert_equal(codec_handler_configure_fake.call_count, 1);
	zassert_equal(codec_handler_configure_fake.arg0_val, 48000);
	zassert_equal(codec_handler_configure_fake.arg1_val, 10000);
}

ZTEST(app_streamctrl, test_stream_recv_decodes_and_writes_audio)
{
	uint8_t frame[4] = {1, 2, 3, 4};

	codec_handler_decode_fake.return_val = 480;

	zassert_ok(app_streamctrl_start());

	captured_cb->stream_recv(frame, sizeof(frame));

	zassert_equal(codec_handler_decode_fake.call_count, 1);
	zassert_equal_ptr(codec_handler_decode_fake.arg0_val, frame);
	zassert_equal(codec_handler_decode_fake.arg1_val, sizeof(frame));

	zassert_equal(audio_handler_write_fake.call_count, 1);
	zassert_equal(audio_handler_write_fake.arg1_val, 480);
}

ZTEST(app_streamctrl, test_stream_recv_skips_audio_write_on_decode_error)
{
	codec_handler_decode_fake.return_val = -EINVAL;

	zassert_ok(app_streamctrl_start());

	captured_cb->stream_recv(NULL, 0);

	zassert_equal(audio_handler_write_fake.call_count, 0,
		      "should not write audio when decode fails");
}

ZTEST(app_streamctrl, test_stream_stopped_resets_codec)
{
	zassert_ok(app_streamctrl_start());

	captured_cb->stream_stopped();

	zassert_equal(codec_handler_reset_fake.call_count, 1);
}

ZTEST_SUITE(app_streamctrl, NULL, NULL, app_streamctrl_test_before, NULL, NULL);
