#ifndef VGM_PARSER_H
#define VGM_PARSER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    VGM_ERROR_ARGUMENT = -1,
    VGM_ERROR_HEADER = -2,
    VGM_ERROR_TRUNCATED = -3,
    VGM_ERROR_UNSUPPORTED = -4,
    VGM_ERROR_IO = -5,
    VGM_ERROR_CHIP = -6
};

typedef enum {
    VGM_CHIP_UNKNOWN = 0,
    VGM_CHIP_YM2151
} VGMChipId;

typedef struct VGMParserCallbacks {
    /* post_wait_samples is retained for API compatibility and is always zero.
     * Each wait is delivered separately, in order, through on_wait. */
    void (*on_reg_write)(void *user, VGMChipId chip, uint8_t reg, uint8_t val,
                         uint32_t post_wait_samples);
    void (*on_wait)(void *user, uint32_t samples);
    void (*on_end)(void *user);
    /* Called immediately before rejecting an unknown or malformed command. */
    void (*on_unknown)(void *user, uint8_t opcode, uint32_t offset);
    void (*on_data_block)(void *user, uint8_t type_byte,
                          const uint8_t *data_ptr, uint32_t data_size);
} VGMParserCallbacks;

/* Plain or gzip-compressed VGM versions 1.10--1.71, one pass through the stream.
 * Unknown/truncated commands fail; loops are not followed; foreign-chip writes fail. */
int vgm_parse_file(const char *path, const VGMParserCallbacks *cbs, void *user);
int vgm_parse_buffer(const uint8_t *buf, size_t size,
                     const VGMParserCallbacks *cbs, void *user);

/* Read the clock of a single standard YM2151. Reject zero clock, dual-chip and
 * reserved clock flags and malformed headers. Gzip compression is detected by magic. */
int vgm_read_ym2151_clock(const char *path, uint32_t *clock_hz);

#ifdef __cplusplus
}
#endif
#endif
