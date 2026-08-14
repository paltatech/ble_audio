#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/adc/adc_emul.h>
#include <zephyr/drivers/adc/voltage_divider.h>
#include <zephyr/ztest.h>

#include "power_handler.h"

/* Raw<->mV conversion rounds to the nearest ADC count (~0.8 mV at the pin
 * for this suite's 12-bit/3300 mV setup, ~4x that once the divider scales
 * it back up) - not an exact match, so comparisons below allow this much
 * slack either way.
 */
#define MV_EPS 15

static const struct voltage_divider_dt_spec power_sense =
	VOLTAGE_DIVIDER_DT_SPEC_GET(DT_ALIAS(power_sense));

/* power_handler_read_mv() reports the real-world voltage on the far side
 * of the divider - adc_emul_const_value_set() takes the voltage actually
 * seen at the pin, so callers here divide by the same ratio (full-ohms /
 * output-ohms = 4, see boards/native_sim.overlay) before simulating.
 */
static void set_simulated_input_mv(int32_t real_world_mv)
{
	int32_t pin_mv = real_world_mv * power_sense.output_ohms / power_sense.full_ohms;

	zassert_ok(adc_emul_const_value_set(power_sense.port.dev, power_sense.port.channel_id,
					    pin_mv));
}

static void power_handler_test_before(void *fixture)
{
	ARG_UNUSED(fixture);

	zassert_ok(power_handler_init());
}

ZTEST(power_handler, test_init_succeeds)
{
	zassert_ok(power_handler_init());
}

ZTEST(power_handler, test_read_mv_scales_low_voltage)
{
	int32_t mv;

	set_simulated_input_mv(3300);

	zassert_ok(power_handler_read_mv(&mv));
	zassert_within(mv, 3300, MV_EPS, "expected ~3300 mV, got %d", mv);
}

ZTEST(power_handler, test_read_mv_scales_high_voltage)
{
	int32_t mv;

	set_simulated_input_mv(12000);

	zassert_ok(power_handler_read_mv(&mv));
	zassert_within(mv, 12000, MV_EPS, "expected ~12000 mV, got %d", mv);
}

ZTEST(power_handler, test_read_mv_at_threshold_boundary)
{
	int32_t mv;

	set_simulated_input_mv(4000);

	zassert_ok(power_handler_read_mv(&mv));
	zassert_within(mv, 4000, MV_EPS, "expected ~4000 mV, got %d", mv);
}

/* A single stale reading (e.g. from caching the first ADC sample) would
 * pass every test above individually - only checking that a second,
 * different input produces a correspondingly different reading catches
 * that class of bug.
 */
ZTEST(power_handler, test_read_mv_reflects_updated_input)
{
	int32_t first_mv, second_mv;

	set_simulated_input_mv(3300);
	zassert_ok(power_handler_read_mv(&first_mv));

	set_simulated_input_mv(6600);
	zassert_ok(power_handler_read_mv(&second_mv));

	zassert_true(second_mv > first_mv + MV_EPS,
		     "expected a higher reading after raising the simulated input");
}

ZTEST_SUITE(power_handler, NULL, NULL, power_handler_test_before, NULL, NULL);
