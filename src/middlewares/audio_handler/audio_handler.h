#ifndef AUDIO_HANDLER_H_
#define AUDIO_HANDLER_H_

#include <stddef.h>
#include <stdint.h>

int audio_handler_init(void);

/* Writes mono S16 PCM samples to I2S, duplicated to stereo output. */
int audio_handler_write(const int16_t *pcm_mono, size_t num_samples);

#endif /* AUDIO_HANDLER_H_ */
