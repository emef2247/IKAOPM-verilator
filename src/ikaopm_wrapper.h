#ifndef IKAOPM_WRAPPER_H
#define IKAOPM_WRAPPER_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int ikaopm_set_clock_hz(uint32_t hz);
uint32_t ikaopm_get_clock_hz(void);
uint64_t ikaopm_get_clock_count(void);
uint64_t ikaopm_get_sim_time(void);
void ikaopm_init(void);
void ikaopm_release(void);
void ikaopm_reset(void);
void ikaopm_step_emuclk_posedge(void);
void ikaopm_step_emuclk_negedge(void);
static inline void ikaopm_step_emuclk_1cycle(void) {
    ikaopm_step_emuclk_posedge(); ikaopm_step_emuclk_negedge();
}
void ikaopm_set_phiM_pcen_n(uint8_t v);
void ikaopm_set_CS_n(uint8_t v);
void ikaopm_set_RD_n(uint8_t v);
void ikaopm_set_WR_n(uint8_t v);
void ikaopm_set_A0(uint8_t v);
void ikaopm_set_D(uint8_t v);
uint8_t ikaopm_get_D(void);
uint8_t ikaopm_get_D_OE(void);
void ikaopm_trace_init(const char* path);
void ikaopm_trace_close(void);
void ikaopm_audio_wav_open(const char* path, int sample_rate);
int ikaopm_audio_csv_open(const char* path, const char* native_path);
int ikaopm_audio_wav_close(void);
void ikaopm_audio_wav_abort(void);
#ifdef __cplusplus
}
#endif
#endif
