# IKAOPM-verilator project knowledge

## Purpose and boundaries

Read OPM/YM2151 VGM, drive upstream IKAOPM RTL under Verilator, and write
stereo WAV. The host performs parsing, bus scheduling, capture and explicit
sample-rate conversion. It must not replace RTL synthesis, interpret a musical
score, quantize source times, normalize gain or remap to another chip.

Keep original source timestamps/register bytes, actual bus timing, native
output samples, derived resampled PCM and final WAV separately inspectable.
WAV generation alone does not establish correct sound or hardware equivalence.

## Versioned RTL and implementation parameters

The supported upstream is https://github.com/ika-musume/IKAOPM, revision
08a5b40ed14418319287df450f7d4a523ddeed5e. All eleven Verilog sources are retained
unmodified under ignored rtl/src. docs/rtl_provenance.json records SHA256 hashes
and the BSD-2-Clause license hash. tools/prepare_rtl.py verifies them before
build and supports repeatable local preparation without committing vendor files.

Explicit parameters: FULLY_SYNCHRONOUS=1, FAST_RESET=0, USE_BRAM=0. OPLL-specific
multiplier/gain/MO/RO settings do not apply. The upstream README requires at
least64 phiM clocks of asserted reset with clock enable operating. The harness
uses1024 asserted master clocks and256 released clocks, all excluded from the
playback time origin and audio recording.

## Four distinct rates

- VGM time is integer units of1/44100second.
- The YM2151 master clock comes from the VGM0x30 header; the usual reference
  clock is3579545Hz; CSV uses recorded metadata or explicit --clock. Clock flags are separate from the frequency.
- Each native stereo channel updates every64 master clocks in this pinned RTL,
  measured from both sample-strobe rising edges. At3579545Hz this is55930.390625Hz.
- WAV is44100Hz by default, or48000Hz. Conversion uses rational integer
  master-clock positions and linear interpolation. Relabeling the WAV header
  is not resampling. This is not a claim of ideal antialias/analog filtering.

i_phiM_PCEN_n is held low, enabling every master rising edge. The RTL's
internal phi1 divider is not an additional host timebase. Reset cannot be
shortened by importing the IKAOPLL72-clock or pipeline-parameter observations.

## Native stereo output and signed values

Use recommended signed16 o_EMU_L/o_EMU_R, which retain YM3012-type lossy output
behavior. o_EMU_L_EX/o_EMU_R_EX are different full-range paths and are not
substituted. There is no heuristic output fallback or automatic level scaling.
Verilator stores these ports unsigned; sign interpretation is explicit.

The two output strobes are staggered. Log both edge times and both held values
in .native.csv. Pair stable left/right outputs at L rising edges for stereo
frames, as upstream explicitly permits; retain the other channel's distinct
update times as evidence. Do not call both strobes simultaneous or treat every
high-level clock as a newly synthesized frame. Sample capture belongs in the
shared clock-advance function, including writes, busy polls and setup waits.

The WAV is interleaved signed16 little-endian stereo. Linear interpolation and
the finite held final partial interval are derived conversion behavior. The
native output, channel ordering, capture timing and signs remain inspectable.

## Physical bus and absolute scheduling

The synchronous RTL has two input synchronizers and additional request/ack
stages on internal phi1 phases. Address/data writes hold CS/WR active for4
master clocks, then leave16 clocks to settle before status polling or changing
the byte. OPLL's12/84-clock delays are not used. Busy is read at DATA/A0=1,
with RD/CS asserted and o_D_OE checked. Polling is bounded to2048 clocks;
failed status/busy or deadline overflow propagates to the process exit code.
Test-mode status-bus rerouting is not validated as a general playback path.

Bus transactions occupy the same master-clock timeline as VGM waits. Requested
absolute deadlines absorb transaction time; do not append full relative waits
after every write. Preserve event order, requested times and actual service
lateness. A dense initial burst can delay notes slightly; report that fact
instead of silently spreading source timing. Retain trailing waits and silence.
Round source deadlines up to the next master-clock boundary; an output frame
count can consequently differ by one from an ideal-duration floor.

## Parser and replay contract

Support one primary YM2151, opcode54, VGM1.10..1.71. Gzip is detected by magic,
including compressed files named .vgm; corrupted streams fail. Validate bounds,
clock flags, command sizes and malformed input. Unknown or foreign active
chip writes fail explicitly. Unused header declarations alone are not active
sound. Playback is one traversal through66; song loops are not expanded.

Generated events CSV records # clock_hz metadata and source-relative waits.
Replay uses that clock and rejects conflicting overrides. CSV without metadata
requires explicit --clock. This preserves nondefault-clock VGM/CSV equivalence.
Paths are checked before any output opens, including symlinks and diagnostic
aliases, so diagnostics cannot overwrite source files.

## Evidence and verification

Public/self-authored regressions cover pitch/sign, left/right isolation,
measured stereo strobe periods,44100/48000 durations, nondefault master clock,
VGM/CSV equality, redundant dense writes, tails/silence, deterministic rerender,
gzip, malformed/foreign/dual inputs and output alias protection. Separate C99
unit checks exercise parser, CSV metadata/errors and bounded busy failure.

A known simple KC4A/KF0 carrier measured439.944Hz at3579545Hz, positive and
negative peaks510, and64-clock intervals on both channels. This is a scoped
RTL/harness check, not a full chip, FPGA, physical cartridge or SPDIF proof.
Keep failed hypotheses and detailed measurements in external field notes.

Sources: the pinned upstream README and IKAOPM.v / IKAOPM_reg.v /
IKAOPM_acc.v; https://github.com/ika-musume/IKAOPM and the VGM specification
https://vgmrips.vgmrips.net/wiki/VGM_Specification.

A failed render does not replace an existing WAV: callers must inspect process
exit status before using a pre-existing output. Bus CSV write/close failures
also produce nonzero status, even if the WAV itself was written successfully.
