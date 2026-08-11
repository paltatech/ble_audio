#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "led_handler.h"

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

int led_handler_init(void)
{
	if (!gpio_is_ready_dt(&led)) {
		return -ENODEV;
	}

	return gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
}

int led_handler_set(bool on)
{
	return gpio_pin_set_dt(&led, on);
}
