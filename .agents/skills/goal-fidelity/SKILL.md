---
name: goal-fidelity
description: Establish the sole task contract for repository or external-state changes and scope-sensitive multi-step work; keep domain skills subordinate to the user's requested outcome. Skip simple read-only answers with one obvious result.
metadata:
  role: task-contract
  core-version: 1
---

Project scope: `Hosts3D`. Use this repository's tools and instructions.

<!-- goal-fidelity-core:start -->
# Goal fidelity

Use the current user request as the authority for the outcome. Project rules
and matching domain skills constrain execution; they do not create another
deliverable or authorize another phase.

This skill is the sole owner of the task contract. Topic skills may also apply,
but they cannot redefine its objective, phases, scope, output, acceptance or
stop boundary. Report another claimed task-contract owner as a conflict.

## Establish the task contract

Before acting, derive these six fields:

- **Objective:** one concrete outcome from the current request.
- **Authorized phases:** only the requested `inspect`, `propose`, `implement`,
  `verify` and `deliver` phases. Minimal read-only inspection and bounded proof
  may support an authorized phase.
- **Scope:** the repositories, paths, objects, Targets and external systems
  directly required for the objective.
- **Output:** one requested result or deliverable.
- **Acceptance:** observable evidence that proves that result.
- **Stop boundary:** the first action, artifact or later phase not authorized by
  this contract.

Derive a value from selected context when it is reliable. Ask only when a
missing value would materially change the result.

## Execute the bounded contract

- Keep the objective unchanged. A finding, prior failure, recommendation or
  general best practice is evidence, not authority for adjacent work.
- Trace every edit and external mutation directly to the objective or a
  mandatory project contract. If a required dependency lies outside scope,
  report the blocker instead of editing it.
- Keep inspection, proposal, implementation, verification and delivery
  distinct. One request may authorize several phases; never infer the next one.
- Implement the minimum requested result. Verification observes the agreed
  result and does not silently repair another defect. Deliver only when
  requested, using the exact reviewed change set.
- Load only matching domain skills. They remain subordinate to this contract.
  Give subagents the same contract and a narrower assignment; they cannot add
  phases or scope.
- Preserve unrelated user work. Report an out-of-scope finding only when it
  affects acceptance or the user's next decision; do not turn it into work.

## Preserve reusable learning

At completion, promote only verified reusable results to their owning source:

- lasting behavior, decisions and operating facts belong in the owning
  repository's functional documentation, code or standing project rules;
- unfinished work, evidence, last state and next action belong in the owning
  planning source; and
- a workspace-wide rule belongs to its coordination owner, while facts needed
  for ordinary standalone work remain in the affected project.

Do not create another memory, handover or status mirror. An external authority
may remain a reference, but it cannot replace local facts required for ordinary
work in an isolated checkout. Report such a missing local fact as a portability
gap rather than silently depending on an absent sibling repository.

Before completion, reconcile every changed path and external action with the
contract. Report the result, bounded evidence and any material deviation or
blocker, then stop.
<!-- goal-fidelity-core:end -->
