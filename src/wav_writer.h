#ifndef WAV_WRITER_H
#define WAV_WRITER_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int wav_write_stereo16(const char* path,const int16_t* interleaved,size_t frames,int sample_rate);
#ifdef __cplusplus
}
#endif
#endif
