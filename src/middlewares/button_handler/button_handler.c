#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "button_handler.h"

static const struct gpio_dt_spec buttons[] = {
	GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios),
};

/* Pairs each button's gpio_callback with its own index, so the shared
 * interrupt handler below can recover which button fired via
 * CONTAINER_OF instead of bit-scanning `pins`.
 */
struct button_entry {
	struct gpio_callback cb;
	uint8_t id;
};

static struct button_entry button_entries[ARRAY_SIZE(buttons)];
static button_handler_pressed_cb_t user_cb;

static void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	struct button_entry *entry = CONTAINER_OF(cb, struct button_entry, cb);

	ARG_UNUSED(dev);
	ARG_UNUSED(pins);

	if (user_cb != NULL) {
		user_cb(entry->id);
	}
}

int button_handler_init(button_handler_pressed_cb_t cb)
{
	int err;

	user_cb = cb;

	for (uint8_t i = 0; i < ARRAY_SIZE(buttons); i++) {
		if (!gpio_is_ready_dt(&buttons[i])) {
			return -ENODEV;
		}

		err = gpio_pin_configure_dt(&buttons[i], GPIO_INPUT);
		if (err != 0) {
			return err;
		}

		err = gpio_pin_interrupt_configure_dt(&buttons[i], GPIO_INT_EDGE_TO_ACTIVE);
		if (err != 0) {
			return err;
		}

		button_entries[i].id = i;
		gpio_init_callback(&button_entries[i].cb, button_pressed, BIT(buttons[i].pin));

		err = gpio_add_callback(buttons[i].port, &button_entries[i].cb);
		if (err != 0) {
			return err;
		}
	}

	return 0;
}
