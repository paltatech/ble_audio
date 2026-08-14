#include <errno.h>

#include <zephyr/fff.h>
#include <zephyr/ztest.h>
#include <zephyr/ztress.h>

#include "app_streamctrl.h"
/* audio_handler is disabled in app_streamctrl.c on this board (nrf5340dk
 * has no I2S codec chip wired up) - fakes/assertions for it are commented
 * out below to match, not deleted.
 */
/* #include "audio_handler.h" */
#include "ble_audio_handler.h"
#include "button_handler.h"
#include "codec_handler.h"
#include "led_handler.h"
#include "power_handler.h"

DEFINE_FFF_GLOBALS;

FAKE_VALUE_FUNC(int, led_handler_init);
FAKE_VALUE_FUNC(int, led_handler_set, uint8_t, bool);

FAKE_VALUE_FUNC(int, button_handler_init, button_handler_pressed_cb_t);

FAKE_VALUE_FUNC(int, power_handler_init);
FAKE_VALUE_FUNC(int, power_handler_read_mv, int32_t *);

/* FAKE_VALUE_FUNC(int, audio_handler_init); */
/* FAKE_VALUE_FUNC(int, audio_handler_write, const int16_t *, size_t); */

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
static button_handler_pressed_cb_t captured_button_cb;

static int fake_ble_audio_handler_start(const struct ble_audio_handler_cb *cb)
{
	captured_cb = cb;
	return 0;
}

static int fake_button_handler_init(button_handler_pressed_cb_t cb)
{
	captured_button_cb = cb;
	return 0;
}

/* Tests set this directly before calling app_streamctrl_start() to drive
 * a specific simulated input voltage through power_handler_read_mv()'s
 * out-param.
 */
static int32_t fake_power_mv;

static int fake_power_handler_read_mv(int32_t *mv)
{
	*mv = fake_power_mv;
	return 0;
}

static void app_streamctrl_test_before(void *fixture)
{
	ARG_UNUSED(fixture);

	RESET_FAKE(led_handler_init);
	RESET_FAKE(led_handler_set);
	RESET_FAKE(button_handler_init);
	/* RESET_FAKE(audio_handler_init); */
	/* RESET_FAKE(audio_handler_write); */
	RESET_FAKE(power_handler_init);
	RESET_FAKE(power_handler_read_mv);
	RESET_FAKE(codec_handler_configure);
	RESET_FAKE(codec_handler_decode);
	RESET_FAKE(codec_handler_reset);
	RESET_FAKE(ble_audio_handler_start);
	FFF_RESET_HISTORY();

	captured_cb = NULL;
	captured_button_cb = NULL;
	fake_power_mv = 5000;
	ble_audio_handler_start_fake.custom_fake = fake_ble_audio_handler_start;
	button_handler_init_fake.custom_fake = fake_button_handler_init;
	power_handler_read_mv_fake.custom_fake = fake_power_handler_read_mv;
}

ZTEST(app_streamctrl, test_start_initializes_all_middlewares)
{
	zassert_ok(app_streamctrl_start());

	zassert_equal(led_handler_init_fake.call_count, 1);
	zassert_equal(button_handler_init_fake.call_count, 1);
	/* audio_handler_init() is commented out in app_streamctrl.c on this
	 * board - see the include comment above.
	 */
	/* zassert_equal(audio_handler_init_fake.call_count, 1); */
	zassert_equal(power_handler_init_fake.call_count, 1);
	zassert_equal(power_handler_read_mv_fake.call_count, 1);
	zassert_equal(ble_audio_handler_start_fake.call_count, 1);
	zassert_not_null(captured_cb, "app_streamctrl should register BLE audio callbacks");
}

ZTEST(app_streamctrl, test_power_mode_high_above_threshold)
{
	fake_power_mv = 4001;

	zassert_ok(app_streamctrl_start());

	zassert_equal(app_streamctrl_get_power_mode(), APP_STREAMCTRL_POWER_MODE_HIGH);
}

ZTEST(app_streamctrl, test_power_mode_low_below_threshold)
{
	fake_power_mv = 3999;

	zassert_ok(app_streamctrl_start());

	zassert_equal(app_streamctrl_get_power_mode(), APP_STREAMCTRL_POWER_MODE_LOW);
}

/* The threshold check is "> 4000", not ">=" - a value exactly at 4000 mV
 * is the case most likely to catch an off-by-one/wrong-comparison bug
 * that the comfortably-above/below cases above can't.
 */
ZTEST(app_streamctrl, test_power_mode_boundary_exactly_at_threshold_is_low)
{
	fake_power_mv = 4000;

	zassert_ok(app_streamctrl_start());

	zassert_equal(app_streamctrl_get_power_mode(), APP_STREAMCTRL_POWER_MODE_LOW,
		      "exactly at the threshold should not count as high power");
}

ZTEST(app_streamctrl, test_start_fails_when_power_read_fails)
{
	power_handler_read_mv_fake.custom_fake = NULL;
	power_handler_read_mv_fake.return_val = -EIO;

	zassert_equal(app_streamctrl_start(), -EIO);
}

ZTEST(app_streamctrl, test_connected_turns_led_on)
{
	zassert_ok(app_streamctrl_start());

	captured_cb->connected();

	zassert_equal(led_handler_set_fake.call_count, 1);
	zassert_equal(led_handler_set_fake.arg0_val, 0, "should drive the status LED (id 0)");
	zassert_true(led_handler_set_fake.arg1_val, "LED should turn on when connected");
}

ZTEST(app_streamctrl, test_disconnected_turns_led_off)
{
	zassert_ok(app_streamctrl_start());

	captured_cb->disconnected();

	zassert_equal(led_handler_set_fake.call_count, 1);
	zassert_equal(led_handler_set_fake.arg0_val, 0, "should drive the status LED (id 0)");
	zassert_false(led_handler_set_fake.arg1_val, "LED should turn off when disconnected");
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

ZTEST(app_streamctrl, test_stream_recv_decodes_frame)
{
	uint8_t frame[4] = {1, 2, 3, 4};

	codec_handler_decode_fake.return_val = 480;

	zassert_ok(app_streamctrl_start());

	captured_cb->stream_recv(frame, sizeof(frame));

	zassert_equal(codec_handler_decode_fake.call_count, 1);
	zassert_equal_ptr(codec_handler_decode_fake.arg0_val, frame);
	zassert_equal(codec_handler_decode_fake.arg1_val, sizeof(frame));

	/* Used to also assert decoded PCM got forwarded to audio_handler_
	 * write() here - that call is commented out in app_streamctrl.c on
	 * this board (see the include comment above), so there's nothing
	 * left to assert past decode.
	 */
	/* zassert_equal(audio_handler_write_fake.call_count, 1); */
	/* zassert_equal(audio_handler_write_fake.arg1_val, 480); */
}

/* This suite used to also have test_stream_recv_skips_audio_write_on_
 * decode_error, checking audio_handler_write_fake.call_count stayed 0
 * when decode failed. With audio_handler_write() commented out entirely
 * in app_streamctrl.c, that count is 0 unconditionally now - the test
 * would no longer distinguish anything, so it's commented out here too
 * rather than kept as a check that always trivially passes.
 */
/*
 * ZTEST(app_streamctrl, test_stream_recv_skips_audio_write_on_decode_error)
 * {
 *	codec_handler_decode_fake.return_val = -EINVAL;
 *
 *	zassert_ok(app_streamctrl_start());
 *
 *	captured_cb->stream_recv(NULL, 0);
 *
 *	zassert_equal(audio_handler_write_fake.call_count, 0,
 *		      "should not write audio when decode fails");
 * }
 */

ZTEST(app_streamctrl, test_stream_stopped_resets_codec)
{
	zassert_ok(app_streamctrl_start());

	captured_cb->stream_stopped();

	zassert_equal(codec_handler_reset_fake.call_count, 1);
}

/* On real hardware, incoming audio (BT ISO RX) and a button press (GPIO
 * IRQ/workqueue) are genuinely independent contexts that can interleave -
 * unlike two concurrent stream_recv calls, which the BLE stack never
 * produces (there is exactly one RX path). ztress drives both at once to
 * check app_streamctrl doesn't corrupt state when they overlap.
 */
static bool ztress_stream_recv_handler(void *user_data, uint32_t cnt, bool last, int prio)
{
	uint8_t frame[4] = {1, 2, 3, 4};

	ARG_UNUSED(user_data);
	ARG_UNUSED(cnt);
	ARG_UNUSED(last);
	ARG_UNUSED(prio);

	captured_cb->stream_recv(frame, sizeof(frame));

	return true;
}

static bool ztress_button_press_handler(void *user_data, uint32_t cnt, bool last, int prio)
{
	ARG_UNUSED(user_data);
	ARG_UNUSED(cnt);
	ARG_UNUSED(last);
	ARG_UNUSED(prio);

	captured_button_cb(0);

	return true;
}

ZTEST(app_streamctrl, test_stream_recv_survives_concurrent_button_presses)
{
	uint32_t repeat = 200;

	codec_handler_decode_fake.return_val = 480;

	zassert_ok(app_streamctrl_start());
	zassert_not_null(captured_button_cb, "button callback should be registered");

	ZTRESS_EXECUTE(
		ZTRESS_THREAD(ztress_stream_recv_handler, NULL, repeat, 0, Z_TIMEOUT_TICKS(2)),
		ZTRESS_THREAD(ztress_button_press_handler, NULL, repeat, 0, Z_TIMEOUT_TICKS(3)));

	/* Exact counts aren't meaningful under ztress's timing jitter (see
	 * upstream tests/ztest/ztress's own range-based asserts) - what
	 * matters is that both contexts actually ran and nothing faulted.
	 */
	zassert_true(ztress_exec_count(0) > 0, "stream_recv thread never ran");
	zassert_true(ztress_exec_count(1) > 0, "button thread never ran");
	/* audio_handler_write() is commented out in app_streamctrl.c on this
	 * board - codec_handler_decode() is the last active step in the
	 * stream_recv path, so that's the "real work happened" signal now.
	 */
	zassert_true(codec_handler_decode_fake.call_count > 0,
		     "expected stream_recv to have decoded at least one frame");
}

ZTEST_SUITE(app_streamctrl, NULL, NULL, app_streamctrl_test_before, NULL, NULL);
