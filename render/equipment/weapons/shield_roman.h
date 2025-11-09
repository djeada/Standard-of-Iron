#pragma once

#include "../../humanoid/rig.h"
#include "../../palette.h"
#include "../i_equipment_renderer.h"
#include "shield_renderer.h"

namespace Render::GL {

class RomanShieldRenderer : public ShieldRenderer {
public:
  RomanShieldRenderer();
};

} // namespace Render::GL
