#!/usr/bin/env bash
# The beat pipeline: a synthesised bed, the grid recovered from it, and a spec
# quantised onto that grid.
#
# A reel is cut *to* the music, so the two failures that matter are silent: a
# tempo read an octave out, and a track with no pulse at all quantising a spec
# onto a grid that is not there. Both are asserted here. Needs python3 and
# ffmpeg; no compiler.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT

green() { printf '\033[0;32m%s\033[0m\n' "$1"; }
die() {
  printf '\033[0;31m%s\033[0m\n' "$1" >&2
  exit 1
}

echo "Test 1: a synthesised bed carries its own grid..."
python3 "${root}/tools/audio_synth/reel_score.py" --bpm 150 --bars 6 \
  --out "${work}/bed.ogg" >/dev/null
[ -f "${work}/bed.grid.json" ] || die "no grid sidecar was written"
python3 - "${work}/bed.grid.json" <<'PY'
import json, sys
grid = json.load(open(sys.argv[1]))
assert grid["bpm"] == 150.0, grid
assert abs(grid["beat_seconds"] - 0.4) < 1e-6, grid
PY
green "✓ Test 1 passed: the bed states its own tempo"

echo
echo "Test 2: the detector recovers that tempo, or its octave, from the audio..."
python3 "${root}/scripts/beat-align.py" --track "${work}/bed.ogg" --detect \
  >"${work}/detected.json"
python3 - "${work}/detected.json" <<'PY'
import json, sys
found = json.load(open(sys.argv[1]))
bpm = found["bpm"]
octaves = [75.0, 150.0, 300.0]
assert any(abs(bpm - o) <= 2.5 for o in octaves), f"detected {bpm}, expected 150 or an octave of it"
assert found["confidence"] >= 2.0, found
PY
green "✓ Test 2 passed: $(python3 -c "import json;print(json.load(open('${work}/detected.json'))['bpm'])") BPM detected"

echo
echo "Test 3: a track with no pulse is refused rather than quantised to nothing..."
ffmpeg -v error -f lavfi -i "sine=frequency=220:duration=8" -c:a libvorbis \
  "${work}/drone.ogg"
if python3 "${root}/scripts/beat-align.py" --track "${work}/drone.ogg" --detect \
  >"${work}/drone.json" 2>"${work}/drone.err"; then
  die "a steady drone was accepted as a beat"
fi
grep -qE "no steady beat|no onsets" "${work}/drone.err" || die "refused for the wrong reason"
green "✓ Test 3 passed: refused, with the reason"

echo
echo "Test 4: shot lengths land on whole beats, freeze included..."
cat >"${work}/spec.json" <<'JSON'
{
  "id": "beat_test",
  "width": 1080, "height": 1920, "fps": 60,
  "shots": [
    {"name": "a", "scenario": "s", "start": 0, "duration": 2.9, "slow_motion": 1.0,
     "focus": {"mode": "all"},
     "camera": [{"time": 0, "distance": 10, "pitch": 10, "yaw": 90, "fov": 40}]},
    {"name": "b", "scenario": "s", "start": 5, "duration": 1.1, "slow_motion": 2.0,
     "freeze": 0.3, "freeze_text": "HELD",
     "focus": {"mode": "all"},
     "camera": [{"time": 0, "distance": 10, "pitch": 10, "yaw": 90, "fov": 40}]}
  ]
}
JSON
python3 "${root}/scripts/beat-align.py" "${work}/spec.json" --track "${work}/bed.ogg" \
  --punch-every 2 >/dev/null
python3 - "${work}/spec.json" <<'PY'
import json, sys
spec = json.load(open(sys.argv[1]))
beat = spec["beat"]["beat_seconds"]
for shot in spec["shots"]:
    clip = shot["duration"] * shot.get("slow_motion", 1.0) + shot.get("freeze", 0.0)
    beats = clip / beat
    assert abs(beats - round(beats)) < 1e-3, f"{shot['name']} is {beats} beats long"
    assert shot["punch"], f"{shot['name']} was given no punch marks"
    for mark in shot["punch"]:
        on = mark["at"] / beat
        assert abs(on - round(on)) < 1e-3, f"punch at {mark['at']}s is off the grid"
held = next(s for s in spec["shots"] if s["name"] == "b")
assert abs(held["freeze"] / beat - round(held["freeze"] / beat)) < 1e-3, held
PY
green "✓ Test 4 passed: every cut and punch is on the grid"

echo
echo "===================================="
green "All tests passed!"
