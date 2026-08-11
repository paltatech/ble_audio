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
