# Role-based model selection — Opus main mode

## Goal
Improve development accuracy while lowering expensive main-session token use. Opus remains responsible for judgment. Sonnet and Haiku handle token-heavy or mechanical work.

## Model roles

- Main session / Opus: product judgment, architecture, design tradeoffs, unclear requirements, final integration review, risk review, and deciding whether a result is acceptable.
- code-explore / Sonnet: broad repository reading, symbol tracing, dependency mapping, locating likely causes, and returning concise file_path:line findings.
- implementer / Sonnet: well-specified edits, mechanical refactors, small feature implementation, test additions, and documentation updates.
- heavy-implementer / Opus: cross-cutting implementation, difficult debugging, unclear root cause analysis, migrations, and changes where a wrong edit is expensive.
- test-runner / Haiku: running tests, collecting failures, compressing logs, and reporting pass/fail with the smallest useful detail.
- reviewer / Opus: final review of nontrivial changes before completion.

## Delegate
Delegate when the task likely requires any of the following:

- Reading 3 or more files.
- Editing 2 or more files.
- Searching symbols, references, call sites, routes, schemas, or tests across the repository.
- Running tests and summarizing long output.
- Repeating a command after a fix.
- Producing a first implementation from a clear plan.

## Do not delegate
Keep it in the main session when:

- The task is a design decision, product decision, naming decision, or scope decision.
- Only 1 or 2 files need to be read or edited.
- The user is asking for reasoning rather than implementation.
- The next action depends on subtle user intent.
- The main session needs to inspect a specific known line or small code fragment.

## Practical routing

- Unknown codebase question: use code-explore first.
- Clear implementation step: use implementer.
- Ambiguous bug with multiple possible causes: use heavy-implementer.
- Tests or logs: use test-runner.
- Nontrivial final check: use reviewer.

## Cost control rule
The delegated task must be self-contained. Prefer passing a plan path, issue path, or file path over rewriting long instructions. Batch related small tasks into one delegation.
