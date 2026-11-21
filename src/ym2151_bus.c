#include "ym2151_bus.h"
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
static FILE* debug=NULL;
static void advance(unsigned clocks) {while(clocks--)ikaopm_step_emuclk_1cycle();}
static int deadline(ym2151_bus_t* bus,uint64_t sample,uint64_t* result) {
    const uint64_t hz=ikaopm_get_clock_hz();
    const uint64_t extra=((sample%44100)*hz+44099)/44100;
    if(bus->origin_cycle>UINT64_MAX-extra || sample/44100>(UINT64_MAX-bus->origin_cycle-extra)/hz){bus->failed=1;return -1;}
    *result=bus->origin_cycle+(sample/44100)*hz+extra;return 0;
}
void ym2151_bus_init(ym2151_bus_t* bus) {memset(bus,0,sizeof(*bus));bus->origin_cycle=ikaopm_get_clock_count();}
void ym2151_bus_wait_until_sample(ym2151_bus_t* bus,uint64_t sample) {
    uint64_t target;if(bus->failed||deadline(bus,sample,&target))return;
    while(ikaopm_get_clock_count()<target)ikaopm_step_emuclk_1cycle();
    bus->scheduled_sample=sample;
}
static int wait_ready(ym2151_bus_t* bus) {
    // YM2151 status is the DATA port (A0=1), not the address port.
    ikaopm_set_WR_n(1);ikaopm_set_A0(1);ikaopm_set_CS_n(0);ikaopm_set_RD_n(0);advance(4);
    unsigned waited=0;
    while((ikaopm_get_D()&0x80)&&waited<2048){advance(1);++waited;}
    const int failed=!ikaopm_get_D_OE()||(ikaopm_get_D()&0x80);
    ikaopm_set_RD_n(1);ikaopm_set_CS_n(1);advance(4);
    if(failed){fprintf(stderr,"YM2151 status/busy timeout after %u clocks\n",waited);bus->failed=1;return -1;}
    return 0;
}
static void write_port(ym2151_bus_t* bus,uint8_t a0,uint8_t value) {
    if(bus->failed||wait_ready(bus))return;
    ikaopm_set_A0(a0);ikaopm_set_D(value);advance(1);
    ikaopm_set_CS_n(0);ikaopm_set_WR_n(0);advance(4);
    ikaopm_set_WR_n(1);ikaopm_set_CS_n(1);
    bus->last_write_cycle=ikaopm_get_clock_count();
    uint64_t target;if(deadline(bus,bus->scheduled_sample,&target))return;
    const uint64_t actual=bus->last_write_cycle;
    const uint64_t late=actual>target?actual-target:0;
    ++bus->accesses;if(late)++bus->late_events;if(late>bus->max_late_cycles)bus->max_late_cycles=late;
    if(debug)fprintf(debug,"%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%s,0x%02X\n",
        bus->accesses,bus->scheduled_sample,actual-bus->origin_cycle,late,a0?"DATA":"ADDR",value);
    // Two input synchronizers plus phi1 request/ack stages must settle before
    // changing the input byte or accepting an initially deasserted busy flag.
    advance(16);wait_ready(bus);
}
void ym2151_bus_write_addr(ym2151_bus_t* bus,uint8_t value){write_port(bus,0,value);}
void ym2151_bus_write_data(ym2151_bus_t* bus,uint8_t value){write_port(bus,1,value);}
int ym2151_bus_debug_open(const char* path) {
    debug=fopen(path,"w");if(!debug)return -1;
    fprintf(debug,"access_idx,vgm_sample,actual_clock,late_clocks,op,value_hex\n");return 0;
}
int ym2151_bus_debug_close(void){
    int rc=0;if(debug){if(ferror(debug))rc=-1;if(fclose(debug))rc=-1;}
    debug=NULL;if(rc)fprintf(stderr,"Failed writing bus diagnostic CSV\n");return rc;
}
void ym2151_bus_report(const ym2151_bus_t* bus) {
    printf("Bus writes=%" PRIu64 ", delayed writes=%" PRIu64 ", max lateness=%" PRIu64 " clocks (%.3f ms)\n",
        bus->accesses,bus->late_events,bus->max_late_cycles,bus->max_late_cycles*1000.0/ikaopm_get_clock_hz());
}
