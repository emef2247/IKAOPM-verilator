/* Exercise bounded failure without modifying or recompiling vendor RTL. */
#include "ym2151_bus.h"
#include <assert.h>
#include <stdio.h>
static uint64_t clock_count=0;
static uint8_t status=0,oe=1;
uint32_t ikaopm_get_clock_hz(void){return 3579545;}
uint64_t ikaopm_get_clock_count(void){return clock_count;}
void ikaopm_step_emuclk_posedge(void){++clock_count;}
void ikaopm_step_emuclk_negedge(void){}
void ikaopm_set_CS_n(uint8_t v){(void)v;}
void ikaopm_set_RD_n(uint8_t v){(void)v;}
void ikaopm_set_WR_n(uint8_t v){(void)v;}
void ikaopm_set_A0(uint8_t v){(void)v;}
void ikaopm_set_D(uint8_t v){(void)v;}
uint8_t ikaopm_get_D(void){return status;}
uint8_t ikaopm_get_D_OE(void){return oe;}
int main(void){
    ym2151_bus_t bus;ym2151_bus_init(&bus);
    status=128;ym2151_bus_write_addr(&bus,0x20);
    assert(bus.failed && bus.accesses==0 && clock_count<2100);
    status=0;oe=0;ym2151_bus_init(&bus);ym2151_bus_write_data(&bus,0);
    assert(bus.failed);
    oe=1;ym2151_bus_init(&bus);ym2151_bus_wait_until_sample(&bus,UINT64_MAX);
    assert(bus.failed);
    ym2151_bus_init(&bus);ym2151_bus_write_addr(&bus,0x20);ym2151_bus_write_data(&bus,0xc7);
    assert(!bus.failed && bus.accesses==2);
    const uint64_t end=bus.origin_cycle+3579545;
    ym2151_bus_wait_until_sample(&bus,44100);assert(clock_count==end);
    puts("YM2151 bus bounded-error and absolute-clock tests passed");return 0;
}
