#pragma once

#include "../../humanoid/humanoid_renderer_base.h"
#include "../../palette.h"
#include "../i_equipment_renderer.h"
#include "sword_renderer.h"

namespace Render::GL {

class RomanSwordRenderer : public SwordRenderer {
public:
  RomanSwordRenderer();
};

} // namespace Render::GL
