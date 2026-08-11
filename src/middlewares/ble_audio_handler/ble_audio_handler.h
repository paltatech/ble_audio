#ifndef BLE_AUDIO_HANDLER_H_
#define BLE_AUDIO_HANDLER_H_

#include <stddef.h>
#include <stdint.h>

struct ble_audio_handler_stream_info {
	int freq_hz;
	int frame_duration_us;
};

struct ble_audio_handler_cb {
	/* A central connected/disconnected. */
	void (*connected)(void);
	void (*disconnected)(void);

	/* The sink stream was configured and is about to start, with the
	 * negotiated codec parameters. */
	void (*stream_configured)(const struct ble_audio_handler_stream_info *info);

	/* One LC3-encoded audio frame was received. data is NULL if the
	 * packet was lost (caller should conceal/interpolate). */
	void (*stream_recv)(const uint8_t *data, size_t len);

	void (*stream_stopped)(void);
};

/* Initializes Bluetooth, registers PACS/ASCS callbacks, and starts
 * advertising as an LE Audio unicast server (headset, sink only). */
int ble_audio_handler_start(const struct ble_audio_handler_cb *cb);

#endif /* BLE_AUDIO_HANDLER_H_ */
