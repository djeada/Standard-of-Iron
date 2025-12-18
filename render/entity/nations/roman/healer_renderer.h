#pragma once

#include "../../registry.h"
#include <string>

namespace Render::GL::Roman {

struct HealerStyleConfig;

void register_healer_style(const std::string &nation_id,
                           const HealerStyleConfig &style);

void register_healer_renderer(EntityRendererRegistry &registry);

} // namespace Render::GL::Roman
