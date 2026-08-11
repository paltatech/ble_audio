#ifndef APP_VERSION_H_
#define APP_VERSION_H_

#include <stdint.h>

/**
 * @brief Initialize and print firmware version at startup
 * @return 0 on success
 */
int app_version_set_and_print_fw_version(void);

/**
 * @brief Get major firmware version
 * @return Major version number
 */
uint16_t app_version_get_major(void);

/**
 * @brief Get minor firmware version
 * @return Minor version number
 */
uint16_t app_version_get_minor(void);

/**
 * @brief Get build firmware version
 * @return Build version number
 */
uint16_t app_version_get_build(void);

#endif /* APP_VERSION_H_ */
