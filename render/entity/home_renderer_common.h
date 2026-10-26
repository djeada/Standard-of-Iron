#pragma once

#include <QVector3D>

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include "building_render_common.h"
#include "building_state.h"
#include "building_torches.h"
#include "registry.h"
#include "render/render_archetype.h"

namespace Engine::Core {
class Entity;
}

namespace Render::GL {

inline constexpr int k_home_visual_variant_count = 6;
auto home_visual_variant(std::uint32_t seed) -> int;
using HomeArchetypeResolver = const RenderArchetype& (*)(BuildingState, int);

[[nodiscard]] auto home_placement_seed(float world_x,
                                       float world_z,
                                       std::uint32_t fallback) -> std::uint32_t;

[[nodiscard]] auto home_tint_pick(std::uint32_t seed, std::size_t count) -> std::size_t;
[[nodiscard]] auto home_tint_jitter(std::uint32_t seed) -> float;

inline constexpr std::size_t k_home_palette_slots = 2;
using HomePaletteSlotsResolver = std::array<QVector3D, k_home_palette_slots> (*)(
    const QVector3D& team, std::uint32_t seed, BuildingState state);

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

  int variant{0};
  float wall_foot_y{0.16F};
  float wall_face{0.96F};
  float wall_foot_door_gap{0.30F};
  QVector3D wall_foot{0.45F, 0.40F, 0.33F};
};

void add_home_yard(BuildingArchetypeDesc& desc, const HomeYardStyle& style);

void register_home_renderer_variant(EntityRendererRegistry& registry,
                                    const HomeRendererConfig& config);

} // namespace Render::GL
