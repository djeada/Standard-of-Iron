#include "unit_layout_spacing.h"

#include <algorithm>

#include "../../game/units/troop_config.h"
#include "../horse/dimensions.h"

namespace Render::GL {

auto cavalry_formation_spacing(float mount_scale) -> float {
  HorseDimensions dims = make_horse_dimensions(0U);
  scale_horse_dimensions(dims, std::max(0.1F, mount_scale));
  float const horse_length =
      dims.body_length + dims.head_length * 0.85F + dims.tail_length * 0.10F;
  return horse_length * 1.10F;
}

auto resolve_formation_spacing(Game::Units::SpawnType spawn_type,
                               float configured_spacing,
                               float mount_scale) -> float {
  switch (spawn_type) {
  case Game::Units::SpawnType::MountedKnight:
  case Game::Units::SpawnType::HorseArcher:
  case Game::Units::SpawnType::HorseSpearman:
    return cavalry_formation_spacing(mount_scale);
  default:
    break;
  }

  if (configured_spacing > 0.0F) {
    return configured_spacing;
  }
  return Game::Units::TroopConfig::instance().get_formation_spacing(spawn_type);
}

} // namespace Render::GL
