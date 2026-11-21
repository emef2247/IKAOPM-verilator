#include "ikaopm_wrapper.h"
#include "ym2151_bus.h"
#include "vgm_player.h"
#include "vgm_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <strings.h>
#include <string>
#include <vector>
#include <limits.h>
#include <sys/stat.h>

static std::string resolved_path(const std::string& path) {
    char buffer[PATH_MAX];
    if (realpath(path.c_str(),buffer)) return buffer;
    const size_t slash = path.find_last_of('/');
    const std::string parent = slash == std::string::npos ? "." : (slash == 0 ? "/" : path.substr(0,slash));
    if (!realpath(parent.c_str(),buffer)) return path;
    return std::string(buffer)+"/"+path.substr(slash == std::string::npos ? 0 : slash+1);
}
static bool same_file(const std::string& a, const std::string& b) {
    struct stat sa, sb;
    if (stat(a.c_str(),&sa) == 0 && stat(b.c_str(),&sb) == 0 &&
        sa.st_dev == sb.st_dev && sa.st_ino == sb.st_ino) return true;
    return resolved_path(a) == resolved_path(b);
}
static void usage(const char* name) {
    printf("Usage: %s INPUT.vgm|INPUT.csv [--output FILE.wav] [--sample-rate 44100|48000]\n",name);
    printf("  --enable-csv     raw stereo strobes, output PCM, and source event CSVs\n");
    printf("  --debug [FILE]   source timestamp / actual bus-clock log\n");
    printf("  --vcd [FILE]     full RTL trace (large; disabled by default)\n");
    printf("  --clock HZ       master clock for CSV without clock metadata (VGM uses its header)\n");
}
static bool is_vgm(const char* path) {
    const size_t n = strlen(path);
    return n >= 4 && (!strcasecmp(path+n-4,".vgm") || !strcasecmp(path+n-4,".vgz"));
}
int main(int argc, char** argv) {
    const char* input = NULL; const char* output = NULL;
    const char* debug = NULL; const char* vcd = NULL;
    bool csv = false; uint32_t clock = 3579545; int rate = 44100; bool clock_override = false;
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (!strcmp(arg,"--help") || !strcmp(arg,"-h")) { usage(argv[0]); return 0; }
        if (!strcmp(arg,"--enable-csv")) csv = true;
        else if (!strcmp(arg,"--no-csv")) csv = false;
        else if (!strcmp(arg,"--vcd") || !strcmp(arg,"--debug")) {
            const char* name = !strcmp(arg,"--vcd") ? "ikaopm_dump.vcd" : "ym2151_bus_calls.log";
            if (i+1 < argc && argv[i+1][0] != '-') name = argv[++i];
            if (!strcmp(arg,"--vcd")) vcd = name; else debug = name;
        } else if (!strcmp(arg,"--output") || !strcmp(arg,"--sample-rate") || !strcmp(arg,"--clock")) {
            if (i+1 >= argc) { fprintf(stderr,"Missing value for %s\n",arg); return 1; }
            const char* value = argv[++i];
            if (!strcmp(arg,"--output")) output = value;
            else {
                char* end = NULL; const unsigned long n = strtoul(value,&end,10);
                if (!*value || *end || n == 0 || n > 100000000) { fprintf(stderr,"Invalid %s\n",arg); return 1; }
                if (!strcmp(arg,"--clock")) { clock = (uint32_t)n; clock_override = true; }
                else if (n == 44100 || n == 48000) rate = (int)n;
                else { fprintf(stderr,"Sample rate must be 44100 or 48000\n"); return 1; }
            }
        } else if (arg[0] == '-' || input) { fprintf(stderr,"Unexpected argument: %s\n",arg); return 1; }
        else input = arg;
    }
    if (!input) { usage(argv[0]); return 1; }
    const bool vgm = is_vgm(input);
    if (vgm) {
        if (clock_override) { fprintf(stderr,"VGM uses its header clock; --clock is for CSV input\n"); return 1; }
        if (vgm_read_ym2151_clock(input,&clock)) {
            fprintf(stderr,"Cannot read a supported single-YM2151 VGM header (dual-chip/reserved clock flags unsupported)\n"); return 1;
        }
    }
    if (!vgm) {
        uint32_t metadata_clock = 0;
        const int clock_result = vgm_player_read_csv_clock(input,&metadata_clock);
        if (clock_result < 0 || (clock_result == 0 && clock_override && metadata_clock != clock)) {
            fprintf(stderr,"Invalid/conflicting CSV clock metadata\n"); return 1;
        }
        if (clock_result == 0) clock = metadata_clock;
        else if (!clock_override) {
            fprintf(stderr,"CSV requires '# clock_hz=HZ' metadata or --clock HZ\n"); return 1;
        }
    }
    if (ikaopm_set_clock_hz(clock)) { fprintf(stderr,"Invalid master clock\n"); return 1; }
    std::string wav;
    if (output) wav = output;
    else {
        wav = input; const size_t slash = wav.find_last_of("/\\");
        if (slash != std::string::npos) wav.erase(0,slash+1);
        const size_t dot = wav.find_last_of('.');
        if (dot != std::string::npos) wav.erase(dot);
        wav += ".wav";
    }
    if (wav == input) { fprintf(stderr,"Output must differ from input\n"); return 1; }
    std::string base = wav;
    if (base.size() >= 4 && !strcasecmp(base.c_str()+base.size()-4,".wav")) base.resize(base.size()-4);
    // Validate all destinations before trace/debug/diagnostic files can open.
    std::vector<std::string> paths;
    paths.push_back(input); paths.push_back(wav);
    if (vcd) paths.push_back(vcd);
    if (debug) paths.push_back(debug);
    if (csv) {
        paths.push_back(base+".audio.csv"); paths.push_back(base+".native.csv");
        if (vgm) paths.push_back(base+".events.csv");
    }
    for (size_t i = 1; i < paths.size(); ++i) {
        for (size_t j = 0; j < i; ++j) {
            if (same_file(paths[i],paths[j])) {
                fprintf(stderr,"Input/output paths must be distinct: %s and %s\n",paths[j].c_str(),paths[i].c_str());
                return 1;
            }
        }
    }
    printf("IKAOPM RTL simulation: %s\n",input);
    printf("Master clock=%u Hz, WAV stereo=%d Hz; native update intervals measured during capture\n",clock,rate);
    printf("RTL parameters: FULLY_SYNCHRONOUS=1 FAST_RESET=0 USE_BRAM=0\n");
    ikaopm_init(); if (vcd) ikaopm_trace_init(vcd);
    ikaopm_reset();
    ym2151_bus_t bus; ym2151_bus_init(&bus);
    ikaopm_audio_wav_open(wav.c_str(),rate);
    if ((csv && ikaopm_audio_csv_open((base+".audio.csv").c_str(),(base+".native.csv").c_str())) ||
        (debug && ym2151_bus_debug_open(debug))) {
        fprintf(stderr,"Cannot open diagnostic output\n"); ikaopm_audio_wav_abort(); ikaopm_release(); return 1;
    }
    const uint64_t start_ps = ikaopm_get_sim_time();
    const std::string events = base+".events.csv";
    const int rc = vgm ? vgm_player_run_vgm(input,&bus,csv ? events.c_str() : NULL) : vgm_player_run_csv(input,&bus);
    if (rc || bus.failed) {
        fprintf(stderr,"Input rendering failed (%d)\n",rc);
        ikaopm_audio_wav_abort(); ym2151_bus_debug_close(); ikaopm_release(); return 1;
    }
    ym2151_bus_report(&bus); const int debug_rc = ym2151_bus_debug_close();
    const uint64_t ticks = ikaopm_get_sim_time()-start_ps;
    printf("Simulation finished. sim_time = %" PRIu64 " ticks (%.6f s)\n",ticks,ticks*1e-12);
    const int wav_rc = ikaopm_audio_wav_close(); ikaopm_release(); return (wav_rc || debug_rc) ? 1 : 0;
}
