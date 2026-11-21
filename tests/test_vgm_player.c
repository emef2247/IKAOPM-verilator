/* Standalone player/clock-contract checks using a fake bus, not sound synthesis.
 * cc -std=c99 -Wall -Wextra -Werror -Isrc tests/test_vgm_player.c src/vgm_player.c src/vgm_parser.c -lz -o build/test_vgm_player
 */
#include "vgm_player.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint64_t at=0;
static unsigned writes=0;
uint32_t ikaopm_get_clock_hz(void) { return 4000000; }
void ym2151_bus_wait_until_sample(ym2151_bus_t* bus,uint64_t sample) { (void)bus; at=sample; }
void ym2151_bus_write_addr(ym2151_bus_t* bus,uint8_t value) { (void)bus; (void)value; ++writes; }
void ym2151_bus_write_data(ym2151_bus_t* bus,uint8_t value) { ++writes; if (value==0xfe) bus->failed=1; }
static void save(const char* path,const char* text) {
    FILE* fp=fopen(path,"w"); assert(fp); assert(fputs(text,fp)>=0); assert(fclose(fp)==0);
}
int main(int argc,char** argv) {
    assert(argc==2); uint32_t hz=0; ym2151_bus_t bus; memset(&bus,0,sizeof(bus));
    save(argv[1],"# clock_hz=4000000\ndelay,reg,data\n0,01,28\n0,00,4a\n100,wait,00\n");
    assert(vgm_player_read_csv_clock(argv[1],&hz)==0 && hz==4000000);
    assert(vgm_player_run_csv(argv[1],&bus)==0 && at==100 && writes==2);
    save(argv[1],"delay,reg,data\n0,wait,00\n");
    assert(vgm_player_read_csv_clock(argv[1],&hz)==1);
    save(argv[1],"# clock_hz=4000000\n# clock_hz=3579545\n");
    assert(vgm_player_read_csv_clock(argv[1],&hz)<0);
    save(argv[1],"# clock_hz=-1\n");assert(vgm_player_read_csv_clock(argv[1],&hz)<0);
    save(argv[1],"# clock_hz=0\n");assert(vgm_player_read_csv_clock(argv[1],&hz)<0);
    save(argv[1],"# clock_hz=4000000oops\n");assert(vgm_player_read_csv_clock(argv[1],&hz)<0);
    save(argv[1],"0,,01,28\n");assert(vgm_player_run_csv(argv[1],&bus)<0);
    save(argv[1],"18446744073709551615,wait,00\n1,wait,00\n");
    assert(vgm_player_run_csv(argv[1],&bus)<0);
    save(argv[1],"0,00,fe\n1,00,ff\n");writes=0;
    assert(vgm_player_run_csv(argv[1],&bus)<0 && writes==1 && bus.failed);
    assert(remove(argv[1])==0); puts("VGM player tests passed"); return 0;
}
