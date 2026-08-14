#ifndef POWER_HANDLER_H_
#define POWER_HANDLER_H_

#include <stdint.h>

/* Reads the board's input power sense line (see
 * boards/nrf5340dk_nrf5340_cpuapp.overlay for the resistor-divider/ADC
 * setup). Purely a sensor readout - deciding what a given voltage means
 * (e.g. "high" vs "low" power mode) is application-level policy, not
 * this middleware's job.
 */

int power_handler_init(void);
int power_handler_read_mv(int32_t *mv);

#endif /* POWER_HANDLER_H_ */
