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

### Compiler cache

"A few minutes with a warm cache" only holds while the cache survives, and on 27–28 September 2026 it did not: every pull-request build compiled all ~1480 objects from scratch, about 40 minutes. The repository gets 10 GB of Actions cache and GitHub evicts the least recently used entries past that. Weekly, extended and release-tag runs each saved 0.5–1.3 GB, the total reached 11.4 GB, and the pull-request cache was the one that went. The rules that keep it alive:

- **Only a push to `main` saves `pr-fast-ubuntu-24.04-Debug`.** Pull requests restore it; a per-PR save is readable by no other ref.
- **Nothing scheduled saves.** GitHub drops a cache nobody reads for seven days, so a Monday save is gone before the next Monday and only evicts `main`'s meanwhile. The Debug lanes that share the pull-request configuration (portability, the extended full-test and terrain jobs) restore its key instead, and must configure with exactly the same flags to hit it.
- **Release tags do not save.** A tag's cache can only be read by that tag.
- **`max-size` must hold a whole build.** At 1G ccache evicted the build's own objects mid-build (1256 cleanups), so the saved cache never matched what the next run compiled.

CI configures Debug with `-DSOI_DEBUG_INFO=lines -DSOI_LINKER=mold` (`cmake/BuildSpeed.cmake`): line-table debug info keeps backtraces and sanitizer reports and makes the objects smaller to link and cache. When builds get slow, start from `gh api repos/{owner}/{repo}/actions/cache/usage`, `gh cache list`, and the ccache statistics block the job prints in its post-job cleanup.

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

The full test profile runs as one job per group rather than one job for everything, because one job could not finish the set. `ai_tests` carries the headless AI matches and the mission wave assaults; in the 21 September 2026 run it was still going after 2 h 12 m when the job died on its three-hour budget, which meant `render_tests`, `app_tests`, `arena_tests` and `tools_tests` had never reached the full profile at all — for three weeks the lane reported a timeout and nobody could tell the difference between "these suites pass" and "these suites did not run". `ai_tests` now has a job and a five-hour budget of its own, and the other seven suites share a second job that also runs the acceptance binaries.

`scripts/run-tests.sh` takes `SOI_TEST_SUITES` (a space-separated subset of its `suites` array; an unknown name is an error, not a silent no-op) and `SOI_RUN_ACCEPTANCE=1`, which is how exactly one of those jobs runs the acceptance binaries rather than each of them running the set. The suite array itself stays the whole truth about which suites exist, and `ModuleBoundaries.TheSuiteListsCiRunsAndCmakeBuildsAgree` still parses it against CMake.

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
