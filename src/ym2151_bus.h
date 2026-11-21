#ifndef YM2151_BUS_H
#define YM2151_BUS_H
#include "ikaopm_wrapper.h"
typedef struct {
    uint64_t origin_cycle,last_write_cycle,accesses,late_events,max_late_cycles,scheduled_sample;
    int failed;
} ym2151_bus_t;
void ym2151_bus_init(ym2151_bus_t* bus);
void ym2151_bus_wait_until_sample(ym2151_bus_t* bus,uint64_t sample);
void ym2151_bus_write_addr(ym2151_bus_t* bus,uint8_t addr);
void ym2151_bus_write_data(ym2151_bus_t* bus,uint8_t data);
int ym2151_bus_debug_open(const char* path);
int ym2151_bus_debug_close(void);
void ym2151_bus_report(const ym2151_bus_t* bus);
#endif
