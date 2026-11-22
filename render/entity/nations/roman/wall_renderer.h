#pragma once

#include "render/entity/nations/roman/wall_authoring.h"
#include "render/entity/registry.h"
#include "render/entity/wall_renderer_common.h"

namespace Render::GL::Roman {

void register_wall_renderer(EntityRendererRegistry& registry);

} // namespace Render::GL::Roman
