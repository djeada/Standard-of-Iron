#pragma once

#include <QVector3D>

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include "render/entity/building_render_common.h"
#include "render/entity/building_state.h"
#include "render/entity/building_torches.h"
#include "render/entity/home_yard.h"
#include "render/entity/registry.h"
#include "render/frame/render_archetype.h"

namespace Engine::Core {
class Entity;
}

namespace Render::GL {

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

void register_home_renderer_variant(EntityRendererRegistry& registry,
                                    const HomeRendererConfig& config);

} // namespace Render::GL
