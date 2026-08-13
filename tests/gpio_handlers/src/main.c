#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/ztest.h>

#include "button_handler.h"
#include "led_handler.h"

#define LED0_ID	    0
#define LED1_ID	    1

#define LED0_PIN    0
#define BUTTON0_PIN 1
#define LED1_PIN    2
#define BUTTON1_PIN 3

static const struct device *gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
static int button0_press_count;
static int button1_press_count;

/* Records which button fired, not just that one did - this is what lets
 * test_button0_press_does_not_report_as_button1 below actually catch a
 * button_handler bug that reports the wrong id (a bug two independent
 * "did a callback fire" counters, each only ever driven by their own
 * pin, would never be able to see).
 */
static void on_button_pressed(uint8_t button_id)
{
	if (button_id == 0) {
		button0_press_count++;
	} else if (button_id == 1) {
		button1_press_count++;
	}
}

/* led_handler_init()/button_handler_init() are meant to be called once
 * at startup (that's how app_streamctrl.c uses them) - button_handler_
 * init() in particular registers a GPIO callback on a static struct via
 * gpio_add_callback(), which is not safe to call repeatedly on the same
 * struct without an intervening gpio_remove_callback(). So init happens
 * once here too, in the suite-level setup, not per test.
 */
static void *gpio_handlers_test_setup(void)
{
	zassert_ok(led_handler_init());
	zassert_ok(button_handler_init(on_button_pressed));

	return NULL;
}

static void gpio_handlers_test_before(void *fixture)
{
	ARG_UNUSED(fixture);

	button0_press_count = 0;
	button1_press_count = 0;
	/* Drive both emulated button pins back to their inactive level
	 * between tests, so a leftover "pressed" state from one test can't
	 * cause a spurious edge (or lack of one) in the next.
	 */
	gpio_emul_input_set(gpio_dev, BUTTON0_PIN, 0);
	gpio_emul_input_set(gpio_dev, BUTTON1_PIN, 0);
}

ZTEST(gpio_handlers, test_led0_set_true_drives_pin_active)
{
	zassert_ok(led_handler_set(LED0_ID, true));

	zassert_equal(gpio_emul_output_get(gpio_dev, LED0_PIN), 1);
}

ZTEST(gpio_handlers, test_led0_set_false_drives_pin_inactive)
{
	zassert_ok(led_handler_set(LED0_ID, true));
	zassert_ok(led_handler_set(LED0_ID, false));

	zassert_equal(gpio_emul_output_get(gpio_dev, LED0_PIN), 0);
}

ZTEST(gpio_handlers, test_led1_set_true_drives_pin_active)
{
	zassert_ok(led_handler_set(LED1_ID, true));

	zassert_equal(gpio_emul_output_get(gpio_dev, LED1_PIN), 1);
}

ZTEST(gpio_handlers, test_led1_set_false_drives_pin_inactive)
{
	zassert_ok(led_handler_set(LED1_ID, true));
	zassert_ok(led_handler_set(LED1_ID, false));

	zassert_equal(gpio_emul_output_get(gpio_dev, LED1_PIN), 0);
}

ZTEST(gpio_handlers, test_led_set_out_of_range_id_fails)
{
	zassert_equal(led_handler_set(2, true), -EINVAL);
}

ZTEST(gpio_handlers, test_led0_and_led1_are_independent)
{
	zassert_ok(led_handler_set(LED0_ID, true));
	zassert_ok(led_handler_set(LED1_ID, false));

	zassert_equal(gpio_emul_output_get(gpio_dev, LED0_PIN), 1,
		      "led0 should be unaffected by led1's state");
	zassert_equal(gpio_emul_output_get(gpio_dev, LED1_PIN), 0,
		      "led1 should be unaffected by led0's state");
}

ZTEST(gpio_handlers, test_button0_press_triggers_callback)
{
	gpio_emul_input_set(gpio_dev, BUTTON0_PIN, 1);

	zassert_equal(button0_press_count, 1);
}

ZTEST(gpio_handlers, test_button0_release_does_not_trigger_callback)
{
	gpio_emul_input_set(gpio_dev, BUTTON0_PIN, 1);
	zassert_equal(button0_press_count, 1);

	gpio_emul_input_set(gpio_dev, BUTTON0_PIN, 0);

	zassert_equal(button0_press_count, 1, "release (inactive edge) should not re-trigger");
}

ZTEST(gpio_handlers, test_button1_press_triggers_callback_with_correct_id)
{
	gpio_emul_input_set(gpio_dev, BUTTON1_PIN, 1);

	zassert_equal(button1_press_count, 1);
	zassert_equal(button0_press_count, 0, "button1 firing should not count as button0");
}

/* The case that would actually catch a copy-paste bug in button_handler's
 * CONTAINER_OF/array-indexing logic (see button_handler.c): pressing only
 * button0 and checking button1's counter never moves. Two counters that
 * are each only ever driven by their own pin (like the two tests above)
 * can't tell an id-reporting bug apart from correct behavior - only a
 * cross-check like this one can.
 */
ZTEST(gpio_handlers, test_button0_press_does_not_report_as_button1)
{
	gpio_emul_input_set(gpio_dev, BUTTON0_PIN, 1);

	zassert_equal(button0_press_count, 1);
	zassert_equal(button1_press_count, 0, "button0 firing should not count as button1");
}

ZTEST_SUITE(gpio_handlers, NULL, gpio_handlers_test_setup, gpio_handlers_test_before, NULL, NULL);
