#pragma once

#include <vector>

#include "army_formation_types.h"
#include "formation_doctrine.h"

namespace Game::Formation::planning {

struct SlotExtents {
  std::vector<float> half_width;
  std::vector<float> half_depth;

  std::vector<RoleTagSet> roles;
  std::vector<int> troop;
  std::vector<unsigned char> allied;
};

struct SilhouetteParams {
  ArmyFormationIntent intent{ArmyFormationIntent::FactionDefault};
  float frontage{0.0F};
  float spacing{1.0F};
  const ArmyFormationOptions& options;
  const DoctrineIntentTemplate& tmpl;
};

void regularize_silhouette(std::vector<FormationSlot>& slot_list,
                           const SlotExtents& extents,
                           const SilhouetteParams& params);

} // namespace Game::Formation::planning
