#include <errno.h>
#include <math.h>
#include <string.h>

#include <zephyr/ztest.h>

#include <lc3.h>

#include "codec_handler.h"
#include "audio_defs.h"

#define TEST_FREQ_HZ	 AUDIO_SAMPLE_RATE_HZ
#define TEST_FRAME_US	 AUDIO_FRAME_DURATION_US
#define TEST_FRAME_BYTES 80
#define PCM_SENTINEL	 0x5555

static lc3_encoder_t encoder;
static lc3_encoder_mem_48k_t encoder_mem;

static void encode_test_frame(uint8_t *out, size_t out_len)
{
	int16_t pcm[AUDIO_MAX_SAMPLES_PER_FRAME];
	int ret;

	/* Content doesn't matter for these tests - just needs to be a real,
	 * non-silent tone so the encoder produces a genuine bitstream.
	 */
	for (size_t i = 0; i < ARRAY_SIZE(pcm); i++) {
		pcm[i] = (int16_t)(10000.0 * sin(2.0 * 3.14159 * 100.0 * i / TEST_FREQ_HZ));
	}

	ret = lc3_encode(encoder, LC3_PCM_FORMAT_S16, pcm, 1, (int)out_len, out);
	zassert_ok(ret, "failed to encode test frame");
}

static void *codec_handler_test_setup(void)
{
	encoder = lc3_setup_encoder(TEST_FRAME_US, TEST_FREQ_HZ, 0, &encoder_mem);
	zassert_not_null(encoder, "failed to set up test LC3 encoder");

	return NULL;
}

/* codec_handler keeps module-level state (the configured decoder), so
 * reset it before every test - tests must not depend on execution order
 * (ztest doesn't guarantee one, and CONFIG_ZTEST_SHUFFLE actively
 * randomizes it).
 */
static void codec_handler_test_before(void *fixture)
{
	ARG_UNUSED(fixture);

	codec_handler_reset();
}

ZTEST(codec_handler, test_decode_before_configure_fails)
{
	int16_t pcm[AUDIO_MAX_SAMPLES_PER_FRAME];
	uint8_t frame[TEST_FRAME_BYTES] = {0};

	zassert_equal(codec_handler_decode(frame, sizeof(frame), pcm), -ENODEV,
		      "decode before configure should fail with -ENODEV");
}

ZTEST(codec_handler, test_configure_valid_params_succeeds)
{
	zassert_ok(codec_handler_configure(TEST_FREQ_HZ, TEST_FRAME_US),
		   "configure with valid params should succeed");
}

ZTEST(codec_handler, test_configure_is_idempotent)
{
	zassert_ok(codec_handler_configure(TEST_FREQ_HZ, TEST_FRAME_US));
	zassert_ok(codec_handler_configure(TEST_FREQ_HZ, TEST_FRAME_US));
}

ZTEST(codec_handler, test_decode_valid_frame_produces_pcm)
{
	uint8_t frame[TEST_FRAME_BYTES];
	int16_t pcm[AUDIO_MAX_SAMPLES_PER_FRAME];
	int num_samples;
	bool changed = false;

	zassert_ok(codec_handler_configure(TEST_FREQ_HZ, TEST_FRAME_US));

	encode_test_frame(frame, sizeof(frame));

	for (size_t i = 0; i < ARRAY_SIZE(pcm); i++) {
		pcm[i] = PCM_SENTINEL;
	}

	num_samples = codec_handler_decode(frame, sizeof(frame), pcm);

	zassert_equal(num_samples, AUDIO_MAX_SAMPLES_PER_FRAME,
		      "decode should produce a full frame of samples");

	for (size_t i = 0; i < ARRAY_SIZE(pcm); i++) {
		if (pcm[i] != PCM_SENTINEL) {
			changed = true;
			break;
		}
	}
	zassert_true(changed, "decoded PCM buffer was never written");
}

struct lc3_param_case {
	int freq_hz;
	int frame_duration_us;
};

/* The full grid LC3 supports (lc3.h: "sr_hz: 8000, 16000, 24000, 32000 or
 * 48000", "dt_us: 7500 or 10000") - our codec cap declares
 * BT_AUDIO_CODEC_CAP_FREQ_ANY, so a real peer can negotiate any of these,
 * not just the 48 kHz/10 ms case the other tests above happen to use.
 */
static const struct lc3_param_case lc3_param_cases[] = {
	{8000, 7500},	{8000, 10000}, {16000, 7500},  {16000, 10000}, {24000, 7500},
	{24000, 10000}, {32000, 7500}, {32000, 10000}, {48000, 7500},  {48000, 10000},
};

#define PARAM_TEST_BITRATE 32000

ZTEST(codec_handler, test_decode_sample_count_matches_freq_and_duration)
{
	/* LC3's encoder/decoder memory and PCM buffers are several KB - kept
	 * static like everywhere else in this codebase (see codec_handler.c
	 * itself), not on the stack. A stack-local lc3_encoder_mem_48k_t
	 * here is exactly what overflowed CONFIG_ZTEST_STACK_SIZE under
	 * mps3/an547 the first time this test was written.
	 */
	static lc3_encoder_t param_encoder;
	static lc3_encoder_mem_48k_t param_encoder_mem;
	static int16_t pcm_in[AUDIO_MAX_SAMPLES_PER_FRAME];
	static int16_t pcm_out[AUDIO_MAX_SAMPLES_PER_FRAME];
	static uint8_t frame[TEST_FRAME_BYTES];

	for (size_t i = 0; i < ARRAY_SIZE(lc3_param_cases); i++) {
		const struct lc3_param_case *tc = &lc3_param_cases[i];
		int frame_bytes;
		int expected_samples;
		int num_samples;

		expected_samples = lc3_frame_samples(tc->frame_duration_us, tc->freq_hz);
		zassert_true(expected_samples > 0, "case %zu: bad freq/duration", i);

		frame_bytes = lc3_frame_bytes(tc->frame_duration_us, PARAM_TEST_BITRATE);
		zassert_true(frame_bytes > 0 && frame_bytes <= (int)sizeof(frame),
			     "case %zu: unexpected frame size %d", i, frame_bytes);

		param_encoder = lc3_setup_encoder(tc->frame_duration_us, tc->freq_hz, 0,
						  &param_encoder_mem);
		zassert_not_null(param_encoder, "case %zu: encoder setup failed", i);

		for (int s = 0; s < expected_samples; s++) {
			pcm_in[s] =
				(int16_t)(10000.0 * sin(2.0 * 3.14159 * 100.0 * s / tc->freq_hz));
		}
		zassert_ok(lc3_encode(param_encoder, LC3_PCM_FORMAT_S16, pcm_in, 1, frame_bytes,
				      frame),
			   "case %zu: encode failed", i);

		zassert_ok(codec_handler_configure(tc->freq_hz, tc->frame_duration_us),
			   "case %zu: configure failed", i);

		num_samples = codec_handler_decode(frame, frame_bytes, pcm_out);

		zassert_equal(num_samples, expected_samples,
			      "case %zu (freq=%d dur=%d): decode returned %d "
			      "samples, expected %d",
			      i, tc->freq_hz, tc->frame_duration_us, num_samples, expected_samples);
	}
}

ZTEST(codec_handler, test_decode_packet_loss_produces_concealment)
{
	int16_t pcm[AUDIO_MAX_SAMPLES_PER_FRAME];
	int num_samples;

	zassert_ok(codec_handler_configure(TEST_FREQ_HZ, TEST_FRAME_US));

	/* NULL data requests packet-loss concealment (PLC) - the decoder
	 * should still produce a full frame instead of erroring out.
	 */
	num_samples = codec_handler_decode(NULL, 0, pcm);

	zassert_equal(num_samples, AUDIO_MAX_SAMPLES_PER_FRAME,
		      "PLC concealment should still produce a full frame");
}

ZTEST_SUITE(codec_handler, NULL, codec_handler_test_setup, codec_handler_test_before, NULL, NULL);
