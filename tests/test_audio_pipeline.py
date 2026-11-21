#!/usr/bin/env python3
"""IKAOPM RTL stereo checks with self-authored tones; no game-derived material.

Usage: python3 tests/test_audio_pipeline.py build/obj_dir/ikaOPM_sim
"""
import csv
import gzip
import math
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import wave
from array import array

CLOCK=3579545
SIM=Path(sys.argv.pop(1)).resolve() if len(sys.argv)>1 else Path('build/obj_dir/ikaOPM_sim').resolve()


def reg(address,value):
    return bytes((0x54,address,value))


def wait(samples):
    result=bytearray()
    while samples:
        count=min(samples,65535)
        result.extend(b'\x61'+struct.pack('<H',count));samples-=count
    return bytes(result)


def tone_setup(pan=0xc0,kc=0x4a,channel=0):
    # Algorithm7, only C2 keyed, multiplier1: a simple sustained sine.
    commands=reg(0x20+channel,pan|7)+reg(0x28+channel,kc)+reg(0x30+channel,0)
    for slot in range(4):
        offset=8*slot+channel
        commands+=reg(0x40+offset,1)+reg(0x60+offset,32)
        commands+=reg(0x80+offset,31)+reg(0xa0+offset,0)
        commands+=reg(0xc0+offset,0)+reg(0xe0+offset,15)
    return commands+reg(8,0x40|channel)


def write_vgm(path,commands,samples,clock=CLOCK):
    data=bytearray(256);data[:4]=b'Vgm '
    struct.pack_into('<I',data,8,0x171)
    struct.pack_into('<I',data,0x30,clock)
    struct.pack_into('<I',data,0x18,samples)
    struct.pack_into('<I',data,0x34,256-0x34)
    data.extend(commands+b'\x66');struct.pack_into('<I',data,4,len(data)-4)
    Path(path).write_bytes(data)


def read_wav(path):
    with wave.open(str(path),'rb') as wav:
        assert wav.getnchannels()==2 and wav.getsampwidth()==2
        rate=wav.getframerate();frames=wav.getnframes()
        data=array('h',wav.readframes(frames))
    if sys.byteorder!='little':data.byteswap()
    return rate,data[0::2],data[1::2]


def frequency(values,rate):
    values=values[rate//10:len(values)-rate//20]
    if not values:raise AssertionError('Insufficient steady tone')
    center=sum(values)/len(values);crossings=[]
    for index in range(1,len(values)):
        a,b=values[index-1]-center,values[index]-center
        if a<=0<b:crossings.append(index-1-a/(b-a))
    if len(crossings)<40:raise AssertionError('Too few tone cycles')
    return (len(crossings)-1)*rate/(crossings[-1]-crossings[0])


def rows(path):
    with Path(path).open(newline='') as stream:return list(csv.DictReader(stream))


class AudioPipeline(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory(prefix='ikaopm-tests-')
        cls.root=Path(cls.temp.name)
        if not SIM.is_file():raise RuntimeError('Build simulator first: '+str(SIM))

    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()

    def run_sim(self,*args):
        return subprocess.run([str(SIM),*map(str,args)],cwd=self.root,capture_output=True,text=True,timeout=60)

    def render(self,name,commands,samples,*,clock=CLOCK,rate=44100,diagnostics=False):
        source=self.root/(name+'.vgm');output=self.root/(name+'.wav')
        write_vgm(source,commands,samples,clock)
        args=[source,'--output',output,'--sample-rate',rate]
        if diagnostics:args+=['--enable-csv','--debug',self.root/(name+'.bus.csv')]
        result=self.run_sim(*args)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        actual,left,right=read_wav(output);self.assertEqual(actual,rate)
        clocks=(samples*clock+44099)//44100
        self.assertEqual(len(left),clocks*rate//clock,result.stdout)
        return source,output,left,right,result.stdout

    def test_known_pitch_signed_stereo_and_native_strobes(self):
        _,_,left,right,_=self.render('tone',tone_setup()+wait(22050),22050,diagnostics=True)
        self.assertAlmostEqual(frequency(left,44100),440,delta=2)
        for values in (left,right):
            self.assertLess(min(values),-100);self.assertGreater(max(values),100)
        native=rows(self.root/'tone.native.csv')
        self.assertEqual({row['edge'] for row in native},{'L','R'})
        for edge in ('L','R'):
            clocks=[int(row['clock']) for row in native if row['edge']==edge]
            self.assertGreater(len(clocks),27000)
            self.assertEqual({b-a for a,b in zip(clocks,clocks[1:])},{64})
            self.assertLess(clocks[0],64)
        self.assertEqual(len(rows(self.root/'tone.audio.csv')),22050)
        # The other channel remains held at each channel's update edge.
        last={}
        for row in native:
            if row['edge']=='L' and 'R' in last:self.assertEqual(row['right'],last['R'])
            if row['edge']=='R' and 'L' in last:self.assertEqual(row['left'],last['L'])
            last[row['edge']]=row['left'] if row['edge']=='L' else row['right']
        bus=rows(self.root/'tone.bus.csv')
        self.assertGreater(max(int(row['late_clocks']) for row in bus),0)
        self.assertTrue(all(int(b['actual_clock'])>int(a['actual_clock']) for a,b in zip(bus,bus[1:])))

    def test_same_timestamp_burst_preserves_order_and_final_pan(self):
        commands=tone_setup()
        values=[0x40 if i%2==0 else 0x80 for i in range(64)]
        commands+=b''.join(reg(0x20,v|7) for v in values)+wait(22050)
        source,output,left,right,log=self.render('burst',commands,22050,diagnostics=True)
        bus=rows(self.root/'burst.bus.csv')
        actual=[int(r['actual_clock']) for r in bus]
        self.assertTrue(all(b>a for a,b in zip(actual,actual[1:])))
        self.assertTrue(all(int(r['vgm_sample'])==0 for r in bus))
        self.assertGreater(max(int(r['late_clocks']) for r in bus),2000)
        self.assertEqual([int(r['value_hex'],16) for r in bus[-128:] if r['op']=='DATA'],[v|7 for v in values])
        self.assertLess(max(abs(v) for v in left[4410:]),2)
        self.assertGreater(max(abs(v) for v in right[4410:]),100)

    def test_bus_diagnostic_io_error_is_not_success(self):
        path=self.root/'bus_error.vgm';write_vgm(path,wait(100),100)
        run=subprocess.run([str(SIM),str(path),'--output',str(self.root/'bus_error.wav'),'--debug','/dev/full'],capture_output=True,text=True)
        self.assertNotEqual(run.returncode,0)
        self.assertIn('bus diagnostic',run.stderr)

    def test_left_right_pan_isolation(self):
        for name,pan,active in (('left',0x40,0),('right',0x80,1)):
            _,_,left,right,_=self.render(name,tone_setup(pan)+wait(22050),22050)
            channels=(left,right)
            self.assertGreater(max(map(abs,channels[active])),100)
            self.assertTrue(all(value==0 for value in channels[1-active]))

    def test_48000_rate_is_real_resampling(self):
        _,_,left,_,_=self.render('48k',tone_setup()+wait(22050),22050,rate=48000)
        self.assertAlmostEqual(frequency(left,48000),440,delta=2)
        self.render('short48k',wait(113),113,rate=48000)

    def test_source_clock_controls_pitch_and_csv_replay(self):
        _,output,left,_,_=self.render('clock4m',tone_setup()+wait(22050),22050,clock=4000000,diagnostics=True)
        self.assertAlmostEqual(frequency(left,44100),440*4000000/CLOCK,delta=2.5)
        events=self.root/'clock4m.events.csv'
        self.assertIn('# clock_hz=4000000',events.read_text())
        replay=self.root/'clock4m_replay.wav'
        result=self.run_sim(events,'--output',replay)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        self.assertEqual(replay.read_bytes(),output.read_bytes())
        result=self.run_sim(events,'--output',replay,'--clock',CLOCK)
        self.assertNotEqual(result.returncode,0)

    def test_silence_and_single_tick_tail(self):
        _,_,left,right,_=self.render('silence',wait(22050),22050)
        self.assertTrue(all(value==0 for value in left))
        self.assertTrue(all(value==0 for value in right))
        self.render('one_tick',wait(1),1)

    def test_keyoff_and_trailing_waits_preserved(self):
        commands=tone_setup()+wait(11025)+reg(8,0)+wait(5512)+wait(5513)
        _,_,left,right,_=self.render('release',commands,22050)
        self.assertGreater(max(map(abs,left[:11025])),100)
        self.assertLessEqual(max(map(abs,left[-4410:])),4)
        self.assertLessEqual(max(map(abs,right[-4410:])),4)

    def test_dense_redundant_writes_do_not_stretch_time(self):
        _,_,sparse_l,sparse_r,_=self.render('sparse',tone_setup()+wait(22050),22050)
        commands=tone_setup()
        for _ in range(49):commands+=wait(441)+reg(0x78,32)
        commands+=wait(441)
        _,_,dense_l,dense_r,_=self.render('dense',commands,22050)
        self.assertEqual(dense_l,sparse_l);self.assertEqual(dense_r,sparse_r)

    def test_vgm_csv_equivalence_and_repeatability(self):
        commands=tone_setup()+wait(11025)+reg(8,0)+wait(11025)
        source,output,_,_,_=self.render('repeat',commands,22050,diagnostics=True)
        expected=output.read_bytes();rerun=self.root/'rerun.wav'
        for item in (source,self.root/'repeat.events.csv'):
            result=self.run_sim(item,'--output',rerun)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            self.assertEqual(rerun.read_bytes(),expected)

    def test_gzip_vgm_vgz_and_corrupt_streams(self):
        source,output,_,_,_=self.render('gzip_ref',tone_setup()+wait(22050),22050)
        expected=output.read_bytes();result_path=self.root/'gzip_result.wav'
        for extension in ('.vgm','.vgz'):
            packed=self.root/('compressed'+extension)
            packed.write_bytes(gzip.compress(source.read_bytes(),mtime=0))
            result=self.run_sim(packed,'--output',result_path)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            self.assertEqual(result_path.read_bytes(),expected)
        good=packed.read_bytes();bad=bytearray(good);bad[-8]^=1
        for data in (bytes(bad),good[:-6]):
            packed.write_bytes(data)
            self.assertNotEqual(self.run_sim(packed,'--output',result_path).returncode,0)

    def test_foreign_dual_unknown_truncated_and_unwritable_fail(self):
        for name,commands,clock in (
            ('truncated',b'\x54\x28',CLOCK),('unknown',b'\xff',CLOCK),
            ('foreign',b'\x50\x90',CLOCK),('dual',wait(10),CLOCK|0x40000000),
            ('reserved_clock',wait(10),CLOCK|0x80000000)):
            source=self.root/(name+'.vgm');output=self.root/(name+'.wav')
            write_vgm(source,commands,0,clock)
            self.assertNotEqual(self.run_sim(source,'--output',output).returncode,0,name)
            self.assertFalse(output.exists(),name)
        source=self.root/'out_error.vgm';write_vgm(source,wait(100),100)
        self.assertNotEqual(self.run_sim(source,'--output',self.root/'absent/bad.wav').returncode,0)

    def test_output_aliases_protect_source(self):
        source=self.root/'protected.vgm';write_vgm(source,wait(100),100)
        original=source.read_bytes();alias=self.root/'alias.vgm';alias.symlink_to(source)
        for args in (('--debug',source),('--vcd',alias),('--output',source),('--output',alias)):
            self.assertNotEqual(self.run_sim(source,*args).returncode,0)
            self.assertEqual(source.read_bytes(),original)
        source=self.root/'collision.native.csv'
        source.write_text('# clock_hz=3579545\ndelay,reg,data\n100,wait,00\n')
        original=source.read_bytes()
        result=self.run_sim(source,'--output',self.root/'collision.wav','--enable-csv')
        self.assertNotEqual(result.returncode,0);self.assertEqual(source.read_bytes(),original)

    def test_csv_clock_contract_and_invalid_rows(self):
        source=self.root/'bad.csv';output=self.root/'bad_csv.wav'
        source.write_text('delay,reg,data\n100,wait,00\n')
        self.assertNotEqual(self.run_sim(source,'--output',output).returncode,0)
        result=self.run_sim(source,'--output',output,'--clock',CLOCK)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        for text in ('# clock_hz=0\n',
                     '# clock_hz=3579545\n# clock_hz=4000000\n',
                     '# clock_hz=3579545\n0,,01,28\n'):
            source.write_text(text)
            self.assertNotEqual(self.run_sim(source,'--output',output).returncode,0)


if __name__=='__main__':unittest.main(verbosity=2)
