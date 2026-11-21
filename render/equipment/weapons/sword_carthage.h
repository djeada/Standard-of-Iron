#pragma once

#include "render/creature/humanoid/runtime/humanoid_renderer.h"
#include "render/equipment/i_equipment_renderer.h"
#include "render/equipment/weapons/sword_renderer.h"
#include "render/frame/palette.h"

namespace Render::GL {

class CarthageSwordRenderer : public SwordRenderer {
public:
  CarthageSwordRenderer();
};

} // namespace Render::GL
