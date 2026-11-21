#!/usr/bin/env python3
"""Create small self-authored OPM VGM/CSV fixtures; never overwrite other data.

Usage: python3 tests/make_fixtures.py [--outdir PATH_TO_TESTS]
"""
import argparse
from pathlib import Path
import struct

CLOCK=3579545

def reg(address,value):return bytes((0x54,address,value))

def wait(samples):
    data=bytearray()
    while samples:
        count=min(samples,65535);data+=b'\x61'+struct.pack('<H',count);samples-=count
    return bytes(data)

def voice(channel=0,pan=0xc0):
    result=reg(0x20+channel,pan|7)+reg(0x30+channel,0)
    for slot in range(4):
        offset=8*slot+channel
        result+=reg(0x40+offset,1)+reg(0x60+offset,32)+reg(0x80+offset,31)
        result+=reg(0xa0+offset,0)+reg(0xc0+offset,0)+reg(0xe0+offset,15)
    return result

def binary(commands,samples):
    data=bytearray(256);data[:4]=b'Vgm '
    struct.pack_into('<I',data,8,0x171);struct.pack_into('<I',data,0x30,CLOCK)
    struct.pack_into('<I',data,0x18,samples);struct.pack_into('<I',data,0x34,256-0x34)
    data+=commands+b'\x66';struct.pack_into('<I',data,4,len(data)-4)
    return bytes(data)

def event_csv(commands):
    lines=[f'# clock_hz={CLOCK}','delay,reg,data'];delay=0;pos=0
    while pos<len(commands):
        command=commands[pos]
        if command==0x54:
            address,value=commands[pos+1:pos+3]
            lines.extend((f'{delay},01,{address:02X}',f'0,00,{value:02X}'));delay=0;pos+=3
        elif command==0x61:
            delay+=int.from_bytes(commands[pos+1:pos+3],'little');pos+=3
        else:raise ValueError('Fixture generator only emits YM2151 writes and waits')
    if delay:lines.append(f'{delay},wait,00')
    return ('\n'.join(lines)+'\n').encode()

def write_once(path,data):
    path.parent.mkdir(parents=True,exist_ok=True)
    if path.exists():
        if path.read_bytes()!=data:raise FileExistsError(f'Refusing to overwrite different fixture: {path}')
    else:path.write_bytes(data)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--outdir',type=Path,default=Path(__file__).resolve().parent)
    args=parser.parse_args();fixtures={}
    # Legacy filename retained for build-script compatibility. YM2151 has no
    # instrument ROM: this is an explicit algorithm7 sine voice, not ROM1.
    scale=voice()
    for kc in (0x3e,0x41,0x44,0x45,0x48,0x4a,0x4d,0x4e):
        scale+=reg(0x28,kc)+reg(8,0x40)+wait(4410)+reg(8,0)+wait(1102)
    fixtures['ym2151_scale_rom1']=(scale,8*(4410+1102))
    fixtures['ym2151_sine_a4']=(voice()+reg(0x28,0x4a)+reg(8,0x40)+wait(22050),22050)
    pan=voice(pan=0x40)+reg(0x28,0x4a)+reg(8,0x40)+wait(11025)
    pan+=reg(0x20,0x87)+wait(11025)+reg(0x20,0xc7)+wait(11025)
    fixtures['ym2151_stereo_pan']=(pan,33075)
    fixtures['ym2151_silence']=(wait(4410),4410)
    retrigger=voice()+reg(0x28,0x4a)
    for _ in range(4):retrigger+=reg(8,0x40)+wait(2205)+reg(8,0)+wait(2205)
    fixtures['ym2151_retrigger']=(retrigger,17640)
    for stem,(commands,samples) in fixtures.items():
        write_once(args.outdir/'vgm'/f'{stem}.vgm',binary(commands,samples))
        write_once(args.outdir/'csv'/f'{stem}.vgm.csv',event_csv(commands))
        print(stem,samples,'VGM ticks')

if __name__=='__main__':main()
