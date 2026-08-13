#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "led_handler.h"

static const struct gpio_dt_spec leds[] = {
	GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios),
};

int led_handler_init(void)
{
	int err;

	for (size_t i = 0; i < ARRAY_SIZE(leds); i++) {
		if (!gpio_is_ready_dt(&leds[i])) {
			return -ENODEV;
		}

		err = gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_INACTIVE);
		if (err != 0) {
			return err;
		}
	}

	return 0;
}

int led_handler_set(uint8_t led_id, bool on)
{
	if (led_id >= ARRAY_SIZE(leds)) {
		return -EINVAL;
	}

	return gpio_pin_set_dt(&leds[led_id], on);
}
