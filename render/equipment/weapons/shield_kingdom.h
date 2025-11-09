#pragma once

#include "../../humanoid/rig.h"
#include "../../palette.h"
#include "../i_equipment_renderer.h"
#include "shield_renderer.h"

namespace Render::GL {

class KingdomShieldRenderer : public ShieldRenderer {
public:
  KingdomShieldRenderer();
};

} // namespace Render::GL
