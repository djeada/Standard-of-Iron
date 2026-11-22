#include "render/entity/home_renderer_common.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "game/core/component_core.h"
#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_decay.h"
#include "render/entity/home_activity_submit.h"
#include "render/entity/home_props.h"
#include "render/frame/entity_appearance.h"

namespace Render::GL {

auto home_placement_seed(float world_x,
                         float world_z,
                         std::uint32_t fallback) -> std::uint32_t {
  if (!std::isfinite(world_x) || !std::isfinite(world_z)) {
    return fallback;
  }
  auto const qx = static_cast<std::uint32_t>(
      static_cast<std::int32_t>(std::lround(world_x * 2.0F)));
  auto const qz = static_cast<std::uint32_t>(
      static_cast<std::int32_t>(std::lround(world_z * 2.0F)));
  std::uint32_t seed = (qx * 0x9E3779B1U) ^ (qz * 0x85EBCA77U) ^ 0x27D4EB2FU;
  seed ^= seed >> 13U;
  seed *= 0xC2B2AE3DU;
  seed ^= seed >> 16U;

  constexpr float k_lot_pitch = 5.0F;
  auto const lot_x = static_cast<std::int32_t>(std::floor(world_x / k_lot_pitch));
  auto const lot_z = static_cast<std::int32_t>(std::floor(world_z / k_lot_pitch));
  auto const parity = static_cast<std::uint32_t>(lot_x + lot_z) & 1U;
  return (seed & ~1U) | parity;
}

auto home_tint_pick(std::uint32_t seed, std::size_t count) -> std::size_t {
  if (count == 0U) {
    return 0U;
  }
  std::uint32_t mixed = seed * 0x2C1B3C6DU;
  mixed ^= mixed >> 12U;
  mixed *= 0x297A2D39U;
  mixed ^= mixed >> 15U;
  return static_cast<std::size_t>(mixed % static_cast<std::uint32_t>(count));
}

auto home_tint_jitter(std::uint32_t seed) -> float {
  std::uint32_t mixed = (seed ^ 0x5BD1E995U) * 0x1B873593U;
  mixed ^= mixed >> 14U;
  return (static_cast<float>(mixed & 0xFFFFU) / 65535.0F) - 0.5F;
}

void register_home_renderer_variant(EntityRendererRegistry& registry,
                                    const HomeRendererConfig& config) {

  static const bool props_registered = [] {
    register_home_prop_archetypes();
    return true;
  }();
  (void)props_registered;
  register_building_renderer(
      registry,
      config.nation_slug,
      "home",
      [config](const DrawContext& ctx, ISubmitter& out) {
        if (ctx.entity == nullptr) {
          return;
        }

        auto* r = ctx.entity->get_component<Engine::Core::RenderableComponent>();
        if (r == nullptr) {
          return;
        }

        const QVector3D team = Render::entity_color(*ctx.entity);
        const std::uint32_t seed =
            home_placement_seed(ctx.model(0, 3),
                                ctx.model(2, 3),
                                static_cast<std::uint32_t>(ctx.entity->get_id()));
        const BuildingState state = resolve_building_state(ctx);
        const auto palette_slots = config.palette_slots(team, seed, state);
        submit_building_instance(out,
                                 ctx,
                                 config.archetype(state, home_visual_variant(seed)),
                                 palette_slots);
        submit_home_activity(ctx, out, config.nation_slug == "carthage");
        submit_building_torches(ctx, out, config.torches);
        draw_building_selection_overlay(out, ctx, config.selection);
      });
}

} // namespace Render::GL
