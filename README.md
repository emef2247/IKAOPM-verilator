# IKAOPM-verilator

OPM/YM2151のVGMをIKAOPM RTLとVerilatorで実行し、ステレオWAVにします。
音声はRTLが生成します。ソフトウェア音源への置換は行いません。

WSLに `verilator`、`make`、C/C++コンパイラ、Python3、zlib開発環境が必要です。

```bash
# 初回のみ: 指定リビジョンの上流RTLとライセンスをローカル取得
bash tools/create_rtl_dir.sh

# ビルドして変換
mkdir -p outputs
bash build_and_run.sh tests/vgm/ym2151_scale_rom1.vgm --output outputs/scale.wav

# ビルド後は実行ファイルを直接利用できます
build/obj_dir/ikaOPM_sim input.vgm --output outputs/song.wav
```

入力は単一YM2151のVGM/VGZ、または出力したイベントCSVです。VGMの先頭から
終了コマンドまでを一度再生します。ループの繰り返し再生は行いません。
他の音源への書き込みを含むVGMと、dual/YM2164フラグは明示的に拒否します。
PCM等を伴う曲を黙ってFMだけで再生することはありません。

通常はWAVだけを生成します。44.1kHzが既定で、`--sample-rate 48000`も指定できます。
VGMではヘッダーの音源クロックを使用します。イベントCSVにはクロックを保存します。
クロック情報のない手書きCSVでは `--clock HZ` を指定してください。

`export_vgm.py`で、ファイルまたはディレクトリ単位でまとめてWAVへ変換できます。入力は`.vgm`/`.vgz`で、ディレクトリは直下だけを見ます。各入力は`<stem>.wav`として`--outdir`へ出力され、結果一覧が`results.csv`に残ります。既にWAVがある場合はスキップし、`--force`で上書きします。シミュレータが未ビルドなら自動でビルドします。

```bash
python3 export_vgm.py input.vgm --outdir outputs
python3 export_vgm.py /path/to/vgm_dir --outdir outputs --sample-rate 48000
```

```bash
# 問題を調べる場合のみ、元イベント・実バス時刻・生出力を保存
build/obj_dir/ikaOPM_sim input.vgm --output outputs/inspect.wav --enable-csv --debug outputs/inspect.bus.csv

# 自作データによる回帰テスト
python3 tests/test_audio_pipeline.py build/obj_dir/ikaOPM_sim
```

ヘルプは `build/obj_dir/ikaOPM_sim --help` で確認できます。
RTLの取得元・ライセンス・固定リビジョンは [NOTICE.md](NOTICE.md) と
[docs/rtl_provenance.json](docs/rtl_provenance.json) に記録しています。
RTLや生成音声、ビルド成果物はリポジトリへ含めません。

音程・符号・左右分離・時刻・更新周期・CSV再生の自動確認を行っています。
実カートリッジやアナログ出力との一致を保証するものではありません。
