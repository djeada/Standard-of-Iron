#pragma once

#include "../../game/core/entity.h"
#include "../gl/humanoid/humanoid_types.h"
#include "formation_calculator.h"
#include "prepare.h"

#include <cstdint>
#include <vector>

namespace Render::Humanoid {

struct HumanoidLayoutCacheComponent : public Engine::Core::Component {
  std::vector<SoldierLayout> soldiers;
  Render::GL::FormationParams formation{};
  Render::GL::FormationCalculatorFactory::Nation nation{
      Render::GL::FormationCalculatorFactory::Nation::Roman};
  Render::GL::FormationCalculatorFactory::UnitCategory category{
      Render::GL::FormationCalculatorFactory::UnitCategory::Infantry};
  int rows{0};
  int cols{0};
  std::uint32_t layout_version{0};
  std::uint32_t seed{0};
  std::uint32_t frame_number{0};
  bool valid{false};
};

} // namespace Render::Humanoid
