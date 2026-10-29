---
name: goal-fidelity
description: Bound repository changes, external-state changes and scope-sensitive multi-step work to the user's stated objective. Skip simple single-result read-only answers.
---

# Goal fidelity

Use this skill before changing repository or external state and before
scope-sensitive multi-step work. Skip it for a simple read-only answer with one
result.

Establish one task contract before acting:

- **Objective:** the user's requested end state.
- **Authorized phases:** only the requested `inspect`, `propose`, `implement`,
  `verify` and `deliver` phases.
- **Scope:** the repository objects, external targets and actions needed for the
  objective.
- **Output:** one concrete result for this task.
- **Acceptance evidence:** the bounded checks that establish that result.
- **Stop boundary:** the condition or next phase at which work stops and is
  reported.

A request may authorize several phases at once. Minimal read-only inspection and
bounded proof may support an authorized phase. Do not infer a later
phase merely because the current phase found something useful.

Every mutation must trace directly to the objective or a mandatory project
contract. Domain skills narrow how work is performed; they do not expand this
task contract. Findings, subagents, validation failures and possible follow-up
improvements do not authorize adjacent work. Report only decision-relevant
out-of-scope findings as optional follow-up.

Verification reports the agreed evidence and does not silently repair another
defect. Delivery occurs only when requested and includes exactly the reviewed
change set. Preserve unrelated user changes. Before completion, compare the
actual changed paths and external actions with the task contract and stop at its
boundary.
