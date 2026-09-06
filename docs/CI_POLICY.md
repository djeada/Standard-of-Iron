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
- **Fast build/test**: Debug-build `soi_test_binaries` and `content_validator`,
  then run `SOI_TEST_PROFILE=pr scripts/run-tests.sh`. All nine test binaries
  are built and run; the profile subtracts the individual tests named in
  `tests/extended_tests.txt`.

## What the fast profile leaves out, and why

Linking the test binaries was never the expensive part -- with a warm ccache the
whole build is a couple of minutes. Running them was. A handful of tests
simulate a battle, a siege or a whole AI match tick by tick, and another handful
walk every shipped map, mission or creature asset on disk; each costs seconds,
and in a Debug build that becomes minutes. The lane reached 90 minutes and was
killed by its own timeout with `ai_tests` still on its first test, which had
been running for over an hour.

`tests/extended_tests.txt` names those tests, one GoogleTest filter pattern per
line with the reason it is there. Everything else -- every binary, and the
thousands of tests that measure in milliseconds -- runs on every pull request.

Two checks keep the split honest, both in `scripts/check-test-speed.py`:

- a test that ran in the fast profile and took longer than the per-test budget
  fails the lane, so the next slow test is caught when it is written rather
  than when the lane times out; and
- a manifest pattern that matches no test fails the lane, so renaming a fixture
  cannot quietly retire the gate the pattern named.

The PR path also does **not** run the battlefield verifier, the replay
round-trip or the QML suite, execute simulation performance budgets, or build
the terrain probe.

## Weekly and manual validation

`.github/workflows/weekly.yml` remains the broad whole-project lane: full test
binaries are exercised under sanitizers and coverage, whole-tree lint/Apple
portability runs, and all supported platforms perform packaging validation.

`.github/workflows/extended-validation.yml` runs every Monday and through
`workflow_dispatch`. It owns the expensive gates removed from pull requests:

- the Release `sim_benchmark` amplification budgets;
- the engine-backed terrain-surface authored-placement audit; and
- the full test profile, which is the fast profile plus everything in
  `tests/extended_tests.txt` and the acceptance binaries that are not
  GoogleTest suites. Dispatch this one by hand when a change touches shipped
  content: those are the tests that read it.

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
