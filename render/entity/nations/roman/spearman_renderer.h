#pragma once

#include "../../registry.h"
#include "spearman_style.h"
#include <string>

namespace Render::GL::Roman {

struct SpearmanStyleConfig;

void register_spearman_style(const std::string &nation_id,
                             const SpearmanStyleConfig &style);

void registerSpearmanRenderer(EntityRendererRegistry &registry);

} // namespace Render::GL::Roman
