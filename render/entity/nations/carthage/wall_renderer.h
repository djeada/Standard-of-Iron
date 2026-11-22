#pragma once

#include "render/entity/nations/carthage/wall_authoring.h"
#include "render/entity/registry.h"
#include "render/entity/wall_renderer_common.h"

namespace Render::GL::Carthage {

void register_wall_renderer(EntityRendererRegistry& registry);

} // namespace Render::GL::Carthage
