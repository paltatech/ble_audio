#ifndef APP_STREAMCTRL_H_
#define APP_STREAMCTRL_H_

enum app_streamctrl_power_mode {
	APP_STREAMCTRL_POWER_MODE_LOW,
	APP_STREAMCTRL_POWER_MODE_HIGH,
};

/* Initializes all middlewares (LED, button, power sense, audio output,
 * codec, BLE audio) and starts advertising as an LE Audio headset. */
int app_streamctrl_start(void);

/* Power mode detected the last time app_streamctrl_start() read the
 * input power sense ADC (see power_handler.h). */
enum app_streamctrl_power_mode app_streamctrl_get_power_mode(void);

#endif /* APP_STREAMCTRL_H_ */
