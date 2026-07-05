# Game Boy Rogue Clone — Claude Code Prompt Pack

このZIPは、Mac上でGBDK-2020を使い、最終的に実機ゲームボーイで動くRogue系ゲームを作るためのClaude Code向けプロンプト集です。

## 目的

Palm PilotのiRogueに近い体験を目標にしつつ、参照仕様としてUNIX Rogue 5.4系を使います。最初はオリジナルRogueと同じASCII/記号表示で実装し、後から8x8ドット絵に差し替えられる構造にします。

## 推奨の使い方

1. `00_project_brief.md` をClaude Codeに最初に渡す
2. `01_setup_environment.md` でMacの開発環境を整備させる
3. `02_architecture_prompt.md` で全体設計を作らせる
4. `03_milestones.md` に沿って段階的に実装させる
5. 各段階で `04_acceptance_tests.md` を使って確認する
6. 仕様がブレたら `05_non_negotiables.md` を再投入する

## 注意

Claude Codeには「ローグライク風に簡略化しない」と繰り返し指示してください。Rogueらしさは、ASCII表示ではなく、未識別アイテム、空腹、呪い、罠、永久死、ターン制、限られた情報で判断する構造にあります。
