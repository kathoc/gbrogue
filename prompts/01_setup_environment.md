# Prompt 01 — Mac Development Environment Setup

MacでGame Boy向け開発環境を整備してください。GBDK-2020を使います。Apple Silicon Macを前提にしますが、Intel Macでも動くようにしてください。

## やること

1. 必要なツールを確認する
2. Homebrewがなければ導入手順を示す
3. git, make, cmake, python3 など必要なものを入れる
4. GBDK-2020を取得する
5. 環境変数を設定する
6. 最小のHello World ROMをビルドする
7. エミュレータで確認できるようにする
8. 実機確認用のROM出力場所を決める

## 推奨コマンド例

必要に応じて調整してよいが、最初の案は次をベースにする。

```bash
brew install git make cmake python
mkdir -p ~/dev/gameboy
cd ~/dev/gameboy
git clone https://github.com/gbdk-2020/gbdk-2020.git
cd gbdk-2020
make
```

ビルド済みリリースを使うほうが安定する場合は、その方法も提示する。

## 成果物

- `docs/setup-mac.md`
- `Makefile`
- `src/main.c`
- `build/` にROMが生成される構成
- `make`
- `make clean`
- `make run` ただしエミュレータが未設定なら説明だけでもよい

## 注意

Claude Codeは、ユーザーのMac環境を壊さないように、グローバル環境への変更を最小限にしてください。`.envrc` や `scripts/env.sh` でGBDK_HOMEを指定できる構成が望ましいです。
