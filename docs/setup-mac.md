# Mac Setup

Apple Silicon 前提(Intel でも GBDK のリリースバイナリを差し替えれば同手順)。

## 必要なもの

- Xcode Command Line Tools (`make`, `git`)
- Python 3.10+(ヘッドレス検証用)
- GBDK-2020 4.5.0 — `vendor/gbdk/` に同梱済み。別の場所を使う場合は
  `GBDK_HOME=/path/to/gbdk make` か `. scripts/env.sh` で上書き。

グローバル環境は汚さない。ツールチェーンは `vendor/`、Python は `.venv/` に閉じる。

## 初回セットアップ

```bash
cd gb-rogue-org
python3 -m venv .venv
.venv/bin/pip install pyboy pillow
make            # build/gbrogue.gb を生成
make verify     # ヘッドレス検証(PyBoy、ウィンドウなし)
```

## コマンド

| コマンド | 動作 |
|---|---|
| `make` | ROM をビルド → `build/gbrogue.gb` |
| `make clean` | build/ を削除 |
| `make verify` | ビルド後、`tests/verify_*.py` を全部実行 |
| `make run` | SameBoy / Emulicious があれば GUI で開く |
| `make font` | `assets/tiles_ascii_data.c` を再生成 |

## 実機

`build/gbrogue.gb` を EverDrive-GB / EZ-Flash Jr. 等のフラッシュカートに
コピーする。カートタイプは MBC1+RAM+BATTERY、SRAM 8KB(中断セーブ用)。
