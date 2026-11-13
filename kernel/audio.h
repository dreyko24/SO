#ifndef VORTEX_AUDIO_H
#define VORTEX_AUDIO_H
#include "fs.h"
/* Sonido PCM WAV en dispositivo AC'97 PCI real o emulado (modo sin interrupciones).
 * Requiere soporte DMA de 32 bits; no reproduce MP3/OGG ni garantiza HDA.
 */
int audio_hardware(void); /* 1 AC97, 0 no compatible */
int audio_play(FsNode *file); /* 0 ok, -1 formato, -2 dispositivo */
void audio_tick(void);
int audio_active(void);
int audio_paused(void);
void audio_pause(int pause);
void audio_stop(void);
int audio_progress(void); /* 0..100 */
const char *audio_error(void);
#endif
