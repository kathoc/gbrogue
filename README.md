# GB-ROGUE (gb-rogue-org)

実機 Game Boy (DMG) で動く Rogue クローン。GBDK-2020 / C。
UNIX Rogue 5.4 を参照仕様に、iRogue の手触りを目標にした再実装。
仕様の出典は `prompts/`(このリポジトリの要求プロンプト一式)。

- 全26種モンスター(A〜Z)、薬14/巻物13/杖14/指輪14 の未識別システム、
  呪い、空腹、罠、永久死、ターン制、部屋+通路、スクロールダンジョン
- 中断セーブ(SRAM、ロード時消費=スカム不可、死亡で破棄)
- 斜め移動・高速移動・足踏み(シレンGB風操作)、1歩グライドの
  スムーズ移動+ピクセルスクロール
- ASCII 表示 ⇔ 8x8 グラフィックタイルをメニューから切替(内部ID分離)
- ゲームボーイカラー対応: GBC 実機では黒基調のカラーテーマ
  (壁=琥珀/アイテム=アクア/階段=緑/敵=橙赤)、DMG では白基調のまま
- 日本語/英語切替(メニューから)。日本語はかな表記で
  [美咲フォント](https://littlelimit.net/misaki.htm)(門真なむ氏、
  フリーライセンス)を使用 — third_party/misaki/ 参照
- SELECT長押しで全体マップ俯瞰(1マス=2×2ドット)

## ビルドと検証

```bash
make            # build/gbrogue.gb (32KB, MBC1+RAM+BATTERY)
make verify     # PyBoyヘッドレス検証 7本(要 .venv、docs/setup-mac.md)
make run        # SameBoy / Emulicious があれば起動
```

## ドキュメント

| ファイル | 内容 |
|---|---|
| docs/setup-mac.md | Mac 開発環境の準備 |
| docs/architecture.md | モジュール構成・描画パス・テスト戦略・メモリ予算 |
| docs/controls.md | 操作仕様(実装済み) |
| docs/status.md | 実装状況・ROM/WRAM 使用量・教訓 |
| docs/realhw.md | 実機確認チェックリスト(M11) |
| docs/open-questions.md | 仕様判断の記録 |
