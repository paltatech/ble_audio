#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/adc/voltage_divider.h>

#include "power_handler.h"

static const struct voltage_divider_dt_spec power_sense =
	VOLTAGE_DIVIDER_DT_SPEC_GET(DT_ALIAS(power_sense));

int power_handler_init(void)
{
	if (!adc_is_ready_dt(&power_sense.port)) {
		return -ENODEV;
	}

	return adc_channel_setup_dt(&power_sense.port);
}

int power_handler_read_mv(int32_t *mv)
{
	int16_t raw;
	int32_t val;
	int err;
	struct adc_sequence sequence = {
		.buffer = &raw,
		.buffer_size = sizeof(raw),
	};

	err = adc_sequence_init_dt(&power_sense.port, &sequence);
	if (err != 0) {
		return err;
	}

	err = adc_read_dt(&power_sense.port, &sequence);
	if (err != 0) {
		return err;
	}

	val = raw;
	err = adc_raw_to_millivolts_dt(&power_sense.port, &val);
	if (err != 0) {
		return err;
	}

	err = voltage_divider_scale_dt(&power_sense, &val);
	if (err != 0) {
		return err;
	}

	*mv = val;

	return 0;
}
