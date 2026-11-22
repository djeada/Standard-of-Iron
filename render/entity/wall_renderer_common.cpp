#include "render/entity/wall_renderer_common.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <string>

#include "game/core/component_gameplay.h"
#include "game/core/wall_walk_geometry.h"
#include "game/core/world.h"
#include "render/entity/building_archetype_library.h"
#include "render/entity/building_decay.h"

namespace Render::GL {

namespace WW = Game::Systems::WallWalk;

namespace {

constexpr std::uint8_t k_connection_north = 1U << 0U;
constexpr std::uint8_t k_connection_east = 1U << 1U;
constexpr std::uint8_t k_connection_south = 1U << 2U;
constexpr std::uint8_t k_connection_west = 1U << 3U;

constexpr std::size_t k_dir_count = 4U;

struct PlanarDir {
  float x{0.0F};
  float z{0.0F};
};

constexpr std::array<PlanarDir, k_dir_count> k_directions{PlanarDir{0.0F, -1.0F},
                                                          PlanarDir{1.0F, 0.0F},
                                                          PlanarDir{0.0F, 1.0F},
                                                          PlanarDir{-1.0F, 0.0F}};

constexpr std::array<std::uint8_t, k_dir_count> k_direction_bits{
    k_connection_north, k_connection_east, k_connection_south, k_connection_west};

auto wall_archetype_index(WallVariant variant) -> std::size_t {
  return static_cast<std::size_t>(variant);
}

void submit_walk_piece(ISubmitter& out,
                       const DrawContext& ctx,
                       const BuildingArchetypeSet* piece,
                       const QVector3D& origin,
                       float nx,
                       float nz,
                       const QVector3D& offset) {
  if (piece == nullptr) {
    return;
  }
  DrawContext placed = ctx;
  QMatrix4x4 model;
  model.translate(origin + offset);
  model.rotate(
      std::atan2(nx, nz) * 180.0F / std::numbers::pi_v<float>, 0.0F, 1.0F, 0.0F);
  placed.model = model;
  submit_building_instance(out, placed, piece->for_state(resolve_building_state(ctx)));
}

void submit_wall_walk(ISubmitter& out,
                      const DrawContext& ctx,
                      const WallArchetypeSet& archetypes) {
  if (ctx.entity == nullptr || ctx.world == nullptr ||
      resolve_building_state(ctx) == BuildingState::Destroyed) {
    return;
  }
  const auto id = ctx.entity->get_id();
  const auto* wall = ctx.world->try_get<Engine::Core::WallSegmentComponent>(id);
  if (wall == nullptr || (wall->inner_x == 0 && wall->inner_z == 0) ||
      ctx.world->has<Engine::Core::WallConstructionSiteComponent>(id)) {
    return;
  }
  const QVector3D origin = ctx.model.column(3).toVector3D();
  const auto ix = static_cast<float>(wall->inner_x);
  const auto iz = static_cast<float>(wall->inner_z);
  const std::uint8_t mask = wall->connection_mask;
  const int connections = ((mask & k_connection_north) != 0U ? 1 : 0) +
                          ((mask & k_connection_east) != 0U ? 1 : 0) +
                          ((mask & k_connection_south) != 0U ? 1 : 0) +
                          ((mask & k_connection_west) != 0U ? 1 : 0);
  for (std::size_t dir = 0; dir < k_dir_count; ++dir) {
    const auto& d = k_directions[dir];
    const std::size_t opposite = (dir + 2U) % k_dir_count;
    const bool connected = (mask & k_direction_bits[dir]) != 0U;
    const bool open_end = connections == 1 && (mask & k_direction_bits[opposite]) != 0U;
    const bool isolated_run =
        connections == 0 && (d.x != 0.0F ? wall->inner_z != 0 : wall->inner_x != 0);
    if (!connected && !open_end && !isolated_run) {
      continue;
    }
    const float nx = d.x != 0.0F ? 0.0F : ix;
    const float nz = d.x != 0.0F ? iz : 0.0F;
    if (nx == 0.0F && nz == 0.0F) {
      continue;
    }
    submit_walk_piece(out,
                      ctx,
                      archetypes.walk_span,
                      origin,
                      nx,
                      nz,
                      QVector3D(d.x * 0.5F, 0.0F, d.z * 0.5F));
  }
  if (wall->inner_x != 0 && wall->inner_z != 0) {
    const float lane = (WW::k_deck_inner_edge + WW::k_deck_outer_edge) * 0.5F;
    submit_walk_piece(out,
                      ctx,
                      archetypes.walk_landing,
                      origin,
                      ix,
                      iz,
                      QVector3D(ix * lane, 0.0F, iz * lane));
  }
  if (wall->has_stair) {
    submit_walk_piece(
        out, ctx, archetypes.walk_stair, origin, ix, iz, QVector3D(0.0F, 0.0F, 0.0F));
  }
}

} // namespace

void submit_wall_ladder(ISubmitter& out,
                        const DrawContext& ctx,
                        const BuildingArchetypeSet& archetype) {
  submit_building_instance(out, ctx, archetype.for_state(resolve_building_state(ctx)));
  draw_building_selection_overlay(out, ctx, BuildingSelectionStyle{0.7F, 1.1F});
}

auto wall_archetype_set(std::string_view name_prefix) -> WallArchetypeSet {
  WallArchetypeSet out{};

  for (int i = 0; i < static_cast<int>(out.variants.size()); ++i) {
    out.variants[static_cast<std::size_t>(i)] =
        &building_archetype_set(std::string(name_prefix) + "_" + std::to_string(i));
  }
  out.walk_span = &building_archetype_set(std::string(name_prefix) + "_walk_span");
  out.walk_landing =
      &building_archetype_set(std::string(name_prefix) + "_walk_landing");
  out.walk_stair = &building_archetype_set(std::string(name_prefix) + "_walk_stair");

  return out;
}

void submit_wall_segment_variant(ISubmitter& out,
                                 const DrawContext& ctx,
                                 const WallArchetypeSet& archetypes,
                                 WallVariant variant) {
  submit_building_instance(
      out,
      ctx,
      archetypes.variants[wall_archetype_index(variant)]->for_state(
          resolve_building_state(ctx)));
  submit_wall_walk(out, ctx, archetypes);
  draw_building_selection_overlay(out, ctx, BuildingSelectionStyle{2.0F, 2.0F});
}

} // namespace Render::GL
