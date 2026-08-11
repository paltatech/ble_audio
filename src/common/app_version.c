#include <zephyr/init.h>
#include <zephyr/logging/log.h>

#include <stdint.h>

#include "app_version.h"

#include "app_build_version.h"

LOG_MODULE_REGISTER(app_version, LOG_LEVEL_INF);

#define FW_VERSION_HASH_SIZE_BYTES 4

#define VXPACKED(__declaration__)  __declaration__ __attribute__((packed))

VXPACKED(struct app_fw_version {
	uint16_t major;
	uint16_t minor;
	uint16_t build;
	bool is_dirty;
	uint8_t hash[FW_VERSION_HASH_SIZE_BYTES];
	const char *describe;
	const char *time;
	const char *date;
});

static struct app_fw_version fw_version;

int app_version_set_and_print_fw_version(void)
{
	fw_version.major = APP_BUILD_MAJOR_FIRMWARE_VERSION;
	fw_version.minor = APP_BUILD_MINOR_FIRMWARE_VERSION;
	fw_version.build = APP_BUILD_FIRMWARE_VERSION;
	fw_version.describe = APP_BUILD_VERSION;
	fw_version.date = __DATE__;
	fw_version.time = __TIME__;

	fw_version.is_dirty = APP_BUILD_FIRMWARE_IS_DIRTY;

	/* Extract each byte and store it in little endian */
	for (size_t i = 0; i < sizeof(fw_version.hash); i++) {
		fw_version.hash[i] = (APP_BUILD_FIRMWARE_HASH >> (i * 8)) & 0xFF;
	}

	LOG_INF("===========================================");
	LOG_INF("BLE Audio Firmware");
	LOG_INF("Version: %s", fw_version.describe);
	LOG_INF("Compile time: %s %s", fw_version.date, fw_version.time);
	LOG_INF("===========================================");

	return 0;
}

uint16_t app_version_get_major(void)
{
	return fw_version.major;
}

uint16_t app_version_get_minor(void)
{
	return fw_version.minor;
}

uint16_t app_version_get_build(void)
{
	return fw_version.build;
}

SYS_INIT(app_version_set_and_print_fw_version, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
