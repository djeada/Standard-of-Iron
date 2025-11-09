#pragma once

#include "../../humanoid/rig.h"
#include "../../palette.h"
#include "../i_equipment_renderer.h"
#include "shield_renderer.h"

namespace Render::GL {

class CarthageShieldRenderer : public ShieldRenderer {
public:
  CarthageShieldRenderer();
};

} // namespace Render::GL
