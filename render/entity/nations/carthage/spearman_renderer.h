#pragma once

#include <string>

#include "render/entity/nations/carthage/spearman_style.h"
#include "render/entity/registry.h"

namespace Render::GL::Carthage {

void register_spearman_style(const std::string& nation_id,
                             const SpearmanStyleConfig& style);

void register_spearman_renderer(EntityRendererRegistry& registry);

} // namespace Render::GL::Carthage
