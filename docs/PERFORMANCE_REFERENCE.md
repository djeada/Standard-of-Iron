# Reference hardware and the `soi-performance` lane

This file is the checklist for turning frame pacing from an investigation into a
qualification. Nothing here is certified yet. No reference machine has been
designated, and the `soi-performance` runner has not been registered; both are
owner actions that no change in this repository can perform for you.

## What a reference host is

`scripts/check-perf-host.py` is the machine-checkable half of the definition. It
fails closed and records what it found:

```sh
python3 scripts/check-perf-host.py --output artifacts/reference-host.json
```

It rejects a host that has no graphical session, that reports a software
renderer, whose active mode is not 60 Hz, that is running a competing game or
build, or whose one-minute load average is above `--max-load` (1.0 by default).
`.github/workflows/frame-pacing.yml` runs it before it configures anything, so a
contaminated runner fails the lane instead of publishing a number.

The rest of the definition is human: a host is a reference host only once its
exact CPU, GPU, RAM, OS, driver, resolution and refresh rate are recorded in the
table below and the owner has accepted it as the tier's representative.

## Tier matrix (proposed, not certified)

Fill one row per tier before accepting any budget. Every column is required;
"about a 3060" is not a reference machine.

| Tier | CPU | GPU | RAM | OS / kernel | Driver | Resolution | Refresh | Accepted on |
| ---- | --- | --- | --- | ----------- | ------ | ---------- | ------: | ----------- |
| Low  |     |     |     |             |        |            |         |             |
| Mid  |     |     |     |             |        |            |         |             |
| High |     |     |     |             |        |            |         |             |

The local RTX 5060 at 1280 × 720 is an investigation host, not a tier. Its
measurements are recorded in `docs/FRAME_PACING.md` and must not be quoted as
reference results.

## The qualification matrix

For each accepted tier, run both fixture families, all four presets, at least
three repeats of sixty seconds, on an idle host:

```sh
python3 scripts/check-perf-host.py --output artifacts/qualify/reference-host.json

python3 scripts/check-frame-pacing.py \
  --binary build/bin/standard_of_iron \
  --seconds 60 --repeats 3 --camera-cycle \
  --output artifacts/qualify/campaign

python3 scripts/check-frame-pacing.py \
  --binary build/bin/standard_of_iron \
  --mission-file assets/benchmarks/first_contact.mission.json \
  --action-fixture assets/benchmarks/battle_coverage.action.json \
  --seconds 60 --repeats 3 \
  --output artifacts/qualify/actions
```

Omitting `--preset` runs low, medium, high and ultra. Keep every run, including
the failures: a discarded bad repeat is a discarded measurement. The runner
writes `summary.json` with a per-run verdict and an `outcome_comparison` block
that reports, per behaviour, whether it was observed on every preset — that is
the check that a pacing change did not quietly stop orders, combat, destruction
or production from happening at one quality level.

Only after that matrix exists may the proposed budgets in `docs/FRAME_PACING.md`
be accepted, adjusted with the evidence attached, or rejected.

## Provisioning the runner

1. Dedicate a machine from the accepted tier. It must not build, host CI for
   anything else, or run a desktop the operator uses during a measurement.
2. Install the toolchain the workflow assumes: `cmake`, `ninja`, `python3`, a
   hardware OpenGL driver, `glxinfo` (mesa-utils) and `xrandr` (x11-xserver-utils),
   plus the Qt 6 development packages the normal build needs.
3. Give the runner service an active graphical session: `DISPLAY` (and
   `XAUTHORITY` where the session needs it) must reach a real X server on a real
   output, with the output set to 60 Hz. A greeter or locked screen changes GPU
   behaviour; keep the session logged in and unlocked on its own VT.
4. Confirm the host with `python3 scripts/check-perf-host.py`. Do not register
   the runner until it prints `Reference host ready`.
5. Register the GitHub Actions self-hosted runner with the labels
   `self-hosted, linux, x64, soi-performance`, matching `runs-on` in
   `.github/workflows/frame-pacing.yml`.
6. Dispatch the workflow once per preset and check that the uploaded artifact
   contains `reference-host.json`, both `summary.json` files, every per-run
   report and every log.
7. Record the host in the tier matrix above with the date it was accepted.

Steps 1 and 5 need hardware and GitHub administrator access. Until they are
done, the workflow exists but the lane cannot run, and the gate in
`docs/FRAME_PACING.md` stays unqualified.
