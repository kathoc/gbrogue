# Subagent report contract

Subagents must not paste full files, long logs, or large code blocks into the final report unless explicitly asked.

Every report should use this format:

1. Result: completed / blocked / partial.
2. Changed files: file paths only.
3. Key decisions: maximum 5 bullets.
4. Verification: commands run and pass/fail.
5. Main-session follow-up: exact files and line ranges worth reading.

For investigation tasks, return findings as file_path:line_number references. If line numbers are uncertain, provide the nearest symbol/function name instead.

For test output, include only the failing test names, error class, and the shortest relevant excerpt. Do not paste complete logs.
