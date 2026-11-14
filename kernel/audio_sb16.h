/* Controlador ISA Sound Blaster 16 para VMware (sound.virtualDev = "sb16")
 * y QEMU (-device sb16). WAV PCM -> DMA mono 8 bits 22050 Hz.
 */
#ifndef VORTEX_AUDIO_SB16_H
#define VORTEX_AUDIO_SB16_H
#include "fs.h"
int sb16_probe(void);
int sb16_play(FsNode *file);
void sb16_stop(void);
void sb16_pause(int p);
void sb16_tick(void);
int sb16_active(void);
int sb16_progress(void);
int sb16_paused(void);
const char *sb16_error(void);
#endif
