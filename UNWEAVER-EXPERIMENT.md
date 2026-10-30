# Unweaver experimental branch

Baseline: `4dead78a30b88f7536b385da82c572d4132db8e9`.
Branch: `codex/unweaver-architecture-review-20261030`. Never merge automatically.

## Mission checkpoints

- A: phase/damage ledger and safe combat boundary — in progress.
- B: positioning, shrapnel and ordinary interception — pending.
- C: four phases, silk, echoes, seals, formations and rendering — pending.
- D: bounded observation and tactical learning — pending.
- E: late Director and earlier boss support changes — pending.
- F: regression, executable QA, performance and portable packaging — pending.

## Consequential contracts

- Damage is credited once. Vitality may reach zero before encounter completion;
  the HUD must say that required sequences remain, never invent HP or damage.
- Phase experiences require executed attacks. Optional seals may expire.
- Transitions clear hostile state only at the end of the combat update.
- Positional guards use existing contact threats, without solid blocking/shields.
- Fixtures must not award campaign records or cosmetics.
- Runtime admission establishes bounded modeled response witnesses, not a proof
  of human fairness. Human fun, pacing and readability require acceptance.

## Verification

No implementation verification completed yet. Preserve this distinction until
actual checks are recorded below.
