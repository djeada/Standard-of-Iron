#include "formation_instance_layout.h"

#include "animation/layout_manifest.h"
#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/formation/unit_layout_state_system.h"
#include "game/systems/formation_combat_geometry.h"

namespace Render::Entity {

auto build_formation_instance(const FormationInstanceRequest& request)
    -> FormationInstance {
  FormationInstance instance{};
  auto const policy = Animation::resolve_soldier_layout_policy({
      .idx = request.idx,
      .row = request.row,
      .col = request.col,
      .rows = request.rows,
      .cols = request.cols,
      .formation_spacing = request.formation_spacing,
      .seed = request.seed,
      .force_single_soldier = request.force_single_soldier,
      .melee_attack = request.melee_attack,
      .sample_time = request.animation_time,
  });
  instance.row_index = policy.row_index;
  instance.col_index = policy.col_index;
  instance.rank_band = policy.rank_band;
  instance.inst_seed = policy.inst_seed;
  instance.vertical_jitter = policy.vertical_jitter;
  instance.individuality = policy.individuality;

  if (!request.force_single_soldier) {
    Game::Formation::UnitLayoutQuery query;
    query.layout = request.unit_layout;
    query.index = request.idx;
    query.row = request.row;
    query.col = request.col;
    query.rows = request.rows;
    query.cols = request.cols;
    query.count = request.count;
    query.spacing = request.formation_spacing;
    query.seed = request.seed;
    query.formed_ratio = request.formed_ratio;
    query.blend_from = request.blend_from;
    query.blend_ratio = request.blend_ratio;
    if (request.soldier_offsets != nullptr) {
      auto const formation_offset = request.soldier_offsets->offset(query);
      instance.offset_x = formation_offset.offset_x;
      instance.offset_z = formation_offset.offset_z;
      instance.yaw_offset = formation_offset.yaw_offset;
    }
  }

  instance.offset_x += policy.offset_x_delta;
  instance.offset_z += policy.offset_z_delta;
  instance.yaw_offset += policy.yaw_delta;

  return instance;
}

auto resolve_formation_instances(FormationLayoutCache* cache,
                                 std::vector<FormationInstance>& scratch,
                                 const FormationLayoutRequest& request)
    -> FormationLayoutResult {
  FormationLayoutResult result;
  result.instances = &scratch;

  if (cache != nullptr && cache->valid) {
    result.preserve_state_prefix =
        cache->seed == request.seed && cache->rows == request.rows &&
        cache->cols == request.cols &&
        cache->layout_version == request.layout_version &&
        cache->formation.individuals_per_unit ==
            request.formation.individuals_per_unit &&
        cache->formation.max_per_row == request.formation.max_per_row &&
        cache->formation.spacing == request.formation.spacing &&
        cache->unit_layout == request.unit_layout &&
        cache->blend_from == request.blend_from &&
        cache->blend_ratio == request.blend_ratio;
    bool const matches =
        result.preserve_state_prefix &&
        cache->instances.size() == static_cast<std::size_t>(request.total_count);
    bool const cache_valid =
        matches && (request.frame_index - cache->frame_number) <= request.max_cache_age;
    if (cache_valid) {
      cache->frame_number = request.frame_index;
      result.instances = &cache->instances;
      result.reused_cache = true;
      return result;
    }
  }

  result.instances = cache != nullptr ? &cache->instances : &scratch;
  auto& generated = *result.instances;
  generated.clear();
  generated.reserve(static_cast<std::size_t>(request.total_count));
  for (int idx = 0; idx < request.total_count; ++idx) {
    auto const slot =
        Game::Formation::rank_slot_for(idx, request.total_count, request.cols);
    FormationInstanceRequest instance_request{};
    instance_request.idx = idx;
    instance_request.row = request.force_single_soldier ? request.rows - 1 : slot.row;
    instance_request.col = slot.col;
    instance_request.rows = request.rows;
    instance_request.cols = request.cols;
    instance_request.count = request.total_count;
    instance_request.formation_spacing = request.formation.spacing;
    instance_request.seed = request.seed;
    instance_request.force_single_soldier = request.force_single_soldier;
    instance_request.melee_attack = request.melee_attack;
    instance_request.animation_time = request.animation_time;
    instance_request.unit_layout = request.unit_layout;
    instance_request.formed_ratio = request.formed_ratio;
    instance_request.blend_from = request.blend_from;
    instance_request.blend_ratio = request.blend_ratio;
    instance_request.soldier_offsets = request.soldier_offsets;
    generated.push_back(build_formation_instance(instance_request));
  }

  if (cache != nullptr) {
    cache->formation = request.formation;
    cache->unit_layout = request.unit_layout;
    cache->blend_from = request.blend_from;
    cache->blend_ratio = request.blend_ratio;
    cache->rows = request.rows;
    cache->cols = request.cols;
    cache->layout_version = request.layout_version;
    cache->seed = request.seed;
    cache->frame_number = request.frame_index;
    cache->valid = true;
  }
  return result;
}

void apply_authoritative_formation_slots(
    std::span<FormationInstance> instances,
    const Engine::Core::FormationPresentationComponent* presentation,
    Engine::Core::Entity* entity,
    bool force_single_soldier) {
  if (presentation != nullptr && presentation->soldiers.size() == instances.size()) {
    for (std::size_t index = 0; index < instances.size(); ++index) {
      auto const& shared_slot = presentation->soldiers[index];
      auto& instance = instances[index];
      instance.offset_x = shared_slot.local_x;
      instance.offset_z = shared_slot.local_z;
      instance.yaw_offset = shared_slot.local_yaw;
      instance.row_index = static_cast<std::uint8_t>(shared_slot.row);
      instance.col_index = static_cast<std::uint8_t>(shared_slot.col);
    }
    return;
  }

  if (entity == nullptr || force_single_soldier) {
    return;
  }

  auto const layout = Game::Systems::FormationCombat::resolve_layout(*entity);
  for (auto const& slot : layout.occupied_slots) {
    if (slot.index >= instances.size()) {
      continue;
    }
    auto& instance = instances[slot.index];
    instance.offset_x = slot.local_x;
    instance.offset_z = slot.local_z;
    instance.yaw_offset = slot.local_yaw;
    instance.row_index = static_cast<std::uint8_t>(slot.row);
    instance.col_index = static_cast<std::uint8_t>(slot.col);
  }
}

} // namespace Render::Entity
