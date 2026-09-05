# CI policy

CI is split by feedback speed. Pull requests run only gates that should finish
quickly enough to support normal review; expensive acceptance, performance and
surface audits run weekly or on demand.

## Pull requests

The required pull-request path has three parts:

- **Source policy**: `python3 scripts/check-pr-policy.py` runs compiler-free
  architecture boundaries, the architecture-document contract, QML frame-lock
  rules, and the three migration ratchets. It runs every gate, preserves the
  underlying script output and publishes the first actionable failure in the
  Actions summary.
- **Formatting/static validation**: formatting, lint, quality markers,
  typography, static resource validation and compiler-free portability checks.
- **Fast build/test**: Debug-build only the core/headless suites
  (`simulation_tests`, `combat_balance_tests`, `ai_tests`, `campaign_tests`,
  `persistence_tests`) plus `content_validator`, then run
  `SOI_TEST_PROFILE=pr scripts/run-tests.sh`. The job has a 30-minute ceiling.

The PR path deliberately does **not** build renderer, application, arena or tool
test binaries, run the battlefield verifier, run the replay round-trip, execute
simulation performance budgets, or build the terrain probe.

## Weekly and manual validation

`.github/workflows/weekly.yml` remains the broad whole-project lane: full test
binaries are exercised under sanitizers and coverage, whole-tree lint/Apple
portability runs, and all supported platforms perform packaging validation.

`.github/workflows/extended-validation.yml` runs every Monday and through
`workflow_dispatch`. It owns the expensive gates removed from pull requests:

- the Release `sim_benchmark` amplification budgets; and
- the engine-backed terrain-surface authored-placement audit.

Release validation remains the final exhaustive ship gate.

## Local source-policy preflight

Run the same source-policy command as CI:

```bash
python3 scripts/check-pr-policy.py
```

For a pull request targeting another branch, pass it explicitly:

```bash
python3 scripts/check-pr-policy.py --base-ref origin/develop
```

In Actions, the runner resolves `GITHUB_BASE_REF` and uses the pull-request merge
parent as a fallback. Local runs fall back to `origin/main` and then `main`.
Migration ratchet diagnostics show the target branch's checked-in budget and any
budget values changed by the pull request.

The runner has a compiler-free self-test:

```bash
python3 scripts/check-pr-policy.py --self-test
```
