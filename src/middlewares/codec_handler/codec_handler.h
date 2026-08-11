#ifndef CODEC_HANDLER_H_
#define CODEC_HANDLER_H_

#include <stddef.h>
#include <stdint.h>

/* Configures (or reconfigures) the LC3 decoder. Must be called before
 * codec_handler_decode(). */
int codec_handler_configure(int freq_hz, int frame_duration_us);

/* Decodes one LC3 frame into mono S16 PCM samples in pcm_out, which must
 * be at least AUDIO_MAX_SAMPLES_PER_FRAME samples. data may be NULL to
 * request packet-loss concealment for a missed frame. Returns the number
 * of PCM samples written, or a negative error code. */
int codec_handler_decode(const uint8_t *data, size_t len, int16_t *pcm_out);

#endif /* CODEC_HANDLER_H_ */
