# Quick start

WSLでVerilator、make、C/C++コンパイラを用意し、IKAOPMのRTLとライセンスを`rtl/`に配置してください。

```bash
# RTLが未配置の場合のみ
bash tools/create_rtl_dir.sh --fetch-raw https://github.com/ika-musume/IKAOPM

# VGM → RTLシミュレーション → WAV
mkdir -p outputs
bash build_and_run.sh tests/vgm/ym2151_scale_rom1.vgm --output outputs/scale.wav

# 48 kHzで出力
build/obj_dir/ikaOPM_sim tests/vgm/ym2151_scale_rom1.vgm --output outputs/scale48.wav --sample-rate 48000

# 問題を調査するときの中間CSVとバス記録
build/obj_dir/ikaOPM_sim tests/vgm/ym2151_scale_rom1.vgm --output outputs/inspect.wav --enable-csv --debug outputs/inspect.bus.csv
```

通常はWAVだけを出力します。VGMヘッダーのクロックでIKAOPMを動かし、ネイティブの出力更新を取得して、指定のWAVレートへ実際に変換します。

詳細、対応範囲、回帰テストは[README.md](README.md)を参照してください。
