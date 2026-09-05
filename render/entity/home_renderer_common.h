#pragma once

#include <QVector3D>

#include <array>
#include <string_view>

#include "building_render_common.h"
#include "building_state.h"
#include "registry.h"
#include "render/render_archetype.h"

namespace Render::GL {

using HomeArchetypeResolver = const RenderArchetype& (*)(BuildingState);

inline constexpr std::size_t k_home_palette_slots = 2;
using HomePaletteSlotsResolver =
    std::array<QVector3D, k_home_palette_slots> (*)(const QVector3D&);

struct HomeRendererConfig {
  std::string_view nation_slug;
  HomeArchetypeResolver archetype;
  HomePaletteSlotsResolver palette_slots;
  BuildingSelectionStyle selection;
};

void register_home_renderer_variant(EntityRendererRegistry& registry,
                                    const HomeRendererConfig& config);

} // namespace Render::GL
