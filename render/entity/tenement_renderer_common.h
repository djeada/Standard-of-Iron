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

// What a nation supplies to have its tenements drawn: the catalog names of its
// baked variants, the colours an instance takes, and where its hearth vents.
struct TenementRendererConfig {
  std::string_view nation_slug;
  // "<prefix><variant>" names the baked archetype set of a variant.
  std::string_view archetype_prefix;
  TownPaletteResolver palette{nullptr};
  QVector3D (*vent)(int variant);
  bool carthage{false};
};

// Registers the renderer for a nation's home in the tenement style.
void register_tenement_renderer_variant(EntityRendererRegistry& registry,
                                        const TenementRendererConfig& config);

} // namespace Render::GL
