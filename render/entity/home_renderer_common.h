#pragma once

#include <QVector3D>

#include <array>
#include <span>
#include <string_view>

#include "building_render_common.h"
#include "building_state.h"
#include "building_torches.h"
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
  std::span<const TorchMount> torches{};
};

class BuildingArchetypeDesc;

struct HomeYardStyle {
  float plinth_half{1.18F};
  float wall_half{0.96F};
  float door_half_width{0.60F};
  QVector3D vent{0.0F, 1.66F, -0.42F};
  float chimney_base_y{1.30F};
  bool clay_oven_chimney{false};
  QVector3D stone{0.62F, 0.58F, 0.52F};
  QVector3D clay{0.66F, 0.40F, 0.24F};
  QVector3D clay_dark{0.46F, 0.25F, 0.15F};
  QVector3D chimney{0.58F, 0.54F, 0.49F};
  int seed{1};
};

void add_home_yard(BuildingArchetypeDesc& desc, const HomeYardStyle& style);

void register_home_renderer_variant(EntityRendererRegistry& registry,
                                    const HomeRendererConfig& config);

} // namespace Render::GL
