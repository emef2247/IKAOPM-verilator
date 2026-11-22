# IKAOPM-verilator

OPM/YM2151のVGMを読み込み、Verilator上でIKAOPMのRTLをシミュレーションして、結果をWAVとして取得します。音を生成するのはRTLです。C/C++側はVGMの解読、クロックとレジスタ書き込み、PCM取得、WAVへの変換を担当します。

## 準備と実行

WSL上のBash、Verilator、make、C/C++コンパイラ、zlib開発ライブラリが必要です。回帰テストと一括変換にはPython 3を使います（追加Pythonパッケージは不要）。

まず、IKAOPMのソースとライセンスを`rtl/`に配置してください。既に配置されているものを上書きする必要はありません。新規取得の場合は、上流ソースを確認したうえで次を使用できます。

```bash
bash tools/create_rtl_dir.sh --fetch-raw https://github.com/ika-musume/IKAOPM/tree/main
```

リポジトリのルートから実行します。

```bash
bash build_and_run.sh tests/vgm/ym2151_scale_rom1.vgm --output outputs/scale.wav
```

出力先のディレクトリは先に作成してください。出力レートも指定できます。

```bash
mkdir -p outputs
bash build_and_run.sh tests/vgm/ym2151_scale_rom1.vgm --output outputs/scale.wav --sample-rate 48000
```

既にビルド済みなら、実行ファイルを直接使えます。

```bash
build/obj_dir/ikaOPM_sim input.vgm --output outputs/result.wav
```

### 一括変換

`export_vgm.py`で、ファイルまたはディレクトリ単位でまとめてWAVへ変換できます。入力は`.vgm`/`.vgz`で、ディレクトリは直下だけを見ます。各入力は`<stem>.wav`として`--outdir`へ出力され、結果一覧が`results.csv`に残ります。既にWAVがある場合はスキップし、`--force`で上書きします。シミュレータが未ビルドなら自動でビルドします（RTLが無い場合は先に配置してください）。

通常の出力はWAVと`results.csv`のみです。診断用のCSVやバス記録は、解析用のオプションを指定した場合だけ生成されます。

```bash
python3 export_vgm.py input.vgm --outdir outputs
python3 export_vgm.py /path/to/vgm_dir --outdir outputs --sample-rate 48000
```

## 音声と時間の扱い

- VGMの待ち時間は1/44,100秒単位です。
- マスタークロックはVGMヘッダーのYM2151クロック値を使用します。通常は3,579,545 Hzです。
- ネイティブのステレオ出力を、サンプルストローブの立ち上がりごとに符号付き16-bit PCMとして取得します。更新レートはマスタークロックの1/64で、3,579,545 Hzのとき約55,930 Hzです。
- 取得したPCMは、整数時刻に基づく線形補間で44,100 Hz（既定）または48,000 Hzへ変換します。
- ピーク正規化、音色補正は行いません。

線形補間はPCMの変換方法です。実機のアナログ出力との完全一致を保証するものではありません。

## 入力のVGMについて

入力は単一YM2151のVGM/VGZ、または本ツールが出力したイベントCSVです。VGMの先頭から終了コマンドまでを一度だけ再生し、ループの繰り返し再生は行いません。他の音源への書き込みを含むVGMと、dual/YM2164フラグは明示的に拒否します。PCM等を伴う曲を黙ってFMだけで再生することはありません。

イベントCSVにはクロックを保存します。クロック情報のない手書きCSVでは `--clock HZ` を指定してください。

## ライセンス

Copyright (c) 2026 emef2247.

Licensed under the [BSD 2-Clause License](LICENSE).

本プロジェクトで利用または参照している第三者コンポーネントのライセンスおよび著作権表示については、[NOTICE.md](NOTICE.md)を参照してください。IKAOPMのRTL本体は本リポジトリに含まず、各自で取得する形です（取得時に上流のライセンス文が`rtl/IKAOPM_LICENSE`へ保存されます）。RTLの取得元と固定リビジョンは[docs/rtl_provenance.json](docs/rtl_provenance.json)に記録しています。

## 謝辞

本プロジェクトの開発、検証、およびテスト生成では、以下のツールやプロジェクトを利用または参考にしています。開発者・メンテナー・貢献者の皆様に感謝します。

とりわけ、YM2151（OPM）のRTL再実装であるIKAOPMを公開してくださったika-musumeさんに感謝します。本プロジェクトはIKAOPMのRTLそのものを音源として鳴らすもので、その公開がなければ成り立ちませんでした。

| プロジェクト | 用途 |
| --- | --- |
| [IKAOPM](https://github.com/ika-musume/IKAOPM) | 音源となるYM2151のRTL本体（ika-musumeさん） |
| [Verilator](https://www.veripool.org/verilator/) | RTLシミュレーション |

使用した第三者コンポーネントの参照元、著作権表示、ライセンス条件は[NOTICE.md](NOTICE.md)に記録しています。
