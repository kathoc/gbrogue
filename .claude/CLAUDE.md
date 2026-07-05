# Personal Claude Code Instructions — Opus Cost/Precision Mode

Use Opus for planning, architecture decisions, ambiguous debugging strategy, and final review. Do not spend Opus tokens on broad file exploration, mechanical edits, repeated test runs, or long log summarization when a subagent can do it.

@references/role-based-model-selection.md
@references/report-contract.md
@references/delegation-checklist.md

Default operating rule: keep the main session short, ask subagents to return only file references, decisions, and verification results, then read only the necessary files/lines in the main session.

## Language

- ユーザーへの返答は必ず日本語で行う。
- 作業開始時の実況（"Let me..."など）も日本語で行う。
- 作業報告・完了報告・エラー説明も日本語で行う。
- コード・API・エラーメッセージ・識別子のみ英語を維持する。