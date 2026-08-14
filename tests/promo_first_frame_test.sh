#!/bin/bash
# Frame zero of a published reel is the social-media thumbnail. This asserts the
# two properties that keep it from being black, against the real promo specs and
# the real scripts/promo-edit.py.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PROMO_EDIT="$REPO_ROOT/scripts/promo-edit.py"
TRAILER_SPEC="$REPO_ROOT/tools/arena/promos/trailer.json"

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

echo "Promo first-frame tests"
echo "======================="
echo ""

if ! command -v ffmpeg >/dev/null 2>&1; then
  echo -e "${RED}✗ ffmpeg is required${NC}"
  exit 1
fi

TEMP_DIR=$(mktemp -d)
trap 'rm -rf "$TEMP_DIR"' EXIT
CLIPS="$TEMP_DIR/clips"
mkdir -p "$CLIPS"

# Stand-in footage. The point of the test is the editorial layer the script
# puts in front of the footage, so a flat mid-grey clip is enough and keeps a
# 1080p encode down to a couple of seconds. It is deliberately not black.
for index in 1 2; do
  ffmpeg -hide_banner -loglevel error -y \
    -f lavfi -i "color=c=0x6a6a6a:size=1920x1080:rate=60:duration=3" \
    -c:v libx264 -preset ultrafast -crf 24 -pix_fmt yuv420p \
    "$CLIPS/0${index}_shot.mp4"
done

python3 - "$CLIPS" <<'EOF'
import json
import sys
from pathlib import Path

clips = Path(sys.argv[1])
(clips / "shots.json").write_text(
    json.dumps(
        {
            "width": 1920,
            "height": 1080,
            "fps": 60,
            "shots": [
                {"name": "card_open", "clip": "01_shot.mp4", "clip_seconds": 3.0},
                {"name": "valley_reveal", "clip": "02_shot.mp4", "clip_seconds": 3.0},
            ],
        }
    )
)
EOF

# The spec drives the opening card, so the test reads the shipped one rather
# than a copy that could drift away from it. Music and sound effects are
# dropped because this asserts on picture and their assets are derived.
python3 - "$TRAILER_SPEC" "$TEMP_DIR/trailer.json" <<'EOF'
import json
import sys
from pathlib import Path

spec = json.loads(Path(sys.argv[1]).read_text())
spec["shots"] = spec["shots"][:2]
spec.pop("music", None)
spec.pop("sfx", None)
Path(sys.argv[2]).write_text(json.dumps(spec))
EOF

read_first_frame() {
  ffmpeg -hide_banner -loglevel error -i "$1" -frames:v 1 \
    -f rawvideo -pix_fmt gray - 2>/dev/null |
    python3 -c "
import sys
data = sys.stdin.buffer.read()
visible = sum(1 for sample in data if sample >= 32)
print(f'{max(data)} {visible / len(data) * 100.0:.4f}')
"
}

echo "Test 1: the shipped trailer spec opens on a readable frame..."
if ! python3 "$PROMO_EDIT" --spec "$TEMP_DIR/trailer.json" --clips "$CLIPS" \
  --out "$TEMP_DIR/trailer.mp4" --allow-flashes >"$TEMP_DIR/edit.log" 2>&1; then
  echo -e "${RED}✗ Test 1 failed: promo-edit.py refused the shipped spec${NC}"
  cat "$TEMP_DIR/edit.log"
  exit 1
fi
if [ ! -f "$TEMP_DIR/trailer.mp4" ]; then
  echo -e "${RED}✗ Test 1 failed: no output was published${NC}"
  exit 1
fi
STATS=$(read_first_frame "$TEMP_DIR/trailer.mp4")
PEAK=$(echo "$STATS" | cut -d' ' -f1)
VISIBLE=$(echo "$STATS" | cut -d' ' -f2)
if [ "$PEAK" -lt 8 ]; then
  echo -e "${RED}✗ Test 1 failed: frame zero peak luma is $PEAK${NC}"
  exit 1
fi
echo -e "${GREEN}✓ Test 1 passed: frame zero peak $PEAK, ${VISIBLE}% visible${NC}"
echo ""

# The card is what regressed before: it paints the frame black for its whole
# hold, so its text has to be at full opacity on the very first frame. A
# fade-in there is invisible in review and is a black thumbnail on upload.
echo "Test 2: the opening card carries its text on frame zero..."
if ! grep -q "advisory" "$TRAILER_SPEC"; then
  echo -e "${RED}✗ Test 2 failed: the trailer spec no longer has an advisory${NC}"
  exit 1
fi
CARD_VISIBLE=$(python3 -c "print(1 if float('$VISIBLE') >= 0.1 else 0)")
if [ "$CARD_VISIBLE" -ne 1 ]; then
  echo -e "${RED}✗ Test 2 failed: only ${VISIBLE}% of frame zero is above the"
  echo -e "  visibility floor, so the card is fading up from black${NC}"
  exit 1
fi
echo -e "${GREEN}✓ Test 2 passed: the card is legible on frame zero${NC}"
echo ""

# A check that only reports is not a guarantee. Publishing a black-opening cut
# and returning non-zero still leaves an uploadable file under the name the
# upload step expects, which is how a black frame reached social media before.
echo "Test 3: a black opening is refused and not left where it publishes..."
python3 - "$TEMP_DIR/trailer.json" "$TEMP_DIR/black.json" <<'EOF'
import json
import sys
from pathlib import Path

spec = json.loads(Path(sys.argv[1]).read_text())
spec["advisory"] = ""
spec["shots"][0].pop("act_title", None)
Path(sys.argv[2]).write_text(json.dumps(spec))
EOF
ffmpeg -hide_banner -loglevel error -y \
  -f lavfi -i "color=c=black:size=1920x1080:rate=60:duration=3" \
  -c:v libx264 -preset ultrafast -crf 24 -pix_fmt yuv420p \
  "$CLIPS/01_shot.mp4"
rm -f "$TEMP_DIR/black.mp4" "$TEMP_DIR/black.rejected.mp4"
if python3 "$PROMO_EDIT" --spec "$TEMP_DIR/black.json" --clips "$CLIPS" \
  --out "$TEMP_DIR/black.mp4" --allow-flashes >"$TEMP_DIR/black.log" 2>&1; then
  echo -e "${RED}✗ Test 3 failed: a black opening was published${NC}"
  exit 1
fi
if [ -f "$TEMP_DIR/black.mp4" ]; then
  echo -e "${RED}✗ Test 3 failed: the rejected cut was left at black.mp4,"
  echo -e "  where an upload step would find it${NC}"
  exit 1
fi
if [ ! -f "$TEMP_DIR/black.rejected.mp4" ]; then
  echo -e "${RED}✗ Test 3 failed: the rejected cut was not kept for inspection${NC}"
  exit 1
fi
if ! grep -q "frame zero is black" "$TEMP_DIR/black.log"; then
  echo -e "${RED}✗ Test 3 failed: refused for the wrong reason${NC}"
  cat "$TEMP_DIR/black.log"
  exit 1
fi
echo -e "${GREEN}✓ Test 3 passed: refused, quarantined, nothing left to upload${NC}"
echo ""

# A stale reel under the published name is as dangerous as a black one: the
# upload step cannot tell that the run which should have replaced it failed.
echo "Test 4: a failed re-cut removes the previous publish..."
printf 'stale' >"$TEMP_DIR/black.mp4"
if python3 "$PROMO_EDIT" --spec "$TEMP_DIR/black.json" --clips "$CLIPS" \
  --out "$TEMP_DIR/black.mp4" --allow-flashes >/dev/null 2>&1; then
  echo -e "${RED}✗ Test 4 failed: a black opening was published${NC}"
  exit 1
fi
if [ -f "$TEMP_DIR/black.mp4" ]; then
  echo -e "${RED}✗ Test 4 failed: the stale reel survived a failed re-cut${NC}"
  exit 1
fi
echo -e "${GREEN}✓ Test 4 passed: the stale reel was removed${NC}"
echo ""

echo "===================================="
echo -e "${GREEN}All tests passed!${NC}"
