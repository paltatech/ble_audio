#ifndef LED_HANDLER_H_
#define LED_HANDLER_H_

#include <stdbool.h>
#include <stdint.h>

int led_handler_init(void);
int led_handler_set(uint8_t led_id, bool on);

#endif /* LED_HANDLER_H_ */
