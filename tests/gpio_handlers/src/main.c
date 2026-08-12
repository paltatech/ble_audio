#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/ztest.h>

#include "button_handler.h"
#include "led_handler.h"

#define LED_PIN	   0
#define BUTTON_PIN 1

static const struct device *gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
static int button_press_count;

static void on_button_pressed(void)
{
	button_press_count++;
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

	button_press_count = 0;
	/* Drive the emulated button pin back to its inactive level between
	 * tests, so a leftover "pressed" state from one test can't cause a
	 * spurious edge (or lack of one) in the next.
	 */
	gpio_emul_input_set(gpio_dev, BUTTON_PIN, 0);
}

ZTEST(gpio_handlers, test_led_set_true_drives_pin_active)
{
	zassert_ok(led_handler_set(true));

	zassert_equal(gpio_emul_output_get(gpio_dev, LED_PIN), 1);
}

ZTEST(gpio_handlers, test_led_set_false_drives_pin_inactive)
{
	zassert_ok(led_handler_set(true));
	zassert_ok(led_handler_set(false));

	zassert_equal(gpio_emul_output_get(gpio_dev, LED_PIN), 0);
}

ZTEST(gpio_handlers, test_button_press_triggers_callback)
{
	gpio_emul_input_set(gpio_dev, BUTTON_PIN, 1);

	zassert_equal(button_press_count, 1);
}

ZTEST(gpio_handlers, test_button_release_does_not_trigger_callback)
{
	gpio_emul_input_set(gpio_dev, BUTTON_PIN, 1);
	zassert_equal(button_press_count, 1);

	gpio_emul_input_set(gpio_dev, BUTTON_PIN, 0);

	zassert_equal(button_press_count, 1, "release (inactive edge) should not re-trigger");
}

ZTEST_SUITE(gpio_handlers, NULL, gpio_handlers_test_setup, gpio_handlers_test_before, NULL, NULL);
