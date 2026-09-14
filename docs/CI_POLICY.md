# Continuous Integration Policy

The CI pipeline is organized around feedback speed. Pull requests run the checks that are fast enough to support normal review, while expensive acceptance, performance, and whole-surface audits run weekly, on demand, or as part of release validation.

This split keeps everyday development responsive without weakening the project's broader validation strategy.

## Pull-request validation

The required pull-request path has three layers.

### Source policy

`python3 scripts/check-pr-policy.py` runs compiler-free checks for:

- architecture boundaries;
- the architecture-document contract;
- QML frame-lock rules; and
- the three migration ratchets.

The runner executes every gate, preserves the underlying script output, and publishes the first actionable failure in the GitHub Actions summary.

### Formatting and static validation

The static layer covers formatting, linting, quality markers, typography, static resource validation, and compiler-free portability checks.

### Fast build and test profile

CI builds `soi_test_binaries` and `content_validator` in Debug mode, then runs:

```sh
SOI_TEST_PROFILE=pr scripts/run-tests.sh
```

All nine test binaries are built and executed. The pull-request profile excludes only the individual tests listed in `tests/extended_tests.txt`.

## Why the fast profile excludes some tests

Linking the test binaries is not the expensive part. With a warm compiler cache, the build itself completes in a few minutes. The cost comes from a small set of runtime-heavy tests.

Some tests simulate a battle, siege, or complete AI match tick by tick. Others inspect every shipped map, mission, or creature asset on disk. Each test can take seconds, and the combined cost becomes substantial in a Debug build. Before the profile was split, the lane reached its 90-minute timeout while `ai_tests` was still running its first test.

`tests/extended_tests.txt` names the expensive tests, one GoogleTest filter pattern per line, together with the reason for the exclusion. Everything else—including every binary and the thousands of millisecond-scale tests—runs on every pull request.

Two checks in `scripts/check-test-speed.py` keep the split from silently degrading:

- a test that runs in the fast profile and exceeds the per-test time budget fails the lane, catching newly slow tests before they can make CI time out; and
- an extended-test manifest pattern that matches no test also fails the lane, preventing fixture renames from quietly disabling a gate.

The pull-request path intentionally does **not** run the battlefield verifier, replay round-trip, QML suite, simulation performance budgets, or terrain-probe build.

## Weekly and manual validation

`.github/workflows/weekly.yml` is the broad whole-project lane. It runs every test binary under sanitizers and coverage with the fast (`pr`) test profile — the extended tests do not fit a two-hour budget instrumented and stay with `extended-validation.yml` — performs the Clang + libc++ (Apple) portability check, and validates packaging on every supported platform. Whole-tree clang-tidy is not part of it; that pass ran past three hours on one runner and stays local (`make lint-deep`).

`.github/workflows/extended-validation.yml` runs every Monday and can also be started through `workflow_dispatch`. It owns the expensive gates removed from pull requests:

- Release-mode `sim_benchmark` amplification budgets;
- the engine-backed terrain-surface authored-placement audit; and
- the full test profile, which combines the fast profile, every entry in `tests/extended_tests.txt`, and acceptance binaries that are not GoogleTest suites.

Run extended validation manually when a change touches shipped content. Those are the checks that read and validate the complete content surface.

Release validation remains the final exhaustive shipping gate.

## Running the source-policy checks locally

Use the same command that CI runs:

```sh
python3 scripts/check-pr-policy.py
```

When a pull request targets a branch other than `main`, pass the base explicitly:

```sh
python3 scripts/check-pr-policy.py --base-ref origin/develop
```

In GitHub Actions, the runner resolves `GITHUB_BASE_REF` and uses the pull-request merge parent as a fallback. Local runs fall back to `origin/main` and then `main`.

Migration-ratchet diagnostics show the target branch's checked-in budget together with any budget values changed by the pull request.

The runner also provides a compiler-free self-test:

```sh
python3 scripts/check-pr-policy.py --self-test
```

The result is a CI policy that optimizes for two different needs: fast feedback during review and exhaustive validation before changes are trusted broadly or shipped.
