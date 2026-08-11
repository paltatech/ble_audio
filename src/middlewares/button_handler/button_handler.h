#ifndef BUTTON_HANDLER_H_
#define BUTTON_HANDLER_H_

typedef void (*button_handler_pressed_cb_t)(void);

int button_handler_init(button_handler_pressed_cb_t cb);

#endif /* BUTTON_HANDLER_H_ */
