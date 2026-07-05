# Delegation checklist

Before using a subagent, decide:

- Is this mostly judgment? If yes, keep it in Opus.
- Is this mostly reading, editing, or testing? If yes, delegate.
- Is the task clear enough that another agent can finish without more user context? If no, keep it in Opus and clarify the plan first.
- Can several small tasks be batched into one instruction? If yes, batch them.
- Can the instruction reference an existing plan file instead of restating details? If yes, pass the path.

After a subagent reports back:

- Do not trust the result blindly.
- Read only the cited files/line ranges needed for verification.
- Use Opus to decide whether the tradeoff is acceptable.
- Ask test-runner to verify when command output would be long.
