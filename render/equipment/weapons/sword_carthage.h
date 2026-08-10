#pragma once

#include "render/equipment/i_equipment_renderer.h"
#include "render/humanoid/humanoid_renderer_base.h"
#include "render/palette.h"
#include "sword_renderer.h"

namespace Render::GL {

class CarthageSwordRenderer : public SwordRenderer {
public:
  CarthageSwordRenderer();
};

} // namespace Render::GL
