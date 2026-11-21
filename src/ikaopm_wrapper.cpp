#include "ikaopm_wrapper.h"
#include "VIKAOPM.h"
#include "verilated.h"
#include "verilated_vcd_c.h"
#include "wav_writer.h"
#include <cstdio>
#include <cinttypes>
#include <string>
#include <vector>
static VIKAOPM* top=nullptr;
static VerilatedVcdC* trace=nullptr;
static uint32_t clock_hz=3579545;
static uint64_t cycles=0,time_ps=0,time_remainder=0;
static bool recording=false,prev_l=false,prev_r=false;
static uint64_t origin=0,origin_ps=0,previous_tick=0,next_tick=0;
static uint64_t count_l=0,count_r=0,last_l=0,last_r=0,min_l=UINT64_MAX,max_l=0,min_r=UINT64_MAX,max_r=0;
static int rate=44100;
static int16_t previous_l=0,previous_r=0;
static std::vector<int16_t> pcm;
static std::string wav_path;
static FILE* audio_csv=nullptr;
static FILE* native_csv=nullptr;
int ikaopm_set_clock_hz(uint32_t hz) { if(top || !hz || hz>100000000)return -1;clock_hz=hz;return 0; }
uint32_t ikaopm_get_clock_hz(void) {return clock_hz;}
uint64_t ikaopm_get_clock_count(void) {return cycles;}
uint64_t ikaopm_get_sim_time(void) {return time_ps;}
static int16_t signed_port(uint16_t v) {return v<32768 ? int16_t(v) : int16_t(int32_t(v)-65536);}
static void emit(int16_t l,int16_t r) {
    pcm.push_back(l);pcm.push_back(r);
    if(audio_csv)std::fprintf(audio_csv,"%zu,%d,%d\n",pcm.size()/2-1,int(l),int(r));
    next_tick+=clock_hz;
}
static int16_t interpolate(int16_t from,int16_t to,uint64_t offset,uint64_t width) {
    int64_t n=(int64_t(to)-from)*int64_t(offset);
    n+=n>=0 ? int64_t(width/2) : -int64_t(width/2);
    return int16_t(from+n/int64_t(width));
}
static void capture(void) {
    const bool l=top->o_EMU_L_SAMPLE,r=top->o_EMU_R_SAMPLE;
    const bool edge_l=l&&!prev_l,edge_r=r&&!prev_r;
    const int16_t left=signed_port(top->o_EMU_L),right=signed_port(top->o_EMU_R);
    if(recording) {
        if(edge_l||edge_r) {
            if(native_csv)std::fprintf(native_csv,"%" PRIu64 ",%" PRIu64 ",%s,%d,%d\n",
                cycles-origin,time_ps,edge_l?"L":"R",int(left),int(right));
            uint64_t& count=edge_l?count_l:count_r;
            uint64_t& last=edge_l?last_l:last_r;
            uint64_t& minimum=edge_l?min_l:min_r;
            uint64_t& maximum=edge_l?max_l:max_r;
            if(count) {const uint64_t interval=cycles-last;if(interval<minimum)minimum=interval;if(interval>maximum)maximum=interval;}
            ++count;last=cycles;
        }
        // Upstream permits pairing stable outputs at one channel's strobe.
        // Keep both strobe times above; use L edges for stereo resampling.
        if(edge_l) {
            const uint64_t tick=(cycles-origin)*rate;
            while(next_tick<=tick && tick>previous_tick) {
                emit(interpolate(previous_l,left,next_tick-previous_tick,tick-previous_tick),
                     interpolate(previous_r,right,next_tick-previous_tick,tick-previous_tick));
            }
            previous_tick=tick;previous_l=left;previous_r=right;
        }
    }
    prev_l=l;prev_r=r;
}
static void half_tick(uint8_t value) {
    if(!top)return;
    time_remainder+=1000000000000ULL;time_ps+=time_remainder/(2ULL*clock_hz);time_remainder%=2ULL*clock_hz;
    if(value)++cycles;
    top->i_EMUCLK=value;top->eval();
#if VM_TRACE
    if(trace)trace->dump(time_ps);
#endif
    if(value)capture();
}
void ikaopm_step_emuclk_posedge(void) {half_tick(1);}
void ikaopm_step_emuclk_negedge(void) {half_tick(0);}
void ikaopm_init(void) {
    if(top)return;Verilated::traceEverOn(true);top=new VIKAOPM;
    cycles=time_ps=time_remainder=0;top->i_EMUCLK=0;top->i_phiM_PCEN_n=0;top->i_IC_n=1;
    top->i_CS_n=top->i_RD_n=top->i_WR_n=1;top->i_A0=top->i_D=0;top->eval();prev_l=prev_r=false;
}
void ikaopm_reset(void) {
    if(!top)return;top->i_IC_n=0;
    for(int i=0;i<1024;i++)ikaopm_step_emuclk_1cycle();
    top->i_IC_n=1;for(int i=0;i<256;i++)ikaopm_step_emuclk_1cycle();
}
void ikaopm_set_phiM_pcen_n(uint8_t v) {if(top)top->i_phiM_PCEN_n=!!v;}
void ikaopm_set_CS_n(uint8_t v) {if(top)top->i_CS_n=!!v;}
void ikaopm_set_RD_n(uint8_t v) {if(top)top->i_RD_n=!!v;}
void ikaopm_set_WR_n(uint8_t v) {if(top)top->i_WR_n=!!v;}
void ikaopm_set_A0(uint8_t v) {if(top)top->i_A0=!!v;}
void ikaopm_set_D(uint8_t v) {if(top)top->i_D=v;}
uint8_t ikaopm_get_D(void) {return top?top->o_D:0;}
uint8_t ikaopm_get_D_OE(void) {return top?top->o_D_OE:0;}
void ikaopm_audio_wav_open(const char* path,int sample_rate) {
    if(recording)return;pcm.clear();wav_path=path?path:"audio_samples.wav";rate=sample_rate;
    origin=cycles;origin_ps=time_ps;previous_tick=0;next_tick=clock_hz;
    previous_l=signed_port(top->o_EMU_L);previous_r=signed_port(top->o_EMU_R);
    prev_l=top->o_EMU_L_SAMPLE;prev_r=top->o_EMU_R_SAMPLE;
    count_l=count_r=last_l=last_r=max_l=max_r=0;min_l=min_r=UINT64_MAX;recording=true;
}
int ikaopm_audio_csv_open(const char* path,const char* native_path) {
    audio_csv=std::fopen(path,"w");if(!audio_csv)return -1;
    native_csv=std::fopen(native_path,"w");if(!native_csv){std::fclose(audio_csv);audio_csv=nullptr;return -1;}
    std::fprintf(audio_csv,"frame,left,right\n");std::fprintf(native_csv,"clock,t_ps,edge,left,right\n");return 0;
}
static int close_csv(void) {
    int rc=0;
    if(audio_csv){if(std::ferror(audio_csv))rc=-1;if(std::fclose(audio_csv))rc=-1;}
    if(native_csv){if(std::ferror(native_csv))rc=-1;if(std::fclose(native_csv))rc=-1;}
    audio_csv=native_csv=nullptr;return rc;
}
int ikaopm_audio_wav_close(void) {
    if(!recording)return 0;
    const uint64_t end=(cycles-origin)*rate;
    while(next_tick<=end)emit(previous_l,previous_r);
    int rc=close_csv();if(wav_write_stereo16(wav_path.c_str(),pcm.data(),pcm.size()/2,rate))rc=-1;
    if(rc)std::fprintf(stderr,"Failed writing audio outputs: %s\n",wav_path.c_str());
    else std::printf("WAV written: %s (%zu stereo frames @ %d Hz)\n",wav_path.c_str(),pcm.size()/2,rate);
    std::printf("Native L captures=%" PRIu64 " interval=%" PRIu64 "..%" PRIu64 "; R captures=%" PRIu64 " interval=%" PRIu64 "..%" PRIu64 " clocks\n",
        count_l,min_l==UINT64_MAX?0:min_l,max_l,count_r,min_r==UINT64_MAX?0:min_r,max_r);
    recording=false;return rc;
}
void ikaopm_audio_wav_abort(void) {close_csv();recording=false;pcm.clear();}
void ikaopm_trace_init(const char* path) {
#if VM_TRACE
    if(!top||trace)return;trace=new VerilatedVcdC;top->trace(trace,99);trace->open(path);
#else
    (void)path;
#endif
}
void ikaopm_trace_close(void) {
#if VM_TRACE
    if(trace){trace->close();delete trace;trace=nullptr;}
#endif
}
void ikaopm_release(void) {ikaopm_audio_wav_close();ikaopm_trace_close();if(top){top->final();delete top;top=nullptr;}}
