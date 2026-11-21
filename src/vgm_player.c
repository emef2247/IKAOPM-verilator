#include "vgm_player.h"
#include "vgm_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <ctype.h>

typedef struct {
    ym2151_bus_t* bus;
    uint64_t sample, last_output_sample, events;
    FILE* csv;
    int failed;
} player_t;
static void on_wait(void* user, uint32_t samples) {
    player_t* p = (player_t*)user;
    if (p->failed || p->bus->failed) return;
    if (samples > UINT64_MAX-p->sample) {
        fprintf(stderr,"VGM sample timeline overflow\n"); p->failed = 1; return;
    }
    p->sample += samples;
}
static void on_write(void* user, VGMChipId chip, uint8_t reg, uint8_t value, uint32_t post_wait) {
    player_t* p = (player_t*)user;
    if (p->failed || p->bus->failed) return;
    if (chip != VGM_CHIP_YM2151) { p->failed = 1; return; }
    ym2151_bus_wait_until_sample(p->bus,p->sample);
    ym2151_bus_write_addr(p->bus,reg); ym2151_bus_write_data(p->bus,value);
    if (p->bus->failed) return;
    if (p->csv && fprintf(p->csv,"%" PRIu64 ",01,%02X\n0,00,%02X\n",
                          p->sample-p->last_output_sample,reg,value) < 0) p->failed = 1;
    p->last_output_sample = p->sample; ++p->events;
    on_wait(user,post_wait);
}
static void on_unknown(void* user, uint8_t opcode, uint32_t offset) {
    (void)user;
    fprintf(stderr,"Unsupported or malformed VGM command 0x%02X at 0x%X (single-OPM render only)\n",opcode,offset);
}
int vgm_player_run_vgm(const char* path, ym2151_bus_t* bus, const char* event_csv_path) {
    if (!path || !bus) return -1;
    player_t p; memset(&p,0,sizeof(p)); p.bus=bus;
    if (event_csv_path) {
        p.csv=fopen(event_csv_path,"w"); if (!p.csv) return -1;
        if (fprintf(p.csv,"# clock_hz=%u\ndelay,reg,data\n",ikaopm_get_clock_hz()) < 0) p.failed=1;
    }
    VGMParserCallbacks cb; memset(&cb,0,sizeof(cb));
    cb.on_reg_write=on_write; cb.on_wait=on_wait; cb.on_unknown=on_unknown;
    int rc=vgm_parse_file(path,&cb,&p);
    if (!rc && !p.failed && !bus->failed) {
        ym2151_bus_wait_until_sample(bus,p.sample);
        if (p.csv && p.sample>p.last_output_sample &&
            fprintf(p.csv,"%" PRIu64 ",wait,00\n",p.sample-p.last_output_sample)<0) p.failed=1;
    }
    if (p.csv && fclose(p.csv)) p.failed=1;
    printf("VGM events=%" PRIu64 ", timeline=%" PRIu64 " samples (%.6f s), first traversal only\n",p.events,p.sample,p.sample/44100.0);
    return rc ? rc : (p.failed || bus->failed ? -1 : 0);
}
static int parse_number(char* text,int base,uint64_t* result) {
    while (isspace((unsigned char)*text)) ++text;
    if (!*text || *text=='-') return -1;
    errno=0; char* end=NULL; unsigned long long n=strtoull(text,&end,base);
    if (errno || end==text) return -1;
    while (isspace((unsigned char)*end)) ++end;
    if (*end) return -1;
    *result=n; return 0;
}
int vgm_player_read_csv_clock(const char* path,uint32_t* clock_hz) {
    if (!path || !clock_hz) return -1;
    FILE* fp=fopen(path,"r"); if (!fp) return -1;
    char line[512]; uint32_t found=0; int rc=1;
    while (fgets(line,sizeof(line),fp)) {
        if (!strchr(line,'\n') && !feof(fp)) { rc=-2; break; }
        if (!strncmp(line,"# clock_hz=",11)) {
            uint64_t value=0;
            if (parse_number(line+11,10,&value) || value==0 || value>100000000 ||
                (found && found!=value)) { rc=-2; break; }
            found=(uint32_t)value; rc=0;
        }
    }
    if (ferror(fp)) rc=-1;
    if (fclose(fp)) rc=-1;
    if (rc==0) *clock_hz=found;
    return rc;
}
int vgm_player_run_csv(const char* path,ym2151_bus_t* bus) {
    if (!path || !bus) return -1;
    FILE* fp=fopen(path,"r"); if (!fp) return -1;
    char line[512]; uint64_t sample=0,events=0; unsigned row=0; int rc=0;
    while (fgets(line,sizeof(line),fp)) {
        ++row;
        if (!strchr(line,'\n') && !feof(fp)) { rc=-2; break; }
        if (!strcmp(line,"delay,reg,data\n") || !strcmp(line,"delay,reg,data\r\n")) continue;
        if (line[0]=='#' || line[0]=='\n' || line[0]=='\r') continue;
        char* first=strchr(line,','); char* second=first?strchr(first+1,','):NULL;
        if (!first || !second || strchr(second+1,',')) { rc=-2; break; }
        *first=0; *second=0;
        char* port=first+1; uint64_t delay=0,value=0;
        if (parse_number(line,10,&delay) || parse_number(second+1,16,&value) || value>255 ||
            delay>UINT64_MAX-sample || (strcmp(port,"01") && strcmp(port,"00") && strcmp(port,"wait"))) {
            rc=-2; break;
        }
        sample+=delay; ym2151_bus_wait_until_sample(bus,sample);
        if (!strcmp(port,"01")) { ym2151_bus_write_addr(bus,(uint8_t)value); ++events; }
        else if (!strcmp(port,"00")) { ym2151_bus_write_data(bus,(uint8_t)value); ++events; }
        if (bus->failed) { rc=-3; break; }
    }
    if (rc==-2) fprintf(stderr,"Invalid event CSV row %u\n",row);
    if (ferror(fp)) rc=-3;
    if (fclose(fp)) rc=-3;
    printf("CSV port events=%" PRIu64 ", timeline=%" PRIu64 " samples (%.6f s)\n",events,sample,sample/44100.0);
    return rc;
}
