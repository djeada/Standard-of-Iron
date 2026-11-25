#pragma once

#include <QVector3D>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "render/entity/building_state.h"
#include "render/entity/tenement_layout.h"
#include "render/entity/town_palette.h"

namespace Render::GL {

class EntityRendererRegistry;

struct TenementRendererConfig {
  std::string_view nation_slug;

  std::string_view archetype_prefix;
  TownPaletteResolver palette{nullptr};
  QVector3D (*vent)(int variant);
  bool carthage{false};
};

void register_tenement_renderer_variant(EntityRendererRegistry& registry,
                                        const TenementRendererConfig& config);

} // namespace Render::GL
