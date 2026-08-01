#!/usr/bin/env bash
# Rebuild assets/creatures/elephant/elephant.cmesh from the committed export.
#
# The authored export ships one eye and sinks it into the head, so the runtime
# used to mirror and patch it while the game was running. Both repairs now
# happen here instead, offline, and the result is what gets committed. Re-run
# after `git checkout` of the .cmesh to reproduce the shipped asset.
#
# Usage: tools/mesh_preview/build_elephant_asset.sh [EYE_RADIUS] [EYE_TARGET]
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
package="$here/../../assets/creatures/elephant/elephant.cmesh"
eye_radius="${1:-0.11}"
# Cheek position, above the tusk root and forward of the ear.
eye_target="${2:-0.80,1.10,3.30}"

skin=elephant_production_material_0
eyes=elephant_production_material_2

# The model is mirror-symmetric about x=0.03, not x=0: the tusks pair about that
# plane to within 7e-5. Mirroring about 0 would sit the second eye off-centre.
python3 "$here/complete_creature_mirror.py" "$package" \
    --material "$eyes" --plane 0.03

# The authored eyes are large flat wedges half sunk into the head; replace them
# with discs laid against the actual skin surface. --min-z keeps the tail tuft,
# which shares the eye material, out of it.
python3 "$here/author_creature_eyes.py" "$package" \
    --detail "$eyes" --body "$skin" \
    --min-z 0 --radius "$eye_radius" --margin 0.03 --lateral-plane 0.03 \
    --target "$eye_target"

printf '\nsha256 for k_elephant_config: %s\n' \
    "$(sha256sum "$package" | cut -d' ' -f1)"
