#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/logging/log.h>

#include "audio_handler.h"
#include "audio_defs.h"

LOG_MODULE_REGISTER(audio_handler, LOG_LEVEL_INF);

#define I2S_CHANNELS	   2U
#define I2S_WORD_SIZE_BITS 16U
#define I2S_BLOCK_SIZE	   (AUDIO_MAX_SAMPLES_PER_FRAME * I2S_CHANNELS * sizeof(int16_t))
#define I2S_NUM_BLOCKS	   4U

K_MEM_SLAB_DEFINE_STATIC(i2s_tx_mem_slab, I2S_BLOCK_SIZE, I2S_NUM_BLOCKS, 4);

static const struct device *i2s_dev = DEVICE_DT_GET(DT_NODELABEL(i2s0));
static bool i2s_started;

int audio_handler_init(void)
{
	struct i2s_config cfg;

	if (!device_is_ready(i2s_dev)) {
		LOG_ERR("I2S device not ready");
		return -ENODEV;
	}

	cfg.word_size = I2S_WORD_SIZE_BITS;
	cfg.channels = I2S_CHANNELS;
	cfg.format = I2S_FMT_DATA_FORMAT_I2S;
	cfg.options = I2S_OPT_FRAME_CLK_MASTER | I2S_OPT_BIT_CLK_MASTER;
	cfg.frame_clk_freq = AUDIO_SAMPLE_RATE_HZ;
	cfg.mem_slab = &i2s_tx_mem_slab;
	cfg.block_size = I2S_BLOCK_SIZE;
	cfg.timeout = 1000;

	return i2s_configure(i2s_dev, I2S_DIR_TX, &cfg);
}

int audio_handler_write(const int16_t *pcm_mono, size_t num_samples)
{
	void *block;
	int16_t *stereo;
	int ret;

	if (num_samples * I2S_CHANNELS * sizeof(int16_t) > I2S_BLOCK_SIZE) {
		return -EINVAL;
	}

	ret = k_mem_slab_alloc(&i2s_tx_mem_slab, &block, K_MSEC(100));
	if (ret != 0) {
		LOG_WRN("No free I2S TX block");
		return ret;
	}

	stereo = (int16_t *)block;
	for (size_t i = 0; i < num_samples; i++) {
		stereo[2 * i] = pcm_mono[i];
		stereo[2 * i + 1] = pcm_mono[i];
	}

	ret = i2s_write(i2s_dev, block, num_samples * I2S_CHANNELS * sizeof(int16_t));
	if (ret != 0) {
		k_mem_slab_free(&i2s_tx_mem_slab, block);
		return ret;
	}

	if (!i2s_started) {
		ret = i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_START);
		if (ret != 0) {
			LOG_ERR("Failed to start I2S TX: %d", ret);
			return ret;
		}
		i2s_started = true;
	}

	return 0;
}
