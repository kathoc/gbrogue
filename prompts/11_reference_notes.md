# Reference Notes

## Rogue 5.4 / BSD / iRogueについて

このプロジェクトでは、厳密な歴史再現よりも、ユーザーがPalm PilotのiRogueで感じたプレイ体験を優先します。実装参照としてUNIX Rogue 5.4系の資料やソースを使います。

## 方針

- ルール感：iRogue体験優先
- 実装参照：Rogue 5.4系
- 表示：最初はASCII
- 最終表示：8x8ドット絵に差し替え可能
- プラットフォーム：Game Boy実機

## Claude Codeへの補足

Rogueの仕様は版によって差があります。細部で迷った場合は、次の優先順にしてください。

1. Game Boyで破綻なく遊べること
2. iRogueに近い手触り
3. Rogue 5.4系に近いルール
4. 後から調整しやすいデータ設計

細部が不明な場合は、仕様を勝手に削除せず、`docs/open-questions.md` に記録して仮実装してください。
