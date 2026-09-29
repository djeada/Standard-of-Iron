#include "ai_force_assignment.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "ai_doctrine_rules.h"
#include "ai_utils.h"

namespace Game::Systems::AI {

namespace {

auto available_combat_role_count(const AISnapshot& snapshot) -> int {
  int count = 0;
  for (const auto& entity : snapshot.friendly_units) {
    if (is_combat_role_unit(entity)) {
      count++;
    }
  }
  return count;
}

auto compute_effective_reserve_units(const AISnapshot& snapshot,
                                     const AIContext& ctx) -> int {
  if (!ctx.anchor_is_structural) {
    return 0;
  }

  const int combat_units = available_combat_role_count(snapshot);
  const int max_reserve =
      std::max(0, combat_units - reactive_attack_size(ctx.strategy_config));
  return std::min(ctx.strategy_config.reserve_units, max_reserve);
}

auto compute_effective_harass_units(const AISnapshot& snapshot,
                                    const AIContext& ctx) -> int {
  if (ctx.strategy_config.harass_units <= 0 ||
      ctx.strategy_config.harassment_range <= 0.0F ||
      holds_garrison(ctx.strategy_config)) {
    return 0;
  }

  const int combat_units = available_combat_role_count(snapshot);
  const int max_harass = std::max(0,
                                  combat_units - ctx.effective_reserve_units -
                                      reactive_attack_size(ctx.strategy_config));
  return std::min(ctx.strategy_config.harass_units, max_harass);
}

using UnitIds = std::vector<Engine::Core::EntityID>;
using UnitRefs = std::vector<const EntitySnapshot*>;

auto surviving_reserve_ids(const AISnapshot& snapshot,
                           const AIContext& ctx) -> UnitIds {
  UnitIds surviving;
  surviving.reserve(static_cast<std::size_t>(ctx.effective_reserve_units));
  for (auto unit_id : ctx.reserve_unit_ids) {
    auto it = std::find_if(snapshot.friendly_units.begin(),
                           snapshot.friendly_units.end(),
                           [unit_id](const EntitySnapshot& entity) {
                             return entity.id == unit_id && is_combat_role_unit(entity);
                           });
    if (it != snapshot.friendly_units.end()) {
      surviving.push_back(unit_id);
    }
    if (static_cast<int>(surviving.size()) >= ctx.effective_reserve_units) {
      break;
    }
  }
  return surviving;
}

auto reserve_candidates(const AISnapshot& snapshot,
                        const AIContext& ctx,
                        const UnitIds& surviving) -> UnitRefs {
  UnitRefs candidates;
  candidates.reserve(snapshot.friendly_units.size());
  for (const auto& entity : snapshot.friendly_units) {
    if (!is_combat_role_unit(entity)) {
      continue;
    }
    if (std::find(surviving.begin(), surviving.end(), entity.id) != surviving.end()) {
      continue;
    }
    candidates.push_back(&entity);
  }

  std::sort(candidates.begin(),
            candidates.end(),
            [&ctx](const EntitySnapshot* a, const EntitySnapshot* b) {
              const float distance_a = distance_squared(a->pos_x,
                                                        a->pos_y,
                                                        a->pos_z,
                                                        ctx.base_pos_x,
                                                        ctx.base_pos_y,
                                                        ctx.base_pos_z);
              const float distance_b = distance_squared(b->pos_x,
                                                        b->pos_y,
                                                        b->pos_z,
                                                        ctx.base_pos_x,
                                                        ctx.base_pos_y,
                                                        ctx.base_pos_z);
              if (distance_a != distance_b) {
                return distance_a < distance_b;
              }
              return a->id < b->id;
            });
  return candidates;
}

auto surviving_harass_ids(const AISnapshot& snapshot, const AIContext& ctx) -> UnitIds {
  UnitIds surviving;
  surviving.reserve(static_cast<std::size_t>(ctx.effective_harass_units));
  for (auto unit_id : ctx.harass_unit_ids) {
    auto it = std::find_if(snapshot.friendly_units.begin(),
                           snapshot.friendly_units.end(),
                           [&](const EntitySnapshot& entity) {
                             return entity.id == unit_id &&
                                    marches_with_the_army(entity) &&
                                    !is_reserved_unit(entity.id, ctx);
                           });
    if (it != snapshot.friendly_units.end()) {
      surviving.push_back(unit_id);
    }
    if (static_cast<int>(surviving.size()) >= ctx.effective_harass_units) {
      break;
    }
  }
  return surviving;
}

auto harass_candidates(const AISnapshot& snapshot,
                       const AIContext& ctx,
                       const UnitIds& surviving) -> UnitRefs {
  UnitRefs candidates;
  candidates.reserve(snapshot.friendly_units.size());
  for (const auto& entity : snapshot.friendly_units) {
    if (!marches_with_the_army(entity) || is_reserved_unit(entity.id, ctx)) {
      continue;
    }
    if (std::find(surviving.begin(), surviving.end(), entity.id) != surviving.end()) {
      continue;
    }
    candidates.push_back(&entity);
  }

  const float assembly_radius_sq =
      ctx.macro_targets.assembly_radius * ctx.macro_targets.assembly_radius;
  std::sort(
      candidates.begin(),
      candidates.end(),
      [&ctx, assembly_radius_sq](const EntitySnapshot* a, const EntitySnapshot* b) {
        const float rally_distance_a = distance_squared(
            a->pos_x, a->pos_y, a->pos_z, ctx.station.x, 0.0F, ctx.station.z);
        const float rally_distance_b = distance_squared(
            b->pos_x, b->pos_y, b->pos_z, ctx.station.x, 0.0F, ctx.station.z);
        const bool a_outside_assembly = rally_distance_a > assembly_radius_sq;
        const bool b_outside_assembly = rally_distance_b > assembly_radius_sq;
        if (a_outside_assembly != b_outside_assembly) {
          return static_cast<int>(a_outside_assembly) >
                 static_cast<int>(b_outside_assembly);
        }

        const float distance_a = distance_squared(a->pos_x,
                                                  a->pos_y,
                                                  a->pos_z,
                                                  ctx.base_pos_x,
                                                  ctx.base_pos_y,
                                                  ctx.base_pos_z);
        const float distance_b = distance_squared(b->pos_x,
                                                  b->pos_y,
                                                  b->pos_z,
                                                  ctx.base_pos_x,
                                                  ctx.base_pos_y,
                                                  ctx.base_pos_z);
        if (distance_a != distance_b) {
          return distance_a > distance_b;
        }
        return a->id < b->id;
      });
  return candidates;
}

void fill_up_to(UnitIds& selected, const UnitRefs& candidates, int wanted) {
  for (const auto* entity : candidates) {
    if (static_cast<int>(selected.size()) >= wanted) {
      break;
    }
    selected.push_back(entity->id);
  }
}

} // namespace

void update_assault_unit_ids(const AISnapshot& snapshot, AIContext& ctx) {
  ctx.assault_unit_ids.clear();
  for (const auto& entity : snapshot.friendly_units) {
    if (entity.is_assault && is_combat_role_unit(entity)) {
      ctx.assault_unit_ids.push_back(entity.id);
    }
  }
  ctx.assault_unit_count = static_cast<int>(ctx.assault_unit_ids.size());
}

void update_reserve_unit_ids(const AISnapshot& snapshot, AIContext& ctx) {
  ctx.effective_reserve_units = compute_effective_reserve_units(snapshot, ctx);
  if (ctx.effective_reserve_units <= 0) {
    ctx.reserve_unit_ids.clear();
    return;
  }

  UnitIds selected = surviving_reserve_ids(snapshot, ctx);
  fill_up_to(selected,
             reserve_candidates(snapshot, ctx, selected),
             ctx.effective_reserve_units);
  ctx.reserve_unit_ids = std::move(selected);
}

void update_harass_unit_ids(const AISnapshot& snapshot, AIContext& ctx) {
  ctx.effective_harass_units = compute_effective_harass_units(snapshot, ctx);
  if (ctx.effective_harass_units <= 0) {
    ctx.harass_unit_ids.clear();
    return;
  }

  UnitIds selected = surviving_harass_ids(snapshot, ctx);
  fill_up_to(
      selected, harass_candidates(snapshot, ctx, selected), ctx.effective_harass_units);
  ctx.harass_unit_ids = std::move(selected);
}

auto committed_army_count(const AISnapshot& snapshot, const AIContext& ctx) -> int {
  if (!ctx.has_base_anchor) {
    return 0;
  }

  if (!ctx.anchor_is_structural) {
    return std::max(0, ctx.total_units - ctx.builder_count);
  }

  const float assembly_radius_sq =
      ctx.macro_targets.assembly_radius * ctx.macro_targets.assembly_radius;
  int committed_units = 0;
  for (const auto& entity : snapshot.friendly_units) {
    if (entity.is_building || entity.spawn_type == Game::Units::SpawnType::Builder) {
      continue;
    }
    if (is_harass_unit(entity.id, ctx)) {
      continue;
    }

    const float dx = entity.pos_x - ctx.station.x;
    const float dz = entity.pos_z - ctx.station.z;
    const float dist_sq = dx * dx + dz * dz;
    if (dist_sq <= assembly_radius_sq ||
        is_entity_engaged(entity, snapshot.visible_enemies)) {
      committed_units++;
    }
  }

  return committed_units;
}

} // namespace Game::Systems::AI
