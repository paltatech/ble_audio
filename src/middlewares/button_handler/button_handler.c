#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "button_handler.h"

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static struct gpio_callback button_cb_data;
static button_handler_pressed_cb_t user_cb;

static void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	if (user_cb != NULL) {
		user_cb();
	}
}

int button_handler_init(button_handler_pressed_cb_t cb)
{
	int err;

	if (!gpio_is_ready_dt(&button)) {
		return -ENODEV;
	}

	err = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (err != 0) {
		return err;
	}

	err = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
	if (err != 0) {
		return err;
	}

	user_cb = cb;

	gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));

	return gpio_add_callback(button.port, &button_cb_data);
}
