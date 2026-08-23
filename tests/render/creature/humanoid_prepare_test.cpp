

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <numbers>
#include <numeric>
#include <unordered_set>
#include <vector>

#include "animation/action_manifest.h"
#include "animation/ambient_pose_manifest.h"
#include "animation/attack_pose_manifest.h"
#include "animation/bpat/bpat_format.h"
#include "animation/clip_manifest.h"
#include "animation/guard_manifest.h"
#include "animation/hold_pose_manifest.h"
#include "animation/intent_manifest.h"
#include "animation/layout_manifest.h"
#include "animation/locomotion_manifest.h"
#include "animation/micro_variation_manifest.h"
#include "animation/mounted_pose_manifest.h"
#include "animation/playback_manifest.h"
#include "animation/posture_pose_manifest.h"
#include "animation/selection_manifest.h"
#include "animation/state_manifest.h"
#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "game/formation/unit_layout.h"
#include "game/formation/unit_layout_state_system.h"
#include "game/map/terrain.h"
#include "game/map/terrain_service.h"
#include "render/creature/animation_state_components.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/humanoid_clip_ids.h"
#include "render/creature/pipeline/creature_pipeline.h"
#include "render/creature/pipeline/creature_render_state.h"
#include "render/creature/pipeline/humanoid_animation_selection.h"
#include "render/creature/pipeline/preparation_common.h"
#include "render/creature/pipeline/prepared_submit.h"
#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/creature/pose_intent.h"
#include "render/creature/render_request.h"
#include "render/creature/runtime_bake_guard.h"
#include "render/entity/humanoid_pose_policies.h"
#include "render/entity/registry.h"
#include "render/equipment/armor/arm_guards_renderer.h"
#include "render/equipment/armor/armor_heavy_carthage.h"
#include "render/equipment/armor/armor_light_carthage.h"
#include "render/equipment/armor/carthage_shoulder_cover.h"
#include "render/equipment/armor/cloak_renderer.h"
#include "render/equipment/armor/roman_armor.h"
#include "render/equipment/armor/roman_greaves.h"
#include "render/equipment/armor/roman_shoulder_cover.h"
#include "render/equipment/armor/tool_belt_renderer.h"
#include "render/equipment/armor/work_apron_renderer.h"
#include "render/equipment/helmets/carthage_heavy_helmet.h"
#include "render/equipment/helmets/historical_helmets.h"
#include "render/equipment/helmets/roman_heavy_helmet.h"
#include "render/equipment/helmets/roman_light_helmet.h"
#include "render/equipment/weapons/bow_renderer.h"
#include "render/equipment/weapons/quiver_renderer.h"
#include "render/equipment/weapons/roman_scutum.h"
#include "render/equipment/weapons/shield_carthage.h"
#include "render/equipment/weapons/spear_renderer.h"
#include "render/equipment/weapons/sword_renderer.h"
#include "render/gl/humanoid/animation/animation_inputs.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/humanoid/asset/humanoid_manifest.h"
#include "render/humanoid/asset/humanoid_spec.h"
#include "render/humanoid/runtime/frame_control.h"
#include "render/humanoid/runtime/humanoid_renderer.h"
#include "render/humanoid/runtime/instance_state.h"
#include "render/humanoid/runtime/pose_controller.h"
#include "render/humanoid/runtime/pose_primitives.h"
#include "render/humanoid/runtime/prepare.h"
#include "render/humanoid/runtime/runtime_context.h"
#include "render/humanoid/runtime/runtime_stats.h"
#include "render/humanoid/runtime/skeleton_evaluator.h"
#include "render/humanoid/runtime/unit_layout_spacing.h"
#include "render/rigged_mesh.h"
#include "render/submitter.h"
#include "render/template_cache.h"
#include "render/world_view.h"
#include "tests/support/movement_test_access.h"

namespace {

constexpr std::uint32_t k_roman_senator_role_count = 6;
constexpr std::uint32_t k_carthage_dark_mage_role_count = 6;
constexpr std::uint32_t k_civilian_pack_role_count = 3;
constexpr std::uint32_t k_roman_builder_tunic_role_count = 2;
constexpr std::uint32_t k_roman_civilian_mantle_role_count = 2;
constexpr std::uint32_t k_carthage_builder_headwrap_role_count = 1;
constexpr std::uint32_t k_carthage_builder_robes_role_count = 2;
constexpr std::uint32_t k_carthage_civilian_sash_role_count = 2;

auto test_runtime(std::uint32_t frame_index)
    -> Render::Humanoid::HumanoidRuntimeContext& {
  thread_local Render::Humanoid::HumanoidRuntimeContext runtime;
  runtime.frame_index = frame_index;
  return runtime;
}

class CountingSubmitter : public Render::GL::ISubmitter {
public:
  int rigged_calls{0};
  int meshes{0};
  std::vector<std::uint32_t> role_color_counts;
  std::vector<const Render::GL::RiggedMesh*> rigged_meshes;
  std::vector<float> rigged_world_y;
  std::vector<float> rigged_mesh_min_world_y;
  std::vector<std::vector<QMatrix4x4>> rigged_bone_palettes;
  const QMatrix4x4* last_bone_palette{nullptr};
  std::vector<QMatrix4x4> last_bone_palette_copy;
  void mesh(Render::GL::Mesh*,
            const QMatrix4x4&,
            const QVector3D&,
            Render::GL::Texture*,
            float,
            int) override {
    ++meshes;
  }
  void rigged(const Render::GL::RiggedCreatureCmd& cmd) override {
    ++rigged_calls;
    role_color_counts.push_back(cmd.role_color_count);
    rigged_meshes.push_back(cmd.mesh);
    rigged_world_y.push_back(cmd.world.column(3).y());
    float min_world_y = std::numeric_limits<float>::infinity();
    if (cmd.mesh != nullptr && cmd.bone_palette != nullptr) {
      for (const auto& vertex : cmd.mesh->get_vertices()) {
        QVector4D const bone_local(vertex.position_bone_local[0],
                                   vertex.position_bone_local[1],
                                   vertex.position_bone_local[2],
                                   1.0F);
        QVector4D skinned_local(0.0F, 0.0F, 0.0F, 0.0F);
        for (int i = 0; i < 4; ++i) {
          float const weight = vertex.bone_weights[i];
          if (weight <= 0.0F) {
            continue;
          }
          auto const bone_index = static_cast<std::uint32_t>(vertex.bone_indices[i]);
          if (bone_index >= cmd.bone_count) {
            continue;
          }
          skinned_local += (cmd.bone_palette[bone_index] * bone_local) * weight;
        }
        min_world_y =
            std::min(min_world_y, cmd.world.map(skinned_local.toVector3D()).y());
      }
    }
    rigged_mesh_min_world_y.push_back(min_world_y);
    last_bone_palette = cmd.bone_palette;
    last_bone_palette_copy.assign(cmd.bone_palette, cmd.bone_palette + cmd.bone_count);
    rigged_bone_palettes.push_back(last_bone_palette_copy);
  }
  void cylinder(
      const QVector3D&, const QVector3D&, float, const QVector3D&, float) override {}
  void ground_marker(const Render::GL::GroundMarkerCmd&) override {}
  void grid(const QMatrix4x4&, const QVector3D&, float, float, float) override {}
  void selection_smoke(const QMatrix4x4&, const QVector3D&, float) override {}
  void healing_beam(const QVector3D&,
                    const QVector3D&,
                    const QVector3D&,
                    float,
                    float,
                    float,
                    float) override {}
  void healer_aura(const QVector3D&, const QVector3D&, float, float, float) override {}
  void combat_dust(const QVector3D&, const QVector3D&, float, float, float) override {}
  void stone_impact(const QVector3D&, const QVector3D&, float, float, float) override {}
  void mode_indicator(const QMatrix4x4&, int, const QVector3D&, float) override {}
};

TEST(HumanoidPrepare, SeedMissingWearProfileIsDeterministic) {
  Render::GL::HumanoidVariant first{};
  Render::GL::HumanoidVariant second{};
  Render::GL::HumanoidVariant other{};

  Render::GL::seed_missing_humanoid_wear(first, 12345U);
  Render::GL::seed_missing_humanoid_wear(second, 12345U);
  Render::GL::seed_missing_humanoid_wear(other, 54321U);

  EXPECT_FLOAT_EQ(first.scarring, second.scarring);
  EXPECT_FLOAT_EQ(first.weathering, second.weathering);
  EXPECT_FLOAT_EQ(first.grime, second.grime);
  EXPECT_FLOAT_EQ(first.bloodiness, second.bloodiness);
  EXPECT_FLOAT_EQ(first.pattern_seed, second.pattern_seed);
  EXPECT_GT(first.weathering, 0.0F);
  EXPECT_GT(first.grime, 0.0F);
  EXPECT_NE(first.pattern_seed, other.pattern_seed);
}

auto render_archer_role_color_count(const char* renderer_id) -> std::uint32_t {
  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto renderer = registry.get(renderer_id);
  EXPECT_TRUE(static_cast<bool>(renderer));
  if (!renderer) {
    return 0U;
  }

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  CountingSubmitter sink;
  renderer(ctx, sink);

  EXPECT_GT(sink.rigged_calls, 0);
  if (sink.role_color_counts.empty()) {
    return 0U;
  }
  return *std::max_element(sink.role_color_counts.begin(),
                           sink.role_color_counts.end());
}

auto render_bow_mesh_count(const char* renderer_id,
                           Game::Units::SpawnType spawn_type,
                           Game::Systems::NationID nation_id,
                           bool moving) -> int {
  struct RenderCounts {
    int meshes{0};
    int rigged_calls{0};
    std::vector<std::uint32_t> role_color_counts;
  };

  auto render_counts = [&](const char* id,
                           Game::Units::SpawnType type,
                           Game::Systems::NationID nation,
                           bool is_moving) -> RenderCounts {
    Render::GL::EntityRendererRegistry registry;
    Render::GL::register_built_in_entity_renderers(registry);
    const auto renderer = registry.get(id);
    EXPECT_TRUE(static_cast<bool>(renderer));
    if (!renderer) {
      return {};
    }

    Render::GL::DrawContext ctx{};
    ctx.world_view = Render::WorldView::of_active_session();
    ctx.force_single_soldier = true;
    ctx.allow_template_cache = false;

    Engine::Core::StandaloneEntity entity_scratch(1);
    Engine::Core::Entity& entity = entity_scratch.entity();
    auto* unit =
        entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
    EXPECT_NE(unit, nullptr);
    if (unit == nullptr) {
      return {};
    }
    unit->spawn_type = type;
    unit->nation_id = nation;
    if (is_moving) {
      auto* movement = entity.add_component<Engine::Core::MovementComponent>();
      EXPECT_NE(movement, nullptr);
      if (movement == nullptr) {
        return {};
      }
      MovementTestAccess::set_has_target(*movement, true);
      MovementTestAccess::set_target_x(*movement, 2.0F);
      MovementTestAccess::set_target_y(*movement, 0.0F);
      MovementTestAccess::set_vx(*movement, 1.0F);
      MovementTestAccess::set_vz(*movement, 0.0F);
    }
    ctx.entity = &entity;

    CountingSubmitter sink;
    renderer(ctx, sink);
    return {
        .meshes = sink.meshes,
        .rigged_calls = sink.rigged_calls,
        .role_color_counts = sink.role_color_counts,
    };
  };

  auto const counts = render_counts(renderer_id, spawn_type, nation_id, moving);
  EXPECT_GT(counts.rigged_calls, 0);
  int visible_bow_count = counts.meshes;
  if (visible_bow_count == 0 && !counts.role_color_counts.empty()) {
    auto const max_roles = *std::max_element(counts.role_color_counts.begin(),
                                             counts.role_color_counts.end());
    if (max_roles > Render::Humanoid::k_humanoid_role_count) {
      visible_bow_count = 1;
    }
  }
  return visible_bow_count;
}

auto render_direct_bow_mesh_count(const char* renderer_id,
                                  Game::Units::SpawnType spawn_type,
                                  Game::Systems::NationID nation_id,
                                  bool hold_active) -> int {
  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto renderer = registry.get(renderer_id);
  EXPECT_TRUE(static_cast<bool>(renderer));
  if (!renderer) {
    return 0;
  }

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(1);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  EXPECT_NE(unit, nullptr);
  if (unit == nullptr) {
    return 0;
  }
  unit->spawn_type = spawn_type;
  unit->nation_id = nation_id;
  if (hold_active) {
    auto* hold = entity.add_component<Engine::Core::HoldModeComponent>();
    EXPECT_NE(hold, nullptr);
    if (hold == nullptr) {
      return 0;
    }
    hold->active = true;
    hold->kneel_entry_progress = 1.0F;
  }
  ctx.entity = &entity;

  CountingSubmitter sink;
  renderer(ctx, sink);
  EXPECT_GT(sink.rigged_calls, 0);
  return sink.meshes;
}

auto render_runtime_mesh_count(const char* renderer_id,
                               Game::Units::SpawnType spawn_type,
                               Game::Systems::NationID nation_id,
                               bool moving) -> int {
  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto renderer = registry.get(renderer_id);
  EXPECT_TRUE(static_cast<bool>(renderer));
  if (!renderer) {
    return 0;
  }

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(1);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  EXPECT_NE(unit, nullptr);
  if (unit == nullptr) {
    return 0;
  }
  unit->spawn_type = spawn_type;
  unit->nation_id = nation_id;
  if (moving) {
    auto* movement = entity.add_component<Engine::Core::MovementComponent>();
    EXPECT_NE(movement, nullptr);
    if (movement == nullptr) {
      return 0;
    }
    MovementTestAccess::set_has_target(*movement, true);
    MovementTestAccess::set_target_x(*movement, 2.0F);
    MovementTestAccess::set_target_y(*movement, 0.0F);
    MovementTestAccess::set_vx(*movement, 1.0F);
    MovementTestAccess::set_vz(*movement, 0.0F);
  }
  ctx.entity = &entity;

  CountingSubmitter sink;
  renderer(ctx, sink);

  EXPECT_GT(sink.rigged_calls, 0);
  return sink.meshes;
}

auto find_archetype_id(std::string_view debug_name) -> Render::Creature::ArchetypeId {
  auto& registry = Render::Creature::ArchetypeRegistry::instance();
  auto find_registered = [&registry, debug_name] {
    for (std::size_t i = 0; i < registry.size(); ++i) {
      auto const id = static_cast<Render::Creature::ArchetypeId>(i);
      auto const* desc = registry.get(id);
      if (desc != nullptr && desc->debug_name == debug_name) {
        return id;
      }
    }
    return Render::Creature::k_invalid_archetype;
  };
  if (auto const id = find_registered(); id != Render::Creature::k_invalid_archetype) {
    return id;
  }

  Render::GL::EntityRendererRegistry renderer_registry;
  Render::GL::register_built_in_entity_renderers(renderer_registry);
  auto const renderer = renderer_registry.get(std::string(debug_name));
  if (renderer) {
    Engine::Core::StandaloneEntity entity_scratch(1);
    Engine::Core::Entity& entity = entity_scratch.entity();
    entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
    Render::GL::DrawContext ctx{};
    ctx.world_view = Render::WorldView::of_active_session();
    ctx.entity = &entity;
    ctx.force_single_soldier = true;
    ctx.allow_template_cache = false;
    CountingSubmitter sink;
    renderer(ctx, sink);
  }
  return find_registered();
}

auto extra_role_color_count(Render::Creature::ArchetypeId archetype_id)
    -> std::uint32_t {
  auto& registry = Render::Creature::ArchetypeRegistry::instance();
  auto const* desc = registry.get(archetype_id);
  EXPECT_NE(desc, nullptr);
  if (desc == nullptr) {
    return 0U;
  }
  EXPECT_GE(desc->extra_role_color_fn_count, 1U);
  if (desc->extra_role_color_fn_count == 0U) {
    return 0U;
  }

  Render::GL::HumanoidVariant variant{};
  variant.palette.cloth = QVector3D(0.8F, 0.1F, 0.1F);
  variant.palette.leather = QVector3D(0.4F, 0.28F, 0.16F);
  variant.palette.metal = QVector3D(0.7F, 0.7F, 0.72F);
  std::array<QVector3D, 64> roles{};
  std::uint32_t count = 0U;
  for (std::size_t i = 0; i < static_cast<std::size_t>(desc->extra_role_color_fn_count);
       ++i) {
    const auto fn = desc->extra_role_color_fns[i];
    if (fn == nullptr) {
      continue;
    }
    count = fn(&variant, roles.data(), count, roles.size());
  }
  return count;
}

auto render_archer_idle_bone_palette(
    const char* renderer_id, Game::Systems::NationID nation_id) -> const QMatrix4x4* {
  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto renderer = registry.get(renderer_id);
  EXPECT_TRUE(static_cast<bool>(renderer));
  if (!renderer) {
    return nullptr;
  }

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;
  Engine::Core::StandaloneEntity entity_scratch(1);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  EXPECT_NE(unit, nullptr);
  if (unit == nullptr) {
    return nullptr;
  }
  unit->spawn_type = Game::Units::SpawnType::Archer;
  unit->nation_id = nation_id;
  ctx.entity = &entity;

  CountingSubmitter sink;
  renderer(ctx, sink);

  EXPECT_GT(sink.rigged_calls, 0);
  return sink.last_bone_palette;
}

auto lower_body_palette_moves_between(std::span<const QMatrix4x4> first,
                                      std::span<const QMatrix4x4> second) -> bool {
  using Render::Humanoid::HumanoidBone;

  constexpr std::array<HumanoidBone, 7> k_lower_body_bones{{
      HumanoidBone::Pelvis,
      HumanoidBone::HipL,
      HumanoidBone::KneeL,
      HumanoidBone::FootL,
      HumanoidBone::HipR,
      HumanoidBone::KneeR,
      HumanoidBone::FootR,
  }};

  for (auto const bone : k_lower_body_bones) {
    auto const index = static_cast<std::size_t>(bone);
    if (index >= first.size() || index >= second.size()) {
      return false;
    }
    if (first[index] != second[index]) {
      return true;
    }
  }
  return false;
}

auto moving_palette_changes_over_time(const char* renderer_id,
                                      Game::Units::SpawnType spawn_type,
                                      Game::Systems::NationID nation_id,
                                      bool use_motion_snapshot = false) -> bool {
  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  auto const renderer = registry.get(renderer_id);
  EXPECT_TRUE(static_cast<bool>(renderer));
  if (!renderer) {
    return false;
  }

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.allow_template_cache = false;
  ctx.force_humanoid_lod = true;
  ctx.forced_humanoid_lod = Render::Creature::CreatureLOD::Full;

  Engine::Core::StandaloneEntity entity_scratch(1);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  auto* movement = entity.add_component<Engine::Core::MovementComponent>();
  auto* transform = entity.add_component<Engine::Core::TransformComponent>();
  auto* motion = use_motion_snapshot
                     ? entity.add_component<Engine::Core::MotionPresentationComponent>()
                     : nullptr;
  EXPECT_NE(unit, nullptr);
  EXPECT_NE(movement, nullptr);
  EXPECT_NE(transform, nullptr);
  if (unit == nullptr || movement == nullptr || transform == nullptr) {
    return false;
  }
  unit->spawn_type = spawn_type;
  unit->nation_id = nation_id;
  MovementTestAccess::set_has_target(*movement, true);
  MovementTestAccess::set_target_x(*movement, 6.0F);
  MovementTestAccess::set_target_y(*movement, 0.0F);
  MovementTestAccess::set_vx(*movement, 1.0F);
  MovementTestAccess::set_vz(*movement, 0.0F);
  transform->position = {0.0F, 0.0F, 0.0F};
  if (motion != nullptr) {
    motion->initialized = true;
    motion->snapshot_valid = true;
    motion->set_state(Engine::Core::MotionPresentationState::Walk);
    motion->has_navigation_intent = true;
    motion->has_movement_target = true;
    motion->movement_target_x = 6.0F;
    motion->movement_target_z = 0.0F;
    motion->direction_x = 1.0F;
    motion->direction_z = 0.0F;
    motion->speed = 1.0F;
  }
  ctx.entity = &entity;

  ctx.animation_time = 0.10F;
  CountingSubmitter first_sink;
  renderer(ctx, first_sink);
  EXPECT_GT(first_sink.rigged_calls, 0);
  if (first_sink.rigged_bone_palettes.empty()) {
    return false;
  }

  Render::GL::begin_humanoid_frame();
  ctx.animation_time = 0.70F;
  CountingSubmitter second_sink;
  renderer(ctx, second_sink);
  EXPECT_GT(second_sink.rigged_calls, 0);
  if (second_sink.rigged_bone_palettes.empty()) {
    return false;
  }
  auto const comparable_count = std::min(first_sink.rigged_bone_palettes.size(),
                                         second_sink.rigged_bone_palettes.size());
  for (std::size_t i = 0; i < comparable_count; ++i) {
    if (lower_body_palette_moves_between(first_sink.rigged_bone_palettes[i],
                                         second_sink.rigged_bone_palettes[i])) {
      return true;
    }
  }
  return false;
}

auto render_builder_submission_count(const char* renderer_id,
                                     Game::Systems::NationID nation_id,
                                     bool constructing) -> int {
  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto renderer = registry.get(renderer_id);
  EXPECT_TRUE(static_cast<bool>(renderer));
  if (!renderer) {
    return 0;
  }

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(1);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  EXPECT_NE(unit, nullptr);
  if (unit == nullptr) {
    return 0;
  }
  unit->spawn_type = Game::Units::SpawnType::Builder;
  unit->nation_id = nation_id;

  if (constructing) {
    auto* builder = entity.add_component<Engine::Core::BuilderProductionComponent>();
    EXPECT_NE(builder, nullptr);
    if (builder == nullptr) {
      return 0;
    }
    builder->in_progress = true;
    builder->has_construction_site = true;
    builder->at_construction_site = true;
    builder->build_time = 10.0F;
    builder->time_remaining = 5.0F;
    builder->construction_site_x = 2.0F;
    builder->construction_site_z = 0.0F;
  }

  ctx.entity = &entity;

  CountingSubmitter sink;
  renderer(ctx, sink);
  return sink.rigged_calls;
}

auto render_builder_unique_tool_mesh_count(
    const char* renderer_id, Game::Systems::NationID nation_id) -> std::size_t {
  Render::GL::reset_humanoid_runtime_context();

  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto renderer = registry.get(renderer_id);
  EXPECT_TRUE(static_cast<bool>(renderer));
  if (!renderer) {
    return 0U;
  }

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(1);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  EXPECT_NE(unit, nullptr);
  if (unit == nullptr) {
    return 0U;
  }
  unit->spawn_type = Game::Units::SpawnType::Builder;
  unit->nation_id = nation_id;
  unit->render_individuals_per_unit_override = 12;

  auto* builder = entity.add_component<Engine::Core::BuilderProductionComponent>();
  EXPECT_NE(builder, nullptr);
  if (builder == nullptr) {
    return 0U;
  }
  builder->in_progress = true;
  builder->has_construction_site = true;
  builder->at_construction_site = true;
  builder->build_time = 10.0F;
  builder->time_remaining = 4.0F;
  builder->construction_site_x = 2.0F;
  builder->construction_site_z = 0.0F;

  ctx.entity = &entity;

  CountingSubmitter sink;
  renderer(ctx, sink);
  EXPECT_GT(sink.rigged_calls, 0);

  std::unordered_set<const Render::GL::RiggedMesh*> unique_meshes;
  for (auto const* mesh : sink.rigged_meshes) {
    if (mesh != nullptr) {
      unique_meshes.insert(mesh);
    }
  }
  return unique_meshes.size();
}

auto render_civilian_submission_count(const char* renderer_id,
                                      Game::Systems::NationID nation_id) -> int {
  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto renderer = registry.get(renderer_id);
  EXPECT_TRUE(static_cast<bool>(renderer));
  if (!renderer) {
    return 0;
  }

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(2);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  EXPECT_NE(unit, nullptr);
  if (unit == nullptr) {
    return 0;
  }
  unit->spawn_type = Game::Units::SpawnType::Civilian;
  unit->nation_id = nation_id;

  ctx.entity = &entity;

  CountingSubmitter sink;
  renderer(ctx, sink);
  return sink.rigged_calls;
}

auto render_builder_bone_palette(const char* renderer_id,
                                 Game::Systems::NationID nation_id,
                                 bool constructing,
                                 float time_remaining) -> const QMatrix4x4* {
  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto renderer = registry.get(renderer_id);
  EXPECT_TRUE(static_cast<bool>(renderer));
  if (!renderer) {
    return nullptr;
  }

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(1);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  EXPECT_NE(unit, nullptr);
  if (unit == nullptr) {
    return nullptr;
  }
  unit->spawn_type = Game::Units::SpawnType::Builder;
  unit->nation_id = nation_id;

  if (constructing) {
    auto* builder = entity.add_component<Engine::Core::BuilderProductionComponent>();
    EXPECT_NE(builder, nullptr);
    if (builder == nullptr) {
      return nullptr;
    }
    builder->in_progress = true;
    builder->has_construction_site = true;
    builder->at_construction_site = true;
    builder->build_time = 10.0F;
    builder->time_remaining = time_remaining;
    builder->construction_site_x = 2.0F;
    builder->construction_site_z = 0.0F;
  }

  ctx.entity = &entity;

  CountingSubmitter sink;
  renderer(ctx, sink);
  EXPECT_GT(sink.rigged_calls, 0);
  return sink.last_bone_palette;
}

auto request_idle_bone_palette(Render::Creature::ArchetypeId archetype_id,
                               float phase) -> const QMatrix4x4* {
  using Render::Creature::AnimationStateId;
  using Render::Creature::CreatureLOD;
  using Render::Creature::CreatureRenderRequest;
  using Render::Creature::Pipeline::CreaturePipeline;

  CreatureRenderRequest req{};
  req.archetype = archetype_id;
  req.state = AnimationStateId::Idle;
  req.phase = phase;
  req.lod = CreatureLOD::Full;

  CountingSubmitter sink;
  CreaturePipeline const pipeline;
  std::array<CreatureRenderRequest, 1> requests{req};
  pipeline.submit_requests(requests, sink);

  EXPECT_GT(sink.rigged_calls, 0);
  return sink.last_bone_palette;
}

auto request_bone_palette_copy(const Render::Creature::CreatureRenderRequest& request)
    -> std::vector<QMatrix4x4> {
  using Render::Creature::Pipeline::CreaturePipeline;

  CountingSubmitter sink;
  CreaturePipeline const pipeline;
  std::array<Render::Creature::CreatureRenderRequest, 1> requests{request};
  pipeline.submit_requests(requests, sink);

  EXPECT_GT(sink.rigged_calls, 0);
  return sink.last_bone_palette_copy;
}

auto submit_request_stats(const Render::Creature::CreatureRenderRequest& request)
    -> Render::Creature::Pipeline::SubmitStats {
  using Render::Creature::Pipeline::CreaturePipeline;

  CountingSubmitter sink;
  CreaturePipeline const pipeline;
  std::array<Render::Creature::CreatureRenderRequest, 1> requests{request};
  return pipeline.submit_requests(requests, sink);
}

auto max_matrix_delta(const QMatrix4x4& a, const QMatrix4x4& b) -> float {
  float delta = 0.0F;
  const float* av = a.constData();
  const float* bv = b.constData();
  for (int i = 0; i < 16; ++i) {
    delta = std::max(delta, std::abs(av[i] - bv[i]));
  }
  return delta;
}

struct ScopedFlatTerrain {
  explicit ScopedFlatTerrain(float height) {
    auto& terrain = Game::Map::TerrainService::instance();
    std::vector<float> const heights(9, height);
    std::vector<Game::Map::TerrainType> const terrain_types(
        9, Game::Map::TerrainType::Flat);
    terrain.restore_from_serialized(
        3, 3, 1.0F, heights, terrain_types, {}, {}, {}, Game::Map::BiomeSettings{});
  }

  ~ScopedFlatTerrain() { Game::Map::TerrainService::instance().clear(); }
};

auto render_builder_min_world_y(const char* renderer_id,
                                Game::Systems::NationID nation_id,
                                float terrain_height) -> float {
  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto renderer = registry.get(renderer_id);
  EXPECT_TRUE(static_cast<bool>(renderer));
  if (!renderer) {
    return std::numeric_limits<float>::infinity();
  }

  ScopedFlatTerrain const terrain(terrain_height);

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(1);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  EXPECT_NE(unit, nullptr);
  if (unit == nullptr) {
    return std::numeric_limits<float>::infinity();
  }
  unit->spawn_type = Game::Units::SpawnType::Builder;
  unit->nation_id = nation_id;
  unit->render_individuals_per_unit_override = 1;

  auto* transform = entity.add_component<Engine::Core::TransformComponent>();
  EXPECT_NE(transform, nullptr);
  if (transform == nullptr) {
    return std::numeric_limits<float>::infinity();
  }
  transform->position.x = 0.35F;
  transform->position.y = 6.0F;
  transform->position.z = -0.2F;
  transform->scale.x = 0.5F;
  transform->scale.y = 0.5F;
  transform->scale.z = 0.5F;
  ctx.entity = &entity;

  CountingSubmitter sink;
  renderer(ctx, sink);
  EXPECT_GT(sink.rigged_calls, 0);
  if (sink.rigged_mesh_min_world_y.empty()) {
    return std::numeric_limits<float>::infinity();
  }
  return sink.rigged_mesh_min_world_y.front();
}

class BeardRenderer : public Render::GL::HumanoidRendererBase {
public:
  BeardRenderer()
      : Render::GL::HumanoidRendererBase(make_visual_spec()) {}

  static auto make_visual_spec() -> Render::Creature::Pipeline::UnitVisualSpec {
    Render::Creature::Pipeline::UnitVisualSpec s{};
    s.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
    s.debug_name = "tests/beard_renderer";
    return s;
  }

  void get_variant(const Render::GL::DrawContext&,
                   std::uint32_t,
                   Render::GL::HumanoidVariant& v) const override {
    v.facial_hair.style = Render::GL::FacialHairStyle::FullBeard;
    v.facial_hair.color = QVector3D(0.20F, 0.14F, 0.10F);
    v.facial_hair.greyness = 0.10F;
  }
};

class BowReadyRegressionRenderer : public Render::GL::HumanoidRendererBase {
public:
  BowReadyRegressionRenderer()
      : Render::GL::HumanoidRendererBase(make_visual_spec()) {}

  static auto make_visual_spec() -> Render::Creature::Pipeline::UnitVisualSpec {
    using Render::Creature::AnimationStateId;
    using Render::Creature::ArchetypeDescriptor;
    using Render::Creature::ArchetypeRegistry;

    auto& registry = ArchetypeRegistry::instance();
    auto const* base_desc = registry.get(ArchetypeRegistry::k_humanoid_base);
    EXPECT_NE(base_desc, nullptr);

    Render::Creature::Pipeline::UnitVisualSpec s{};
    s.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
    s.debug_name = "tests/bow_ready_regression_renderer";
    if (base_desc == nullptr) {
      return s;
    }

    ArchetypeDescriptor desc = *base_desc;
    desc.debug_name = "tests/bow_ready_regression_archetype";
    auto const attack_bow_clip =
        desc.bpat_clip[static_cast<std::size_t>(AnimationStateId::AttackBow)];
    desc.bpat_clip[static_cast<std::size_t>(AnimationStateId::Idle)] = attack_bow_clip;
    desc.bpat_clip[static_cast<std::size_t>(AnimationStateId::Hold)] = attack_bow_clip;
    s.archetype_id = registry.register_archetype(desc);
    return s;
  }
};

class SnapshotPrewarmRenderer : public Render::GL::HumanoidRendererBase {
public:
  SnapshotPrewarmRenderer()
      : Render::GL::HumanoidRendererBase(make_visual_spec()) {}

  static auto make_visual_spec() -> Render::Creature::Pipeline::UnitVisualSpec {
    Render::Creature::Pipeline::UnitVisualSpec s{};
    s.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
    s.debug_name = "tests/snapshot_prewarm_renderer";
    s.archetype_id = Render::Creature::ArchetypeRegistry::k_humanoid_base;
    return s;
  }
};

TEST(HumanoidPrepare, PassIntentFromCtxIsShadowOnPrewarm) {
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  EXPECT_EQ(Render::Creature::Pipeline::pass_intent_from_ctx(ctx),
            Render::Creature::Pipeline::RenderPassIntent::Main);
  ctx.template_prewarm = true;
  EXPECT_EQ(Render::Creature::Pipeline::pass_intent_from_ctx(ctx),
            Render::Creature::Pipeline::RenderPassIntent::Shadow);
}

TEST(HumanoidPrepare, ShadowRowSubmitsZeroDrawCalls) {
  using namespace Render::Creature::Pipeline;

  CountingSubmitter sink;
  CreaturePreparationResult prep;
  Render::Creature::CreatureRenderRequest req{};
  req.archetype = Render::Creature::ArchetypeRegistry::k_humanoid_base;
  req.state = Render::Creature::AnimationStateId::Idle;
  req.lod = Render::Creature::CreatureLOD::Full;
  req.pass = RenderPassIntent::Shadow;
  req.seed = 42U;
  req.world_already_grounded = true;
  prep.bodies.add_request(req);
  const auto stats = submit_preparation(prep, sink);

  EXPECT_EQ(stats.entities_submitted, 0U);
  EXPECT_EQ(sink.rigged_calls, 0);
  EXPECT_EQ(sink.meshes, 0);
}

TEST(HumanoidPrepare, MainRowStillSubmitsOneRiggedCall) {
  using namespace Render::Creature::Pipeline;

  CountingSubmitter sink;
  CreaturePreparationResult prep;
  Render::Creature::CreatureRenderRequest req{};
  req.archetype = Render::Creature::ArchetypeRegistry::k_humanoid_base;
  req.state = Render::Creature::AnimationStateId::Idle;
  req.lod = Render::Creature::CreatureLOD::Full;
  req.pass = RenderPassIntent::Main;
  req.seed = 7U;
  req.world_already_grounded = true;
  prep.bodies.add_request(req);
  const auto stats = submit_preparation(prep, sink);

  EXPECT_EQ(stats.entities_submitted, 1U);
  EXPECT_GT(sink.rigged_calls + sink.meshes, 0);
}

TEST(HumanoidPrepare, TemplatePrewarmRenderWarmsSnapshotCache) {
  SnapshotPrewarmRenderer const renderer;
  Engine::Core::StandaloneEntity entity_scratch(1);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>();
  unit->spawn_type = Game::Units::SpawnType::Knight;
  unit->owner_id = 1;
  unit->max_health = 100;
  unit->health = 100;
  auto* transform = entity.add_component<Engine::Core::TransformComponent>();
  transform->position = {0.0F, 0.0F, 0.0F};
  transform->rotation = {0.0F, 0.0F, 0.0F};
  transform->scale = {1.0F, 1.0F, 1.0F};
  auto* renderable = entity.add_component<Engine::Core::RenderableComponent>();
  renderable->renderer_id = "tests/snapshot_prewarm_renderer";
  renderable->visible = true;

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.entity = &entity;
  ctx.template_prewarm = true;
  ctx.force_single_soldier = true;
  ctx.force_humanoid_lod = true;
  ctx.forced_humanoid_lod = Render::Creature::CreatureLOD::Minimal;

  Render::GL::TemplateRecorder recorder;
  recorder.snapshot_mesh_cache().clear();
  Render::Creature::RuntimeBakeAllowScope const allow_prewarm_bakes;
  renderer.render(ctx, recorder);

  EXPECT_GT(recorder.snapshot_mesh_cache().size(), 0U);
  EXPECT_TRUE(recorder.commands().empty());
}

TEST(HumanoidPrepare, PoseLayerNeverRunsDuringRuntimePreparation) {
  class PoseLayerRenderer final : public Render::GL::HumanoidRendererBase {
  public:
    PoseLayerRenderer()
        : Render::GL::HumanoidRendererBase(make_visual_spec()) {}

    static auto make_visual_spec() -> Render::Creature::Pipeline::UnitVisualSpec {
      Render::Creature::Pipeline::UnitVisualSpec s{};
      s.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
      s.debug_name = "tests/pose_layer_renderer";
      s.archetype_id = Render::Creature::ArchetypeRegistry::k_humanoid_base;

      s.animation_manifest.pose_policy =
          Render::Humanoid::HumanoidPosePolicy::SkeletonProportions;
      return s;
    }
  };

  PoseLayerRenderer const renderer;
  Render::GL::AnimationInputs const anim{};
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Render::GL::HumanoidPose pose{};
  pose.neck_base = QVector3D(0.0F, 1.4F, 0.0F);
  pose.head_pos = QVector3D(0.0F, 1.6F, 0.0F);
  const Render::GL::HumanoidPose untouched = pose;

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      renderer, ctx, anim, test_runtime(0U), prep);

  ctx.template_prewarm = true;
  prep.clear();
  Render::Humanoid::prepare_humanoid_instances(
      renderer, ctx, anim, test_runtime(0U), prep);

  EXPECT_FLOAT_EQ(pose.head_pos.y(), untouched.head_pos.y());
  Render::Entity::apply_humanoid_pose_policy(
      Render::Humanoid::HumanoidPosePolicy::SkeletonProportions, {}, pose);
  EXPECT_LT(pose.head_pos.y(), untouched.head_pos.y());
}

TEST(HumanoidPrepare, EveryPosePolicyResolvesToADefinitionThatMovesThePose) {
  Render::GL::HumanoidPose base{};
  base.neck_base = QVector3D(0.0F, 1.4F, 0.0F);
  base.head_pos = QVector3D(0.0F, 1.6F, 0.0F);
  base.hand_r = QVector3D(0.25F, 1.0F, 0.0F);
  base.hand_l = QVector3D(-0.25F, 1.0F, 0.0F);
  base.elbow_r = QVector3D(0.22F, 1.15F, 0.0F);
  base.elbow_l = QVector3D(-0.22F, 1.15F, 0.0F);

  Render::GL::HumanoidAnimationContext anim{};
  anim.inputs.is_casting = true;
  anim.inputs.cast_kind = Render::GL::CastVisualKind::Fireball;
  anim.attack_phase = 0.5F;

  Render::Entity::HumanoidPosePolicyInputs inputs;
  inputs.animation = &anim;

  for (auto const policy : {Render::Humanoid::HumanoidPosePolicy::SkeletonProportions,
                            Render::Humanoid::HumanoidPosePolicy::HealerChannel,
                            Render::Humanoid::HumanoidPosePolicy::HealerStaff,
                            Render::Humanoid::HumanoidPosePolicy::GravePriestCast}) {
    Render::GL::HumanoidPose pose = base;
    Render::Entity::apply_humanoid_pose_policy(policy, inputs, pose);
    EXPECT_NE(pose.head_pos, base.head_pos)
        << "pose policy " << static_cast<int>(policy) << " left the pose untouched";
  }

  Render::GL::HumanoidPose cast = base;
  Render::Entity::apply_humanoid_pose_policy(
      Render::Humanoid::HumanoidPosePolicy::GravePriestCast, inputs, cast);

  Render::GL::HumanoidAnimationContext resting{};
  Render::Entity::HumanoidPosePolicyInputs resting_inputs;
  resting_inputs.animation = &resting;
  Render::GL::HumanoidPose idle = base;
  Render::Entity::apply_humanoid_pose_policy(
      Render::Humanoid::HumanoidPosePolicy::GravePriestCast, resting_inputs, idle);

  EXPECT_GT(cast.hand_r.z(), idle.hand_r.z())
      << "the grave priest must push his casting hand forward";
  EXPECT_GT(cast.hand_r.y(), idle.hand_r.y())
      << "the grave priest must raise his casting hand";
}

TEST(HumanoidPrepare, FacialHairUsesBakedArchetypeWithoutPostBodyDraw) {
  BeardRenderer const renderer;
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;
  Render::GL::AnimationInputs const anim{};

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      renderer, ctx, anim, test_runtime(0U), prep);

  ASSERT_EQ(prep.bodies.requests().size(), 1U);
  EXPECT_TRUE(prep.post_body_draws.empty());

  auto const& req = prep.bodies.requests().front();
  EXPECT_NE(req.archetype, Render::Creature::ArchetypeRegistry::k_humanoid_base);

  auto const* desc = Render::Creature::ArchetypeRegistry::instance().get(req.archetype);
  ASSERT_NE(desc, nullptr);
  EXPECT_EQ(desc->bake_attachment_count, 1U);
  EXPECT_GT(req.role_color_count, 7U);
}

TEST(HumanoidPrepare, BuiltInArchersPreserveBowRoleColorsBeyondLegacyLimit) {
  EXPECT_GT(render_archer_role_color_count("troops/roman/archer"), 16U);
  EXPECT_GT(render_archer_role_color_count("troops/carthage/archer"), 16U);
}

TEST(HumanoidPrepare, AmbientIdleVariantsAreSuppressedWhenIdleIsRemapped) {
  using Render::Creature::AnimationStateId;
  using Render::Creature::Pipeline::humanoid_clip_variant_for_state;
  auto& registry = Render::Creature::ArchetypeRegistry::instance();

  Render::GL::HumanoidAnimationContext anim{};
  anim.ambient_idle_type = Render::GL::AmbientIdleType::SitDown;
  anim.ambient_idle_blend = 1.0F;

  auto const base_id = Render::Creature::ArchetypeRegistry::k_humanoid_base;
  ASSERT_EQ(registry.bpat_clip(base_id, AnimationStateId::Idle),
            Render::Creature::k_humanoid_idle_clip);
  EXPECT_EQ(humanoid_clip_variant_for_state(base_id, anim, AnimationStateId::Idle),
            Render::GL::ambient_idle_clip_variant(Render::GL::AmbientIdleType::SitDown))
      << "a plain humanoid should still get its ambient idle variant";

  auto const archer_id = find_archetype_id("troops/roman/archer");
  ASSERT_NE(archer_id, Render::Creature::k_invalid_archetype);
  ASSERT_NE(registry.bpat_clip(archer_id, AnimationStateId::Idle),
            Render::Creature::k_humanoid_idle_clip)
      << "test premise: archers remap Idle away from the idle clip";
  EXPECT_EQ(humanoid_clip_variant_for_state(archer_id, anim, AnimationStateId::Idle),
            0U)
      << "archers must fall back to variant 0 instead of indexing off the bow clip";

  auto const rider_id = Render::Creature::ArchetypeRegistry::k_rider_base;
  if (registry.bpat_clip(rider_id, AnimationStateId::Idle) !=
      Render::Creature::k_humanoid_idle_clip) {
    EXPECT_EQ(humanoid_clip_variant_for_state(rider_id, anim, AnimationStateId::Idle),
              0U)
        << "riders must not index ambient variants off the riding idle clip";
  }
}

TEST(HumanoidPrepare, BuiltInArchersUseBowReadyIdleClip) {
  using Render::Creature::AnimationStateId;
  auto& registry = Render::Creature::ArchetypeRegistry::instance();

  auto const* roman_idle_palette = render_archer_idle_bone_palette(
      "troops/roman/archer", Game::Systems::NationID::RomanRepublic);
  auto const roman_id = find_archetype_id("troops/roman/archer");
  ASSERT_NE(roman_id, Render::Creature::k_invalid_archetype);
  EXPECT_EQ(registry.bpat_clip(roman_id, AnimationStateId::Idle),
            registry.bpat_clip(roman_id, AnimationStateId::AttackBow));
  EXPECT_EQ(roman_idle_palette, request_idle_bone_palette(roman_id, 0.5F));
  EXPECT_NE(roman_idle_palette, request_idle_bone_palette(roman_id, 0.0F));

  auto const* carthage_idle_palette = render_archer_idle_bone_palette(
      "troops/carthage/archer", Game::Systems::NationID::Carthage);
  auto const carthage_id = find_archetype_id("troops/carthage/archer");
  ASSERT_NE(carthage_id, Render::Creature::k_invalid_archetype);
  EXPECT_EQ(registry.bpat_clip(carthage_id, AnimationStateId::Idle),
            registry.bpat_clip(carthage_id, AnimationStateId::AttackBow));
  EXPECT_EQ(carthage_idle_palette, request_idle_bone_palette(carthage_id, 0.5F));
  EXPECT_NE(carthage_idle_palette, request_idle_bone_palette(carthage_id, 0.0F));
}

TEST(HumanoidPrepare,
     MotionSnapshotDrivenInfantryMovementChangesVisibleLowerBodyPoseOverTime) {
  EXPECT_TRUE(moving_palette_changes_over_time("troops/roman/swordsman",
                                               Game::Units::SpawnType::Knight,
                                               Game::Systems::NationID::RomanRepublic,
                                               true));
  EXPECT_TRUE(moving_palette_changes_over_time("troops/roman/archer",
                                               Game::Units::SpawnType::Archer,
                                               Game::Systems::NationID::RomanRepublic,
                                               true));
  EXPECT_TRUE(moving_palette_changes_over_time("troops/roman/spearman",
                                               Game::Units::SpawnType::Spearman,
                                               Game::Systems::NationID::RomanRepublic,
                                               true));
}

TEST(HumanoidPrepare, PersistentEntitySwordsmanWalkRequestAdvancesPhaseOverTime) {
  class FixedSpecRenderer final : public Render::GL::HumanoidRendererBase {
  public:
    explicit FixedSpecRenderer(Render::Creature::Pipeline::UnitVisualSpec spec)
        : Render::GL::HumanoidRendererBase(std::move(spec)) {}
  };

  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  auto const warm_renderer = registry.get("troops/roman/swordsman");
  ASSERT_TRUE(static_cast<bool>(warm_renderer));
  if (warm_renderer) {
    Render::GL::DrawContext warm_ctx{};
    warm_ctx.allow_template_cache = false;
    Engine::Core::StandaloneEntity warm_entity_scratch(997);
    Engine::Core::Entity& warm_entity = warm_entity_scratch.entity();
    auto* warm_unit =
        warm_entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
    ASSERT_NE(warm_unit, nullptr);
    warm_unit->spawn_type = Game::Units::SpawnType::Knight;
    warm_unit->nation_id = Game::Systems::NationID::RomanRepublic;
    warm_ctx.entity = &warm_entity;
    CountingSubmitter warm_sink;
    warm_renderer(warm_ctx, warm_sink);
  }

  auto const archetype_id = find_archetype_id("troops/roman/swordsman");
  ASSERT_NE(archetype_id, Render::Creature::k_invalid_archetype);

  Render::Creature::Pipeline::UnitVisualSpec spec{};
  spec.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
  spec.debug_name = "troops/roman/swordsman";
  spec.archetype_id = archetype_id;
  spec.creature_asset_id = Render::Creature::Pipeline::k_humanoid_sword_asset;
  FixedSpecRenderer owner(spec);

  Engine::Core::StandaloneEntity entity_scratch(1);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  auto* movement = entity.add_component<Engine::Core::MovementComponent>();
  auto* transform = entity.add_component<Engine::Core::TransformComponent>();
  auto* motion = entity.add_component<Engine::Core::MotionPresentationComponent>();
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(transform, nullptr);
  ASSERT_NE(motion, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Knight;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  MovementTestAccess::set_has_target(*movement, true);
  MovementTestAccess::set_target_x(*movement, 6.0F);
  MovementTestAccess::set_target_y(*movement, 0.0F);
  MovementTestAccess::set_vx(*movement, 1.0F);
  MovementTestAccess::set_vz(*movement, 0.0F);
  transform->position = {0.0F, 0.0F, 0.0F};
  motion->initialized = true;
  motion->snapshot_valid = true;
  motion->set_state(Engine::Core::MotionPresentationState::Walk);
  motion->has_navigation_intent = true;
  motion->has_movement_target = true;
  motion->movement_target_x = 6.0F;
  motion->movement_target_z = 0.0F;
  motion->direction_x = 1.0F;
  motion->direction_z = 0.0F;
  motion->speed = 1.0F;

  auto request_for_time = [&](float animation_time) {
    Render::GL::DrawContext ctx{};
    ctx.world_view = Render::WorldView::of_active_session();
    ctx.allow_template_cache = false;
    ctx.force_humanoid_lod = true;
    ctx.forced_humanoid_lod = Render::Creature::CreatureLOD::Full;
    ctx.animation_time = animation_time;
    ctx.entity = &entity;
    auto const anim = Render::GL::sample_anim_state(ctx);
    Render::Humanoid::HumanoidPreparation prep;
    Render::Humanoid::prepare_humanoid_instances(
        owner, ctx, anim, test_runtime(0U), prep);
    EXPECT_FALSE(prep.bodies.requests().empty());
    return prep.bodies.requests().front();
  };

  auto const first = request_for_time(0.10F);
  Render::GL::begin_humanoid_frame();
  auto const second = request_for_time(0.70F);

  EXPECT_EQ(first.state, Render::Creature::AnimationStateId::Walk);
  EXPECT_EQ(second.state, Render::Creature::AnimationStateId::Walk);
  EXPECT_NE(first.phase, second.phase);
}

TEST(HumanoidPrepare, StoppingAUnitBlendsTheStrideOutInsteadOfCutting) {
  class FixedSpecRenderer final : public Render::GL::HumanoidRendererBase {
  public:
    explicit FixedSpecRenderer(Render::Creature::Pipeline::UnitVisualSpec spec)
        : Render::GL::HumanoidRendererBase(std::move(spec)) {}
  };

  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  auto const warm_renderer = registry.get("troops/roman/swordsman");
  ASSERT_TRUE(static_cast<bool>(warm_renderer));
  {
    Render::GL::DrawContext warm_ctx{};
    warm_ctx.allow_template_cache = false;
    Engine::Core::StandaloneEntity warm_entity_scratch(998);
    Engine::Core::Entity& warm_entity = warm_entity_scratch.entity();
    auto* warm_unit =
        warm_entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
    ASSERT_NE(warm_unit, nullptr);
    warm_unit->spawn_type = Game::Units::SpawnType::Knight;
    warm_unit->nation_id = Game::Systems::NationID::RomanRepublic;
    warm_ctx.entity = &warm_entity;
    CountingSubmitter warm_sink;
    warm_renderer(warm_ctx, warm_sink);
  }

  auto const archetype_id = find_archetype_id("troops/roman/swordsman");
  ASSERT_NE(archetype_id, Render::Creature::k_invalid_archetype);

  Render::Creature::Pipeline::UnitVisualSpec spec{};
  spec.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
  spec.debug_name = "troops/roman/swordsman";
  spec.archetype_id = archetype_id;
  spec.creature_asset_id = Render::Creature::Pipeline::k_humanoid_sword_asset;
  FixedSpecRenderer owner(spec);

  Engine::Core::StandaloneEntity entity_scratch(1);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  auto* movement = entity.add_component<Engine::Core::MovementComponent>();
  auto* transform = entity.add_component<Engine::Core::TransformComponent>();
  auto* motion = entity.add_component<Engine::Core::MotionPresentationComponent>();
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(transform, nullptr);
  ASSERT_NE(motion, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Knight;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  transform->position = {0.0F, 0.0F, 0.0F};
  motion->initialized = true;
  motion->snapshot_valid = true;
  motion->direction_x = 0.0F;
  motion->direction_z = 1.0F;

  auto walk = [&]() {
    MovementTestAccess::set_has_target(*movement, true);
    MovementTestAccess::set_target_x(*movement, 0.0F);
    MovementTestAccess::set_target_y(*movement, 40.0F);
    MovementTestAccess::set_vx(*movement, 0.0F);
    MovementTestAccess::set_vz(*movement, 2.2F);
    motion->set_state(Engine::Core::MotionPresentationState::Walk);
    motion->has_navigation_intent = true;
    motion->has_movement_target = true;
    motion->movement_target_x = 0.0F;
    motion->movement_target_z = 40.0F;
    motion->speed = 2.2F;
  };
  auto stand = [&]() {
    MovementTestAccess::set_has_target(*movement, false);
    MovementTestAccess::set_vx(*movement, 0.0F);
    MovementTestAccess::set_vz(*movement, 0.0F);
    motion->set_state(Engine::Core::MotionPresentationState::Idle);
    motion->has_navigation_intent = false;
    motion->has_movement_target = false;
    motion->speed = 0.0F;
  };

  auto request_for_time = [&](float animation_time) {
    Render::GL::DrawContext ctx{};
    ctx.world_view = Render::WorldView::of_active_session();
    ctx.allow_template_cache = false;
    ctx.force_humanoid_lod = true;
    ctx.forced_humanoid_lod = Render::Creature::CreatureLOD::Full;
    ctx.animation_time = animation_time;
    ctx.entity = &entity;
    auto const anim = Render::GL::sample_anim_state(ctx);
    Render::Humanoid::HumanoidPreparation prep;
    Render::Humanoid::prepare_humanoid_instances(
        owner, ctx, anim, test_runtime(0U), prep);
    EXPECT_FALSE(prep.bodies.requests().empty());
    Render::GL::begin_humanoid_frame();
    return prep.bodies.requests().front();
  };

  constexpr float k_step = 1.0F / 60.0F;
  float time = 0.0F;
  walk();
  Render::Creature::CreatureRenderRequest settled{};
  for (int frame = 0; frame < 90; ++frame) {
    time += k_step;
    settled = request_for_time(time);
  }
  ASSERT_EQ(settled.state, Render::Creature::AnimationStateId::Walk);
  EXPECT_FALSE(settled.full_body_blend.active())
      << "a settled walk is one clip, not a blend";

  stand();
  std::vector<float> stand_weights;
  std::vector<float> phases;
  for (int frame = 0; frame < 12; ++frame) {
    time += k_step;
    auto const request = request_for_time(time);
    if (request.full_body_blend.active() &&
        request.full_body_blend.state == Render::Creature::AnimationStateId::Walk) {
      stand_weights.push_back(1.0F - request.full_body_blend.weight);
      phases.push_back(request.full_body_blend.phase);
    } else if (request.full_body_blend.active()) {
      stand_weights.push_back(request.full_body_blend.weight);
      phases.push_back(request.phase);
    }
  }

  ASSERT_GE(stand_weights.size(), 8U) << "the stride vanished instead of blending out";
  EXPECT_LT(stand_weights.front(), 0.35F) << "the stand should not appear all at once";
  EXPECT_GT(stand_weights.back(), stand_weights.front())
      << "the stand should keep taking over";

  ASSERT_GE(phases.size(), 8U);
  EXPECT_NE(phases.front(), phases.back());
}

TEST(HumanoidPrepare, MultiSoldierCombatFallbackOffsetsAttackPhasePerSoldier) {
  class FixedSpecRenderer final : public Render::GL::HumanoidRendererBase {
  public:
    explicit FixedSpecRenderer(Render::Creature::Pipeline::UnitVisualSpec spec)
        : Render::GL::HumanoidRendererBase(std::move(spec)) {}
  };

  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  auto const warm_renderer = registry.get("troops/roman/swordsman");
  ASSERT_TRUE(static_cast<bool>(warm_renderer));
  if (warm_renderer) {
    Render::GL::DrawContext warm_ctx{};
    warm_ctx.allow_template_cache = false;
    Engine::Core::StandaloneEntity warm_entity_scratch(996);
    Engine::Core::Entity& warm_entity = warm_entity_scratch.entity();
    auto* warm_unit =
        warm_entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
    ASSERT_NE(warm_unit, nullptr);
    warm_unit->spawn_type = Game::Units::SpawnType::Knight;
    warm_unit->nation_id = Game::Systems::NationID::RomanRepublic;
    warm_ctx.entity = &warm_entity;
    CountingSubmitter warm_sink;
    warm_renderer(warm_ctx, warm_sink);
  }

  auto const archetype_id = find_archetype_id("troops/roman/swordsman");
  ASSERT_NE(archetype_id, Render::Creature::k_invalid_archetype);

  Render::Creature::Pipeline::UnitVisualSpec spec{};
  spec.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
  spec.debug_name = "troops/roman/swordsman";
  spec.archetype_id = archetype_id;
  spec.creature_asset_id = Render::Creature::Pipeline::k_humanoid_sword_asset;
  FixedSpecRenderer const owner(spec);

  Engine::Core::StandaloneEntity entity_scratch(2);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Knight;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  unit->render_individuals_per_unit_override = 15;

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.allow_template_cache = false;
  ctx.force_humanoid_lod = true;
  ctx.forced_humanoid_lod = Render::Creature::CreatureLOD::Full;
  ctx.animation_time = 0.35F;
  ctx.entity = &entity;

  Render::GL::AnimationInputs anim{};
  anim.time = ctx.animation_time;
  anim.is_attacking = true;
  anim.is_melee = true;
  anim.attack_family = Engine::Core::CombatAttackFamily::Sword;
  anim.attack_variant = 1U;
  anim.attack_offset = 0.20F;
  anim.has_attack_offset = true;

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, anim, test_runtime(0U), prep);

  ASSERT_GE(prep.bodies.requests().size(), 15U);

  std::vector<float> phases;
  std::unordered_set<Render::Creature::AnimationStateId> states;
  phases.reserve(prep.bodies.requests().size());
  for (const auto& req : prep.bodies.requests()) {
    states.insert(req.state);
    if (req.state == Render::Creature::AnimationStateId::AttackSword) {
      phases.push_back(req.phase);
    }
  }

  EXPECT_EQ(states.size(), 1U);
  EXPECT_EQ(states.count(Render::Creature::AnimationStateId::AttackSword), 1U);

  ASSERT_GE(phases.size(), 2U);
  std::size_t distinct_phase_count = 0U;
  std::vector<float> unique_phases;
  for (float const phase : phases) {
    bool duplicate = false;
    for (float const unique_phase : unique_phases) {
      if (std::abs(phase - unique_phase) <= 1.0e-5F) {
        duplicate = true;
        break;
      }
    }
    if (!duplicate) {
      unique_phases.push_back(phase);
      ++distinct_phase_count;
    }
  }

  EXPECT_GT(distinct_phase_count, 1U);
  EXPECT_TRUE(std::any_of(phases.begin(), phases.end(), [](float phase) {
    return std::abs(phase) > 1.0e-5F;
  }));
  auto const [min_phase, max_phase] = std::minmax_element(phases.begin(), phases.end());
  ASSERT_NE(min_phase, phases.end());
  ASSERT_NE(max_phase, phases.end());
  float const spread = *max_phase - *min_phase;

  EXPECT_GT(spread, 0.20F);
  EXPECT_LT(spread, 0.36F);
}

TEST(HumanoidPrepare, BuiltInBuildersSubmitRiggedGeometry) {
  EXPECT_GT(render_builder_submission_count(
                "troops/roman/builder", Game::Systems::NationID::RomanRepublic, false),
            0);
  EXPECT_GT(render_builder_submission_count(
                "troops/carthage/builder", Game::Systems::NationID::Carthage, false),
            0);
}

TEST(HumanoidPrepare, BuiltInCiviliansSubmitRiggedGeometry) {
  EXPECT_GT(render_civilian_submission_count("troops/roman/civilian",
                                             Game::Systems::NationID::RomanRepublic),
            0);
  EXPECT_GT(render_civilian_submission_count("troops/carthage/civilian",
                                             Game::Systems::NationID::Carthage),
            0);
}

TEST(HumanoidPrepare, UnarmedSupportArchetypesRouteMeleeToDedicatedBpatClips) {
  EXPECT_GT(render_builder_submission_count(
                "troops/roman/builder", Game::Systems::NationID::RomanRepublic, false),
            0);
  EXPECT_GT(render_builder_submission_count(
                "troops/carthage/builder", Game::Systems::NationID::Carthage, false),
            0);
  EXPECT_GT(render_civilian_submission_count("troops/roman/civilian",
                                             Game::Systems::NationID::RomanRepublic),
            0);
  EXPECT_GT(render_civilian_submission_count("troops/carthage/civilian",
                                             Game::Systems::NationID::Carthage),
            0);
  (void)render_runtime_mesh_count("troops/roman/healer",
                                  Game::Units::SpawnType::Healer,
                                  Game::Systems::NationID::RomanRepublic,
                                  false);
  (void)render_runtime_mesh_count("troops/carthage/healer",
                                  Game::Units::SpawnType::Healer,
                                  Game::Systems::NationID::Carthage,
                                  false);

  auto& registry = Render::Creature::ArchetypeRegistry::instance();
  constexpr std::array<std::string_view, 6> k_unarmed_roles{{
      "troops/roman/builder",
      "troops/carthage/builder",
      "troops/roman/civilian",
      "troops/carthage/civilian",
      "troops/roman/healer",
      "troops/carthage/healer",
  }};
  for (auto const role : k_unarmed_roles) {
    auto const archetype = find_archetype_id(role);
    ASSERT_NE(archetype, Render::Creature::k_invalid_archetype) << role;
    EXPECT_EQ(registry.clip_variant_count(
                  archetype, Render::Creature::AnimationStateId::AttackMelee),
              Animation::k_humanoid_unarmed_variant_count)
        << role;
    EXPECT_EQ(registry.resolve_bpat_clip(
                  archetype, Render::Creature::AnimationStateId::AttackMelee, 0U),
              Animation::k_humanoid_unarmed_jab_clip)
        << role;
    EXPECT_EQ(registry.resolve_bpat_clip(
                  archetype, Render::Creature::AnimationStateId::AttackMelee, 1U),
              Animation::k_humanoid_unarmed_cross_clip)
        << role;
    EXPECT_EQ(registry.resolve_bpat_clip(
                  archetype, Render::Creature::AnimationStateId::AttackMelee, 2U),
              Animation::k_humanoid_unarmed_hook_clip)
        << role;
  }

  auto const armed_healer = find_archetype_id("troops/carthage/healer");
  auto const unarmed_healer = find_archetype_id("troops/carthage/healer/unarmed");
  ASSERT_NE(armed_healer, Render::Creature::k_invalid_archetype);
  ASSERT_NE(unarmed_healer, Render::Creature::k_invalid_archetype);
  auto const* armed_desc = registry.get(armed_healer);
  auto const* unarmed_desc = registry.get(unarmed_healer);
  ASSERT_NE(armed_desc, nullptr);
  ASSERT_NE(unarmed_desc, nullptr);
  EXPECT_EQ(armed_desc->bake_attachment_count, unarmed_desc->bake_attachment_count + 1U)
      << "the Carthaginian healer must put the stave away for bare-hand combat";
}

TEST(HumanoidPrepare, BuiltInBuildersSubmitRiggedGeometryWhileConstructing) {
  EXPECT_GT(render_builder_submission_count(
                "troops/roman/builder", Game::Systems::NationID::RomanRepublic, true),
            0);
  EXPECT_GT(render_builder_submission_count(
                "troops/carthage/builder", Game::Systems::NationID::Carthage, true),
            0);
}

TEST(HumanoidPrepare, BuiltInBuildersVisibleIdleGeometryTouchesTerrain) {
  EXPECT_NEAR(render_builder_min_world_y(
                  "troops/roman/builder", Game::Systems::NationID::RomanRepublic, 2.4F),
              2.4F,
              0.25F);
  EXPECT_NEAR(render_builder_min_world_y(
                  "troops/carthage/builder", Game::Systems::NationID::Carthage, 2.4F),
              2.4F,
              0.25F);
}

TEST(HumanoidPrepare, BuilderConstructionFormationFacesInward) {
  auto const layout =
      Game::Formation::UnitLayoutLibrary::instance().resolve("rome", "work_party");
  ASSERT_NE(layout, Game::Formation::k_invalid_layout);

  float const spacing = 2.0F;
  constexpr int total = 8;
  for (int idx = 0; idx < total; ++idx) {
    Game::Formation::UnitLayoutQuery query;
    query.layout = layout;
    query.index = idx;
    query.row = 0;
    query.col = idx;
    query.rows = 1;
    query.cols = total;
    query.spacing = spacing;
    query.seed = 0x12345678U;
    auto const offset = Game::Formation::UnitLayoutSystem::instance().offset(query);
    float const yaw_rad = offset.yaw_offset * (std::numbers::pi_v<float> / 180.0F);
    float const world_x = offset.offset_x;
    float const world_z = offset.offset_z;

    float const face_x = std::sin(yaw_rad);
    float const face_z = std::cos(yaw_rad);

    float const world_r = std::sqrt(world_x * world_x + world_z * world_z);
    ASSERT_GT(world_r, 0.001F);
    float const inward_x = -world_x / world_r;
    float const inward_z = -world_z / world_r;
    EXPECT_NEAR(face_x, inward_x, 0.01F) << "idx=" << idx;
    EXPECT_NEAR(face_z, inward_z, 0.01F) << "idx=" << idx;
  }
}

TEST(HumanoidPrepare, ActiveBuilderWorkOverridesSharedTravellingRowsWithCircle) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(4242);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 2.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Builder;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  unit->render_individuals_per_unit_override = 4;
  entity.add_component<Engine::Core::TransformComponent>();
  auto* shared = entity.add_component<Engine::Core::FormationPresentationComponent>();
  ASSERT_NE(shared, nullptr);
  shared->rows = 2;
  shared->cols = 2;
  for (std::uint16_t index = 0; index < 4U; ++index) {
    shared->soldiers.push_back({.slot_index = index,
                                .row = static_cast<std::uint16_t>(index / 2U),
                                .col = static_cast<std::uint16_t>(index % 2U),
                                .local_x = 20.0F + static_cast<float>(index),
                                .local_z = 30.0F,
                                .alive = true});
  }
  ctx.entity = &entity;

  Render::GL::AnimationInputs anim{};
  anim.is_constructing = true;
  anim.construction_progress = 0.5F;
  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, anim, test_runtime(0U), prep);

  auto const* cache =
      entity.get_component<Render::Humanoid::HumanoidInstanceStateComponent>();
  ASSERT_NE(cache, nullptr);
  ASSERT_EQ(cache->layout.instances.size(), 4U);
  EXPECT_EQ(
      cache->layout.unit_layout,
      Game::Formation::UnitLayoutLibrary::instance().resolve("rome", "work_party"));
  for (auto const& soldier : cache->layout.instances) {
    EXPECT_LT(std::abs(soldier.offset_x), 10.0F);
    EXPECT_LT(std::abs(soldier.offset_z), 10.0F);
    EXPECT_GT(std::hypot(soldier.offset_x, soldier.offset_z), 1.0F);
  }
}

TEST(HumanoidPrepare, BuilderConstructionPlaybackUsesWorkClip) {
  using Render::Creature::AnimationStateId;
  using Render::Creature::Pipeline::humanoid_bpat_playback_for_anim;
  using Render::Creature::Pipeline::humanoid_clip_variant_for_anim;

  EXPECT_GT(render_builder_submission_count(
                "troops/roman/builder", Game::Systems::NationID::RomanRepublic, true),
            0);

  auto& registry = Render::Creature::ArchetypeRegistry::instance();
  auto const builder_id = find_archetype_id("troops/roman/builder");
  ASSERT_NE(builder_id, Render::Creature::k_invalid_archetype);

  Render::GL::HumanoidAnimationContext construct_anim{};
  construct_anim.inputs.is_constructing = true;
  construct_anim.inputs.construction_progress = 0.35F;
  construct_anim.jitter_seed = 0.11F;

  auto const playback = humanoid_bpat_playback_for_anim(
      builder_id, Render::Creature::Bpat::k_species_humanoid, construct_anim);
  ASSERT_TRUE(playback.has_value());
  EXPECT_EQ(playback->clip_id,
            registry.resolve_bpat_clip(
                builder_id,
                AnimationStateId::AttackSword,
                humanoid_clip_variant_for_anim(builder_id, construct_anim)));
  EXPECT_NE(playback->clip_id, registry.bpat_clip(builder_id, AnimationStateId::Idle));
}

TEST(AnimationCoreClipVariantManifest, RenderClipVariantUsesAnimationCorePolicy) {
  using Render::Creature::AnimationStateId;
  using Render::Creature::ArchetypeRegistry;
  using Render::Creature::Pipeline::humanoid_clip_variant_for_state;

  auto const archetype = ArchetypeRegistry::k_humanoid_base;

  Render::GL::HumanoidAnimationContext attack_anim{};
  attack_anim.inputs.attack_variant = 2U;
  EXPECT_EQ(
      humanoid_clip_variant_for_state(
          archetype, attack_anim, AnimationStateId::AttackSword),
      Animation::resolve_humanoid_clip_variant({
          .state = Animation::StateId::AttackSword,
          .attack_variant = 2U,
          .available_variant_count = Animation::k_humanoid_attack_sword_variant_count,
      }));

  Render::GL::HumanoidAnimationContext construction_anim{};
  construction_anim.inputs.is_constructing = true;
  construction_anim.construction_role = Render::GL::ConstructionRole::Saw;
  EXPECT_EQ(
      humanoid_clip_variant_for_state(
          archetype, construction_anim, AnimationStateId::AttackSword),
      Animation::resolve_humanoid_clip_variant({
          .state = Animation::StateId::AttackSword,
          .is_constructing = true,
          .construction_role = Animation::HumanoidConstructionRole::Saw,
          .available_variant_count = Animation::k_humanoid_attack_sword_variant_count,
      }));

  Render::GL::HumanoidAnimationContext idle_anim{};
  idle_anim.ambient_idle_type = Render::GL::AmbientIdleType::PlantFlag;
  EXPECT_EQ(
      humanoid_clip_variant_for_state(archetype, idle_anim, AnimationStateId::Idle),
      Animation::resolve_humanoid_clip_variant({
          .state = Animation::StateId::Idle,
          .ambient_idle = Animation::HumanoidAmbientIdle::PlantFlag,
          .available_variant_count = Animation::k_humanoid_idle_variant_count,
      }));
}

TEST(AnimationCoreClipVariantManifest,
     ConstructionRoleSelectionUsesAnimationCorePolicy) {
  using Animation::HumanoidConstructionRole;

  EXPECT_EQ(Animation::humanoid_construction_role_for_variant_index(0U),
            HumanoidConstructionRole::Hammer);
  EXPECT_EQ(Animation::humanoid_construction_role_for_variant_index(1U),
            HumanoidConstructionRole::Saw);
  EXPECT_EQ(Animation::humanoid_construction_role_for_variant_index(2U),
            HumanoidConstructionRole::Chisel);
  EXPECT_EQ(Animation::humanoid_construction_role_for_variant_index(3U),
            HumanoidConstructionRole::KneelingChisel);
  EXPECT_EQ(Animation::humanoid_construction_role_for_variant_index(4U),
            HumanoidConstructionRole::Hammer);

  EXPECT_EQ(Animation::resolve_humanoid_construction_role({
                .seed = 123U,
                .force_single_soldier = true,
                .variant_table_can_select_roles = true,
                .variant_stride = 4U,
                .variant_is_seed_based = true,
            }),
            HumanoidConstructionRole::Hammer);
  EXPECT_EQ(Animation::resolve_humanoid_construction_role({
                .seed = 123U,
                .variant_table_can_select_roles = false,
                .variant_stride = 4U,
                .variant_is_seed_based = true,
            }),
            HumanoidConstructionRole::Hammer);

  for (std::uint8_t variant_index = 0U; variant_index < 4U; ++variant_index) {
    auto const expected =
        Animation::humanoid_construction_role_for_variant_index(variant_index);
    bool found_seed = false;
    for (std::uint32_t seed = 0U; seed < 1024U && !found_seed; ++seed) {
      found_seed = Animation::resolve_humanoid_construction_role({
                       .seed = seed,
                       .variant_table_can_select_roles = true,
                       .variant_stride = 4U,
                       .variant_is_seed_based = true,
                   }) == expected;
    }
    EXPECT_TRUE(found_seed) << "variant_index=" << unsigned(variant_index);
  }
}

TEST(AnimationCorePlaybackManifest, RenderHoldAndGuardUseAnimationCorePolicy) {
  Render::GL::AnimationInputs hold_inputs{};
  hold_inputs.is_in_hold_mode = true;
  hold_inputs.hold_entry_progress = 0.5F;
  EXPECT_FLOAT_EQ(Render::GL::hold_transition_amount(hold_inputs),
                  Animation::humanoid_hold_transition_amount({
                      .is_in_hold_mode = true,
                      .hold_entry_progress = 0.5F,
                  }));

  Render::GL::AnimationInputs guard_inputs{};
  guard_inputs.is_guarding = true;
  guard_inputs.guard_pose_progress = 0.25F;
  EXPECT_FLOAT_EQ(Render::GL::guard_pose_amount(guard_inputs),
                  Animation::humanoid_guard_pose_amount({
                      .is_guarding = true,
                      .guard_pose_progress = 0.25F,
                  }));
}

TEST(AnimationCorePlaybackManifest, RenderPlaybackPhaseUsesAnimationCorePolicy) {
  using Render::Creature::AnimationStateId;
  using Render::Creature::MovementAnimationState;
  using Render::Creature::Pipeline::humanoid_phase_for_state;

  Render::GL::HumanoidAnimationContext anim{};

  anim.inputs.death_progress = 1.3F;
  EXPECT_FLOAT_EQ(humanoid_phase_for_state(anim, AnimationStateId::Die),
                  Animation::resolve_humanoid_playback_phase({
                      .state = Animation::StateId::Die,
                      .death_progress = 1.3F,
                  }));

  anim = {};
  anim.inputs.is_in_hold_mode = true;
  anim.inputs.hold_entry_progress = 0.5F;
  EXPECT_FLOAT_EQ(humanoid_phase_for_state(anim, AnimationStateId::Hold),
                  Animation::resolve_humanoid_playback_phase({
                      .state = Animation::StateId::Hold,
                      .is_in_hold_mode = true,
                      .hold_entry_progress = 0.5F,
                  }));

  anim = {};
  anim.inputs.is_constructing = true;
  anim.inputs.construction_progress = 0.8F;
  anim.jitter_seed = 0.45F;
  EXPECT_NEAR(humanoid_phase_for_state(anim, AnimationStateId::AttackSword),
              Animation::resolve_humanoid_playback_phase({
                  .state = Animation::StateId::AttackSword,
                  .is_constructing = true,
                  .construction_progress = 0.8F,
                  .construction_jitter_seed = 0.45F,
              }),
              1.0e-6F);

  anim = {};
  anim.attack_phase = 0.61F;
  EXPECT_FLOAT_EQ(humanoid_phase_for_state(anim, AnimationStateId::AttackSpear),
                  Animation::resolve_humanoid_playback_phase({
                      .state = Animation::StateId::AttackSpear,
                      .attack_phase = 0.61F,
                  }));

  anim = {};
  anim.ambient_idle_type = Render::GL::AmbientIdleType::RaiseWeapon;
  anim.ambient_idle_phase = 0.37F;
  EXPECT_FLOAT_EQ(humanoid_phase_for_state(anim, AnimationStateId::Idle),
                  Animation::resolve_humanoid_playback_phase({
                      .state = Animation::StateId::Idle,
                      .ambient_idle = Animation::HumanoidAmbientIdle::RaiseWeapon,
                      .ambient_idle_phase = 0.37F,
                  }));

  anim = {};
  anim.inputs.is_mounted = true;
  anim.inputs.is_attacking = true;
  anim.inputs.is_melee = true;
  anim.inputs.movement_state = MovementAnimationState::Idle;
  anim.attack_phase = 0.5F;
  EXPECT_FLOAT_EQ(humanoid_phase_for_state(anim, AnimationStateId::RidingCharge),
                  Animation::resolve_humanoid_playback_phase({
                      .state = Animation::StateId::RidingCharge,
                      .is_mounted = true,
                      .is_attacking = true,
                      .is_melee = true,
                      .movement_state = Animation::MovementState::Idle,
                      .attack_phase = 0.5F,
                  }));

  anim = {};
  anim.gait.cycle_phase = 0.82F;
  EXPECT_FLOAT_EQ(humanoid_phase_for_state(anim, AnimationStateId::Walk),
                  Animation::resolve_humanoid_playback_phase({
                      .state = Animation::StateId::Walk,
                      .gait_cycle_phase = 0.82F,
                  }));
}

TEST(AnimationCorePlaybackManifest, SettledHoldUsesItsBreathingCycle) {
  EXPECT_FLOAT_EQ(Animation::resolve_humanoid_playback_phase({
                      .state = Animation::StateId::Hold,
                      .is_in_hold_mode = true,
                      .hold_entry_progress = 1.0F,
                      .gait_cycle_phase = 0.37F,
                  }),
                  0.37F);
}

TEST(AnimationCorePlaybackManifest, ConstructionPoseEasesInAndRetainsItsRole) {
  Animation::HumanoidConstructionTransitionState state{};
  auto sample = Animation::resolve_humanoid_construction_transition({
      .state = state,
      .constructing = false,
      .sample_time = 1.0F,
  });

  sample = Animation::resolve_humanoid_construction_transition({
      .state = sample.state,
      .constructing = true,
      .sample_time = 1.10F,
      .construction_phase = 0.42F,
      .role = Animation::HumanoidConstructionRole::Saw,
  });

  EXPECT_GT(sample.pose_weight, 0.0F);
  EXPECT_LT(sample.pose_weight, 1.0F);
  EXPECT_FLOAT_EQ(sample.phase, 0.42F);
  EXPECT_EQ(sample.role, Animation::HumanoidConstructionRole::Saw);
}

TEST(AnimationCorePlaybackManifest, ColdConstructionStateStartsInWorkPose) {
  auto const sample = Animation::resolve_humanoid_construction_transition({
      .constructing = true,
      .sample_time = 3.25F,
      .construction_phase = 0.42F,
      .role = Animation::HumanoidConstructionRole::Hammer,
  });

  EXPECT_FLOAT_EQ(sample.pose_weight, 1.0F);
  EXPECT_FLOAT_EQ(sample.phase, 0.42F);
  EXPECT_EQ(sample.role, Animation::HumanoidConstructionRole::Hammer);
}

TEST(AnimationCorePlaybackManifest, ConstructionPoseEasesOutFromTheLastWorkFrame) {
  Animation::HumanoidConstructionTransitionState state{};
  state.initialized = true;
  state.blend = 1.0F;
  state.last_sample_time = 2.0F;
  state.retained_phase = 0.68F;
  state.retained_role = Animation::HumanoidConstructionRole::Hammer;

  auto const sample = Animation::resolve_humanoid_construction_transition({
      .state = state,
      .constructing = false,
      .sample_time = 2.10F,
  });

  EXPECT_GT(sample.pose_weight, 0.0F);
  EXPECT_LT(sample.pose_weight, 1.0F);
  EXPECT_FLOAT_EQ(sample.phase, 0.68F);
  EXPECT_EQ(sample.role, Animation::HumanoidConstructionRole::Hammer);
}

TEST(AnimationCoreActionManifest, DeathActionSuppressesOtherActionFlags) {
  auto const sample = Animation::resolve_humanoid_action_sample({
      .death =
          {
              .active = true,
              .dying = true,
              .state_time = 0.5F,
              .state_duration = 2.0F,
              .variant = 3U,
          },
      .combat =
          {
              .has_state = true,
              .phase = Animation::CombatPhase::Strike,
              .attack_family = Animation::CombatAttackFamily::Sword,
          },
      .melee_lock = {.in_lock = true, .participates = true},
      .hit_reaction = {.active = true, .intensity = 1.0F},
  });

  EXPECT_TRUE(sample.is_dying);
  EXPECT_FALSE(sample.is_dead);
  EXPECT_FLOAT_EQ(sample.death_progress, 0.25F);
  EXPECT_EQ(sample.death_variant, 3U);
  EXPECT_FALSE(sample.is_attacking);
  EXPECT_FALSE(sample.is_hit_reacting);
  EXPECT_FALSE(sample.is_in_melee_lock);
}

TEST(AnimationCoreActionManifest, ConstructionProgressWrapsAuthoredCycle) {
  auto const sample = Animation::resolve_humanoid_action_sample({
      .construction =
          {
              .active = true,
              .build_time = 4.0F,
              .time_remaining = 2.5F,
              .cycles_per_second = 1.75F,
          },
  });

  EXPECT_TRUE(sample.is_constructing);
  EXPECT_NEAR(sample.construction_progress, 0.625F, 1.0e-6F);
}

TEST(AnimationCoreActionManifest, CombatStateSetsAttackPhaseAndFamily) {
  auto const sword = Animation::resolve_humanoid_action_sample({
      .combat =
          {
              .has_state = true,
              .phase = Animation::CombatPhase::Recover,
              .phase_time = 0.15F,
              .phase_duration = 0.30F,
              .attack_family = Animation::CombatAttackFamily::Sword,
              .attack_variant = 2U,
              .finisher_attack = true,
              .attack_offset = 0.4F,
          },
  });

  EXPECT_TRUE(sword.is_attacking);
  EXPECT_TRUE(sword.is_melee);
  EXPECT_TRUE(sword.attack_from_combat_state);
  EXPECT_EQ(sword.combat_phase, Animation::CombatPhase::Recover);
  EXPECT_FLOAT_EQ(sword.combat_phase_progress, 0.5F);
  EXPECT_EQ(sword.attack_family, Animation::CombatAttackFamily::Sword);
  EXPECT_EQ(sword.attack_variant, 2U);
  EXPECT_TRUE(sword.finisher_attack);
  EXPECT_TRUE(sword.has_attack_offset);

  auto const fallback = Animation::resolve_humanoid_action_sample({
      .combat =
          {
              .has_state = true,
              .phase = Animation::CombatPhase::Strike,
              .attack_family = Animation::CombatAttackFamily::None,
              .fallback_mode_is_melee = true,
          },
  });
  EXPECT_TRUE(fallback.is_melee);
}

TEST(AnimationCoreActionManifest, MeleeLockCreatesFallbackAttackOnlyWhenNeeded) {
  auto const sample = Animation::resolve_humanoid_action_sample({
      .melee_lock =
          {
              .in_lock = true,
              .participates = true,
              .fallback_attack_family = Animation::CombatAttackFamily::Spear,
          },
  });

  EXPECT_TRUE(sample.is_attacking);
  EXPECT_TRUE(sample.is_melee);
  EXPECT_TRUE(sample.is_in_melee_lock);
  EXPECT_TRUE(sample.attack_from_melee_lock);
  EXPECT_EQ(sample.attack_family, Animation::CombatAttackFamily::Spear);
}

TEST(AnimationCoreActionManifest, AuthoredCombatPhasePreservesPhysicalMeleeLock) {
  auto const sample = Animation::resolve_humanoid_action_sample({
      .combat =
          {
              .has_state = true,
              .phase = Animation::CombatPhase::Strike,
              .attack_family = Animation::CombatAttackFamily::Spear,
          },
      .melee_lock =
          {
              .in_lock = true,
              .participates = true,
              .fallback_attack_family = Animation::CombatAttackFamily::Spear,
          },
  });

  EXPECT_TRUE(sample.is_attacking);
  EXPECT_TRUE(sample.attack_from_combat_state);
  EXPECT_FALSE(sample.attack_from_melee_lock);
  EXPECT_TRUE(sample.is_in_melee_lock);
}

TEST(AnimationCoreActionManifest, HitReactionUsesSquaredRecoilEnvelope) {
  auto const sample = Animation::resolve_humanoid_action_sample({
      .hit_reaction =
          {
              .active = true,
              .reaction_time = 0.25F,
              .reaction_duration = 1.0F,
              .intensity = 2.0F,
              .knockback_x = 4.0F,
              .knockback_z = -2.0F,
          },
  });

  EXPECT_TRUE(sample.is_hit_reacting);
  EXPECT_FLOAT_EQ(sample.hit_reaction_intensity, 1.5F);
  EXPECT_FLOAT_EQ(sample.hit_recoil_x, 2.25F);
  EXPECT_FLOAT_EQ(sample.hit_recoil_z, -1.125F);
}

TEST(AnimationCoreActionManifest, RangedProjectileAttackCanBecomeCast) {
  auto const fireball = Animation::resolve_humanoid_action_sample({
      .combat =
          {
              .has_state = true,
              .phase = Animation::CombatPhase::Strike,
              .attack_family = Animation::CombatAttackFamily::Bow,
          },
      .cast =
          {
              .has_projectile_cast = true,
              .projectile_is_fireball = true,
          },
  });

  EXPECT_TRUE(fireball.is_attacking);
  EXPECT_FALSE(fireball.is_melee);
  EXPECT_TRUE(fireball.is_casting);
  EXPECT_EQ(fireball.cast_kind, Animation::CastVisualKind::Fireball);

  auto const melee = Animation::resolve_humanoid_action_sample({
      .combat =
          {
              .has_state = true,
              .phase = Animation::CombatPhase::Strike,
              .attack_family = Animation::CombatAttackFamily::Sword,
          },
      .cast = {.has_projectile_cast = true, .projectile_is_fireball = true},
  });
  EXPECT_FALSE(melee.is_casting);
}

TEST(AnimationCoreActionManifest, CommanderJumpPoseClampsPhaseAndHeight) {
  auto const missing = Animation::resolve_humanoid_commander_jump_pose({
      .has_commander = false,
      .active = true,
      .phase = 0.5F,
      .height_offset = 2.0F,
  });
  EXPECT_FALSE(missing.active);
  EXPECT_FLOAT_EQ(missing.phase, 0.0F);
  EXPECT_FLOAT_EQ(missing.height_offset, 0.0F);

  auto const active = Animation::resolve_humanoid_commander_jump_pose({
      .has_commander = true,
      .active = true,
      .phase = 1.4F,
      .height_offset = -0.25F,
  });
  EXPECT_TRUE(active.active);
  EXPECT_FLOAT_EQ(active.phase, 1.0F);
  EXPECT_FLOAT_EQ(active.height_offset, 0.0F);
}

TEST(AnimationCoreActionManifest, CommanderFlagRallyPoseOwnsPlantPhase) {
  auto inactive = Animation::resolve_humanoid_commander_flag_rally_pose({
      .has_commander = true,
      .planting = false,
      .animation_timer = 0.5F,
      .cost = 1.0F,
  });
  EXPECT_FALSE(inactive.active);
  EXPECT_FLOAT_EQ(inactive.phase, 0.0F);

  auto const free = Animation::resolve_humanoid_commander_flag_rally_pose({
      .has_commander = true,
      .planting = true,
      .animation_timer = 9.0F,
      .cost = 0.0F,
  });
  EXPECT_TRUE(free.active);
  EXPECT_FLOAT_EQ(free.phase, 1.0F);

  auto const clamped = Animation::resolve_humanoid_commander_flag_rally_pose({
      .has_commander = true,
      .planting = true,
      .animation_timer = 0.25F,
      .cost = 1.0F,
  });
  EXPECT_TRUE(clamped.active);
  EXPECT_FLOAT_EQ(clamped.phase, 0.75F);
}

TEST(AnimationCoreActionManifest, ApproximateAttackPhaseUsesIdleWrapOrPhaseWindow) {
  EXPECT_NEAR(Animation::approximate_humanoid_attack_phase({
                  .is_attacking = true,
                  .combat_phase = Animation::CombatPhase::Idle,
                  .sample_time = 1.75F,
                  .attack_offset = 0.5F,
                  .has_attack_offset = true,
              }),
              0.25F,
              1.0e-6F);

  auto const window =
      Animation::attack_phase_window(Animation::CombatPhase::Strike, false, true);
  EXPECT_NEAR(Animation::approximate_humanoid_attack_phase({
                  .is_attacking = true,
                  .combat_phase = Animation::CombatPhase::Strike,
                  .combat_phase_progress = 0.25F,
                  .finisher_attack = true,
              }),
              window.start + (window.end - window.start) * 0.25F,
              1.0e-6F);
}

TEST(AnimationCoreMicroVariationManifest,
     DisabledWithoutAuthoritativeMultiSoldierCombat) {
  Animation::HumanoidCombatMicroVariationInputs inputs{};
  inputs.lane = Animation::SoldierCombatLane::LeadStrike;
  inputs.combat_phase = Animation::CombatPhase::Impact;
  inputs.is_attacking = true;
  inputs.attack_phase = 0.55F;
  inputs.sample_time = 1.0F;

  auto disabled = Animation::resolve_humanoid_combat_micro_variation(inputs);
  EXPECT_FALSE(disabled.active);
  EXPECT_FLOAT_EQ(disabled.torso_lean, 0.0F);
  EXPECT_FLOAT_EQ(disabled.impact_stagger, 0.0F);

  inputs.multi_soldier_unit = true;
  disabled = Animation::resolve_humanoid_combat_micro_variation(inputs);
  EXPECT_FALSE(disabled.active);

  inputs.authoritative_combat = true;
  inputs.is_dying = true;
  disabled = Animation::resolve_humanoid_combat_micro_variation(inputs);
  EXPECT_FALSE(disabled.active);

  inputs.is_dying = false;
  inputs.is_dead = true;
  disabled = Animation::resolve_humanoid_combat_micro_variation(inputs);
  EXPECT_FALSE(disabled.active);
}

TEST(AnimationCoreMicroVariationManifest,
     LeadStrikeUsesAttackPressureAndImpactStagger) {
  auto const variation = Animation::resolve_humanoid_combat_micro_variation({
      .multi_soldier_unit = true,
      .authoritative_combat = true,
      .lane = Animation::SoldierCombatLane::LeadStrike,
      .combat_phase = Animation::CombatPhase::Strike,
      .is_attacking = true,
      .attack_phase = 0.55F,
      .sample_time = 1.0F,
      .inst_seed = 0U,
  });

  EXPECT_TRUE(variation.active);
  EXPECT_FLOAT_EQ(variation.lane_sign, -1.0F);
  EXPECT_NEAR(variation.breath, std::sin(5.3F) * 0.004F, 1.0e-6F);
  EXPECT_FLOAT_EQ(variation.torso_lean, 0.030F);
  EXPECT_FLOAT_EQ(variation.shoulder_delay, 0.018F);
  EXPECT_FLOAT_EQ(variation.wrist_angle, 0.014F);
  EXPECT_FLOAT_EQ(variation.foot_adjust, 0.022F);
  EXPECT_FLOAT_EQ(variation.head_tracking, 0.010F);
  EXPECT_FLOAT_EQ(variation.impact_stagger, 0.016F);
}

TEST(AnimationCoreMicroVariationManifest, ShieldBraceUsesLaneSignAndShieldReaction) {
  auto const variation = Animation::resolve_humanoid_combat_micro_variation({
      .multi_soldier_unit = true,
      .authoritative_combat = true,
      .lane = Animation::SoldierCombatLane::ShieldBrace,
      .combat_phase = Animation::CombatPhase::Idle,
      .is_attacking = false,
      .attack_phase = 0.25F,
      .sample_time = 1.0F,
      .inst_seed = 0U,
  });

  EXPECT_TRUE(variation.active);
  EXPECT_FLOAT_EQ(variation.lane_sign, -1.0F);
  EXPECT_FLOAT_EQ(variation.lateral_tilt, 0.018F);
  EXPECT_FLOAT_EQ(variation.shield_reaction, 0.020F);
  EXPECT_FLOAT_EQ(variation.foot_adjust, -0.010F);
  EXPECT_FLOAT_EQ(variation.head_tracking, -0.006F);
  EXPECT_FLOAT_EQ(variation.impact_stagger, 0.0F);
}

TEST(AnimationCoreMicroVariationManifest, RangedReloadOffsetsReloadSide) {
  auto const variation = Animation::resolve_humanoid_combat_micro_variation({
      .multi_soldier_unit = true,
      .authoritative_combat = true,
      .lane = Animation::SoldierCombatLane::RangedReload,
      .combat_phase = Animation::CombatPhase::Recover,
      .is_attacking = false,
      .attack_phase = 0.25F,
      .sample_time = 1.0F,
      .inst_seed = 128U,
  });

  EXPECT_TRUE(variation.active);
  EXPECT_FLOAT_EQ(variation.lane_sign, 1.0F);
  EXPECT_FLOAT_EQ(variation.torso_lean, -0.014F);
  EXPECT_FLOAT_EQ(variation.shoulder_delay, -0.012F);
  EXPECT_FLOAT_EQ(variation.wrist_angle, -0.010F);
  EXPECT_FLOAT_EQ(variation.foot_adjust, -0.012F);
}

TEST(AnimationCoreMicroVariationManifest, HitReactionTransformDisablesBelowThreshold) {
  auto sample = Animation::resolve_humanoid_hit_reaction_transform({
      .active = false,
      .intensity = 1.0F,
      .recoil_x = 0.5F,
      .recoil_z = -0.25F,
      .inst_seed = 17U,
  });

  EXPECT_FALSE(sample.active);
  EXPECT_FLOAT_EQ(sample.recoil_x, 0.0F);
  EXPECT_FLOAT_EQ(sample.recoil_z, 0.0F);
  EXPECT_FLOAT_EQ(sample.squash, 0.0F);
  EXPECT_FLOAT_EQ(sample.tilt_degrees, 0.0F);

  sample = Animation::resolve_humanoid_hit_reaction_transform({
      .active = true,
      .intensity = 0.01F,
      .recoil_x = 0.5F,
      .recoil_z = -0.25F,
      .inst_seed = 17U,
  });

  EXPECT_FALSE(sample.active);
  EXPECT_FLOAT_EQ(sample.recoil_x, 0.0F);
  EXPECT_FLOAT_EQ(sample.recoil_z, 0.0F);
  EXPECT_FLOAT_EQ(sample.squash, 0.0F);
  EXPECT_FLOAT_EQ(sample.tilt_degrees, 0.0F);
}

TEST(AnimationCoreMicroVariationManifest,
     HitReactionTransformOwnsRecoilSquashAndTiltPolicy) {
  std::uint32_t constexpr seed = 0x12345678U;
  float const desync =
      0.7F +
      0.6F * static_cast<float>(((seed ^ 0x68F2A3B1U) * 2654435761U) >> 24U & 0xFFU) /
          255.0F;

  auto const sample = Animation::resolve_humanoid_hit_reaction_transform({
      .active = true,
      .intensity = 2.25F,
      .recoil_x = 0.4F,
      .recoil_z = -0.2F,
      .inst_seed = seed,
  });

  EXPECT_TRUE(sample.active);
  EXPECT_NEAR(sample.recoil_x, 0.4F * 1.6F * desync, 1.0e-6F);
  EXPECT_NEAR(sample.recoil_z, -0.2F * 1.6F * desync, 1.0e-6F);
  EXPECT_NEAR(sample.squash, 1.5F * 0.05F * desync, 1.0e-6F);
  EXPECT_NEAR(sample.tilt_degrees, 1.5F * 9.0F * desync, 1.0e-6F);
}

TEST(AnimationCoreGuardManifest, ShieldlessAndUnknownFamiliesResolveNone) {
  EXPECT_EQ(Animation::resolve_humanoid_guard_shield_pose({
                .has_left_hand_shield = false,
                .shield_family = Animation::GuardShieldFamily::Roman,
            }),
            Animation::ShieldFormationPose::None);

  EXPECT_EQ(Animation::resolve_humanoid_guard_shield_pose({
                .has_left_hand_shield = true,
                .shield_family = Animation::GuardShieldFamily::None,
            }),
            Animation::ShieldFormationPose::None);
}

TEST(AnimationCoreGuardManifest, RomanInteriorFormationUsesTopShield) {
  auto const pose = Animation::resolve_humanoid_guard_shield_pose({
      .has_left_hand_shield = true,
      .infantry_formation_unit = true,
      .formation_active = true,
      .defensive_layout_locked = true,
      .shield_family = Animation::GuardShieldFamily::Roman,
      .row = 1,
      .col = 1,
      .rows = 3,
      .cols = 3,
  });

  EXPECT_EQ(pose, Animation::ShieldFormationPose::RomanTop);
}

TEST(AnimationCoreGuardManifest, TheShieldRoofWaitsForTheTestudoToLockIn) {
  auto const inputs_for = [](bool locked) {
    return Animation::HumanoidGuardShieldPoseInputs{
        .has_left_hand_shield = true,
        .infantry_formation_unit = true,
        .formation_active = true,
        .guard_mode_active = true,
        .defensive_layout_locked = locked,
        .shield_family = Animation::GuardShieldFamily::Roman,
        .row = 1,
        .col = 1,
        .rows = 3,
        .cols = 3,
    };
  };

  EXPECT_EQ(Animation::resolve_humanoid_guard_shield_pose(inputs_for(false)),
            Animation::ShieldFormationPose::RomanFront)
      << "guard mode alone must not raise shields overhead";
  EXPECT_EQ(Animation::resolve_humanoid_guard_shield_pose(inputs_for(true)),
            Animation::ShieldFormationPose::RomanTop);
}

TEST(AnimationCoreGuardManifest, GuardFallbackUsesNationFrontShield) {
  EXPECT_EQ(Animation::resolve_humanoid_guard_shield_pose({
                .has_left_hand_shield = true,
                .infantry_formation_unit = false,
                .formation_active = false,
                .guard_mode_active = false,
                .shield_family = Animation::GuardShieldFamily::Roman,
            }),
            Animation::ShieldFormationPose::RomanFront);

  EXPECT_EQ(Animation::resolve_humanoid_guard_shield_pose({
                .has_left_hand_shield = true,
                .infantry_formation_unit = true,
                .formation_active = true,
                .shield_family = Animation::GuardShieldFamily::Carthage,
                .row = 1,
                .col = 1,
                .rows = 3,
                .cols = 3,
            }),
            Animation::ShieldFormationPose::CarthageFront);
}

TEST(AnimationCoreGuardManifest, AttachmentProfilesExposeShieldTurns) {
  auto const roman_front = Animation::guard_shield_attachment_profile(
      Animation::ShieldFormationPose::RomanFront);
  auto const roman_top = Animation::guard_shield_attachment_profile(
      Animation::ShieldFormationPose::RomanTop);
  auto const carthage = Animation::guard_shield_attachment_profile(
      Animation::ShieldFormationPose::CarthageFront);

  EXPECT_FLOAT_EQ(roman_front.base_yaw_degrees, -90.0F);
  EXPECT_FLOAT_EQ(roman_front.yaw_degrees, 180.0F);
  EXPECT_FLOAT_EQ(roman_top.yaw_degrees, 180.0F);

  EXPECT_GT(roman_front.pitch_degrees, -25.0F);
  EXPECT_LT(roman_top.pitch_degrees, -60.0F);
  EXPECT_LT(roman_top.pitch_degrees, carthage.pitch_degrees);
  EXPECT_LT(carthage.pitch_degrees, roman_front.pitch_degrees);

  EXPECT_GT(roman_top.translate_y, roman_front.translate_y);
  EXPECT_GT(roman_front.translate_z, 0.0F);
}

TEST(AnimationCoreAttackPoseManifest, CombatSwordVariantOwnsReachAndBodyDrive) {
  auto const sample = Animation::resolve_humanoid_weapon_attack_pose({
      .kind = Animation::HumanoidWeaponAttackKind::CombatSwordSlash,
      .attack_phase = 0.62F,
      .variant = 0U,
      .reach_scale = 1.20F,
      .attack_emphasis = 1.25F,
      .finisher_attack = true,
      .shoulder_y = 1.20F,
      .waist_y = 0.75F,
  });

  EXPECT_GT(sample.right_hand.z, 0.25F);
  EXPECT_GT(sample.left_hand.z, 0.25F);
  EXPECT_GT(sample.shoulder_r_z_delta, 0.0F);
  EXPECT_GT(sample.pelvis_z_delta, 0.0F);
  EXPECT_LT(sample.pelvis_y_delta, 0.0F);
  EXPECT_NE(sample.foot_l_z_delta, 0.0F);
}

TEST(AnimationCoreAttackPoseManifest, CombatSwordSpendsLateCycleInFollowThrough) {
  auto const early_followthrough = Animation::resolve_humanoid_weapon_attack_pose({
      .kind = Animation::HumanoidWeaponAttackKind::CombatSwordSlash,
      .attack_phase = 0.68F,
      .variant = 0U,
      .shoulder_y = 1.20F,
      .waist_y = 0.75F,
  });
  auto const late_followthrough = Animation::resolve_humanoid_weapon_attack_pose({
      .kind = Animation::HumanoidWeaponAttackKind::CombatSwordSlash,
      .attack_phase = 0.86F,
      .variant = 0U,
      .shoulder_y = 1.20F,
      .waist_y = 0.75F,
  });
  auto const brief_recovery = Animation::resolve_humanoid_weapon_attack_pose({
      .kind = Animation::HumanoidWeaponAttackKind::CombatSwordSlash,
      .attack_phase = 0.98F,
      .variant = 0U,
      .shoulder_y = 1.20F,
      .waist_y = 0.75F,
  });

  EXPECT_GT(
      std::abs(late_followthrough.right_hand.x - early_followthrough.right_hand.x),
      0.02F);
  EXPECT_LT(brief_recovery.right_hand.z, late_followthrough.right_hand.z);
}

TEST(AnimationCoreAttackPoseManifest, SwordSlashVariantCanReverseDirection) {
  auto const right_to_left = Animation::resolve_humanoid_weapon_attack_pose({
      .kind = Animation::HumanoidWeaponAttackKind::SwordSlash,
      .attack_phase = 0.58F,
      .variant = 0U,
      .shoulder_y = 1.20F,
      .waist_y = 0.75F,
  });
  auto const left_to_right = Animation::resolve_humanoid_weapon_attack_pose({
      .kind = Animation::HumanoidWeaponAttackKind::SwordSlash,
      .attack_phase = 0.58F,
      .variant = 1U,
      .shoulder_y = 1.20F,
      .waist_y = 0.75F,
  });

  EXPECT_LT(right_to_left.right_hand.x, left_to_right.right_hand.x);
  EXPECT_GT(right_to_left.shoulder_r_z_delta, left_to_right.shoulder_r_z_delta);
}

TEST(AnimationCoreAttackPoseManifest, SpearVariantExposesOffhandGripPolicy) {
  auto const thrust = Animation::resolve_humanoid_weapon_attack_pose({
      .kind = Animation::HumanoidWeaponAttackKind::SpearThrust,
      .attack_phase = 0.50F,
      .variant = 1U,
      .shoulder_y = 1.20F,
      .waist_y = 0.75F,
  });

  EXPECT_TRUE(thrust.use_offhand_spear_grip);
  EXPECT_GT(thrust.right_hand.y, 1.05F);
  EXPECT_LT(thrust.right_hand.y, 1.20F);
  EXPECT_GT(thrust.right_hand.z, 0.40F);
  EXPECT_LT(thrust.right_hand.z, 0.45F);
  EXPECT_GT(thrust.foot_r_z_delta, 0.0F);
  EXPECT_GT(thrust.offhand_along_offset, 0.25F);
  EXPECT_FLOAT_EQ(thrust.offhand_lateral_offset, 0.0F);
}

TEST(AnimationCoreAttackPoseManifest, ArcherMeleeUsesTwoHandedBowDrive) {
  auto const chamber = Animation::resolve_humanoid_weapon_attack_pose({
      .kind = Animation::HumanoidWeaponAttackKind::BowMeleeStrike,
      .attack_phase = 0.20F,
      .shoulder_y = 1.20F,
      .waist_y = 0.75F,
  });
  auto const contact = Animation::resolve_humanoid_weapon_attack_pose({
      .kind = Animation::HumanoidWeaponAttackKind::BowMeleeStrike,
      .attack_phase = 0.60F,
      .shoulder_y = 1.20F,
      .waist_y = 0.75F,
  });

  EXPECT_GT(contact.right_hand.z, chamber.right_hand.z + 0.35F);
  EXPECT_GT(contact.left_hand.z, chamber.left_hand.z + 0.35F);
  EXPECT_LT(std::abs(contact.right_hand.x - contact.left_hand.x), 0.45F);
  EXPECT_GT(contact.foot_r_z_delta, 0.0F);
  EXPECT_FALSE(contact.use_offhand_spear_grip);
}

TEST(AnimationCoreClipManifest, InfantryAndMountedSpearClipsAreNotCrossWired) {
  auto const infantry = Animation::humanoid_clip_manifest();
  auto const rider = Animation::rider_clip_manifest();
  auto const spear_state = Animation::state_index(Animation::StateId::AttackSpear);

  EXPECT_EQ(infantry.clips[spear_state], Animation::k_humanoid_attack_spear_a_clip);
  EXPECT_EQ(infantry.variant_counts[spear_state],
            Animation::k_humanoid_attack_spear_variant_count);
  EXPECT_EQ(rider.clips[spear_state], Animation::k_humanoid_riding_spear_thrust_clip);
  EXPECT_EQ(rider.variant_counts[spear_state], 1U);
}

TEST(AnimationCoreAttackPoseManifest, SpearVariantOwnsOffhandDirectionDuringStrike) {
  auto const sample = Animation::resolve_humanoid_weapon_attack_pose({
      .kind = Animation::HumanoidWeaponAttackKind::SpearThrust,
      .attack_phase = 0.40F,
      .variant = 0U,
      .shoulder_y = 1.20F,
      .waist_y = 0.75F,
  });

  EXPECT_TRUE(sample.use_offhand_spear_grip);
  EXPECT_LT(std::abs(sample.offhand_spear_direction.y), 0.20F);
  EXPECT_GT(sample.offhand_spear_direction.z, 0.98F);
}

TEST(AnimationCoreAttackPoseManifest, SpearDirectionBlendsHoldAndStrikePolicy) {
  auto const idle = Animation::resolve_humanoid_spear_direction({});
  EXPECT_NEAR(idle.x, 0.0493264F, 0.0001F);
  EXPECT_NEAR(idle.y, 0.542590F, 0.0001F);
  EXPECT_NEAR(idle.z, 0.838548F, 0.0001F);

  auto const held = Animation::resolve_humanoid_spear_direction({
      .hold_blend = 1.0F,
      .is_attacking = true,
      .is_melee = true,
      .attack_phase = 0.40F,
  });
  EXPECT_NEAR(held.x, 0.0453777F, 0.001F);
  EXPECT_NEAR(held.y, 0.4174751F, 0.001F);
  EXPECT_NEAR(held.z, 0.9075546F, 0.001F);
  EXPECT_GT(held.y, 0.30F);
  EXPECT_LT(held.y, idle.y);

  auto const strike = Animation::resolve_humanoid_spear_direction({
      .is_attacking = true,
      .is_melee = true,
      .attack_phase = 0.40F,
  });
  EXPECT_LT(std::abs(strike.y), 0.20F);
  EXPECT_GT(strike.z, 0.98F);
}

TEST(AnimationCoreAttackPoseManifest, SpearDirectionHoldsThroughPiercingContact) {
  auto const before_contact = Animation::resolve_humanoid_spear_direction({
      .is_attacking = true,
      .is_melee = true,
      .attack_phase = 0.49F,
  });
  auto const at_contact = Animation::resolve_humanoid_spear_direction({
      .is_attacking = true,
      .is_melee = true,
      .attack_phase = 0.50F,
  });
  auto const held_contact = Animation::resolve_humanoid_spear_direction({
      .is_attacking = true,
      .is_melee = true,
      .attack_phase = 0.64F,
  });

  EXPECT_NEAR(before_contact.x, at_contact.x, 0.01F);
  EXPECT_NEAR(before_contact.y, at_contact.y, 0.01F);
  EXPECT_NEAR(before_contact.z, at_contact.z, 0.01F);
  EXPECT_GT(at_contact.y, 0.05F);
  EXPECT_GT(at_contact.z, 0.98F);
  EXPECT_GT(held_contact.z, 0.98F);
  EXPECT_GT(held_contact.y, 0.15F);
}

TEST(AnimationCoreAttackPoseManifest, InfantryAndMountedSpearsUseDifferentHeights) {
  auto const infantry = Animation::resolve_humanoid_spear_direction({
      .is_attacking = true,
      .is_melee = true,
      .attack_phase = 0.52F,
  });
  auto const mounted = Animation::resolve_humanoid_spear_direction({
      .is_mounted = true,
      .is_attacking = true,
      .is_melee = true,
      .attack_phase = 0.52F,
  });

  EXPECT_GT(infantry.y, 0.05F);
  EXPECT_LT(mounted.y, -0.25F);
  EXPECT_GT(infantry.z, 0.98F);
  EXPECT_GT(mounted.z, 0.95F);
}

TEST(AnimationCoreAttackPoseManifest, ClassicSpearThrustOwnsBodyDrive) {
  auto const sample = Animation::resolve_humanoid_weapon_attack_pose({
      .kind = Animation::HumanoidWeaponAttackKind::SpearThrustClassic,
      .attack_phase = 0.52F,
      .shoulder_y = 1.20F,
      .waist_y = 0.75F,
  });

  EXPECT_TRUE(sample.use_offhand_spear_grip);
  EXPECT_GT(sample.right_hand.z, 0.40F);
  EXPECT_LT(sample.right_hand.z, 0.45F);
  EXPECT_GT(sample.shoulder_r_z_delta, 0.0F);
  EXPECT_GT(sample.foot_r_z_delta, 0.0F);
  EXPECT_GT(sample.offhand_along_offset, 0.25F);
  EXPECT_FLOAT_EQ(sample.offhand_lateral_offset, 0.0F);
  EXPECT_GT(sample.offhand_spear_direction.z, 0.98F);
}

TEST(AnimationCoreAttackPoseManifest, InfantrySpearUsesLinearChamberPushAndRecovery) {
  auto sample_at = [](float phase) {
    return Animation::resolve_humanoid_weapon_attack_pose({
        .kind = Animation::HumanoidWeaponAttackKind::SpearThrustClassic,
        .attack_phase = phase,
        .shoulder_y = 1.20F,
        .waist_y = 0.75F,
    });
  };

  auto const retracted = sample_at(0.05F);
  auto const contact = sample_at(0.58F);
  auto const recovery = sample_at(0.88F);
  EXPECT_GT(contact.right_hand.z, retracted.right_hand.z + 0.25F);
  EXPECT_GT(contact.left_hand.z, retracted.left_hand.z + 0.25F);
  EXPECT_GT(contact.right_hand.y, retracted.right_hand.y + 0.05F);
  EXPECT_GT(contact.left_hand.y, retracted.left_hand.y + 0.05F);
  EXPECT_LT(recovery.right_hand.z, contact.right_hand.z);
  EXPECT_LT(recovery.left_hand.z, contact.left_hand.z);
  EXPECT_FLOAT_EQ(contact.right_hand.x, retracted.right_hand.x);
  EXPECT_FLOAT_EQ(contact.left_hand.x, retracted.left_hand.x);
  EXPECT_GT(contact.offhand_spear_direction.y, 0.05F);
  EXPECT_LT(
      std::abs(contact.offhand_spear_direction.y - retracted.offhand_spear_direction.y),
      0.001F);
}

TEST(AnimationCoreAttackPoseManifest, SpearVariantsShareTheInfantryThrustContract) {
  for (std::uint8_t variant = 0U; variant < 3U; ++variant) {
    auto const sample = Animation::resolve_humanoid_weapon_attack_pose({
        .kind = Animation::HumanoidWeaponAttackKind::SpearThrust,
        .attack_phase = 0.58F,
        .variant = variant,
        .shoulder_y = 1.20F,
        .waist_y = 0.75F,
    });
    EXPECT_GT(sample.right_hand.z, 0.40F);
    EXPECT_GT(sample.left_hand.z, sample.right_hand.z);
    EXPECT_GT(sample.offhand_spear_direction.y, 0.05F);
  }
}

TEST(AnimationCoreAttackPoseManifest, HoldSpearThrustAppliesHoldDepth) {
  auto const upright = Animation::resolve_humanoid_weapon_attack_pose({
      .kind = Animation::HumanoidWeaponAttackKind::SpearThrustFromHold,
      .attack_phase = 0.45F,
      .hold_depth = 0.0F,
      .shoulder_y = 1.20F,
      .waist_y = 0.75F,
  });
  auto const crouched = Animation::resolve_humanoid_weapon_attack_pose({
      .kind = Animation::HumanoidWeaponAttackKind::SpearThrustFromHold,
      .attack_phase = 0.45F,
      .hold_depth = 1.0F,
      .shoulder_y = 1.20F,
      .waist_y = 0.75F,
  });

  EXPECT_TRUE(crouched.use_offhand_spear_grip);
  EXPECT_LT(crouched.right_hand.y, upright.right_hand.y);
  EXPECT_GT(crouched.right_hand.z, 0.55F);
  EXPECT_LT(crouched.right_hand.z, 0.70F);
  EXPECT_GT(crouched.shoulder_r_z_delta, 0.0F);
  EXPECT_FLOAT_EQ(crouched.offhand_lateral_offset, -0.05F);
}

TEST(HumanoidPoseController, SpearAttackStaysWithinReachAndKeepsBodyPlanted) {
  using HP = Render::GL::HumanProportions;
  float const k_max_arm_reach =
      Render::Humanoid::PosePrimitives::humanoid_arm_reach_limit(
          1.0F, Render::Humanoid::PosePrimitives::k_braced_arm_reach_fraction);
  std::vector<float> const phases{
      0.0F, 0.10F, 0.20F, 0.30F, 0.40F, 0.50F, 0.60F, 0.70F, 0.84F, 1.0F};

  auto neutral_pose = [] {
    Render::GL::HumanoidPose pose{};
    pose.head_pos = {0.0F, HP::HEAD_CENTER_Y, 0.0F};
    pose.neck_base = {0.0F, HP::NECK_BASE_Y, 0.0F};
    pose.shoulder_l = {-HP::SHOULDER_WIDTH * 0.5F, HP::SHOULDER_Y, 0.0F};
    pose.shoulder_r = {HP::SHOULDER_WIDTH * 0.5F, HP::SHOULDER_Y, 0.0F};
    pose.pelvis_pos = {0.0F, HP::WAIST_Y, 0.0F};
    return pose;
  };

  Render::GL::HumanoidAnimationContext const anim{};
  for (std::uint8_t variant = 0; variant < 3; ++variant) {
    for (float const phase : phases) {
      auto pose = neutral_pose();
      Render::GL::HumanoidPoseController controller(pose, anim);
      controller.spear_thrust_variant(phase, variant);

      EXPECT_LE((pose.hand_r - pose.shoulder_r).length(), k_max_arm_reach + 1.0e-5F)
          << "variant=" << unsigned(variant) << " phase=" << phase;
      EXPECT_LE((pose.hand_l - pose.shoulder_l).length(), k_max_arm_reach + 1.0e-5F)
          << "variant=" << unsigned(variant) << " phase=" << phase;
      Render::Humanoid::BonePalette palette{};
      Render::Humanoid::evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F), palette);
      auto const shoulder_l =
          static_cast<std::size_t>(Render::Humanoid::HumanoidBone::ShoulderL);
      auto const hand_l =
          static_cast<std::size_t>(Render::Humanoid::HumanoidBone::HandL);
      auto const shoulder_r =
          static_cast<std::size_t>(Render::Humanoid::HumanoidBone::ShoulderR);
      auto const hand_r =
          static_cast<std::size_t>(Render::Humanoid::HumanoidBone::HandR);
      EXPECT_LE((palette[hand_l].column(3).toVector3D() -
                 palette[shoulder_l].column(3).toVector3D())
                    .length(),
                k_max_arm_reach + 1.0e-5F);
      EXPECT_LE((palette[hand_r].column(3).toVector3D() -
                 palette[shoulder_r].column(3).toVector3D())
                    .length(),
                k_max_arm_reach + 1.0e-5F);
      EXPECT_LT(std::abs(pose.head_pos.z() - pose.pelvis_pos.z()), 0.12F)
          << "variant=" << unsigned(variant) << " phase=" << phase;
      EXPECT_GT(pose.head_pos.y(), pose.shoulder_l.y() + 0.12F)
          << "variant=" << unsigned(variant) << " phase=" << phase;
    }
  }

  auto contact_pose = neutral_pose();
  Render::GL::HumanoidPoseController controller(contact_pose, anim);
  controller.spear_thrust_variant(0.55F, 0U);
  EXPECT_GT(contact_pose.hand_l.z() - contact_pose.hand_r.z(), 0.0F);
}

TEST(AnimationCoreAttackPoseManifest, BasicMeleeStrikeOwnsGenericHitCurve) {
  auto const guard = Animation::resolve_humanoid_weapon_attack_pose({
      .kind = Animation::HumanoidWeaponAttackKind::BasicMeleeStrike,
      .attack_phase = 0.0F,
      .shoulder_y = 1.20F,
      .waist_y = 0.75F,
  });
  EXPECT_GT(guard.left_hand.y, 1.25F);
  EXPECT_GT(guard.right_hand.y, 1.25F);
  EXPECT_GT(guard.left_hand.z, 0.20F);
  EXPECT_GT(guard.right_hand.z, 0.20F);
  EXPECT_LT(guard.pelvis_y_delta, 0.0F);
  EXPECT_GT(guard.foot_l_z_delta, guard.foot_r_z_delta);

  auto contact = [](std::uint8_t variant) {
    return Animation::resolve_humanoid_weapon_attack_pose({
        .kind = Animation::HumanoidWeaponAttackKind::BasicMeleeStrike,
        .attack_phase = 0.50F,
        .variant = variant,
        .shoulder_y = 1.20F,
        .waist_y = 0.75F,
    });
  };
  auto const jab = contact(0U);
  auto const cross = contact(1U);
  auto const hook = contact(2U);

  EXPECT_GT(jab.left_hand.z, 0.70F);
  EXPECT_LT(jab.right_hand.z, 0.35F);
  EXPECT_GT(cross.right_hand.z, 0.74F);
  EXPECT_LT(cross.left_hand.z, 0.35F);
  EXPECT_GT(cross.shoulder_r_z_delta - cross.shoulder_l_z_delta, 0.10F);
  EXPECT_GT(hook.left_hand.x, 0.08F);
  EXPECT_GT(hook.left_hand.z, 0.50F);
  EXPECT_LT(hook.left_hand.z, jab.left_hand.z);

  auto const recovered = Animation::resolve_humanoid_weapon_attack_pose({
      .kind = Animation::HumanoidWeaponAttackKind::BasicMeleeStrike,
      .attack_phase = 1.0F,
      .variant = 2U,
      .shoulder_y = 1.20F,
      .waist_y = 0.75F,
  });
  EXPECT_NEAR(recovered.left_hand.x, guard.left_hand.x, 0.0001F);
  EXPECT_NEAR(recovered.left_hand.y, guard.left_hand.y, 0.0001F);
  EXPECT_NEAR(recovered.left_hand.z, guard.left_hand.z, 0.0001F);
  EXPECT_NEAR(recovered.right_hand.x, guard.right_hand.x, 0.0001F);
  EXPECT_NEAR(recovered.right_hand.y, guard.right_hand.y, 0.0001F);
  EXPECT_NEAR(recovered.right_hand.z, guard.right_hand.z, 0.0001F);
}

TEST(AnimationCoreAttackPoseManifest, HumanoidBowDrawStartsAtAimPose) {
  auto const sample = Animation::resolve_humanoid_bow_draw_pose({
      .draw_phase = 0.0F,
      .jitter_seed = 0.35F,
      .shoulder_y = 1.20F,
  });

  EXPECT_FLOAT_EQ(sample.left_hand.x, -0.02F);
  EXPECT_FLOAT_EQ(sample.left_hand.y, 1.38F);
  EXPECT_FLOAT_EQ(sample.left_hand.z, 0.42F);
  EXPECT_FLOAT_EQ(sample.right_hand.x, 0.03F);
  EXPECT_FLOAT_EQ(sample.right_hand.y, 1.28F);
  EXPECT_FLOAT_EQ(sample.right_hand.z, 0.62F);
  EXPECT_FLOAT_EQ(sample.shoulder_r_z_delta, 0.10F);
  EXPECT_FLOAT_EQ(sample.pelvis_z_delta, 0.0F);
}

TEST(AnimationCoreAttackPoseManifest, HumanoidBowDrawBuildsTension) {
  auto const sample = Animation::resolve_humanoid_bow_draw_pose({
      .draw_phase = 0.35F,
      .jitter_seed = 0.0F,
      .shoulder_y = 1.20F,
  });

  EXPECT_GT(sample.left_hand.z, 0.13F);
  EXPECT_LT(sample.left_hand.z, 0.22F);
  EXPECT_GT(sample.right_hand.z, 0.62F);
  EXPECT_LT(sample.shoulder_r_y_delta, 0.0F);
  EXPECT_GT(sample.shoulder_r_z_delta, 0.10F);
  EXPECT_GT(sample.neck_z_delta, 0.0F);
  EXPECT_LT(sample.pelvis_z_delta, 0.0F);
}

TEST(AnimationCoreAttackPoseManifest, HumanoidBowDrawRecoversToAimPose) {
  auto const sample = Animation::resolve_humanoid_bow_draw_pose({
      .draw_phase = 1.0F,
      .jitter_seed = 0.15F,
      .shoulder_y = 1.20F,
  });

  EXPECT_FLOAT_EQ(sample.left_hand.x, -0.02F);
  EXPECT_FLOAT_EQ(sample.left_hand.y, 1.38F);
  EXPECT_FLOAT_EQ(sample.left_hand.z, 0.42F);
  EXPECT_FLOAT_EQ(sample.right_hand.x, 0.03F);
  EXPECT_FLOAT_EQ(sample.right_hand.y, 1.28F);
  EXPECT_FLOAT_EQ(sample.right_hand.z, 0.62F);
  EXPECT_FLOAT_EQ(sample.shoulder_r_z_delta, 0.10F);
  EXPECT_NEAR(sample.head_z_delta, 0.0F, 0.0001F);
}

TEST(AnimationCoreAttackPoseManifest, ConstructionSawOwnsGripAndBodyDeltas) {
  auto const sample = Animation::resolve_humanoid_construction_pose({
      .kind = Animation::HumanoidConstructionPoseKind::Saw,
      .work_phase = 0.25F,
      .jitter_seed = 0.0F,
      .shoulder_y = 1.20F,
  });

  EXPECT_TRUE(sample.use_two_handed_grip);
  EXPECT_NEAR(sample.grip_center.x, 0.04F, 0.0001F);
  EXPECT_NEAR(sample.grip_center.y, 1.086F, 0.0001F);
  EXPECT_NEAR(sample.grip_center.z, 0.632F, 0.0001F);
  EXPECT_NEAR(sample.hand_separation, 0.25F, 0.0001F);
  EXPECT_NEAR(sample.shoulder_r_z_delta, 0.095F, 0.0001F);
  EXPECT_NEAR(sample.shoulder_l_y_delta, -0.04F, 0.0001F);
  EXPECT_NEAR(sample.pelvis_x_delta, 0.018F, 0.0001F);
  EXPECT_LT(sample.pelvis_y_delta, 0.0F);
  EXPECT_LT(sample.pelvis_z_delta, 0.0F);
  EXPECT_FLOAT_EQ(sample.foot_l_z_delta, -0.030F);
  EXPECT_FLOAT_EQ(sample.foot_r_z_delta, 0.045F);
}

TEST(AnimationCoreAttackPoseManifest, ConstructionChiselOwnsStandingStrikePose) {
  auto const sample = Animation::resolve_humanoid_construction_pose({
      .kind = Animation::HumanoidConstructionPoseKind::Chisel,
      .work_phase = 0.47F,
      .shoulder_y = 1.20F,
  });

  EXPECT_FALSE(sample.use_two_handed_grip);
  EXPECT_FLOAT_EQ(sample.left_hand.x, -0.05F);
  EXPECT_FLOAT_EQ(sample.left_hand.y, 1.06F);
  EXPECT_FLOAT_EQ(sample.left_hand.z, 0.48F);
  EXPECT_NEAR(sample.right_hand.x, 0.12F, 0.0001F);
  EXPECT_NEAR(sample.right_hand.y, 1.165F, 0.0001F);
  EXPECT_NEAR(sample.right_hand.z, 0.38F, 0.0001F);
  EXPECT_NEAR(sample.shoulder_r_y_delta, 0.02F, 0.0001F);
  EXPECT_NEAR(sample.shoulder_l_z_delta, 0.0525F, 0.0001F);
  EXPECT_FLOAT_EQ(sample.foot_r_z_delta, 0.03F);
}

TEST(AnimationCoreAttackPoseManifest, ConstructionChiselOwnsKneelingStrikePose) {
  auto const sample = Animation::resolve_humanoid_construction_pose({
      .kind = Animation::HumanoidConstructionPoseKind::KneelingChisel,
      .work_phase = 0.60F,
      .shoulder_y = 1.20F,
  });

  EXPECT_FLOAT_EQ(sample.left_hand.y, 1.01F);
  EXPECT_FLOAT_EQ(sample.right_hand.x, 0.09F);
  EXPECT_FLOAT_EQ(sample.right_hand.y, 1.04F);
  EXPECT_FLOAT_EQ(sample.right_hand.z, 0.58F);
  EXPECT_NEAR(sample.shoulder_l_y_delta, -0.02F, 0.0001F);
  EXPECT_NEAR(sample.shoulder_r_y_delta, 0.02F, 0.0001F);
  EXPECT_NEAR(sample.shoulder_r_z_delta, 0.12F, 0.0001F);
  EXPECT_FLOAT_EQ(sample.foot_r_z_delta, 0.0F);
}

TEST(AnimationCoreHoldPoseManifest, SpearIdleExposesOffhandGripContract) {
  auto const sample = Animation::resolve_humanoid_held_pose({
      .kind = Animation::HumanoidHeldPoseKind::SpearIdle,
      .shoulder_y = 1.20F,
  });

  EXPECT_TRUE(sample.use_offhand_spear_grip);
  EXPECT_FLOAT_EQ(sample.right_hand.x, 0.34F);
  EXPECT_FLOAT_EQ(sample.right_hand.y, 1.18F);
  EXPECT_FLOAT_EQ(sample.right_hand.z, 0.30F);
  EXPECT_FLOAT_EQ(sample.offhand_along_offset, 0.46F);
  EXPECT_FLOAT_EQ(sample.offhand_y_drop, -0.03F);
  EXPECT_FLOAT_EQ(sample.offhand_lateral_offset, -0.08F);
  EXPECT_NEAR(sample.offhand_spear_direction.x, 0.0493264F, 0.0001F);
  EXPECT_NEAR(sample.offhand_spear_direction.y, 0.542590F, 0.0001F);
  EXPECT_NEAR(sample.offhand_spear_direction.z, 0.838548F, 0.0001F);
  EXPECT_TRUE(sample.clamp_left_hand_x_min);
  EXPECT_FLOAT_EQ(sample.left_hand_x_min, 0.10F);
  EXPECT_TRUE(sample.clamp_left_hand_y_max);
  EXPECT_FLOAT_EQ(sample.left_hand_y_max, 1.32F);
  EXPECT_FLOAT_EQ(sample.shoulder_r_x_delta, 0.025F);
  EXPECT_FLOAT_EQ(sample.shoulder_l_z_delta, 0.020F);
}

TEST(AnimationCoreHoldPoseManifest, SpearBraceOwnsBodyReadinessDeltas) {
  auto const sample = Animation::resolve_humanoid_held_pose({
      .kind = Animation::HumanoidHeldPoseKind::SpearBrace,
      .shoulder_y = 1.20F,
  });

  EXPECT_TRUE(sample.use_offhand_spear_grip);
  EXPECT_FLOAT_EQ(sample.right_hand.x, 0.30F);
  EXPECT_FLOAT_EQ(sample.right_hand.y, 0.90F);
  EXPECT_FLOAT_EQ(sample.right_hand.z, 0.46F);
  EXPECT_FLOAT_EQ(sample.offhand_along_offset, -0.24F);
  EXPECT_NEAR(sample.offhand_spear_direction.x, 0.0453777F, 0.0001F);
  EXPECT_NEAR(sample.offhand_spear_direction.y, 0.4174751F, 0.0001F);
  EXPECT_NEAR(sample.offhand_spear_direction.z, 0.9075546F, 0.0001F);
  EXPECT_GT(sample.offhand_spear_direction.y, 0.30F);
  EXPECT_LT(sample.offhand_spear_direction.y, 0.55F);
  EXPECT_FLOAT_EQ(sample.left_hand_z_delta, 0.03F);
  EXPECT_FLOAT_EQ(sample.shoulder_r_y_delta, -0.05F);
  EXPECT_FLOAT_EQ(sample.shoulder_r_z_delta, 0.08F);
  EXPECT_FLOAT_EQ(sample.neck_z_delta, 0.07F);
  EXPECT_FLOAT_EQ(sample.head_y_delta, -0.01F);
}

TEST(AnimationCoreHoldPoseManifest, BowReadyAndSwordShieldCarryExposeStableTargets) {
  auto const bow = Animation::resolve_humanoid_held_pose({
      .kind = Animation::HumanoidHeldPoseKind::BowReady,
      .shoulder_y = 1.20F,
  });
  auto const shield_moving = Animation::resolve_humanoid_held_pose({
      .kind = Animation::HumanoidHeldPoseKind::SwordShieldCarry,
      .shoulder_y = 1.20F,
      .moving = true,
  });

  EXPECT_FALSE(bow.use_offhand_spear_grip);
  EXPECT_FLOAT_EQ(bow.right_hand.z, 0.62F);
  EXPECT_FLOAT_EQ(bow.left_hand.y, 1.17F);
  EXPECT_FLOAT_EQ(bow.shoulder_r_z_delta, 0.11F);
  EXPECT_FLOAT_EQ(bow.head_y_delta, -0.01F);
  EXPECT_GT(bow.right_hand.x, bow.left_hand.x);
  EXPECT_LT(bow.right_hand.y, 1.20F);
  EXPECT_LT(bow.left_hand.y, 1.20F);

  EXPECT_FLOAT_EQ(shield_moving.right_hand.x, 0.37F);
  EXPECT_FLOAT_EQ(shield_moving.right_hand.y, 1.09F);
  EXPECT_FLOAT_EQ(shield_moving.right_hand.z, 0.50F);
  EXPECT_FLOAT_EQ(shield_moving.left_hand.x, -0.35F);
  EXPECT_FLOAT_EQ(shield_moving.left_hand.y, 1.15F);
  EXPECT_FLOAT_EQ(shield_moving.left_hand.z, 0.33F);
}

TEST(AnimationCoreHoldPoseManifest, ResourceCarryPlacesBothHandsAroundChestLoad) {
  auto const carry = Animation::resolve_humanoid_held_pose({
      .kind = Animation::HumanoidHeldPoseKind::ResourceCarry,
      .shoulder_y = Render::GL::HumanProportions::SHOULDER_Y,
  });

  EXPECT_FLOAT_EQ(carry.right_hand.x, Animation::k_resource_carry_hand_half_span);
  EXPECT_FLOAT_EQ(carry.left_hand.x, -Animation::k_resource_carry_hand_half_span);
  EXPECT_FLOAT_EQ(carry.right_hand.z, Animation::k_resource_carry_hand_z);
  EXPECT_GT(carry.right_hand.y, Render::GL::HumanProportions::WAIST_Y);
  EXPECT_LT(carry.right_hand.y, Render::GL::HumanProportions::CHEST_Y);
}

TEST(AnimationCoreHoldPoseManifest, GuardStanceOwnsFormationTargetsAndDeltas) {
  auto const top = Animation::resolve_humanoid_guard_stance_pose({
      .pose = Animation::ShieldFormationPose::RomanTop,
      .amount = 0.50F,
      .shoulder_y = 1.20F,
  });
  auto const inactive = Animation::resolve_humanoid_guard_stance_pose({
      .pose = Animation::ShieldFormationPose::RomanFront,
      .amount = 0.0F,
      .shoulder_y = 1.20F,
  });

  auto const front = Animation::resolve_humanoid_guard_stance_pose({
      .pose = Animation::ShieldFormationPose::RomanFront,
      .amount = 0.50F,
      .shoulder_y = 1.20F,
  });

  EXPECT_TRUE(top.active);
  EXPECT_FLOAT_EQ(top.blend_amount, 0.50F);
  EXPECT_GT(top.left_hand.y, 1.20F + 0.30F);
  EXPECT_GT(top.left_hand.y, front.left_hand.y);
  EXPECT_GT(top.shoulder_l_delta.y, 0.0F);
  EXPECT_GT(front.left_hand.z, top.left_hand.z);
  EXPECT_FALSE(inactive.active);
}

TEST(AnimationCorePosturePoseManifest, MicroIdleIsDeterministicAndMovesBreathingParts) {
  auto const first = Animation::resolve_humanoid_micro_idle_pose({
      .sample_time = 1.25F,
      .seed = 42U,
  });
  auto const second = Animation::resolve_humanoid_micro_idle_pose({
      .sample_time = 1.25F,
      .seed = 42U,
  });

  EXPECT_FLOAT_EQ(first.shoulder_l_y_delta, second.shoulder_l_y_delta);
  EXPECT_FLOAT_EQ(first.hand_r_y_delta, second.hand_r_y_delta);
  EXPECT_GT(std::abs(first.shoulder_l_y_delta) + std::abs(first.pelvis_x_delta) +
                std::abs(first.head_z_delta),
            0.0F);
}

TEST(AnimationCorePosturePoseManifest, KneelPoseOwnsAbsoluteLegPlacement) {
  auto const sample = Animation::resolve_humanoid_kneel_pose({
      .depth = 0.50F,
      .waist_y = 0.75F,
      .ground_y = 0.0F,
      .lower_leg_len = 0.52F,
      .foot_y_offset = 0.02F,
  });

  EXPECT_TRUE(sample.active);
  EXPECT_FLOAT_EQ(sample.pelvis.y, 0.55F);
  EXPECT_FLOAT_EQ(sample.knee_l.x, -0.11F);
  EXPECT_FLOAT_EQ(sample.knee_l.y, 0.035F);
  EXPECT_FLOAT_EQ(sample.knee_l.z, -0.03F);
  EXPECT_FLOAT_EQ(sample.foot_l.x, -0.135F);
  EXPECT_FLOAT_EQ(sample.foot_l.y, 0.02F);
  EXPECT_NEAR(sample.foot_l.z, -0.2718F, 0.0001F);
  EXPECT_FLOAT_EQ(sample.knee_r.y, 0.43F);
  EXPECT_FLOAT_EQ(sample.foot_r.y, 0.02F);
  EXPECT_FLOAT_EQ(sample.foot_r.z, 0.14F);
  EXPECT_FLOAT_EQ(sample.upper_body.shoulder_l_y_delta, -0.20F);
  EXPECT_FLOAT_EQ(sample.upper_body.neck_z_delta, 0.012F);
}

TEST(AnimationCorePosturePoseManifest, KneelPoseRestsTheDownKneeOnItsOwnRadius) {
  auto const sample = Animation::resolve_humanoid_kneel_pose({
      .depth = 0.50F,
      .waist_y = 0.75F,
      .ground_y = 0.0F,
      .lower_leg_len = 0.52F,
      .foot_y_offset = 0.02F,
      .knee_radius = 0.06F,
  });

  EXPECT_TRUE(sample.active);
  EXPECT_FLOAT_EQ(sample.knee_l.y, 0.06F);
}

TEST(AnimationCorePosturePoseManifest,
     KneelTransitionStandingUpPushesRightFootAndHands) {
  auto const sample = Animation::resolve_humanoid_kneel_transition_pose({
      .progress = 0.175F,
      .standing_up = true,
  });

  EXPECT_FLOAT_EQ(sample.foot_r_z_delta, -0.04F);
  EXPECT_FLOAT_EQ(sample.knee_r_z_delta, -0.025F);
  EXPECT_FLOAT_EQ(sample.shoulder_l_z_delta, 0.03F);
  EXPECT_FLOAT_EQ(sample.neck_z_delta, 0.027F);
  EXPECT_FLOAT_EQ(sample.hand_l_z_delta, 0.02F);
  EXPECT_FLOAT_EQ(sample.hand_r_z_delta, 0.02F);
}

TEST(AnimationCorePosturePoseManifest, HitFlinchOwnsBodyReactionDeltas) {
  auto const sample = Animation::resolve_humanoid_hit_flinch_pose({
      .intensity = 0.50F,
  });

  EXPECT_FLOAT_EQ(sample.head_z_delta, -0.03F);
  EXPECT_FLOAT_EQ(sample.head_y_delta, -0.01F);
  EXPECT_FLOAT_EQ(sample.neck_z_delta, -0.024F);
  EXPECT_FLOAT_EQ(sample.shoulder_l_y_delta, -0.015F);
  EXPECT_FLOAT_EQ(sample.shoulder_l_z_delta, -0.018F);
  EXPECT_FLOAT_EQ(sample.pelvis_y_delta, -0.006F);
}

TEST(AnimationCorePosturePoseManifest, LeanPoseNormalizesDirectionAndScalesBody) {
  auto const sample = Animation::resolve_humanoid_lean_pose({
      .direction = {0.0F, 0.0F, 2.0F},
      .amount = 0.50F,
  });

  EXPECT_FLOAT_EQ(sample.shoulder_l_z_delta, 0.06F);
  EXPECT_FLOAT_EQ(sample.shoulder_r_z_delta, 0.06F);
  EXPECT_FLOAT_EQ(sample.neck_z_delta, 0.051F);
  EXPECT_FLOAT_EQ(sample.head_z_delta, 0.045F);
}

TEST(AnimationCorePosturePoseManifest, LookAtPoseMovesHeadAndNeckHorizontally) {
  auto const sample = Animation::resolve_humanoid_look_at_pose({
      .head_position = {0.0F, 0.0F, 0.0F},
      .target = {3.0F, 0.0F, 4.0F},
  });

  EXPECT_NEAR(sample.head_x_delta, 0.018F, 0.0001F);
  EXPECT_FLOAT_EQ(sample.head_y_delta, 0.0F);
  EXPECT_NEAR(sample.head_z_delta, 0.024F, 0.0001F);
  EXPECT_NEAR(sample.neck_x_delta, 0.009F, 0.0001F);
  EXPECT_NEAR(sample.neck_z_delta, 0.012F, 0.0001F);
}

TEST(AnimationCorePosturePoseManifest, TorsoTiltOwnsBodyAndFrameDeltas) {
  auto const sample = Animation::resolve_humanoid_torso_tilt_pose({
      .heading_right = {1.0F, 0.0F, 0.0F},
      .heading_forward = {0.0F, 0.0F, 1.0F},
      .side_tilt = 0.02F,
      .forward_tilt = -0.04F,
  });

  EXPECT_FLOAT_EQ(sample.shoulder_l_x_delta, 0.02F);
  EXPECT_FLOAT_EQ(sample.shoulder_l_z_delta, -0.04F);
  EXPECT_FLOAT_EQ(sample.neck_x_delta, 0.024F);
  EXPECT_FLOAT_EQ(sample.neck_z_delta, -0.048F);
  EXPECT_FLOAT_EQ(sample.head_x_delta, 0.03F);
  EXPECT_FLOAT_EQ(sample.head_z_delta, -0.06F);
  EXPECT_FLOAT_EQ(sample.torso_frame_origin_delta.x, 0.02F);
  EXPECT_FLOAT_EQ(sample.torso_frame_origin_delta.z, -0.04F);
  EXPECT_FLOAT_EQ(sample.head_frame_origin_delta.x, 0.03F);
  EXPECT_FLOAT_EQ(sample.head_frame_origin_delta.z, -0.06F);
}

TEST(AnimationCorePosturePoseManifest, MountedLeanOwnsSeatAxisBodyOffsets) {
  auto const sample = Animation::resolve_mounted_rider_lean_pose({
      .forward_lean = 0.50F,
      .side_lean = -0.50F,
  });

  EXPECT_FLOAT_EQ(sample.shoulder_l_delta.forward, 0.075F);
  EXPECT_FLOAT_EQ(sample.shoulder_l_delta.right, -0.05F);
  EXPECT_FLOAT_EQ(sample.shoulder_r_delta.forward, 0.075F);
  EXPECT_FLOAT_EQ(sample.neck_delta.forward, 0.0675F);
  EXPECT_FLOAT_EQ(sample.neck_delta.right, -0.045F);
  EXPECT_FLOAT_EQ(sample.head_forward_tilt, 0.20F);
  EXPECT_FLOAT_EQ(sample.head_side_tilt, -0.20F);
}

TEST(AnimationCorePosturePoseManifest, MountedChargeOwnsLeanAndCrouch) {
  auto const sample = Animation::resolve_mounted_rider_charge_pose({
      .intensity = 0.40F,
  });

  EXPECT_FLOAT_EQ(sample.shoulder_l_delta.forward, 0.10F);
  EXPECT_FLOAT_EQ(sample.shoulder_l_delta.world_y, -0.032F);
  EXPECT_FLOAT_EQ(sample.neck_delta.forward, 0.085F);
  EXPECT_FLOAT_EQ(sample.neck_delta.world_y, -0.0256F);
}

TEST(AnimationCorePosturePoseManifest, MountedReiningClampsTensionAndLeansBack) {
  auto const sample = Animation::resolve_mounted_rider_reining_pose({
      .left_tension = 0.50F,
      .right_tension = 1.50F,
  });

  EXPECT_FLOAT_EQ(sample.shoulder_l_delta.forward, -0.06F);
  EXPECT_FLOAT_EQ(sample.shoulder_r_delta.forward, -0.06F);
  EXPECT_FLOAT_EQ(sample.neck_delta.forward, -0.054F);
}

TEST(AnimationCorePosturePoseManifest, MountedTorsoSculptOwnsCompressionAndTwist) {
  auto const sample = Animation::resolve_mounted_rider_torso_sculpt_pose({
      .compression = 0.50F,
      .twist = 0.50F,
      .shoulder_dip = -0.50F,
  });

  EXPECT_TRUE(sample.active);
  EXPECT_FLOAT_EQ(sample.shoulder_l_delta.forward, -0.0375F);
  EXPECT_NEAR(sample.shoulder_l_delta.right, -0.01085F, 0.00001F);
  EXPECT_FLOAT_EQ(sample.shoulder_l_delta.up, -0.015F);
  EXPECT_FLOAT_EQ(sample.shoulder_r_delta.forward, -0.0375F);
  EXPECT_NEAR(sample.shoulder_r_delta.right, 0.01085F, 0.00001F);
  EXPECT_FLOAT_EQ(sample.shoulder_r_delta.up, 0.015F);
  EXPECT_FLOAT_EQ(sample.neck_delta.forward, -0.020625F);
  EXPECT_FLOAT_EQ(sample.neck_delta.up, 0.0048F);
  EXPECT_FLOAT_EQ(sample.head_delta.forward, -0.020625F);
  EXPECT_FLOAT_EQ(sample.head_delta.up, 0.0048F);
}

TEST(AnimationCoreAmbientPoseManifest, SelectionPrioritizesCommanderOverrides) {
  auto const jump = Animation::resolve_humanoid_ambient_selection({
      .jump_active = true,
      .jump_phase = 1.40F,
      .flag_rally_active = true,
      .flag_rally_phase = 0.25F,
      .attacking = true,
      .seed = 0U,
      .idle_duration = 5.40F,
  });
  EXPECT_TRUE(jump.sample.active);
  EXPECT_EQ(jump.sample.type, Animation::HumanoidAmbientIdle::Jump);
  EXPECT_FLOAT_EQ(jump.sample.phase, 1.0F);

  EXPECT_FLOAT_EQ(jump.sample.blend, 1.0F);

  auto const rally = Animation::resolve_humanoid_ambient_selection({
      .jump_active = false,
      .flag_rally_active = true,
      .flag_rally_phase = -0.25F,
      .attacking = true,
      .seed = 0U,
      .idle_duration = 5.40F,
  });
  EXPECT_TRUE(rally.sample.active);
  EXPECT_EQ(rally.sample.type, Animation::HumanoidAmbientIdle::PlantFlag);
  EXPECT_FLOAT_EQ(rally.sample.phase, 0.0F);
  EXPECT_FLOAT_EQ(rally.sample.blend, 1.0F);
}

TEST(AnimationCoreAmbientPoseManifest, SelectionSuppressesAmbientWhileBusy) {
  using Inputs = Animation::HumanoidAmbientSelectionInputs;
  struct Case {
    const char* name;
    void (*apply)(Inputs&);
  };
  constexpr std::array<Case, 8> cases{{
      {"moving",
       [](Inputs& in) {
         in.has_locomotion = true;
       }},
      {"attacking",
       [](Inputs& in) {
         in.attacking = true;
       }},
      {"hold mode",
       [](Inputs& in) {
         in.in_hold_mode = true;
       }},
      {"guarding",
       [](Inputs& in) {
         in.guarding = true;
       }},
      {"constructing",
       [](Inputs& in) {
         in.constructing = true;
       }},
      {"healing",
       [](Inputs& in) {
         in.healing = true;
       }},
      {"dying",
       [](Inputs& in) {
         in.dying = true;
       }},
      {"routing",
       [](Inputs& in) {
         in.routing = true;
       }},
  }};

  for (auto const& c : cases) {
    Inputs inputs{};
    inputs.seed = 0U;
    inputs.idle_duration = 60.0F;
    inputs.sample_time = 60.0F;
    c.apply(inputs);
    EXPECT_FALSE(Animation::humanoid_ambient_eligible(inputs))
        << c.name << " should block ambient idles";
  }

  Inputs resting{};
  resting.seed = 0U;
  resting.idle_duration = 60.0F;
  resting.sample_time = 60.0F;
  EXPECT_TRUE(Animation::humanoid_ambient_eligible(resting));
}

TEST(AnimationCoreAmbientPoseManifest, TickSurvivesTimeJumpsWithoutTeleporting) {

  auto const first = Animation::resolve_humanoid_ambient_tick({
      .sample_time = 10.0F,
      .eligible = true,
      .seed = 5U,
      .idle_duration = 10.0F,
  });
  auto const jumped = Animation::resolve_humanoid_ambient_tick({
      .previous = first.state,
      .sample_time = 510.0F,
      .eligible = true,
      .seed = 5U,
      .idle_duration = 510.0F,
  });

  EXPECT_LE(jumped.state.blend,
            Animation::k_ambient_max_step / Animation::k_ambient_blend_in_duration +
                1.0e-4F);
  EXPECT_LE(jumped.state.clip_phase,
            Animation::k_ambient_max_step / Animation::k_ambient_clip_duration +
                1.0e-4F);
}

TEST(AnimationCoreAmbientPoseManifest, SitDownOwnsCrouchAndFootSpread) {
  auto const sample = Animation::resolve_humanoid_ambient_pose({
      .type = Animation::HumanoidAmbientIdle::SitDown,
      .phase = 0.50F,
  });

  EXPECT_FLOAT_EQ(sample.pelvis_y_delta, -0.18F);
  EXPECT_FLOAT_EQ(sample.shoulder_l_y_delta, -0.126F);
  EXPECT_FLOAT_EQ(sample.neck_y_delta, -0.1116F);
  EXPECT_FLOAT_EQ(sample.head_z_delta, 0.025F);
  EXPECT_FLOAT_EQ(sample.knee_l_z_delta, 0.09F);
  EXPECT_FLOAT_EQ(sample.foot_l_x_delta, -0.025F);
  EXPECT_FLOAT_EQ(sample.foot_r_x_delta, 0.025F);
}

TEST(AnimationCoreAmbientPoseManifest, RaiseWeaponOwnsHandAndHeadDeltas) {
  auto const sample = Animation::resolve_humanoid_ambient_pose({
      .type = Animation::HumanoidAmbientIdle::RaiseWeapon,
      .phase = 0.50F,
  });

  EXPECT_FLOAT_EQ(sample.hand_r_y_delta, 0.28F);
  EXPECT_FLOAT_EQ(sample.elbow_r_y_delta, 0.154F);
  EXPECT_NEAR(sample.hand_r_x_delta, 0.0F, 0.0001F);
  EXPECT_FLOAT_EQ(sample.hand_r_z_delta, -0.06F);
  EXPECT_FLOAT_EQ(sample.hand_l_y_delta, 0.07F);
  EXPECT_FLOAT_EQ(sample.head_y_delta, 0.018F);
  EXPECT_FLOAT_EQ(sample.head_z_delta, -0.035F);
}

TEST(AnimationCoreAmbientPoseManifest, JumpScalesWithAirborneState) {
  auto const grounded = Animation::resolve_humanoid_ambient_pose({
      .type = Animation::HumanoidAmbientIdle::Jump,
      .phase = 0.46F,
      .airborne = false,
  });
  auto const airborne = Animation::resolve_humanoid_ambient_pose({
      .type = Animation::HumanoidAmbientIdle::Jump,
      .phase = 0.46F,
      .airborne = true,
  });

  EXPECT_NEAR(grounded.pelvis_y_delta, 0.083F, 0.0001F);
  EXPECT_NEAR(grounded.foot_l_y_delta, 0.0595F, 0.0001F);
  EXPECT_NEAR(grounded.hand_r_y_delta, 0.0225F, 0.0001F);
  EXPECT_GT(airborne.pelvis_y_delta, grounded.pelvis_y_delta);
  EXPECT_GT(airborne.foot_l_y_delta, grounded.foot_l_y_delta);
  EXPECT_GT(airborne.hand_r_y_delta, grounded.hand_r_y_delta);
}

TEST(AnimationCoreAmbientPoseManifest, PlantFlagOwnsPlantDrive) {
  auto const sample = Animation::resolve_humanoid_ambient_pose({
      .type = Animation::HumanoidAmbientIdle::PlantFlag,
      .phase = 0.50F,
  });

  EXPECT_LT(sample.pelvis_y_delta, -0.16F);
  EXPECT_GT(sample.pelvis_z_delta, 0.03F);
  EXPECT_GT(sample.shoulder_r_z_delta, sample.shoulder_l_z_delta);
  EXPECT_GT(sample.knee_r_z_delta, sample.knee_l_z_delta);
  EXPECT_LT(sample.hand_r_y_delta, -0.30F);
  EXPECT_GT(sample.hand_r_z_delta, 0.15F);
  EXPECT_LT(sample.hand_l_x_delta, 0.0F);
  EXPECT_LT(sample.elbow_l_y_delta, 0.0F);
}

TEST(AnimationCoreMountedPoseManifest, IdleRestOwnsMountedHandTargets) {
  auto const sample = Animation::resolve_mounted_rider_hand_pose({
      .kind = Animation::MountedRiderHandPoseKind::IdleRest,
  });

  EXPECT_TRUE(sample.left.active);
  EXPECT_TRUE(sample.right.active);
  EXPECT_FALSE(sample.left.uses_rein);
  EXPECT_EQ(sample.left.anchor, Animation::MountedHandAnchor::Seat);
  EXPECT_FLOAT_EQ(sample.left.offset.forward, 0.12F);
  EXPECT_FLOAT_EQ(sample.left.offset.right, -0.14F);
  EXPECT_FLOAT_EQ(sample.right.offset.right, 0.14F);
  EXPECT_FLOAT_EQ(sample.left.elbow.y_bias, -0.05F);
  EXPECT_STREQ(sample.debug_label, "riding_idle");
}

TEST(AnimationCoreMountedPoseManifest, ReinHoldClampsControlsAndOwnsElbowProfile) {
  auto const sample = Animation::resolve_mounted_rider_hand_pose({
      .kind = Animation::MountedRiderHandPoseKind::ReinHold,
      .left_rein_slack = -0.50F,
      .right_rein_slack = 1.50F,
      .left_rein_tension = 1.25F,
      .right_rein_tension = -0.25F,
  });

  EXPECT_TRUE(sample.left.uses_rein);
  EXPECT_TRUE(sample.right.uses_rein);
  EXPECT_FLOAT_EQ(sample.left.rein_slack, 0.0F);
  EXPECT_FLOAT_EQ(sample.right.rein_slack, 1.0F);
  EXPECT_FLOAT_EQ(sample.left.rein_tension, 1.0F);
  EXPECT_FLOAT_EQ(sample.right.rein_tension, 0.0F);
  EXPECT_FLOAT_EQ(sample.left.elbow.along_frac, 0.45F);
  EXPECT_FLOAT_EQ(sample.left.elbow.lateral_offset, 0.12F);
  EXPECT_FLOAT_EQ(sample.left.elbow.y_bias, -0.08F);
}

TEST(AnimationCoreMountedPoseManifest, StowedShieldAndSwordIdleScaleFromMountDims) {
  auto const shield = Animation::resolve_mounted_rider_hand_pose({
      .kind = Animation::MountedRiderHandPoseKind::ShieldStowed,
      .body_length = 2.0F,
      .body_width = 0.60F,
      .saddle_thickness = 0.20F,
  });
  EXPECT_TRUE(shield.left.active);
  EXPECT_FALSE(shield.right.active);
  EXPECT_EQ(shield.left.anchor, Animation::MountedHandAnchor::Seat);
  EXPECT_FLOAT_EQ(shield.left.offset.forward, -0.10F);
  EXPECT_FLOAT_EQ(shield.left.offset.right, -0.33F);
  EXPECT_FLOAT_EQ(shield.left.offset.up, 0.10F);
  EXPECT_FLOAT_EQ(shield.left.elbow.along_frac, 0.42F);

  auto const sword = Animation::resolve_mounted_rider_hand_pose({
      .kind = Animation::MountedRiderHandPoseKind::SwordIdle,
      .body_length = 2.0F,
      .body_width = 0.60F,
      .body_height = 1.20F,
      .saddle_thickness = 0.20F,
  });
  EXPECT_FALSE(sword.left.active);
  EXPECT_TRUE(sword.right.active);
  EXPECT_EQ(sword.right.anchor, Animation::MountedHandAnchor::RightShoulder);
  EXPECT_FLOAT_EQ(sword.right.offset.forward, 0.44F);
  EXPECT_FLOAT_EQ(sword.right.offset.right, 0.54F);
  EXPECT_FLOAT_EQ(sword.right.offset.up, 0.092F);
  EXPECT_FLOAT_EQ(sword.right.elbow.lateral_offset, 0.24F);
}

TEST(AnimationCoreMountedPoseManifest, PosePlanOwnsSeatBiasAndReinClaims) {
  auto const forward = Animation::resolve_mounted_rider_pose_plan({
      .seat_pose = Animation::MountedSeatPoseKind::Forward,
      .forward_bias = 0.10F,
  });
  EXPECT_FLOAT_EQ(forward.forward_bias, 0.45F);
  EXPECT_TRUE(forward.apply_left_rein);
  EXPECT_TRUE(forward.apply_right_rein);

  auto const defensive = Animation::resolve_mounted_rider_pose_plan({
      .seat_pose = Animation::MountedSeatPoseKind::Defensive,
      .forward_bias = 0.10F,
  });
  EXPECT_FLOAT_EQ(defensive.forward_bias, -0.10F);

  auto const spear = Animation::resolve_mounted_rider_pose_plan({
      .weapon_pose = Animation::MountedWeaponPoseKind::SpearThrust,
  });
  EXPECT_TRUE(spear.weapon_claims_left_hand);
  EXPECT_TRUE(spear.weapon_claims_right_hand);
  EXPECT_FALSE(spear.apply_left_rein);
  EXPECT_FALSE(spear.apply_right_rein);
}

TEST(AnimationCoreMountedPoseManifest, PosePlanKeepsShieldHandForSwordStrike) {
  auto const sword_and_shield = Animation::resolve_mounted_rider_pose_plan({
      .weapon_pose = Animation::MountedWeaponPoseKind::SwordStrike,
      .shield_pose = Animation::MountedShieldPoseKind::Guard,
  });

  EXPECT_TRUE(sword_and_shield.shield_claims_left_hand);
  EXPECT_TRUE(sword_and_shield.weapon_claims_right_hand);
  EXPECT_FALSE(sword_and_shield.weapon_claims_left_hand);
  EXPECT_FALSE(sword_and_shield.apply_left_rein);
  EXPECT_FALSE(sword_and_shield.apply_right_rein);
  EXPECT_TRUE(sword_and_shield.sword_strike_keeps_left_hand);

  auto const sword_only = Animation::resolve_mounted_rider_pose_plan({
      .weapon_pose = Animation::MountedWeaponPoseKind::SwordStrike,
  });
  EXPECT_TRUE(sword_only.apply_left_rein);
  EXPECT_FALSE(sword_only.apply_right_rein);
  EXPECT_FALSE(sword_only.sword_strike_keeps_left_hand);
}

TEST(AnimationCoreAttackPoseManifest, MountedSpearThrustCouchesBeforeStrike) {
  auto const sample = Animation::resolve_mounted_spear_thrust_pose({
      .attack_phase = 0.10F,
  });

  EXPECT_NEAR(sample.right_hand.forward, 0.1025F, 0.0001F);
  EXPECT_NEAR(sample.right_hand.right, 0.1425F, 0.0001F);
  EXPECT_NEAR(sample.right_hand.up, 0.1325F, 0.0001F);
  EXPECT_NEAR(sample.left_hand.forward, 0.1095F, 0.0001F);
  EXPECT_NEAR(sample.left_hand.right, -0.1045F, 0.0001F);
  EXPECT_GT(sample.torso_compression, 0.0F);
  EXPECT_GT(sample.forward_lean, 0.0F);
  EXPECT_STREQ(sample.debug_label, "spear_couch");
}

TEST(AnimationCoreAttackPoseManifest, MountedSpearThrustDrivesForwardAtImpact) {
  auto const sample = Animation::resolve_mounted_spear_thrust_pose({
      .attack_phase = 0.55F,
  });

  EXPECT_GT(sample.right_hand.forward, 0.46F);
  EXPECT_LT(sample.right_hand.forward, 0.53F);
  EXPECT_LT(sample.right_hand.right, 0.08F);
  EXPECT_LT(sample.right_hand.up, 0.02F);
  EXPECT_LT(sample.left_hand.up, 0.02F);
  EXPECT_FLOAT_EQ(sample.forward_lean, 0.14F);
  EXPECT_FLOAT_EQ(sample.torso_twist, 0.05F);
  EXPECT_FLOAT_EQ(sample.shoulder_drop, 0.04F);
  EXPECT_FLOAT_EQ(sample.head_forward_tilt, 0.30F);
  EXPECT_STREQ(sample.debug_label, "spear_extend");
}

TEST(AnimationCoreAttackPoseManifest, MountedSpearPiercesForwardAndDownWithoutSweep) {
  auto const couch = Animation::resolve_mounted_spear_thrust_pose({
      .attack_phase = 0.25F,
  });
  auto const pierce = Animation::resolve_mounted_spear_thrust_pose({
      .attack_phase = 0.60F,
  });

  EXPECT_GT(pierce.right_hand.forward, couch.right_hand.forward + 0.38F);
  EXPECT_LT(pierce.right_hand.up, couch.right_hand.up - 0.08F);
  EXPECT_LT(std::abs(pierce.right_hand.right - couch.right_hand.right), 0.10F);
  EXPECT_GT(pierce.left_hand.forward, couch.left_hand.forward + 0.38F);
  EXPECT_LT(pierce.left_hand.up, couch.left_hand.up - 0.08F);
}

TEST(AnimationCoreAttackPoseManifest, MountedSpearThrustRecoversToGuard) {
  auto const sample = Animation::resolve_mounted_spear_thrust_pose({
      .attack_phase = 1.0F,
  });

  EXPECT_FLOAT_EQ(sample.right_hand.forward, 0.12F);
  EXPECT_FLOAT_EQ(sample.right_hand.right, 0.15F);
  EXPECT_FLOAT_EQ(sample.right_hand.up, 0.15F);
  EXPECT_FLOAT_EQ(sample.left_hand.forward, 0.12F);
  EXPECT_FLOAT_EQ(sample.left_hand.right, -0.10F);
  EXPECT_FLOAT_EQ(sample.left_hand.up, 0.15F);
  EXPECT_NEAR(sample.forward_lean, 0.0F, 0.0001F);
  EXPECT_NEAR(sample.torso_twist, 0.0F, 0.0001F);
  EXPECT_NEAR(sample.shoulder_drop, 0.0F, 0.0001F);
  EXPECT_FLOAT_EQ(sample.head_forward_tilt, 0.0F);
  EXPECT_STREQ(sample.debug_label, "spear_recover");
}

TEST(AnimationCoreAttackPoseManifest, MountedSwordStrikeChambers) {
  auto const sample = Animation::resolve_mounted_sword_strike_pose({
      .attack_phase = 0.11F,
  });

  EXPECT_NEAR(sample.right_hand.forward, 0.0375F, 0.0001F);
  EXPECT_NEAR(sample.right_hand.right, 0.265F, 0.0001F);
  EXPECT_NEAR(sample.right_hand.up, 0.20F, 0.0001F);
  EXPECT_NEAR(sample.torso_twist, -0.0125F, 0.0001F);
  EXPECT_NEAR(sample.torso_commit, -0.005F, 0.0001F);
  EXPECT_NEAR(sample.shoulder_dip, 0.01F, 0.0001F);
  EXPECT_FLOAT_EQ(sample.left_hand_up_offset, -0.02F);
  EXPECT_STREQ(sample.debug_label, "sword_chamber");
}

TEST(AnimationCoreAttackPoseManifest, MountedSwordStrikeCommitsBodyAtImpact) {
  auto const sample = Animation::resolve_mounted_sword_strike_pose({
      .attack_phase = 0.55F,
  });

  EXPECT_NEAR(sample.right_hand.forward, 0.27F, 0.0001F);
  EXPECT_NEAR(sample.right_hand.right, 0.48F, 0.0001F);
  EXPECT_NEAR(sample.right_hand.up, 0.35F, 0.0001F);
  EXPECT_NEAR(sample.torso_twist, 0.045F, 0.0001F);
  EXPECT_NEAR(sample.side_lean, 0.07F, 0.0001F);
  EXPECT_NEAR(sample.forward_lean, 0.065F, 0.0001F);
  EXPECT_NEAR(sample.torso_commit, 0.105F, 0.0001F);
  EXPECT_NEAR(sample.counter_lift, 0.055F, 0.0001F);
  EXPECT_NEAR(sample.left_hand_up_offset, -0.045F, 0.0001F);
  EXPECT_NEAR(sample.head_forward_tilt, 0.20F, 0.0001F);
  EXPECT_NEAR(sample.head_side_tilt, 0.08F, 0.0001F);
  EXPECT_STREQ(sample.debug_label, "sword_strike");
}

TEST(AnimationCoreAttackPoseManifest, MountedSwordStrikeRecoversToRest) {
  auto const sample = Animation::resolve_mounted_sword_strike_pose({
      .attack_phase = 1.0F,
  });

  EXPECT_FLOAT_EQ(sample.right_hand.forward, 0.08F);
  EXPECT_FLOAT_EQ(sample.right_hand.right, 0.24F);
  EXPECT_FLOAT_EQ(sample.right_hand.up, 0.12F);
  EXPECT_NEAR(sample.torso_twist, 0.0F, 0.0001F);
  EXPECT_NEAR(sample.side_lean, 0.0F, 0.0001F);
  EXPECT_NEAR(sample.forward_lean, 0.0F, 0.0001F);
  EXPECT_NEAR(sample.torso_commit, 0.0F, 0.0001F);
  EXPECT_NEAR(sample.counter_lift, 0.0F, 0.0001F);
  EXPECT_NEAR(sample.shoulder_dip, 0.0F, 0.0001F);
  EXPECT_FLOAT_EQ(sample.left_hand_up_offset, -0.02F);
  EXPECT_STREQ(sample.debug_label, "sword_recover");
}

TEST(AnimationCoreAttackPoseManifest, MountedSpearGuardExposesGripTargets) {
  auto const overhand = Animation::resolve_mounted_spear_guard_pose({
      .grip = Animation::MountedSpearGuardGrip::Overhand,
  });
  auto const couched = Animation::resolve_mounted_spear_guard_pose({
      .grip = Animation::MountedSpearGuardGrip::Couched,
  });
  auto const two_handed = Animation::resolve_mounted_spear_guard_pose({
      .grip = Animation::MountedSpearGuardGrip::TwoHanded,
  });

  EXPECT_TRUE(overhand.left_hand_uses_rein);
  EXPECT_FLOAT_EQ(overhand.right_hand.up, 0.55F);
  EXPECT_FLOAT_EQ(overhand.left_rein_slack, 0.30F);
  EXPECT_FLOAT_EQ(couched.right_hand.forward, -0.15F);
  EXPECT_FLOAT_EQ(couched.left_rein_tension, 0.20F);
  EXPECT_FALSE(two_handed.left_hand_uses_rein);
  EXPECT_FLOAT_EQ(two_handed.left_hand.right, -0.10F);
  EXPECT_STREQ(two_handed.debug_label, "spear_guard_pose");
}

TEST(AnimationCoreAttackPoseManifest, MountedBowDrawOwnsDrawHandCurve) {
  auto const prepare = Animation::resolve_mounted_bow_draw_pose({
      .draw_phase = 0.15F,
  });
  auto const hold = Animation::resolve_mounted_bow_draw_pose({
      .draw_phase = 0.50F,
  });
  auto const recover = Animation::resolve_mounted_bow_draw_pose({
      .draw_phase = 1.0F,
  });

  EXPECT_FLOAT_EQ(prepare.left_hand.forward, 0.38F);
  EXPECT_NEAR(prepare.right_hand.forward, 0.265F, 0.0001F);
  EXPECT_NEAR(prepare.right_hand.right, 0.055F, 0.0001F);
  EXPECT_NEAR(prepare.right_hand.up, 0.415F, 0.0001F);
  EXPECT_NEAR(prepare.right_hand_world_y_offset, -0.01125F, 0.0001F);
  EXPECT_FLOAT_EQ(hold.right_hand.forward, 0.04F);
  EXPECT_FLOAT_EQ(hold.right_hand.right, 0.16F);
  EXPECT_FLOAT_EQ(hold.right_hand.up, 0.46F);
  EXPECT_FLOAT_EQ(recover.right_hand.forward, 0.34F);
  EXPECT_NEAR(recover.right_hand_world_y_offset, -0.015F, 0.0001F);
  EXPECT_STREQ(recover.debug_label, "bow_draw");
}

TEST(AnimationCoreAttackPoseManifest, MountedWeaponCurvesStayContinuousAndSeated) {
  auto distance = [](Animation::MountedSeatOffset const& a,
                     Animation::MountedSeatOffset const& b) {
    float const forward = a.forward - b.forward;
    float const right = a.right - b.right;
    float const up = a.up - b.up;
    return std::sqrt(forward * forward + right * right + up * up);
  };

  auto previous_sword = Animation::resolve_mounted_sword_strike_pose({0.0F});
  auto previous_spear = Animation::resolve_mounted_spear_thrust_pose({0.0F});
  auto previous_bow = Animation::resolve_mounted_bow_draw_pose({0.0F});
  for (int frame = 1; frame <= 100; ++frame) {
    float const phase = static_cast<float>(frame) / 100.0F;
    auto const sword = Animation::resolve_mounted_sword_strike_pose({phase});
    auto const spear = Animation::resolve_mounted_spear_thrust_pose({phase});
    auto const bow = Animation::resolve_mounted_bow_draw_pose({phase});

    EXPECT_LT(distance(previous_sword.right_hand, sword.right_hand), 0.08F)
        << "sword phase=" << phase;
    EXPECT_LT(distance(previous_spear.right_hand, spear.right_hand), 0.08F)
        << "spear phase=" << phase;
    EXPECT_LT(distance(previous_bow.right_hand, bow.right_hand), 0.08F)
        << "bow phase=" << phase;
    EXPECT_GT(sword.right_hand.up, 0.05F) << "sword phase=" << phase;
    EXPECT_LT(spear.right_hand.forward, 0.80F) << "spear phase=" << phase;
    EXPECT_GT(bow.left_hand.up, 0.35F) << "bow phase=" << phase;
    EXPECT_GT(bow.right_hand.up + bow.right_hand_world_y_offset, 0.35F)
        << "bow phase=" << phase;

    previous_sword = sword;
    previous_spear = spear;
    previous_bow = bow;
  }
}

TEST(AnimationCoreAttackPoseManifest, MountedShieldDefenseOwnsRaisedAndRestPose) {
  auto const rest = Animation::resolve_mounted_shield_defense_pose({
      .raised = false,
  });
  auto const raised = Animation::resolve_mounted_shield_defense_pose({
      .raised = true,
  });

  EXPECT_FLOAT_EQ(rest.left_hand.forward, 0.05F);
  EXPECT_FLOAT_EQ(rest.left_hand.right, -0.16F);
  EXPECT_FLOAT_EQ(rest.right_rein_slack, 0.30F);
  EXPECT_FLOAT_EQ(rest.right_rein_tension, 0.25F);
  EXPECT_FLOAT_EQ(raised.left_hand.up, 0.40F);
  EXPECT_FLOAT_EQ(raised.right_rein_slack, 0.15F);
  EXPECT_FLOAT_EQ(raised.right_rein_tension, 0.45F);
  EXPECT_STREQ(raised.debug_label, "shield_defense");
}

TEST(AnimationCoreLocomotionManifest, BasePoseOwnsProportionsAndSeededAsymmetry) {
  Animation::HumanoidBasePoseInputs inputs{};
  inputs.seed = 0U;
  inputs.height_scale = 1.0F;
  inputs.bulk_scale = 1.20F;
  inputs.stance_width = 1.10F;
  inputs.posture_slump = 0.05F;
  inputs.shoulder_tilt = 0.02F;
  inputs.proportions = {
      .chest_y = 1.10F,
      .ground_y = 0.0F,
      .head_center_y = 1.60F,
      .head_radius = 0.10F,
      .neck_base_y = 1.45F,
      .shoulder_y = 1.35F,
      .shoulder_width = 0.50F,
      .waist_y = 0.80F,
      .foot_y_offset_default = 0.02F,
  };

  auto const first = Animation::resolve_humanoid_base_pose(inputs);
  auto const second = Animation::resolve_humanoid_base_pose(inputs);
  inputs.seed = 1U;
  auto const other_seed = Animation::resolve_humanoid_base_pose(inputs);

  EXPECT_FLOAT_EQ(first.head_radius, 0.10F);
  EXPECT_FLOAT_EQ(first.head_pos.y, 1.60F);
  EXPECT_FLOAT_EQ(first.head_pos.z, 0.014775F);
  EXPECT_FLOAT_EQ(first.neck_base.z, 0.012675F);
  EXPECT_FLOAT_EQ(first.shoulder_l.x, -0.255F);
  EXPECT_FLOAT_EQ(first.shoulder_l.y, 1.37F);
  EXPECT_FLOAT_EQ(first.shoulder_l.z, 0.0255F);
  EXPECT_FLOAT_EQ(first.shoulder_r.x, 0.255F);
  EXPECT_FLOAT_EQ(first.shoulder_r.y, 1.33F);
  EXPECT_FLOAT_EQ(first.pelvis.y, 0.782F);
  EXPECT_FLOAT_EQ(first.pelvis.z, -0.010F);
  EXPECT_FLOAT_EQ(first.foot_y_offset, 0.02F);
  EXPECT_NEAR(first.foot_l.x + first.foot_r.x, 0.0F, 0.000001F);
  EXPECT_NEAR(first.foot_l.z + first.foot_r.z, 0.0F, 0.000001F);
  EXPECT_FLOAT_EQ(first.foot_l.x, second.foot_l.x);
  EXPECT_FLOAT_EQ(first.hand_l.x, second.hand_l.x);
  EXPECT_NE(first.foot_l.z, other_seed.foot_l.z);
  EXPECT_NE(first.hand_l.x, other_seed.hand_l.x);
}

TEST(AnimationCoreLocomotionManifest, LocomotionPoseIsInactiveForIdle) {
  auto const sample = Animation::resolve_humanoid_locomotion_pose({
      .state = Animation::HumanoidMotionState::Idle,
      .base_foot_l = {-0.2F, 0.02F, 0.02F},
      .base_foot_r = {0.2F, 0.02F, -0.02F},
  });

  EXPECT_FALSE(sample.active);
  EXPECT_FLOAT_EQ(sample.pelvis_delta.x, 0.0F);
  EXPECT_FLOAT_EQ(sample.hand_l_delta.z, 0.0F);
}

TEST(AnimationCoreLocomotionManifest, LocomotionPoseOwnsWalkCycleDeltas) {
  Animation::HumanoidLocomotionPoseInputs inputs{};
  inputs.state = Animation::HumanoidMotionState::Walk;
  inputs.normalized_speed = 1.0F;
  inputs.cycle_phase = 0.25F;
  inputs.stride_distance = 2.35F * 0.92F;
  inputs.locomotion_blend = 1.0F;
  inputs.walk_speed_multiplier = 1.0F;
  inputs.stance_width = 1.0F;
  inputs.arm_swing_amplitude = 1.0F;
  inputs.reference_walk_speed = 2.35F;
  inputs.reference_run_speed = 5.10F;
  inputs.ground_y = 0.0F;
  inputs.foot_y_offset = 0.02F;
  inputs.base_foot_l = {-0.20F, 0.02F, 0.02F};
  inputs.base_foot_r = {0.20F, 0.02F, -0.02F};

  auto const first = Animation::resolve_humanoid_locomotion_pose(inputs);
  auto const second = Animation::resolve_humanoid_locomotion_pose(inputs);

  EXPECT_TRUE(first.active);
  EXPECT_FLOAT_EQ(first.foot_l.x, second.foot_l.x);
  EXPECT_FLOAT_EQ(first.foot_l.z, second.foot_l.z);
  EXPECT_NE(first.foot_l.z, inputs.base_foot_l.z);
  EXPECT_NE(first.foot_r.z, inputs.base_foot_r.z);
  EXPECT_LT(first.pelvis_delta.y, 0.0F);
  EXPECT_NE(first.shoulder_l_delta.z, 0.0F);
  EXPECT_NE(first.hand_l_delta.z, 0.0F);
  EXPECT_LT(first.hand_l_delta.z * first.hand_r_delta.z, 0.0F);
  EXPECT_GT(std::abs(first.hand_l_delta.z), std::abs(first.hand_r_delta.z));
}

TEST(AnimationCoreLocomotionManifest, LocomotionPoseRunDrivesBodyFurtherForward) {
  Animation::HumanoidLocomotionPoseInputs walk{};
  walk.state = Animation::HumanoidMotionState::Walk;
  walk.normalized_speed = 1.0F;
  walk.cycle_phase = 0.25F;
  walk.locomotion_blend = 1.0F;
  walk.walk_speed_multiplier = 1.0F;
  walk.stance_width = 1.0F;
  walk.arm_swing_amplitude = 1.0F;
  walk.reference_walk_speed = 2.35F;
  walk.reference_run_speed = 5.10F;
  walk.ground_y = 0.0F;
  walk.foot_y_offset = 0.02F;
  walk.base_foot_l = {-0.20F, 0.02F, 0.02F};
  walk.base_foot_r = {0.20F, 0.02F, -0.02F};

  auto run = walk;
  run.state = Animation::HumanoidMotionState::Run;
  run.run_blend = 1.0F;

  auto const walk_pose = Animation::resolve_humanoid_locomotion_pose(walk);
  auto const run_pose = Animation::resolve_humanoid_locomotion_pose(run);

  EXPECT_TRUE(walk_pose.active);
  EXPECT_TRUE(run_pose.active);
  EXPECT_GT(run_pose.pelvis_delta.z, walk_pose.pelvis_delta.z);
  EXPECT_GT(run_pose.shoulder_l_delta.z, walk_pose.shoulder_l_delta.z);
  EXPECT_GT(std::abs(run_pose.hand_l_delta.z), std::abs(walk_pose.hand_l_delta.z));
}

TEST(AnimationCoreLocomotionManifest, LocomotionVariationKeepsIdleTuningStable) {
  auto const sample = Animation::resolve_humanoid_locomotion_variation({
      .has_locomotion = false,
      .running = false,
      .walk_speed_multiplier = 1.10F,
      .arm_swing_amplitude = 0.90F,
      .stance_width = 1.20F,
      .posture_slump = 0.04F,
  });

  EXPECT_FLOAT_EQ(sample.walk_speed_multiplier, 1.10F);
  EXPECT_FLOAT_EQ(sample.arm_swing_amplitude, 0.90F);
  EXPECT_FLOAT_EQ(sample.stance_width, 1.20F);
  EXPECT_FLOAT_EQ(sample.posture_slump, 0.04F);
}

TEST(AnimationCoreLocomotionManifest, LocomotionVariationOwnsWalkAndRunTuning) {
  auto const walk = Animation::resolve_humanoid_locomotion_variation({
      .has_locomotion = true,
      .running = false,
      .walk_speed_multiplier = 1.10F,
      .arm_swing_amplitude = 0.90F,
      .stance_width = 1.20F,
      .posture_slump = 0.04F,
  });
  EXPECT_NEAR(walk.walk_speed_multiplier, 1.10F * 1.05F, 1.0e-6F);
  EXPECT_FLOAT_EQ(walk.arm_swing_amplitude, 0.90F);
  EXPECT_FLOAT_EQ(walk.stance_width, 1.20F);
  EXPECT_FLOAT_EQ(walk.posture_slump, 0.04F);

  auto const run = Animation::resolve_humanoid_locomotion_variation({
      .has_locomotion = true,
      .running = true,
      .walk_speed_multiplier = 1.10F,
      .arm_swing_amplitude = 0.90F,
      .stance_width = 1.20F,
      .posture_slump = 0.15F,
  });
  EXPECT_NEAR(run.walk_speed_multiplier, 1.10F * 1.25F, 1.0e-6F);
  EXPECT_NEAR(run.arm_swing_amplitude, 0.90F * 1.12F, 1.0e-6F);
  EXPECT_NEAR(run.stance_width, 1.20F * 0.96F, 1.0e-6F);
  EXPECT_FLOAT_EQ(run.posture_slump, 0.16F);
}

TEST(AnimationCoreLocomotionManifest, LocomotionActionOverrideFreezesJumpingCommander) {
  auto const inactive = Animation::resolve_humanoid_locomotion_action_override({
      .commander_jump_active = false,
  });
  EXPECT_FALSE(inactive.active);

  auto const jump = Animation::resolve_humanoid_locomotion_action_override({
      .commander_jump_active = true,
  });
  EXPECT_TRUE(jump.active);
  EXPECT_EQ(jump.state, Animation::HumanoidMotionState::Idle);
  EXPECT_FLOAT_EQ(jump.move_speed, 0.0F);
  EXPECT_FLOAT_EQ(jump.normalized_speed, 0.0F);
  EXPECT_FALSE(jump.has_target);
  EXPECT_TRUE(jump.airborne);
}

TEST(AnimationCoreLocomotionManifest, LocomotionPhaseOverrideOwnsBowReadyIdle) {
  auto const idle = Animation::resolve_humanoid_locomotion_phase_override({
      .bow_ready_idle = true,
      .has_locomotion = false,
      .attacking = false,
  });
  EXPECT_TRUE(idle.active);
  EXPECT_FLOAT_EQ(idle.cycle_phase, 0.5F);

  EXPECT_FALSE(
      Animation::resolve_humanoid_locomotion_phase_override({
                                                                .bow_ready_idle = false,
                                                                .has_locomotion = false,
                                                                .attacking = false,
                                                            })
          .active);
  EXPECT_FALSE(
      Animation::resolve_humanoid_locomotion_phase_override({
                                                                .bow_ready_idle = true,
                                                                .has_locomotion = true,
                                                                .attacking = false,
                                                            })
          .active);
  EXPECT_FALSE(
      Animation::resolve_humanoid_locomotion_phase_override({
                                                                .bow_ready_idle = true,
                                                                .has_locomotion = false,
                                                                .attacking = true,
                                                            })
          .active);
}

namespace {

auto step_locomotion(Animation::HumanoidLocomotionInputs inputs,
                     float entity_forward_x,
                     float entity_forward_z,
                     float direction_x,
                     float direction_z,
                     int frames,
                     float delta_time) -> Animation::HumanoidLocomotionSample {
  inputs.entity_forward_x = entity_forward_x;
  inputs.entity_forward_z = entity_forward_z;
  inputs.locomotion_direction_x = direction_x;
  inputs.locomotion_direction_z = direction_z;
  inputs.has_persistent_state = true;
  inputs.allow_persistent_update = true;

  Animation::HumanoidLocomotionSample sample{};
  for (int frame = 0; frame < frames; ++frame) {
    inputs.sample_time += delta_time;
    sample = Animation::resolve_humanoid_locomotion_sample(inputs);
    inputs.previous = sample.persistent;
  }
  return sample;
}

auto walking_inputs() -> Animation::HumanoidLocomotionInputs {
  Animation::HumanoidLocomotionInputs inputs{};
  inputs.movement_state = Animation::MovementState::Walk;
  inputs.motion_state = Animation::HumanoidMotionState::Walk;
  inputs.speed = 2.2F;
  inputs.sample_time = 0.0F;
  return inputs;
}

} // namespace

TEST(AnimationCoreLocomotionManifest, WalkingForwardRunsTheStepCycleForward) {
  auto const sample =
      step_locomotion(walking_inputs(), 0.0F, 1.0F, 0.0F, 1.0F, 40, 1.0F / 60.0F);

  EXPECT_FALSE(sample.reverse_gait);
  EXPECT_NEAR(sample.travel_alignment, 1.0F, 0.05F);
}

TEST(AnimationCoreLocomotionManifest, BackingUpReversesTheStepCycle) {
  auto const sample =
      step_locomotion(walking_inputs(), 0.0F, 1.0F, 0.0F, -1.0F, 40, 1.0F / 60.0F);

  EXPECT_TRUE(sample.reverse_gait);
  EXPECT_NEAR(sample.travel_alignment, -1.0F, 0.05F);

  auto inputs = walking_inputs();
  inputs.entity_forward_x = 0.0F;
  inputs.entity_forward_z = 1.0F;
  inputs.locomotion_direction_x = 0.0F;
  inputs.locomotion_direction_z = -1.0F;
  inputs.has_persistent_state = true;
  inputs.allow_persistent_update = true;
  inputs.previous = sample.persistent;
  inputs.sample_time = sample.persistent.last_sample_time + (1.0F / 60.0F);

  auto const next = Animation::resolve_humanoid_locomotion_sample(inputs);
  float delta = next.cycle_phase - sample.cycle_phase;
  if (delta > 0.5F) {
    delta -= 1.0F;
  } else if (delta < -0.5F) {
    delta += 1.0F;
  }
  EXPECT_LT(delta, 0.0F);
}

TEST(AnimationCoreLocomotionManifest, StrafingKeepsTheForwardStepCycle) {
  auto const sample =
      step_locomotion(walking_inputs(), 0.0F, 1.0F, 1.0F, 0.0F, 40, 1.0F / 60.0F);

  EXPECT_FALSE(sample.reverse_gait);
  EXPECT_NEAR(sample.travel_alignment, 0.0F, 0.10F);
}

TEST(AnimationCoreLocomotionManifest, ReverseGaitHoldsThroughASidewaysDrift) {
  auto backing =
      step_locomotion(walking_inputs(), 0.0F, 1.0F, 0.0F, -1.0F, 40, 1.0F / 60.0F);
  ASSERT_TRUE(backing.reverse_gait);

  auto inputs = walking_inputs();
  inputs.has_persistent_state = true;
  inputs.allow_persistent_update = true;
  inputs.previous = backing.persistent;
  inputs.sample_time = backing.persistent.last_sample_time;
  inputs.entity_forward_x = 0.0F;
  inputs.entity_forward_z = 1.0F;

  auto const drifting =
      step_locomotion(inputs, 0.0F, 1.0F, 0.7F, -0.7F, 20, 1.0F / 60.0F);
  EXPECT_TRUE(drifting.reverse_gait);
}

TEST(AnimationCoreLocomotionManifest, BackwardStepsAreShorterThanForwardSteps) {
  Animation::HumanoidLocomotionPoseInputs pose{};
  pose.state = Animation::HumanoidMotionState::Walk;
  pose.normalized_speed = 1.0F;
  pose.cycle_phase = 0.25F;
  pose.stride_distance = 1.4F;
  pose.locomotion_blend = 1.0F;
  pose.travel_alignment = 1.0F;

  auto const forward = Animation::resolve_humanoid_locomotion_pose(pose);
  pose.travel_alignment = -1.0F;
  auto const backward = Animation::resolve_humanoid_locomotion_pose(pose);

  ASSERT_TRUE(forward.active);
  ASSERT_TRUE(backward.active);
  EXPECT_LT(std::abs(backward.foot_l.z), std::abs(forward.foot_l.z));
}

TEST(AnimationCoreLocomotionManifest, RunSampleIsDeterministic) {
  Animation::HumanoidLocomotionInputs inputs{};
  inputs.movement_state = Animation::MovementState::Run;
  inputs.motion_state = Animation::HumanoidMotionState::Run;
  inputs.speed = 4.8F;
  inputs.locomotion_direction_z = 1.0F;
  inputs.sample_time = 1.5F;
  inputs.phase_offset = 0.125F;
  inputs.tuning.walk_speed_multiplier = 1.10F;

  auto const first = Animation::resolve_humanoid_locomotion_sample(inputs);
  auto const second = Animation::resolve_humanoid_locomotion_sample(inputs);

  EXPECT_EQ(first.state, Animation::HumanoidMotionState::Run);
  EXPECT_EQ(first.state, second.state);
  EXPECT_FLOAT_EQ(first.cycle_time, second.cycle_time);
  EXPECT_FLOAT_EQ(first.cycle_phase, second.cycle_phase);
  EXPECT_FLOAT_EQ(first.normalized_speed, second.normalized_speed);
  EXPECT_FLOAT_EQ(first.stride_distance, second.stride_distance);
  EXPECT_GT(first.cycle_time, 0.0F);
  EXPECT_GT(first.stride_distance, 0.0F);
}

namespace {

auto walk_pose_inputs() -> Animation::HumanoidLocomotionPoseInputs {
  Animation::HumanoidLocomotionPoseInputs inputs{};
  inputs.state = Animation::HumanoidMotionState::Walk;
  inputs.normalized_speed = 1.0F;
  inputs.locomotion_blend = 1.0F;
  inputs.stride_distance = 2.35F * 0.92F;
  inputs.walk_speed_multiplier = 1.0F;
  inputs.stance_width = 1.0F;
  inputs.arm_swing_amplitude = 1.0F;
  inputs.reference_walk_speed = 2.35F;
  inputs.reference_run_speed = 5.10F;
  inputs.ground_y = 0.0F;
  inputs.foot_y_offset = 0.02F;
  inputs.base_foot_l = {-0.20F, 0.02F, 0.02F};
  inputs.base_foot_r = {0.20F, 0.02F, -0.02F};
  return inputs;
}

auto foot_z_samples(Animation::HumanoidLocomotionPoseInputs inputs,
                    int steps) -> std::vector<float> {
  std::vector<float> samples;
  samples.reserve(static_cast<std::size_t>(steps) + 1U);
  for (int i = 0; i <= steps; ++i) {
    inputs.cycle_phase = static_cast<float>(i) / static_cast<float>(steps);
    samples.push_back(Animation::resolve_humanoid_locomotion_pose(inputs).foot_l.z);
  }
  return samples;
}

} // namespace

TEST(AnimationCoreLocomotionManifest, WalkSwingLeavesTheGroundWithoutAJerk) {
  constexpr int k_steps = 720;
  auto const samples = foot_z_samples(walk_pose_inputs(), k_steps);

  std::vector<float> deltas;
  deltas.reserve(samples.size());
  for (std::size_t i = 1; i < samples.size(); ++i) {
    deltas.push_back(std::abs(samples[i] - samples[i - 1U]));
  }
  auto const mean_delta = std::accumulate(deltas.begin(), deltas.end(), 0.0F) /
                          static_cast<float>(deltas.size());
  auto const worst_delta = *std::max_element(deltas.begin(), deltas.end());

  ASSERT_GT(mean_delta, 0.0F);
  EXPECT_LT(worst_delta, mean_delta * 3.0F);
}

TEST(AnimationCoreLocomotionManifest, WalkCycleClosesOnItself) {
  auto inputs = walk_pose_inputs();
  constexpr float k_step = 1.0F / 720.0F;

  inputs.cycle_phase = 0.0F;
  auto const start = Animation::resolve_humanoid_locomotion_pose(inputs);
  inputs.cycle_phase = 1.0F - k_step;
  auto const wrap = Animation::resolve_humanoid_locomotion_pose(inputs);
  inputs.cycle_phase = k_step;
  auto const next = Animation::resolve_humanoid_locomotion_pose(inputs);

  float const step_into_wrap = std::abs(start.foot_l.z - wrap.foot_l.z);
  float const step_after_wrap = std::abs(next.foot_l.z - start.foot_l.z);
  EXPECT_LT(step_into_wrap, 0.01F);
  EXPECT_NEAR(step_into_wrap, step_after_wrap, 0.004F);
  EXPECT_NEAR(start.foot_l.y, wrap.foot_l.y, 0.004F);
  EXPECT_NEAR(start.foot_pitch_l, wrap.foot_pitch_l, 0.05F);
}

namespace {

auto stance_retreat_per_cycle(Animation::HumanoidLocomotionPoseInputs inputs,
                              float early_phase,
                              float late_phase) -> float {
  inputs.cycle_phase = early_phase;
  float const early = Animation::resolve_humanoid_locomotion_pose(inputs).foot_l.z;
  inputs.cycle_phase = late_phase;
  float const late = Animation::resolve_humanoid_locomotion_pose(inputs).foot_l.z;
  return (early - late) / (late_phase - early_phase);
}

} // namespace

TEST(AnimationCoreLocomotionManifest, WalkCadenceKeepsThePlantedFootStill) {
  float const retreat = stance_retreat_per_cycle(walk_pose_inputs(), 0.20F, 0.40F);
  ASSERT_GT(retreat, 0.0F);

  for (float const speed : {1.6F, 2.0F, 2.35F, 2.8F}) {
    float const cycle_time = Animation::humanoid_walk_cycle_time_for_speed(speed);
    EXPECT_NEAR(retreat, speed * cycle_time, retreat * 0.05F)
        << "walking at " << speed << " m/s slides the planted foot";
  }
}

TEST(AnimationCoreLocomotionManifest, RunCadenceKeepsThePlantedFootStill) {
  auto inputs = walk_pose_inputs();
  inputs.state = Animation::HumanoidMotionState::Run;
  inputs.run_blend = 1.0F;
  inputs.stride_distance = 0.0F;
  float const retreat = stance_retreat_per_cycle(inputs, 0.10F, 0.25F);
  ASSERT_GT(retreat, 0.0F);

  for (float const speed : {2.8F, 3.3F, 4.0F, 4.8F}) {
    float const cycle_time = Animation::humanoid_run_cycle_time_for_speed(speed);
    EXPECT_NEAR(retreat, speed * cycle_time, retreat * 0.05F)
        << "running at " << speed << " m/s slides the planted foot";
  }
}

TEST(AnimationCoreLocomotionManifest, ArmSwingStaysWithinTheArmsReach) {
  auto inputs = walk_pose_inputs();
  constexpr float k_arm_length = 0.55F;
  inputs.arm_pendulum_length = k_arm_length;

  for (int step = 0; step < 64; ++step) {
    inputs.cycle_phase = static_cast<float>(step) / 64.0F;
    auto const pose = Animation::resolve_humanoid_locomotion_pose(inputs);
    for (auto const& hand : {pose.hand_l_delta, pose.hand_r_delta}) {
      float const shoulder_to_hand_y = -k_arm_length + hand.y;
      float const reach = std::sqrt((shoulder_to_hand_y * shoulder_to_hand_y) +
                                    (hand.z * hand.z) + (hand.x * hand.x));
      EXPECT_LE(reach, k_arm_length)
          << "the swing straightens the elbow at phase " << inputs.cycle_phase;
    }
  }
}

TEST(AnimationCoreLocomotionManifest, PelvisLeansOverTheSupportingLeg) {
  auto inputs = walk_pose_inputs();

  inputs.cycle_phase = 0.25F;
  auto const left_stance = Animation::resolve_humanoid_locomotion_pose(inputs);
  inputs.cycle_phase = 0.75F;
  auto const right_stance = Animation::resolve_humanoid_locomotion_pose(inputs);

  EXPECT_LT(left_stance.pelvis_delta.x, -0.01F);
  EXPECT_GT(right_stance.pelvis_delta.x, 0.01F);

  EXPECT_LT(left_stance.shoulder_l_delta.x, 0.0F);
  EXPECT_GT(std::abs(left_stance.pelvis_delta.x),
            std::abs(left_stance.shoulder_l_delta.x));
}

TEST(AnimationCoreLocomotionManifest, PelvisCounterRotatesAgainstTheShoulders) {
  auto inputs = walk_pose_inputs();

  for (float const phase : {0.05F, 0.20F, 0.55F, 0.70F}) {
    inputs.cycle_phase = phase;
    auto const pose = Animation::resolve_humanoid_locomotion_pose(inputs);
    float const hip_lead = pose.hip_l_delta.z - pose.hip_r_delta.z;
    float const shoulder_lead = pose.shoulder_l_delta.z - pose.shoulder_r_delta.z;
    EXPECT_LT(hip_lead * shoulder_lead, 0.0F)
        << "hips and shoulders rotate together at phase " << phase;
  }
}

TEST(AnimationCoreLocomotionManifest, GaitKeepsCyclingWhileTheStrideBlendsOut) {
  auto const walking =
      step_locomotion(walking_inputs(), 0.0F, 1.0F, 0.0F, 1.0F, 60, 1.0F / 60.0F);
  ASSERT_GT(walking.locomotion_blend, 0.5F);

  auto inputs = walking_inputs();
  inputs.movement_state = Animation::MovementState::Idle;
  inputs.motion_state = Animation::HumanoidMotionState::Idle;
  inputs.speed = 0.0F;
  inputs.has_persistent_state = true;
  inputs.allow_persistent_update = true;
  inputs.previous = walking.persistent;
  inputs.sample_time = walking.persistent.last_sample_time;

  auto previous = walking;
  float advanced = 0.0F;
  for (int frame = 0; frame < 6; ++frame) {
    inputs.sample_time += 1.0F / 60.0F;
    auto const sample = Animation::resolve_humanoid_locomotion_sample(inputs);
    float delta = sample.cycle_phase - previous.cycle_phase;
    if (delta < -0.5F) {
      delta += 1.0F;
    }
    EXPECT_GT(delta, 0.0F);
    advanced += delta;
    EXPECT_NEAR(sample.cycle_time, walking.cycle_time, 1.0e-4F);
    inputs.previous = sample.persistent;
    previous = sample;
  }

  EXPECT_NEAR(advanced, (6.0F / 60.0F) / walking.cycle_time, 0.02F);
  EXPECT_LT(previous.locomotion_blend, walking.locomotion_blend);
}

namespace {

struct SwingSample {
  QVector3D hand;
  QVector3D blade_axis;
};

auto bake_sword_swing(std::string_view clip_name) -> std::vector<SwingSample> {
  auto const& manifest =
      Render::Humanoid::humanoid_bake_recipe(Render::Humanoid::BakeProfile::SwordReady);
  std::size_t clip_index = manifest.clips.size();
  for (std::size_t i = 0; i < manifest.clips.size(); ++i) {
    if (manifest.clips[i].name == clip_name) {
      clip_index = i;
      break;
    }
  }
  if (clip_index >= manifest.clips.size() || manifest.bake_clip_frame == nullptr) {
    return {};
  }

  constexpr auto k_hand_r =
      static_cast<std::size_t>(Render::Humanoid::HumanoidBone::HandR);
  std::vector<SwingSample> samples;
  samples.reserve(manifest.clips[clip_index].frame_count);
  for (std::uint32_t frame = 0; frame < manifest.clips[clip_index].frame_count;
       ++frame) {
    std::vector<QMatrix4x4> palettes;
    manifest.bake_clip_frame(clip_index, frame, palettes, nullptr);
    if (palettes.size() <= k_hand_r) {
      return {};
    }
    samples.push_back({palettes[k_hand_r].column(3).toVector3D(),
                       palettes[k_hand_r].column(1).toVector3D().normalized()});
  }
  return samples;
}

auto angle_between(const QVector3D& a, const QVector3D& b) -> float {
  return std::acos(std::clamp(QVector3D::dotProduct(a, b), -1.0F, 1.0F));
}

} // namespace

TEST(HumanoidSwordSwing, RpgCutsSweepWithoutStallingOnAKey) {
  for (auto const* clip : {"rpg_sword_slash_left",
                           "rpg_sword_slash_right",
                           "rpg_sword_overhead",
                           "rpg_sword_finisher"}) {
    auto const samples = bake_sword_swing(clip);
    ASSERT_GE(samples.size(), 8U) << clip;

    std::vector<float> steps;
    steps.reserve(samples.size());
    for (std::size_t i = 1; i + 1 < samples.size(); ++i) {
      steps.push_back(angle_between(samples[i - 1U].blade_axis, samples[i].blade_axis));
    }
    auto const total = std::accumulate(steps.begin(), steps.end(), 0.0F);
    auto const mean = total / static_cast<float>(steps.size());
    auto const slowest = *std::min_element(steps.begin(), steps.end());
    auto const fastest = *std::max_element(steps.begin(), steps.end());

    EXPECT_GT(slowest, mean * 0.05F) << clip << ": blade stalls mid-swing";
    EXPECT_LT(fastest, mean * 6.0F) << clip << ": blade whips through one frame";
  }
}

TEST(HumanoidSwordSwing, RpgCutsReturnToTheGuardTheyStartedFrom) {
  for (auto const* clip : {"rpg_sword_slash_left",
                           "rpg_sword_slash_right",
                           "rpg_sword_overhead",
                           "rpg_sword_thrust",
                           "rpg_sword_finisher"}) {
    auto const samples = bake_sword_swing(clip);
    ASSERT_GE(samples.size(), 8U) << clip;

    EXPECT_LT((samples.front().hand - samples.back().hand).length(), 0.08F) << clip;
    EXPECT_LT(angle_between(samples.front().blade_axis, samples.back().blade_axis),
              0.20F)
        << clip;
  }
}

TEST(AnimationCoreLocomotionManifest, StridePresenceIsFullEvenForASlowWalk) {
  auto slow = walking_inputs();
  slow.speed = 0.9F;
  auto const sample = step_locomotion(slow, 0.0F, 1.0F, 0.0F, 1.0F, 120, 1.0F / 60.0F);

  EXPECT_LT(sample.locomotion_blend, 0.75F);
  EXPECT_GT(sample.locomotion_presence, 0.99F);
  EXPECT_LT(sample.run_presence, 0.01F);

  auto fast = walking_inputs();
  fast.speed = 2.4F;
  auto const brisk = step_locomotion(fast, 0.0F, 1.0F, 0.0F, 1.0F, 120, 1.0F / 60.0F);
  EXPECT_NEAR(brisk.locomotion_presence, sample.locomotion_presence, 0.01F);

  auto running = walking_inputs();
  running.movement_state = Animation::MovementState::Run;
  running.motion_state = Animation::HumanoidMotionState::Run;
  running.speed = 4.6F;
  auto const sprint =
      step_locomotion(running, 0.0F, 1.0F, 0.0F, 1.0F, 120, 1.0F / 60.0F);
  EXPECT_GT(sprint.run_presence, 0.99F);
}

TEST(AnimationCoreSelectionManifest, LocomotionCrossfadePicksTheTwoLeadingClips) {
  auto const standing = Animation::resolve_locomotion_crossfade({
      .resolved = Animation::StateId::Idle,
      .locomotion_presence = 0.0F,
      .run_presence = 0.0F,
  });
  EXPECT_FALSE(standing.active);
  EXPECT_EQ(standing.primary, Animation::StateId::Idle);

  auto const walking = Animation::resolve_locomotion_crossfade({
      .resolved = Animation::StateId::Walk,
      .locomotion_presence = 1.0F,
      .run_presence = 0.0F,
  });
  EXPECT_FALSE(walking.active);
  EXPECT_EQ(walking.primary, Animation::StateId::Walk);

  auto const starting = Animation::resolve_locomotion_crossfade({
      .resolved = Animation::StateId::Walk,
      .locomotion_presence = 0.35F,
      .run_presence = 0.0F,
  });
  EXPECT_TRUE(starting.active);
  EXPECT_EQ(starting.primary, Animation::StateId::Walk);
  EXPECT_EQ(starting.secondary, Animation::StateId::Idle);
  EXPECT_NEAR(starting.secondary_weight, 0.65F, 1.0e-4F);

  auto const stopping = Animation::resolve_locomotion_crossfade({
      .resolved = Animation::StateId::Idle,
      .locomotion_presence = 0.80F,
      .run_presence = 0.0F,
  });
  EXPECT_TRUE(stopping.active);
  EXPECT_EQ(stopping.primary, Animation::StateId::Idle);
  EXPECT_EQ(stopping.secondary, Animation::StateId::Walk);
  EXPECT_NEAR(stopping.secondary_weight, 0.80F, 1.0e-4F);

  auto const breaking_into_a_run = Animation::resolve_locomotion_crossfade({
      .resolved = Animation::StateId::Run,
      .locomotion_presence = 1.0F,
      .run_presence = 0.6F,
  });
  EXPECT_TRUE(breaking_into_a_run.active);
  EXPECT_EQ(breaking_into_a_run.primary, Animation::StateId::Run);
  EXPECT_EQ(breaking_into_a_run.secondary, Animation::StateId::Walk);
  EXPECT_NEAR(breaking_into_a_run.secondary_weight, 0.40F, 1.0e-4F);
}

TEST(AnimationCoreLocomotionManifest, MotionStateOwnsWalkRunSelection) {
  Animation::HumanoidLocomotionInputs inputs{};
  inputs.movement_state = Animation::MovementState::Walk;
  inputs.motion_state = Animation::HumanoidMotionState::Walk;
  inputs.speed = 3.30F;
  inputs.sample_time = 1.0F;

  auto const walking = Animation::resolve_humanoid_locomotion_sample(inputs);
  EXPECT_EQ(walking.state, Animation::HumanoidMotionState::Walk);

  inputs.movement_state = Animation::MovementState::Run;
  inputs.motion_state = Animation::HumanoidMotionState::Run;
  inputs.speed = 1.20F;
  auto const running = Animation::resolve_humanoid_locomotion_sample(inputs);
  EXPECT_EQ(running.state, Animation::HumanoidMotionState::Run);
}

TEST(AnimationCoreLocomotionManifest, WalkCadenceTightensAsSpeedRises) {
  Animation::HumanoidLocomotionInputs inputs{};
  inputs.movement_state = Animation::MovementState::Walk;
  inputs.motion_state = Animation::HumanoidMotionState::Walk;
  inputs.sample_time = 1.0F;

  inputs.speed = 1.25F;
  auto const slow_walk = Animation::resolve_humanoid_locomotion_sample(inputs);

  inputs.speed = 2.35F;
  auto const brisk_walk = Animation::resolve_humanoid_locomotion_sample(inputs);

  EXPECT_GT(slow_walk.cycle_time, brisk_walk.cycle_time);
  EXPECT_LT(slow_walk.stride_distance, brisk_walk.stride_distance);
}

TEST(AnimationCoreLocomotionManifest, IdlePhaseAdvancesOverTime) {
  Animation::HumanoidLocomotionInputs inputs{};
  inputs.movement_state = Animation::MovementState::Idle;
  inputs.motion_state = Animation::HumanoidMotionState::Idle;
  inputs.sample_time = 0.10F;
  inputs.phase_offset = 0.125F;

  auto const first = Animation::resolve_humanoid_locomotion_sample(inputs);

  inputs.sample_time = 0.90F;
  auto const second = Animation::resolve_humanoid_locomotion_sample(inputs);

  EXPECT_FLOAT_EQ(first.cycle_time, 1.6F);
  EXPECT_LT(first.cycle_phase, second.cycle_phase);
  EXPECT_NEAR(first.cycle_phase, 0.140625F, 1.0e-6F);
  EXPECT_NEAR(second.cycle_phase, 0.640625F, 1.0e-6F);
  EXPECT_FLOAT_EQ(first.stride_distance, 0.0F);
  EXPECT_FLOAT_EQ(second.stride_distance, 0.0F);
}

TEST(AnimationCoreLocomotionManifest,
     PersistentStateKeepsPhaseAdvancingDuringCadenceChanges) {
  auto wrapped_forward_delta = [](float start, float end) {
    float delta = end - start;
    if (delta < 0.0F) {
      delta += 1.0F;
    }
    return delta;
  };

  Animation::HumanoidLocomotionInputs inputs{};
  inputs.movement_state = Animation::MovementState::Walk;
  inputs.motion_state = Animation::HumanoidMotionState::Walk;
  inputs.phase_offset = 0.125F;
  inputs.has_persistent_state = true;
  inputs.allow_persistent_update = true;
  inputs.sample_time = 1.00F;
  inputs.speed = 1.25F;

  auto const first = Animation::resolve_humanoid_locomotion_sample(inputs);
  ASSERT_TRUE(first.write_persistent_state);

  inputs.previous = first.persistent;
  inputs.sample_time += 1.0F / 60.0F;
  inputs.speed = 2.35F;
  auto const second = Animation::resolve_humanoid_locomotion_sample(inputs);

  EXPECT_GT(wrapped_forward_delta(first.cycle_phase, second.cycle_phase), 0.01F);
  EXPECT_LT(second.cycle_time, first.cycle_time);
}

TEST(AnimationCoreLocomotionManifest, PersistentStatePreservesPhaseAcrossWalkRun) {
  auto wrapped_phase_delta = [](float a, float b) {
    float const direct = std::abs(a - b);
    return std::min(direct, 1.0F - direct);
  };

  Animation::HumanoidLocomotionInputs inputs{};
  inputs.movement_state = Animation::MovementState::Walk;
  inputs.motion_state = Animation::HumanoidMotionState::Walk;
  inputs.phase_offset = 0.125F;
  inputs.has_persistent_state = true;
  inputs.allow_persistent_update = true;
  inputs.sample_time = 1.50F;
  inputs.speed = 2.30F;

  auto const walking = Animation::resolve_humanoid_locomotion_sample(inputs);

  inputs.previous = walking.persistent;
  inputs.movement_state = Animation::MovementState::Run;
  inputs.motion_state = Animation::HumanoidMotionState::Run;
  inputs.sample_time += 1.0F / 60.0F;
  inputs.speed = 5.10F;
  auto const running = Animation::resolve_humanoid_locomotion_sample(inputs);

  EXPECT_EQ(walking.state, Animation::HumanoidMotionState::Walk);
  EXPECT_EQ(running.state, Animation::HumanoidMotionState::Run);
  EXPECT_LT(wrapped_phase_delta(walking.cycle_phase, running.cycle_phase), 0.08F);
  EXPECT_GT(running.run_blend, 0.0F);
}

TEST(AnimationCoreLocomotionManifest, DisabledPersistentWritesReturnPreviewOnly) {
  Animation::HumanoidLocomotionInputs inputs{};
  inputs.movement_state = Animation::MovementState::Walk;
  inputs.motion_state = Animation::HumanoidMotionState::Walk;
  inputs.phase_offset = 0.125F;
  inputs.has_persistent_state = true;
  inputs.allow_persistent_update = true;
  inputs.sample_time = 0.75F;
  inputs.speed = 2.30F;
  auto const seeded = Animation::resolve_humanoid_locomotion_sample(inputs);
  ASSERT_TRUE(seeded.write_persistent_state);

  inputs.previous = seeded.persistent;
  inputs.allow_persistent_update = false;
  inputs.movement_state = Animation::MovementState::Run;
  inputs.motion_state = Animation::HumanoidMotionState::Run;
  inputs.sample_time += 1.0F / 60.0F;
  inputs.speed = 5.10F;
  auto const preview = Animation::resolve_humanoid_locomotion_sample(inputs);

  EXPECT_FALSE(preview.write_persistent_state);
  EXPECT_FLOAT_EQ(preview.persistent.phase, seeded.persistent.phase);
  EXPECT_FLOAT_EQ(preview.persistent.filtered_speed, seeded.persistent.filtered_speed);
  EXPECT_FLOAT_EQ(preview.persistent.run_blend, seeded.persistent.run_blend);
  EXPECT_EQ(preview.persistent.state, seeded.persistent.state);
  EXPECT_GT(preview.run_blend, seeded.persistent.run_blend);
}

TEST(AnimationCoreIntentManifest, MeleeLockPrioritizesAttackOverLocomotion) {
  auto const intent = Animation::resolve_humanoid_intent({
      .locomotion = Animation::MovementState::Walk,
      .is_attacking = true,
      .is_melee = true,
      .is_in_melee_lock = true,
      .attack_family = Animation::CombatAttackFamily::Sword,
  });

  EXPECT_EQ(intent.semantic.action, Animation::ActionIntent::AttackMelee);
  EXPECT_TRUE(intent.semantic.prioritize_action_over_locomotion);
  EXPECT_EQ(Animation::pose_intent_for_semantic_intent(intent.semantic),
            Animation::PoseIntent::AttackMelee);
}

TEST(AnimationCoreIntentManifest, ChaseMovementKeepsLocomotionUnlessLocked) {
  auto const intent = Animation::resolve_humanoid_intent({
      .locomotion = Animation::MovementState::Walk,
      .has_chase_intent = true,
      .is_attacking = true,
      .is_melee = true,
      .combat_phase = Animation::CombatPhase::Strike,
      .attack_family = Animation::CombatAttackFamily::Sword,
  });

  EXPECT_EQ(intent.semantic.action, Animation::ActionIntent::AttackMelee);
  EXPECT_FALSE(intent.semantic.prioritize_action_over_locomotion);
  EXPECT_EQ(Animation::pose_intent_for_semantic_intent(intent.semantic),
            Animation::PoseIntent::Walk);
}

TEST(AnimationCoreIntentManifest, MountedSwordAndSpearResolveDifferentActions) {
  auto const sword = Animation::resolve_humanoid_intent({
      .is_mounted = true,
      .is_attacking = true,
      .is_melee = true,
      .attack_family = Animation::CombatAttackFamily::Sword,
  });
  EXPECT_EQ(sword.semantic.action, Animation::ActionIntent::AttackMelee);
  EXPECT_TRUE(sword.semantic.prioritize_action_over_locomotion);

  auto const spear = Animation::resolve_humanoid_intent({
      .is_mounted = true,
      .is_attacking = true,
      .is_melee = true,
      .attack_family = Animation::CombatAttackFamily::Spear,
  });
  EXPECT_EQ(spear.semantic.action, Animation::ActionIntent::MountedCharge);
  EXPECT_TRUE(spear.semantic.prioritize_action_over_locomotion);
}

TEST(AnimationCoreIntentManifest, HoldAttackUsesHeldMeleeOrRangedPose) {
  auto const melee = Animation::resolve_humanoid_intent({
      .is_in_hold_mode = true,
      .is_attacking = true,
      .is_melee = true,
      .attack_family = Animation::CombatAttackFamily::None,
  });
  EXPECT_EQ(melee.semantic.action, Animation::ActionIntent::AttackMelee);
  EXPECT_EQ(melee.semantic.stance, Animation::StanceIntent::Hold);

  auto const ranged = Animation::resolve_humanoid_intent({
      .is_in_hold_mode = true,
      .is_attacking = true,
      .is_melee = false,
      .attack_family = Animation::CombatAttackFamily::Bow,
  });
  EXPECT_EQ(ranged.semantic.action, Animation::ActionIntent::AttackRanged);
  EXPECT_EQ(ranged.semantic.stance, Animation::StanceIntent::Hold);
}

TEST(AnimationCoreIntentManifest, LifecycleActionsOverrideCombatIntent) {
  auto const dying = Animation::resolve_humanoid_intent({
      .is_dying = true,
      .is_attacking = true,
      .is_melee = true,
      .attack_family = Animation::CombatAttackFamily::Sword,
  });
  EXPECT_EQ(dying.semantic.action, Animation::ActionIntent::Dying);

  auto const hit = Animation::resolve_humanoid_intent({
      .is_hit_reacting = true,
      .is_attacking = true,
      .is_melee = true,
      .attack_family = Animation::CombatAttackFamily::Sword,
  });
  EXPECT_EQ(hit.semantic.action, Animation::ActionIntent::HitReaction);
}

TEST(AnimationCoreIndividuality, GaitOffsetIsDeterministicAndBounded) {
  Animation::SoldierIndividualityInputs const inputs{
      .soldier_seed = 0x12345678U, .row = 1, .col = 2, .rows = 3, .cols = 4};
  auto const first = Animation::resolve_soldier_individuality(inputs);
  auto const second = Animation::resolve_soldier_individuality(inputs);

  EXPECT_FLOAT_EQ(first.gait_phase_offset, second.gait_phase_offset);
  EXPECT_GE(first.gait_phase_offset, 0.0F);
  EXPECT_LE(first.gait_phase_offset, 0.25F);
}

TEST(AnimationCoreIndividuality, ALoneFigureIsNotDesynchronizedFromAnything) {
  auto const alone = Animation::resolve_soldier_individuality({
      .soldier_seed = 0x12345678U,
      .single_soldier = true,
  });

  EXPECT_FLOAT_EQ(alone.gait_phase_offset, 0.0F);
  EXPECT_FLOAT_EQ(alone.cadence_scale, 1.0F);
  EXPECT_FLOAT_EQ(alone.swing_phase_offset, 0.0F);
  EXPECT_FLOAT_EQ(alone.swing_tempo_scale, 1.0F);
  EXPECT_FLOAT_EQ(alone.swing_plane_offset, 0.0F);
}

TEST(AnimationCoreIndividuality, NeighboursGetGenuinelyDifferentStrideAndSwing) {

  auto const left = Animation::resolve_soldier_individuality(
      {.soldier_seed = 0x11111111U, .row = 0, .col = 0, .rows = 2, .cols = 4});
  auto const right = Animation::resolve_soldier_individuality(
      {.soldier_seed = 0x22222222U, .row = 0, .col = 1, .rows = 2, .cols = 4});

  EXPECT_NE(left.gait_phase_offset, right.gait_phase_offset);
  EXPECT_NE(left.cadence_scale, right.cadence_scale);
  EXPECT_NE(left.swing_phase_offset, right.swing_phase_offset);
  EXPECT_NE(left.swing_plane_offset, right.swing_plane_offset);

  EXPECT_LT(std::abs(left.cadence_scale - right.cadence_scale),
            2.0F * Animation::k_individuality_cadence_spread);
  EXPECT_LT(std::abs(left.swing_plane_offset - right.swing_plane_offset),
            2.0F * Animation::k_individuality_swing_spread);
}

TEST(AnimationCoreLayoutManifest, SingleSoldierPolicyHasNoJitter) {
  auto const policy = Animation::resolve_soldier_layout_policy({
      .idx = 0,
      .row = 0,
      .col = 0,
      .rows = 1,
      .cols = 1,
      .formation_spacing = 1.0F,
      .seed = 0xCAFEBABEU,
      .force_single_soldier = true,
      .melee_attack = false,
      .sample_time = 0.0F,
  });

  EXPECT_EQ(policy.row_index, 0U);
  EXPECT_EQ(policy.col_index, 0U);
  EXPECT_EQ(policy.rank_band, 0U);
  EXPECT_EQ(policy.inst_seed, 0xCAFEBABEU);
  EXPECT_FLOAT_EQ(policy.offset_x_delta, 0.0F);
  EXPECT_FLOAT_EQ(policy.offset_z_delta, 0.0F);
  EXPECT_FLOAT_EQ(policy.vertical_jitter, 0.0F);
  EXPECT_FLOAT_EQ(policy.yaw_delta, 0.0F);
  EXPECT_FLOAT_EQ(policy.individuality.gait_phase_offset, 0.0F);
}

TEST(AnimationCoreLayoutManifest, MultiSoldierPolicyOwnsRankSeedAndJitter) {
  auto const policy = Animation::resolve_soldier_layout_policy({
      .idx = 3,
      .row = 1,
      .col = 1,
      .rows = 3,
      .cols = 2,
      .formation_spacing = 1.25F,
      .seed = 0x12345678U,
      .force_single_soldier = false,
      .melee_attack = false,
      .sample_time = 2.5F,
  });

  EXPECT_EQ(policy.row_index, 1U);
  EXPECT_EQ(policy.col_index, 1U);
  EXPECT_EQ(policy.rank_band, 1U);
  EXPECT_EQ(policy.inst_seed, 0x12345678U ^ std::uint32_t(3 * 9176U));
  EXPECT_NE(policy.vertical_jitter, 0.0F);
  EXPECT_NE(policy.yaw_delta, 0.0F);
  EXPECT_GE(policy.individuality.gait_phase_offset, 0.0F);
  EXPECT_LE(policy.individuality.gait_phase_offset, 0.25F);
}

TEST(AnimationCoreLayoutManifest, MeleeAttackKeepsFormationRootStable) {
  Animation::SoldierLayoutPolicyInputs inputs{};
  inputs.idx = 3;
  inputs.row = 1;
  inputs.col = 1;
  inputs.rows = 2;
  inputs.cols = 2;
  inputs.formation_spacing = 1.25F;
  inputs.seed = 0x12345678U;
  inputs.force_single_soldier = false;
  inputs.sample_time = 2.5F;

  auto const idle = Animation::resolve_soldier_layout_policy(inputs);
  inputs.melee_attack = true;
  auto const melee = Animation::resolve_soldier_layout_policy(inputs);

  EXPECT_FLOAT_EQ(idle.vertical_jitter, melee.vertical_jitter);
  EXPECT_FLOAT_EQ(idle.individuality.gait_phase_offset,
                  melee.individuality.gait_phase_offset);
  EXPECT_FLOAT_EQ(idle.offset_x_delta, melee.offset_x_delta);
  EXPECT_FLOAT_EQ(idle.offset_z_delta, melee.offset_z_delta);
  EXPECT_FLOAT_EQ(idle.yaw_delta, melee.yaw_delta);
}

TEST(AnimationCoreStateManifest, ActionFlagsGateHoldAndGuardTargets) {
  auto const melee_attack_targets = Animation::resolve_humanoid_stance_targets({
      .hold_requested = true,
      .hold_exit_requested = false,
      .raw_guard_requested = true,
      .hold_entry_progress = 0.75F,
      .previous_hold_pose_progress = 0.5F,
      .action =
          {
              .is_attacking = true,
              .is_melee = true,
          },
  });
  EXPECT_FALSE(melee_attack_targets.hold);
  EXPECT_TRUE(melee_attack_targets.exiting_hold);
  EXPECT_FLOAT_EQ(melee_attack_targets.hold_entry_progress, 0.0F);
  EXPECT_FLOAT_EQ(melee_attack_targets.hold_exit_progress, 0.0F);
  EXPECT_FALSE(melee_attack_targets.guard);

  auto const ranged_attack_targets = Animation::resolve_humanoid_stance_targets({
      .hold_requested = true,
      .hold_exit_requested = false,
      .raw_guard_requested = true,
      .hold_entry_progress = 1.25F,
      .action =
          {
              .is_attacking = true,
              .is_melee = false,
          },
  });
  EXPECT_TRUE(ranged_attack_targets.hold);
  EXPECT_FALSE(ranged_attack_targets.exiting_hold);
  EXPECT_FLOAT_EQ(ranged_attack_targets.hold_entry_progress, 1.0F);
  EXPECT_FALSE(ranged_attack_targets.guard);

  auto const spear_attack_targets = Animation::resolve_humanoid_stance_targets({
      .hold_requested = true,
      .hold_entry_progress = 1.0F,
      .action =
          {
              .is_attacking = true,
              .is_melee = true,
              .preserves_hold_pose = true,
          },
  });
  EXPECT_TRUE(spear_attack_targets.hold);
  EXPECT_FALSE(spear_attack_targets.exiting_hold);
  EXPECT_FLOAT_EQ(spear_attack_targets.hold_entry_progress, 1.0F);

  auto const held_hit_targets = Animation::resolve_humanoid_stance_targets({
      .hold_requested = true,
      .hold_entry_progress = 1.0F,
      .action =
          {
              .preserves_hold_pose = true,
              .is_hit_reacting = true,
          },
  });
  EXPECT_TRUE(held_hit_targets.hold);
  EXPECT_FALSE(held_hit_targets.exiting_hold);
}

TEST(AnimationCoreStateManifest, HoldExitRequestCarriesAuthoredExitProgress) {
  auto const targets = Animation::resolve_humanoid_stance_targets({
      .hold_requested = false,
      .hold_exit_requested = true,
      .hold_exit_progress = 1.2F,
  });

  EXPECT_FALSE(targets.hold);
  EXPECT_TRUE(targets.exiting_hold);
  EXPECT_FLOAT_EQ(targets.hold_exit_progress, 1.0F);
}

TEST(AnimationCoreStateManifest, FirstPersistentSampleStartsPoseBlendsAtRest) {
  auto const state = Animation::resolve_humanoid_persistent_stance_state({
      .sample_time = 3.0F,
      .previous_initialized = false,
      .is_active = false,
      .stance =
          {
              .hold = true,
              .hold_entry_progress = 1.0F,
              .guard = true,
          },
      .guard_enter_duration = 1.0F,
      .guard_exit_duration = 1.0F,
      .hold_enter_duration = 1.5F,
      .hold_exit_duration = 2.0F,
  });

  EXPECT_TRUE(state.initialized);
  EXPECT_FLOAT_EQ(state.last_sample_time, 3.0F);
  EXPECT_TRUE(state.is_guarding);
  EXPECT_TRUE(state.is_in_hold_mode);
  EXPECT_FLOAT_EQ(state.guard_pose_progress, 0.0F);
  EXPECT_FLOAT_EQ(state.hold_pose_progress, 0.0F);
  EXPECT_FLOAT_EQ(state.hold_entry_progress, 0.0F);
  EXPECT_FLOAT_EQ(state.idle_duration, 0.0F);
}

TEST(AnimationCoreStateManifest, PersistentGuardBlendsInAndOut) {
  auto const entering = Animation::resolve_humanoid_persistent_stance_state({
      .sample_time = 1.75F,
      .previous_initialized = true,
      .previous_sample_time = 1.0F,
      .previous_guard_pose_progress = 0.0F,
      .stance = {.guard = true},
      .guard_enter_duration = 1.5F,
      .guard_exit_duration = 2.0F,
  });
  EXPECT_TRUE(entering.is_guarding);
  EXPECT_FALSE(entering.is_exiting_guard);
  EXPECT_NEAR(entering.guard_pose_progress, 0.5F, 1.0e-6F);

  auto const exiting = Animation::resolve_humanoid_persistent_stance_state({
      .sample_time = 3.0F,
      .previous_initialized = true,
      .previous_sample_time = 2.5F,
      .previous_guard_pose_progress = 0.75F,
      .stance = {.guard = false},
      .guard_enter_duration = 1.5F,
      .guard_exit_duration = 2.0F,
  });
  EXPECT_FALSE(exiting.is_guarding);
  EXPECT_TRUE(exiting.is_exiting_guard);
  EXPECT_NEAR(exiting.guard_pose_progress, 0.5F, 1.0e-6F);
}

TEST(AnimationCoreStateManifest, PersistentHoldUsesGameplayAndVisualLimits) {
  auto const entering = Animation::resolve_humanoid_persistent_stance_state({
      .sample_time = 1.75F,
      .previous_initialized = true,
      .previous_sample_time = 1.0F,
      .previous_hold_pose_progress = 0.25F,
      .stance =
          {
              .hold = true,
              .hold_entry_progress = 0.9F,
          },
      .hold_enter_duration = 1.5F,
      .hold_exit_duration = 2.0F,
  });
  EXPECT_TRUE(entering.is_in_hold_mode);
  EXPECT_FALSE(entering.is_exiting_hold);
  EXPECT_NEAR(entering.hold_pose_progress, 0.75F, 1.0e-6F);
  EXPECT_NEAR(entering.hold_entry_progress, 0.75F, 1.0e-6F);

  auto const exiting = Animation::resolve_humanoid_persistent_stance_state({
      .sample_time = 3.0F,
      .previous_initialized = true,
      .previous_sample_time = 2.5F,
      .previous_hold_pose_progress = 0.75F,
      .stance = {.exiting_hold = true},
      .hold_enter_duration = 1.5F,
      .hold_exit_duration = 2.0F,
  });
  EXPECT_FALSE(exiting.is_in_hold_mode);
  EXPECT_TRUE(exiting.is_exiting_hold);
  EXPECT_NEAR(exiting.hold_pose_progress, 0.5F, 1.0e-6F);
  EXPECT_FLOAT_EQ(exiting.hold_entry_progress, 0.0F);
  EXPECT_NEAR(exiting.hold_exit_progress, 0.5F, 1.0e-6F);
}

TEST(AnimationCoreStateManifest, IdleDurationTracksInactiveTimeOnly) {
  auto const inactive = Animation::resolve_humanoid_persistent_stance_state({
      .sample_time = 4.5F,
      .previous_initialized = true,
      .previous_sample_time = 3.0F,
      .previous_idle_duration = 2.0F,
      .is_active = false,
  });
  EXPECT_FLOAT_EQ(inactive.idle_duration, 3.5F);

  auto const active = Animation::resolve_humanoid_persistent_stance_state({
      .sample_time = 4.5F,
      .previous_initialized = true,
      .previous_sample_time = 3.0F,
      .previous_idle_duration = 2.0F,
      .is_active = true,
  });
  EXPECT_FLOAT_EQ(active.idle_duration, 0.0F);
}

TEST(AnimationCoreStateManifest, TimeRegressionResetsPersistentPoseState) {
  auto const state = Animation::resolve_humanoid_persistent_stance_state({
      .sample_time = 1.0F,
      .previous_initialized = true,
      .previous_sample_time = 2.0F,
      .previous_idle_duration = 5.0F,
      .previous_guard_pose_progress = 1.0F,
      .previous_hold_pose_progress = 1.0F,
      .stance =
          {
              .hold = true,
              .hold_entry_progress = 1.0F,
              .guard = true,
          },
      .guard_enter_duration = 1.0F,
      .guard_exit_duration = 1.0F,
      .hold_enter_duration = 1.0F,
      .hold_exit_duration = 1.0F,
  });

  EXPECT_TRUE(state.initialized);
  EXPECT_FLOAT_EQ(state.last_sample_time, 1.0F);
  EXPECT_FLOAT_EQ(state.idle_duration, 0.0F);
  EXPECT_FLOAT_EQ(state.guard_pose_progress, 0.0F);
  EXPECT_FLOAT_EQ(state.hold_pose_progress, 0.0F);
}

TEST(AnimationCoreSelectionManifest, VariantTableOverrideUsesAnimationCorePolicy) {
  Animation::ArchetypeVariantTable table{};
  auto const idle_idx = static_cast<std::size_t>(Animation::PoseIntent::Idle);
  table.archetype_for_pose[idle_idx] = 7U;
  table.state_for_pose[idle_idx] = Animation::StateId::AttackSpear;

  auto const pose_override = Animation::resolve_archetype_variant_override({
      .table = &table,
      .pose_intent = Animation::PoseIntent::Idle,
      .seed = 12U,
  });
  EXPECT_TRUE(pose_override.changed());
  EXPECT_TRUE(pose_override.archetype_changed);
  EXPECT_TRUE(pose_override.state_changed);
  EXPECT_EQ(pose_override.archetype, 7U);
  EXPECT_EQ(pose_override.state, Animation::StateId::AttackSpear);

  table.variant_trigger_pose = Animation::PoseIntent::Count;
  table.variant_stride = 4U;
  table.variant_is_seed_based = true;
  table.archetype_for_variant[2] = 19U;
  table.state_for_variant[2] = Animation::StateId::Hold;

  std::uint32_t seed_for_variant_2 = 0U;
  for (; seed_for_variant_2 < 512U; ++seed_for_variant_2) {
    if (Animation::seeded_visual_variant_index(seed_for_variant_2, 4U) == 2U) {
      break;
    }
  }
  ASSERT_LT(seed_for_variant_2, 512U);

  auto const seeded_override = Animation::resolve_archetype_variant_override({
      .table = &table,
      .pose_intent = Animation::PoseIntent::Walk,
      .seed = seed_for_variant_2,
  });
  EXPECT_TRUE(seeded_override.changed());
  EXPECT_EQ(seeded_override.archetype, 19U);
  EXPECT_EQ(seeded_override.state, Animation::StateId::Hold);

  table.variant_is_seed_based = false;
  table.state_for_variant[3] = Animation::StateId::Cast;
  auto const hinted_override = Animation::resolve_archetype_variant_override({
      .table = &table,
      .pose_intent = Animation::PoseIntent::Walk,
      .variant_index_hint = 99U,
      .has_variant_index_hint = true,
  });
  EXPECT_TRUE(hinted_override.state_changed);
  EXPECT_EQ(hinted_override.state, Animation::StateId::Cast);
}

TEST(AnimationCoreSelectionManifest, CombatLayerPolicyOwnsBlendAndOverlayRules) {
  auto const moving_overlay = Animation::resolve_combat_playback_layer_policy({
      .has_authoritative_combat = true,
      .phase = Animation::CombatTransactionPhase::Strike,
      .phase_progress = 0.5F,
      .attack_emphasis = 1.0F,
      .moving = true,
      .action_state_differs_from_base = true,
  });
  EXPECT_TRUE(moving_overlay.use_base_selection);
  EXPECT_EQ(moving_overlay.upper_body_source, Animation::PlaybackLayerSource::Action);
  EXPECT_EQ(moving_overlay.full_body_source, Animation::PlaybackLayerSource::None);
  EXPECT_GT(moving_overlay.upper_body_weight, 0.7F);

  auto const rooted_overlay = Animation::resolve_combat_playback_layer_policy({
      .has_authoritative_combat = true,
      .phase = Animation::CombatTransactionPhase::Strike,
      .phase_progress = 0.5F,
      .attack_emphasis = 1.0F,
      .rooted_action = true,
      .action_state_differs_from_base = true,
  });
  EXPECT_TRUE(rooted_overlay.use_base_selection);
  EXPECT_EQ(rooted_overlay.upper_body_source, Animation::PlaybackLayerSource::Action);
  EXPECT_EQ(rooted_overlay.full_body_source, Animation::PlaybackLayerSource::None);

  auto const held_overlay = Animation::resolve_combat_playback_layer_policy({
      .has_authoritative_combat = true,
      .phase = Animation::CombatTransactionPhase::Strike,
      .phase_progress = 0.5F,
      .attack_emphasis = 1.0F,
      .preserve_base_stance = true,
      .action_state_differs_from_base = true,
  });
  EXPECT_TRUE(held_overlay.use_base_selection);
  EXPECT_EQ(held_overlay.upper_body_source, Animation::PlaybackLayerSource::None);
  EXPECT_EQ(held_overlay.full_body_source, Animation::PlaybackLayerSource::None);

  auto const enter_blend = Animation::resolve_combat_playback_layer_policy({
      .has_authoritative_combat = true,
      .phase = Animation::CombatTransactionPhase::Anticipation,
      .phase_progress = 0.25F,
      .attack_emphasis = 1.0F,
      .selection_state_differs_from_base = true,
  });
  EXPECT_FALSE(enter_blend.use_base_selection);
  EXPECT_EQ(enter_blend.full_body_source, Animation::PlaybackLayerSource::Base);
  EXPECT_NEAR(enter_blend.full_body_weight, 1.0F - enter_blend.combat_weight, 1.0e-6F);

  auto const settled_exit = Animation::resolve_combat_playback_layer_policy({
      .has_authoritative_combat = true,
      .phase = Animation::CombatTransactionPhase::ExitBlend,
      .exit_blend_progress = 1.0F,
      .attack_emphasis = 1.0F,
  });
  EXPECT_TRUE(settled_exit.use_base_selection);
  EXPECT_EQ(settled_exit.full_body_source, Animation::PlaybackLayerSource::None);
  EXPECT_EQ(settled_exit.upper_body_source, Animation::PlaybackLayerSource::None);
}

TEST(HumanoidPrepare, HealingSelectionStillUsesIdleClipFamily) {
  using Render::Creature::AnimationStateId;
  using Render::Creature::ArchetypeRegistry;
  using Render::Creature::Pipeline::resolve_humanoid_animation_selection;
  using Render::Creature::Pipeline::UnitVisualSpec;

  UnitVisualSpec spec{};
  spec.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
  spec.debug_name = "tests/healing_selection";
  spec.archetype_id = ArchetypeRegistry::k_humanoid_base;

  Render::GL::HumanoidAnimationContext anim{};
  anim.inputs.is_healing = true;
  anim.inputs.healing_target_dx = 1.0F;
  anim.inputs.healing_target_dz = -0.25F;
  anim.gait.cycle_phase = 0.37F;
  anim.idle_breath_phase = 0.37F;

  auto const selection = resolve_humanoid_animation_selection(spec, anim, 11U);

  EXPECT_EQ(selection.pose.intent, Render::Creature::PoseIntent::Healing);
  EXPECT_EQ(selection.state, AnimationStateId::Idle);
  EXPECT_FLOAT_EQ(selection.phase, 0.37F);
  EXPECT_EQ(selection.clip_variant, 0U);
  ASSERT_TRUE(selection.clip_id.has_value());
  EXPECT_EQ(*selection.clip_id,
            ArchetypeRegistry::instance().bpat_clip(ArchetypeRegistry::k_humanoid_base,
                                                    AnimationStateId::Idle));
}

TEST(HumanoidPrepare, WalkingCarrierKeepsWalkLegsAndOverlaysCarryArms) {
  using Render::Creature::AnimationStateId;
  using Render::Creature::ArchetypeRegistry;
  using Render::Creature::Pipeline::resolve_humanoid_animation_selection;
  using Render::Creature::Pipeline::UnitVisualSpec;

  UnitVisualSpec spec{};
  spec.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
  spec.debug_name = "tests/resource_carry_selection";
  spec.archetype_id = ArchetypeRegistry::k_humanoid_base;

  Render::GL::HumanoidAnimationContext anim{};
  anim.inputs.time = 0.9F;
  anim.inputs.movement_state = Render::Creature::MovementAnimationState::Walk;
  anim.inputs.is_carrying_load = true;
  anim.gait.state = Render::GL::HumanoidMotionState::Walk;
  anim.gait.cycle_phase = 0.4F;
  anim.gait.locomotion_presence = 1.0F;

  auto const selection = resolve_humanoid_animation_selection(spec, anim, 13U);

  EXPECT_EQ(selection.state, AnimationStateId::Walk);
  ASSERT_TRUE(selection.upper_body_overlay.active());
  ASSERT_TRUE(selection.upper_body_overlay.clip_id.has_value());
  EXPECT_EQ(*selection.upper_body_overlay.clip_id,
            Animation::k_humanoid_resource_carry_clip);
  EXPECT_EQ(selection.upper_body_overlay.mode,
            Render::Creature::PlaybackLayerMode::UpperBodyOverlay);
}

TEST(HumanoidPrepare, HitReactionSelectionPlaysTheReactionClip) {
  using Render::Creature::AnimationStateId;
  using Render::Creature::ArchetypeRegistry;
  using Render::Creature::Pipeline::resolve_humanoid_animation_selection;
  using Render::Creature::Pipeline::UnitVisualSpec;

  UnitVisualSpec spec{};
  spec.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
  spec.debug_name = "tests/hit_reaction_selection";
  spec.archetype_id = ArchetypeRegistry::k_humanoid_base;

  Render::GL::HumanoidAnimationContext anim{};
  anim.inputs.is_hit_reacting = true;
  anim.inputs.hit_reaction_intensity = 0.8F;
  anim.gait.cycle_phase = 0.61F;
  anim.idle_breath_phase = 0.61F;

  anim.inputs.hit_reaction_progress = 0.35F;
  anim.inputs.hit_reaction_kind = Engine::Core::HitReactionKind::Flinch;

  auto const selection = resolve_humanoid_animation_selection(spec, anim, 19U);

  EXPECT_EQ(selection.pose.intent, Render::Creature::PoseIntent::HitReaction);
  EXPECT_EQ(selection.state, AnimationStateId::Idle);
  EXPECT_FLOAT_EQ(selection.phase, 0.35F)
      << "a reaction clip plays on the reaction's own progress";
  EXPECT_EQ(selection.clip_variant, 0U);
  ASSERT_TRUE(selection.clip_id.has_value());
  EXPECT_EQ(*selection.clip_id, Animation::k_humanoid_react_flinch_clip);

  anim.inputs.hit_reaction_kind = Engine::Core::HitReactionKind::Block;
  auto const block = resolve_humanoid_animation_selection(spec, anim, 19U);
  ASSERT_TRUE(block.clip_id.has_value());
  EXPECT_EQ(*block.clip_id, Animation::k_humanoid_react_block_clip);

  anim.inputs.hit_reaction_kind = Engine::Core::HitReactionKind::Stagger;
  auto const stagger = resolve_humanoid_animation_selection(spec, anim, 19U);
  ASSERT_TRUE(stagger.clip_id.has_value());
  EXPECT_EQ(*stagger.clip_id, Animation::k_humanoid_react_stagger_clip);

  anim.inputs.is_mounted = true;
  auto const mounted = resolve_humanoid_animation_selection(spec, anim, 19U);
  ASSERT_TRUE(mounted.clip_id.has_value());
  EXPECT_NE(*mounted.clip_id, Animation::k_humanoid_react_stagger_clip)
      << "riders keep the saddle pose through a hit";
}

TEST(HumanoidPrepare, VariantTableOverrideRecomputesPhaseAndClipVariant) {
  using Render::Creature::AnimationStateId;
  using Render::Creature::ArchetypeRegistry;
  using Render::Creature::ArchetypeVariantTable;
  using Render::Creature::Pipeline::resolve_humanoid_animation_selection;
  using Render::Creature::Pipeline::UnitVisualSpec;

  static const ArchetypeVariantTable k_variant_table = [] {
    ArchetypeVariantTable table{};
    table.state_for_pose[static_cast<std::size_t>(Render::Creature::PoseIntent::Idle)] =
        AnimationStateId::Hold;
    return table;
  }();

  UnitVisualSpec baseline_spec{};
  baseline_spec.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
  baseline_spec.debug_name = "tests/selection_baseline";
  baseline_spec.archetype_id = ArchetypeRegistry::k_humanoid_base;

  UnitVisualSpec override_spec = baseline_spec;
  override_spec.debug_name = "tests/selection_override";
  override_spec.animation_manifest.variant_table = &k_variant_table;

  Render::GL::HumanoidAnimationContext anim{};
  anim.ambient_idle_type = Render::GL::AmbientIdleType::Jump;
  anim.ambient_idle_phase = 0.5F;

  anim.ambient_idle_blend = 1.0F;

  auto const baseline = resolve_humanoid_animation_selection(baseline_spec, anim, 5U);
  auto const override = resolve_humanoid_animation_selection(override_spec, anim, 5U);

  EXPECT_EQ(baseline.pose.intent, Render::Creature::PoseIntent::Idle);
  EXPECT_EQ(baseline.state, AnimationStateId::Idle);
  EXPECT_FLOAT_EQ(baseline.phase, 0.5F);
  EXPECT_EQ(baseline.clip_variant,
            Render::GL::ambient_idle_clip_variant(Render::GL::AmbientIdleType::Jump));

  EXPECT_TRUE(override.variant_table_changed);
  EXPECT_EQ(override.pose.intent, Render::Creature::PoseIntent::Idle);
  EXPECT_EQ(override.state, AnimationStateId::Hold);
  EXPECT_FLOAT_EQ(override.phase, 0.0F);
  EXPECT_EQ(override.clip_variant, 0U);
  ASSERT_TRUE(override.clip_id.has_value());
  EXPECT_EQ(*override.clip_id,
            ArchetypeRegistry::instance().bpat_clip(ArchetypeRegistry::k_humanoid_base,
                                                    AnimationStateId::Hold));
}

TEST(HumanoidPrepare, VariantTableCanUseFacialHairStyleIndexForOverrides) {
  using Render::Creature::AnimationStateId;
  using Render::Creature::ArchetypeRegistry;
  using Render::Creature::ArchetypeVariantTable;
  using Render::Creature::Pipeline::resolve_humanoid_animation_selection;
  using Render::Creature::Pipeline::UnitVisualSpec;

  static const ArchetypeVariantTable k_variant_table = [] {
    ArchetypeVariantTable table{};
    table.variant_trigger_pose = Render::Creature::PoseIntent::Idle;
    table.variant_stride = 4;
    table.variant_is_seed_based = false;
    table.state_for_variant[3] = AnimationStateId::Hold;
    return table;
  }();

  UnitVisualSpec spec{};
  spec.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
  spec.debug_name = "tests/facial_hair_variant_override";
  spec.archetype_id = ArchetypeRegistry::k_humanoid_base;
  spec.animation_manifest.variant_table = &k_variant_table;

  Render::GL::HumanoidAnimationContext const anim{};
  Render::GL::HumanoidVariant variant{};
  variant.facial_hair.style = Render::GL::FacialHairStyle::FullBeard;

  auto const selection = resolve_humanoid_animation_selection(spec, anim, 7U, &variant);

  EXPECT_TRUE(selection.variant_table_changed);
  EXPECT_EQ(selection.pose.intent, Render::Creature::PoseIntent::Idle);
  EXPECT_EQ(selection.state, AnimationStateId::Hold);
  EXPECT_EQ(selection.clip_variant, 0U);
  ASSERT_TRUE(selection.clip_id.has_value());
  EXPECT_EQ(*selection.clip_id,
            ArchetypeRegistry::instance().bpat_clip(ArchetypeRegistry::k_humanoid_base,
                                                    AnimationStateId::Hold));
}

TEST(HumanoidPrepare, MovingCombatSelectionProducesUpperBodyOverlay) {
  using Render::Creature::AnimationStateId;
  using Render::Creature::ArchetypeRegistry;
  using Render::Creature::CombatVisualTransactionPhase;
  using Render::Creature::PlaybackLayerMode;
  using Render::Creature::Pipeline::resolve_humanoid_animation_selection;
  using Render::Creature::Pipeline::UnitVisualSpec;

  UnitVisualSpec spec{};
  spec.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
  spec.debug_name = "tests/moving_overlay_selection";
  spec.archetype_id = ArchetypeRegistry::k_humanoid_base;

  Render::GL::HumanoidAnimationContext anim{};
  anim.inputs.movement_state = Render::Creature::MovementAnimationState::Walk;
  anim.inputs.is_attacking = true;
  anim.inputs.is_melee = true;
  anim.inputs.attack_family = Engine::Core::CombatAttackFamily::Sword;
  anim.gait.cycle_phase = 0.21F;
  anim.inputs.combat_visual.authoritative = true;
  anim.inputs.combat_visual.active = true;
  anim.inputs.combat_visual.prioritize_action_over_locomotion = true;
  anim.inputs.combat_visual.is_melee = true;
  anim.inputs.combat_visual.phase = CombatVisualTransactionPhase::Strike;
  anim.inputs.combat_visual.phase_progress = 0.5F;
  anim.inputs.combat_visual.attack_family = Animation::CombatAttackFamily::Sword;

  auto const selection = resolve_humanoid_animation_selection(spec, anim, 29U);

  EXPECT_EQ(selection.state, AnimationStateId::Walk);
  EXPECT_TRUE(selection.upper_body_overlay.active());
  EXPECT_EQ(selection.upper_body_overlay.mode, PlaybackLayerMode::UpperBodyOverlay);
  EXPECT_NE(selection.upper_body_overlay.state, selection.state);
  EXPECT_GT(selection.upper_body_overlay.weight, 0.7F);
}

TEST(HumanoidPrepare, BothDefensiveDoctrinesUseBakedUpperBodyPosesWhileWalking) {
  using Render::Creature::ArchetypeRegistry;
  using Render::Creature::PlaybackLayerMode;
  using Render::Creature::Pipeline::resolve_humanoid_animation_selection;
  using Render::Creature::Pipeline::UnitVisualSpec;

  UnitVisualSpec spec{};
  spec.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
  spec.debug_name = "tests/defensive_layout_overlay";
  spec.archetype_id = ArchetypeRegistry::k_humanoid_base;

  auto overlay_clip_for = [&](Render::GL::ShieldFormationPose pose) {
    Render::GL::HumanoidAnimationContext anim{};
    anim.inputs.movement_state = Render::Creature::MovementAnimationState::Walk;
    anim.inputs.is_guarding = true;
    anim.inputs.guard_pose_progress = 1.0F;
    anim.inputs.is_defensive_layout_locked = true;
    anim.inputs.shield_formation_pose = pose;
    anim.gait.cycle_phase = 0.35F;
    auto const selection = resolve_humanoid_animation_selection(spec, anim, 29U);
    EXPECT_TRUE(selection.upper_body_overlay.active());
    EXPECT_EQ(selection.upper_body_overlay.mode, PlaybackLayerMode::UpperBodyOverlay);
    return selection.upper_body_overlay.clip_id;
  };

  auto const roman = overlay_clip_for(Render::GL::ShieldFormationPose::RomanTop);
  auto const carthage =
      overlay_clip_for(Render::GL::ShieldFormationPose::CarthageFront);
  ASSERT_TRUE(roman.has_value());
  ASSERT_TRUE(carthage.has_value());
  EXPECT_EQ(*roman, Animation::k_humanoid_testudo_top_clip);
  EXPECT_EQ(*carthage, Animation::k_humanoid_carthage_shield_wall_front_clip);
  EXPECT_NE(*roman, *carthage);
}

TEST(HumanoidPrepare, HeldSpearCombatKeepsCompleteKneelingWeaponPose) {
  using Render::Creature::AnimationStateId;
  using Render::Creature::ArchetypeRegistry;
  using Render::Creature::CombatVisualTransactionPhase;
  using Render::Creature::Pipeline::resolve_humanoid_animation_selection;
  using Render::Creature::Pipeline::UnitVisualSpec;

  UnitVisualSpec spec{};
  spec.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
  spec.debug_name = "tests/held_spear_overlay";
  spec.archetype_id = ArchetypeRegistry::k_humanoid_base;

  Render::GL::HumanoidAnimationContext anim{};
  anim.inputs.is_in_hold_mode = true;
  anim.inputs.hold_entry_progress = 1.0F;
  anim.inputs.is_attacking = true;
  anim.inputs.is_melee = true;
  anim.inputs.attack_family = Engine::Core::CombatAttackFamily::Spear;
  anim.inputs.combat_visual.authoritative = true;
  anim.inputs.combat_visual.active = true;
  anim.inputs.combat_visual.is_melee = true;
  anim.inputs.combat_visual.phase = CombatVisualTransactionPhase::Strike;
  anim.inputs.combat_visual.phase_progress = 0.5F;
  anim.inputs.combat_visual.attack_family = Animation::CombatAttackFamily::Spear;
  anim.attack_phase = 0.44F;

  auto const selection = resolve_humanoid_animation_selection(spec, anim, 41U);

  EXPECT_EQ(selection.state, AnimationStateId::Hold);
  EXPECT_GE(selection.phase, 0.0F);
  EXPECT_LT(selection.phase, 1.0F);
  EXPECT_FALSE(selection.upper_body_overlay.active());
  EXPECT_FALSE(selection.full_body_blend.active());
}

TEST(HumanoidPrepare, HeldBowCombatKeepsCompleteKneelingWeaponPose) {
  using Render::Creature::AnimationStateId;
  using Render::Creature::ArchetypeRegistry;
  using Render::Creature::CombatVisualTransactionPhase;
  using Render::Creature::Pipeline::resolve_humanoid_animation_selection;
  using Render::Creature::Pipeline::UnitVisualSpec;

  UnitVisualSpec spec{};
  spec.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
  spec.debug_name = "tests/held_bow_overlay";
  spec.archetype_id = ArchetypeRegistry::k_humanoid_base;

  Render::GL::HumanoidAnimationContext anim{};
  anim.inputs.is_in_hold_mode = true;
  anim.inputs.hold_entry_progress = 1.0F;
  anim.inputs.is_attacking = true;
  anim.inputs.is_melee = false;
  anim.inputs.attack_family = Engine::Core::CombatAttackFamily::Bow;
  anim.inputs.combat_visual.authoritative = true;
  anim.inputs.combat_visual.active = true;
  anim.inputs.combat_visual.is_melee = false;
  anim.inputs.combat_visual.phase = CombatVisualTransactionPhase::Strike;
  anim.inputs.combat_visual.phase_progress = 0.5F;
  anim.inputs.combat_visual.attack_family = Animation::CombatAttackFamily::Bow;
  anim.attack_phase = 0.52F;

  auto const selection = resolve_humanoid_animation_selection(spec, anim, 43U);

  EXPECT_EQ(selection.state, AnimationStateId::Hold);
  EXPECT_GE(selection.phase, 0.0F);
  EXPECT_LT(selection.phase, 1.0F);
  EXPECT_FALSE(selection.upper_body_overlay.active());
  EXPECT_FALSE(selection.full_body_blend.active());
}

TEST(HumanoidPrepare, FormationMeleeKeepsLowerBodyOnRootedBaseLayer) {
  using Render::Creature::AnimationStateId;
  using Render::Creature::ArchetypeRegistry;
  using Render::Creature::CombatVisualTransactionPhase;
  using Render::Creature::PlaybackLayerMode;
  using Render::Creature::Pipeline::resolve_humanoid_animation_selection;
  using Render::Creature::Pipeline::UnitVisualSpec;

  UnitVisualSpec spec{};
  spec.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
  spec.debug_name = "tests/rooted_formation_melee";
  spec.archetype_id = ArchetypeRegistry::k_humanoid_base;

  Render::GL::HumanoidAnimationContext anim{};
  anim.inputs.movement_state = Render::Creature::MovementAnimationState::Idle;
  anim.inputs.is_attacking = true;
  anim.inputs.is_melee = true;
  anim.inputs.is_in_melee_lock = true;
  anim.inputs.attack_family = Engine::Core::CombatAttackFamily::Spear;
  anim.inputs.combat_visual.authoritative = true;
  anim.inputs.combat_visual.active = true;
  anim.inputs.combat_visual.prioritize_action_over_locomotion = true;
  anim.inputs.combat_visual.is_melee = true;
  anim.inputs.combat_visual.phase = CombatVisualTransactionPhase::Strike;
  anim.inputs.combat_visual.phase_progress = 0.5F;
  anim.inputs.combat_visual.attack_family = Animation::CombatAttackFamily::Spear;

  auto const selection = resolve_humanoid_animation_selection(spec, anim, 41U);

  EXPECT_EQ(selection.state, AnimationStateId::Idle);
  ASSERT_TRUE(selection.upper_body_overlay.active());
  EXPECT_EQ(selection.upper_body_overlay.mode, PlaybackLayerMode::UpperBodyOverlay);
  EXPECT_EQ(selection.upper_body_overlay.state, AnimationStateId::AttackSpear);
  EXPECT_FALSE(selection.full_body_blend.active());
}

TEST(HumanoidPrepare, CombatAttackEmphasisScalesUpperBodyOverlayWeight) {
  using Render::Creature::ArchetypeRegistry;
  using Render::Creature::CombatVisualTransactionPhase;
  using Render::Creature::Pipeline::resolve_humanoid_animation_selection;
  using Render::Creature::Pipeline::UnitVisualSpec;

  UnitVisualSpec spec{};
  spec.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
  spec.debug_name = "tests/emphasis_overlay_selection";
  spec.archetype_id = ArchetypeRegistry::k_humanoid_base;

  Render::GL::HumanoidAnimationContext anim{};
  anim.inputs.movement_state = Render::Creature::MovementAnimationState::Walk;
  anim.inputs.is_attacking = true;
  anim.inputs.is_melee = true;
  anim.inputs.attack_family = Engine::Core::CombatAttackFamily::Sword;
  anim.inputs.combat_visual.authoritative = true;
  anim.inputs.combat_visual.active = true;
  anim.inputs.combat_visual.prioritize_action_over_locomotion = true;
  anim.inputs.combat_visual.is_melee = true;
  anim.inputs.combat_visual.phase = CombatVisualTransactionPhase::Anticipation;
  anim.inputs.combat_visual.phase_progress = 0.5F;
  anim.inputs.combat_visual.attack_family = Animation::CombatAttackFamily::Sword;

  anim.inputs.combat_visual.attack_emphasis = 0.75F;
  auto const restrained = resolve_humanoid_animation_selection(spec, anim, 29U);

  anim.inputs.combat_visual.attack_emphasis = 1.18F;
  auto const forceful = resolve_humanoid_animation_selection(spec, anim, 29U);

  ASSERT_TRUE(restrained.upper_body_overlay.active());
  ASSERT_TRUE(forceful.upper_body_overlay.active());
  EXPECT_GT(forceful.upper_body_overlay.weight,
            restrained.upper_body_overlay.weight + 0.15F);
}

TEST(HumanoidPrepare, ExitBlendSelectionCarriesFullBodyOutgoingClip) {
  using Render::Creature::AnimationStateId;
  using Render::Creature::ArchetypeRegistry;
  using Render::Creature::CombatVisualTransactionPhase;
  using Render::Creature::PlaybackLayerMode;
  using Render::Creature::Pipeline::resolve_humanoid_animation_selection;
  using Render::Creature::Pipeline::UnitVisualSpec;

  UnitVisualSpec spec{};
  spec.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
  spec.debug_name = "tests/exit_blend_selection";
  spec.archetype_id = ArchetypeRegistry::k_humanoid_base;

  Render::GL::HumanoidAnimationContext anim{};
  anim.inputs.is_attacking = true;
  anim.inputs.is_melee = true;
  anim.inputs.attack_family = Engine::Core::CombatAttackFamily::Sword;
  anim.inputs.combat_visual.authoritative = true;
  anim.inputs.combat_visual.active = true;
  anim.inputs.combat_visual.is_melee = true;
  anim.inputs.combat_visual.phase = CombatVisualTransactionPhase::ExitBlend;
  anim.inputs.combat_visual.exit_blend_progress = 0.2F;
  anim.inputs.combat_visual.attack_family = Animation::CombatAttackFamily::Sword;

  auto const selection = resolve_humanoid_animation_selection(spec, anim, 31U);

  EXPECT_EQ(selection.state, AnimationStateId::AttackSword);
  EXPECT_TRUE(selection.full_body_blend.active());
  EXPECT_EQ(selection.full_body_blend.mode, PlaybackLayerMode::FullBodyBlend);
  EXPECT_NE(selection.full_body_blend.state, selection.state);
  EXPECT_GT(selection.full_body_blend.weight, 0.2F);
}

TEST(HumanoidPrepare, CreaturePipelineUpperBodyOverlayKeepsLegsOnBaseClip) {
  using Render::Creature::AnimationStateId;
  using Render::Creature::ArchetypeRegistry;
  using Render::Creature::CreatureLOD;
  using Render::Creature::CreatureRenderRequest;
  using Render::Creature::PlaybackLayerMode;
  using Bone = Render::Humanoid::HumanoidBone;

  CreatureRenderRequest walk{};
  walk.archetype = ArchetypeRegistry::k_humanoid_base;
  walk.state = AnimationStateId::Walk;
  walk.phase = 0.23F;
  walk.lod = CreatureLOD::Full;

  auto const walk_palette = request_bone_palette_copy(walk);

  CreatureRenderRequest attack = walk;
  attack.state = AnimationStateId::AttackSword;
  attack.phase = 0.41F;
  auto const attack_palette = request_bone_palette_copy(attack);

  CreatureRenderRequest overlay = walk;
  overlay.upper_body_overlay.archetype = ArchetypeRegistry::k_humanoid_base;
  overlay.upper_body_overlay.state = AnimationStateId::AttackSword;
  overlay.upper_body_overlay.phase = 0.41F;
  overlay.upper_body_overlay.weight = 1.0F;
  overlay.upper_body_overlay.mode = PlaybackLayerMode::UpperBodyOverlay;
  auto const overlay_palette = request_bone_palette_copy(overlay);

  auto const foot_index = static_cast<std::size_t>(Bone::FootL);
  auto const hand_index = static_cast<std::size_t>(Bone::HandR);
  auto const forearm_index = static_cast<std::size_t>(Bone::ForearmR);
  auto const pelvis_index = static_cast<std::size_t>(Bone::Pelvis);
  auto const neck_index = static_cast<std::size_t>(Bone::Neck);
  ASSERT_GT(overlay_palette.size(), hand_index);
  ASSERT_GT(attack_palette.size(), hand_index);
  ASSERT_GT(walk_palette.size(), hand_index);

  EXPECT_LT(max_matrix_delta(overlay_palette[foot_index], walk_palette[foot_index]),
            1.0e-5F);
  EXPECT_GT(max_matrix_delta(overlay_palette[foot_index], attack_palette[foot_index]),
            1.0e-3F);
  EXPECT_GT(max_matrix_delta(overlay_palette[hand_index], walk_palette[hand_index]),
            1.0e-3F);

  auto const bind = Render::Humanoid::humanoid_bind_palette();
  auto posed_global = [&](const auto& palette, std::size_t bone) {
    return palette[bone] * bind[bone];
  };
  QMatrix4x4 const overlay_hand_local =
      posed_global(overlay_palette, forearm_index).inverted() *
      posed_global(overlay_palette, hand_index);
  QMatrix4x4 const attack_hand_local =
      posed_global(attack_palette, forearm_index).inverted() *
      posed_global(attack_palette, hand_index);
  EXPECT_LT(max_matrix_delta(overlay_hand_local, attack_hand_local), 1.0e-4F);

  QVector3D const pelvis =
      posed_global(overlay_palette, pelvis_index).column(3).toVector3D();
  QVector3D const neck =
      posed_global(overlay_palette, neck_index).column(3).toVector3D();
  ASSERT_GT((neck - pelvis).lengthSquared(), 1.0e-6F);
  EXPECT_GT((neck - pelvis).normalized().y(), 0.72F);
}

TEST(HumanoidPrepare, CreaturePipelineSpearOverlayKeepsArmChainAnatomical) {
  using Render::Creature::AnimationStateId;
  using Render::Creature::ArchetypeRegistry;
  using Render::Creature::CreatureLOD;
  using Render::Creature::CreatureRenderRequest;
  using Render::Creature::PlaybackLayerMode;
  using Bone = Render::Humanoid::HumanoidBone;

  CreatureRenderRequest request{};
  request.archetype = ArchetypeRegistry::k_humanoid_base;
  request.state = AnimationStateId::Idle;
  request.phase = 0.30F;
  request.lod = CreatureLOD::Full;
  request.upper_body_overlay.archetype = ArchetypeRegistry::k_humanoid_base;
  request.upper_body_overlay.state = AnimationStateId::AttackSpear;
  request.upper_body_overlay.phase = 0.61F;
  request.upper_body_overlay.mode = PlaybackLayerMode::UpperBodyOverlay;

  auto const bind = Render::Humanoid::humanoid_bind_palette();
  auto const shoulder_l = static_cast<std::size_t>(Bone::ShoulderL);
  auto const hand_l = static_cast<std::size_t>(Bone::HandL);
  auto const shoulder_r = static_cast<std::size_t>(Bone::ShoulderR);
  auto const hand_r = static_cast<std::size_t>(Bone::HandR);
  auto max_arm_reach = [&](const auto& palette) {
    auto position = [&](std::size_t bone) {
      return (palette[bone] * bind[bone]).column(3).toVector3D();
    };
    return std::max((position(hand_l) - position(shoulder_l)).length(),
                    (position(hand_r) - position(shoulder_r)).length());
  };

  CreatureRenderRequest attack = request;
  attack.state = AnimationStateId::AttackSpear;
  attack.phase = 0.61F;
  attack.upper_body_overlay = {};
  auto const attack_palette = request_bone_palette_copy(attack);
  ASSERT_GT(attack_palette.size(), hand_r);
  EXPECT_LE(max_arm_reach(attack_palette), 0.60F);

  for (float const weight : {0.25F, 0.50F, 0.75F, 1.0F}) {
    request.upper_body_overlay.weight = weight;
    auto const palette = request_bone_palette_copy(request);
    ASSERT_GT(palette.size(), hand_r);
    EXPECT_LE(max_arm_reach(palette), 0.60F) << "overlay weight=" << weight;
  }
}

TEST(HumanoidPrepare, MountedSpearFrameInterpolationKeepsArmChainAnatomical) {
  using Render::Creature::AnimationStateId;
  using Render::Creature::ArchetypeRegistry;
  using Render::Creature::CreatureLOD;
  using Render::Creature::CreatureRenderRequest;
  using Bone = Render::Humanoid::HumanoidBone;

  CreatureRenderRequest request{};
  request.archetype = ArchetypeRegistry::k_rider_base;
  request.state = AnimationStateId::RidingCharge;
  request.lod = CreatureLOD::Full;

  auto const bind = Render::Humanoid::humanoid_bind_palette();
  auto const shoulder_l = static_cast<std::size_t>(Bone::ShoulderL);
  auto const hand_l = static_cast<std::size_t>(Bone::HandL);
  auto const shoulder_r = static_cast<std::size_t>(Bone::ShoulderR);
  auto const hand_r = static_cast<std::size_t>(Bone::HandR);
  auto max_arm_reach = [&](const auto& palette) {
    auto position = [&](std::size_t bone) {
      return (palette[bone] * bind[bone]).column(3).toVector3D();
    };
    return std::max((position(hand_l) - position(shoulder_l)).length(),
                    (position(hand_r) - position(shoulder_r)).length());
  };

  for (auto const clip : {Animation::k_humanoid_riding_spear_thrust_clip,
                          Animation::k_humanoid_riding_charge_clip}) {
    request.clip_id = clip;
    for (int frame = 0; frame <= 100; ++frame) {
      request.phase = static_cast<float>(frame) / 100.0F;
      auto const palette = request_bone_palette_copy(request);
      ASSERT_GT(palette.size(), hand_r);
      EXPECT_LE(max_arm_reach(palette), 0.60F)
          << "clip=" << clip << " phase=" << request.phase;
    }
  }

  request.clip_id = Animation::k_humanoid_riding_spear_thrust_clip;
  request.full_body_blend.archetype = ArchetypeRegistry::k_rider_base;
  request.full_body_blend.state = AnimationStateId::RidingCharge;
  request.full_body_blend.clip_id = Animation::k_humanoid_riding_charge_clip;
  request.full_body_blend.mode = Render::Creature::PlaybackLayerMode::FullBodyBlend;
  for (float const action_phase : {0.25F, 0.50F, 0.75F}) {
    request.phase = action_phase;
    for (float const base_phase : {0.0F, 0.25F, 0.50F, 0.75F}) {
      request.full_body_blend.phase = base_phase;
      for (float const weight : {0.25F, 0.50F, 0.75F}) {
        request.full_body_blend.weight = weight;
        auto const palette = request_bone_palette_copy(request);
        ASSERT_GT(palette.size(), hand_r);
        EXPECT_LE(max_arm_reach(palette), 0.60F)
            << "action_phase=" << action_phase << " base_phase=" << base_phase
            << " weight=" << weight;
      }
    }
  }
}

TEST(HumanoidPrepare, CreaturePipelineReportsBlendRequestStats) {
  using Render::Creature::AnimationStateId;
  using Render::Creature::ArchetypeRegistry;
  using Render::Creature::CreatureLOD;
  using Render::Creature::CreatureRenderRequest;
  using Render::Creature::PlaybackLayerMode;

  CreatureRenderRequest request{};
  request.archetype = ArchetypeRegistry::k_humanoid_base;
  request.state = AnimationStateId::AttackSword;
  request.phase = 0.35F;
  request.lod = CreatureLOD::Full;
  request.full_body_blend.archetype = ArchetypeRegistry::k_humanoid_base;
  request.full_body_blend.state = AnimationStateId::Walk;
  request.full_body_blend.phase = 0.2F;
  request.full_body_blend.weight = 0.4F;
  request.full_body_blend.mode = PlaybackLayerMode::FullBodyBlend;
  request.upper_body_overlay.archetype = ArchetypeRegistry::k_humanoid_base;
  request.upper_body_overlay.state = AnimationStateId::AttackSword;
  request.upper_body_overlay.phase = 0.35F;
  request.upper_body_overlay.weight = 0.75F;
  request.upper_body_overlay.mode = PlaybackLayerMode::UpperBodyOverlay;

  auto const stats = submit_request_stats(request);

  EXPECT_EQ(stats.entities_submitted, 1U);
  EXPECT_EQ(stats.full_body_blend_requests, 1U);
  EXPECT_EQ(stats.upper_body_overlay_requests, 1U);
}

TEST(HumanoidPrepare, ConstructionVariantTableMapsFourRolesToExpectedRequests) {
  class FixedSpecRenderer final : public Render::GL::HumanoidRendererBase {
  public:
    explicit FixedSpecRenderer(Render::Creature::Pipeline::UnitVisualSpec spec)
        : Render::GL::HumanoidRendererBase(std::move(spec)) {}
  };

  using Render::Creature::AnimationStateId;
  using Render::Creature::ArchetypeVariantTable;

  EXPECT_GT(render_builder_submission_count(
                "troops/roman/builder", Game::Systems::NationID::RomanRepublic, true),
            0);

  auto const archetype_id = find_archetype_id("troops/roman/builder");
  ASSERT_NE(archetype_id, Render::Creature::k_invalid_archetype);

  static const ArchetypeVariantTable k_variant_table = [archetype_id]() {
    ArchetypeVariantTable t{};
    t.variant_trigger_pose = Render::Creature::PoseIntent::Construct;
    t.variant_stride = 4;
    t.variant_is_seed_based = true;
    t.archetype_for_variant[0] = archetype_id;
    t.state_for_variant[0] = AnimationStateId::AttackSword;
    t.archetype_for_variant[1] = archetype_id;
    t.state_for_variant[1] = AnimationStateId::AttackSword;
    t.archetype_for_variant[2] = archetype_id;
    t.state_for_variant[2] = AnimationStateId::AttackSword;
    t.archetype_for_variant[3] = archetype_id;
    t.state_for_variant[3] = AnimationStateId::Hold;
    return t;
  }();

  Render::Creature::Pipeline::UnitVisualSpec spec{};
  spec.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
  spec.debug_name = "tests/construction_variant_role_renderer";
  spec.archetype_id = archetype_id;
  spec.animation_manifest.variant_table = &k_variant_table;
  FixedSpecRenderer const owner(spec);

  Engine::Core::StandaloneEntity entity_scratch(17);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  auto* builder = entity.add_component<Engine::Core::BuilderProductionComponent>();
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(builder, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Builder;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  unit->render_individuals_per_unit_override = 16;
  builder->in_progress = true;
  builder->has_construction_site = true;
  builder->at_construction_site = true;
  builder->build_time = 10.0F;
  builder->time_remaining = 6.0F;
  builder->construction_site_x = 2.0F;
  builder->construction_site_z = 0.0F;

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.allow_template_cache = false;
  ctx.force_humanoid_lod = true;
  ctx.forced_humanoid_lod = Render::Creature::CreatureLOD::Full;
  ctx.animation_time = 0.25F;
  ctx.entity = &entity;

  Render::GL::AnimationInputs anim{};
  anim.time = ctx.animation_time;
  anim.is_constructing = true;
  anim.construction_progress = 0.40F;

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, anim, test_runtime(0U), prep);

  ASSERT_EQ(prep.bodies.requests().size(), 16U);

  for (auto const& req : prep.bodies.requests()) {
    auto const role = Animation::resolve_humanoid_construction_role({
        .seed = req.seed,
        .variant_table_can_select_roles = true,
        .variant_stride = k_variant_table.variant_stride,
        .variant_is_seed_based = k_variant_table.variant_is_seed_based,
    });
    ASSERT_NE(role, Animation::HumanoidConstructionRole::None);

    EXPECT_EQ(req.clip_id, Animation::humanoid_construction_clip_for_role(role))
        << "a constructing soldier plays the dedicated work clip for its role, never a "
           "sword swing";
    EXPECT_EQ(req.clip_variant, 0U);
    EXPECT_GE(req.phase, 0.0F);
    EXPECT_LT(req.phase, 1.0F) << "the work clip loops on the jittered work cycle";
    if (role == Animation::HumanoidConstructionRole::KneelingChisel) {
      EXPECT_EQ(req.state, AnimationStateId::Hold);
    } else {
      EXPECT_EQ(req.state, AnimationStateId::AttackSword);
    }
  }
}

TEST(HumanoidPrepare, TheWorkJobPicksTheToolAndTheClip) {
  EXPECT_EQ(
      Animation::humanoid_construction_role_for_job(Animation::HumanoidWorkJob::Reap),
      Animation::HumanoidConstructionRole::Reap);
  EXPECT_EQ(
      Animation::humanoid_construction_role_for_job(Animation::HumanoidWorkJob::Chop),
      Animation::HumanoidConstructionRole::Hammer);
  EXPECT_EQ(
      Animation::humanoid_construction_role_for_job(Animation::HumanoidWorkJob::Quarry),
      Animation::HumanoidConstructionRole::KneelingChisel);
  EXPECT_EQ(Animation::humanoid_construction_role_for_job(
                Animation::HumanoidWorkJob::Butcher),
            Animation::HumanoidConstructionRole::KneelingChisel);
  EXPECT_EQ(
      Animation::humanoid_construction_role_for_job(Animation::HumanoidWorkJob::Build),
      Animation::HumanoidConstructionRole::None)
      << "building keeps the seeded mix of hammer, saw and chisel crews";

  EXPECT_EQ(Animation::resolve_humanoid_construction_role({
                .seed = 7U,
                .variant_table_can_select_roles = true,
                .variant_stride = 5U,
                .variant_is_seed_based = true,
                .job = Animation::HumanoidWorkJob::Reap,
            }),
            Animation::HumanoidConstructionRole::Reap);
  EXPECT_EQ(Animation::humanoid_construction_variant_for_role(
                Animation::HumanoidConstructionRole::Reap),
            4U);
  EXPECT_EQ(Animation::humanoid_construction_clip_for_role(
                Animation::HumanoidConstructionRole::Reap),
            Animation::k_humanoid_construct_reap_clip);
  EXPECT_LT(Animation::k_humanoid_construct_reap_clip,
            Animation::k_humanoid_clip_count);
}

TEST(HumanoidPrepare, BuiltInBuildersUseDifferentPoseWhileConstructing) {
  auto const* roman_idle = render_builder_bone_palette(
      "troops/roman/builder", Game::Systems::NationID::RomanRepublic, false, 10.0F);
  auto const* roman_constructing = render_builder_bone_palette(
      "troops/roman/builder", Game::Systems::NationID::RomanRepublic, true, 9.0F);
  auto const* carthage_idle = render_builder_bone_palette(
      "troops/carthage/builder", Game::Systems::NationID::Carthage, false, 10.0F);
  auto const* carthage_constructing = render_builder_bone_palette(
      "troops/carthage/builder", Game::Systems::NationID::Carthage, true, 9.0F);

  EXPECT_NE(roman_idle, roman_constructing);
  EXPECT_NE(carthage_idle, carthage_constructing);
}

auto render_builder_rigged_meshes(
    const char* renderer_id,
    Game::Systems::NationID nation_id,
    bool attacking,
    Game::Units::SpawnType spawn_type = Game::Units::SpawnType::Builder)
    -> std::vector<const Render::GL::RiggedMesh*> {
  Render::GL::reset_humanoid_runtime_context();
  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto renderer = registry.get(renderer_id);
  EXPECT_TRUE(static_cast<bool>(renderer));
  if (!renderer) {
    return {};
  }

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;
  ctx.animation_time = 0.5F;

  Engine::Core::StandaloneEntity entity_scratch(1);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  EXPECT_NE(unit, nullptr);
  if (unit == nullptr) {
    return {};
  }
  unit->spawn_type = spawn_type;
  unit->nation_id = nation_id;
  Render::GL::AnimationInputs anim{};
  anim.time = ctx.animation_time;
  if (attacking) {
    anim.is_attacking = true;
    anim.is_melee = true;
    anim.combat_phase = Render::GL::CombatAnimPhase::Strike;
    anim.combat_phase_progress = 0.5F;
    anim.attack_family = Engine::Core::CombatAttackFamily::None;
  }
  ctx.animation_override = &anim;
  ctx.entity = &entity;

  CountingSubmitter sink;
  renderer(ctx, sink);
  return sink.rigged_meshes;
}

TEST(HumanoidPrepare, BuiltInBuildersFightWithTheMalletInHand) {
  for (auto const& [renderer_id, nation] :
       {std::pair{"troops/roman/builder", Game::Systems::NationID::RomanRepublic},
        std::pair{"troops/carthage/builder", Game::Systems::NationID::Carthage}}) {
    auto const idle = render_builder_rigged_meshes(renderer_id, nation, false);
    auto const fighting = render_builder_rigged_meshes(renderer_id, nation, true);
    ASSERT_FALSE(idle.empty()) << renderer_id;
    ASSERT_FALSE(fighting.empty()) << renderer_id;
    EXPECT_NE(idle.front(), fighting.front())
        << renderer_id
        << ": a builder in melee swings the mallet archetype, not the bare idle body";
  }
}

TEST(HumanoidPrepare, BuiltInCiviliansFightWithTheCudgelInHand) {
  for (auto const& [renderer_id, nation] :
       {std::pair{"troops/roman/civilian", Game::Systems::NationID::RomanRepublic},
        std::pair{"troops/carthage/civilian", Game::Systems::NationID::Carthage}}) {
    auto const idle = render_builder_rigged_meshes(
        renderer_id, nation, false, Game::Units::SpawnType::Civilian);
    auto const fighting = render_builder_rigged_meshes(
        renderer_id, nation, true, Game::Units::SpawnType::Civilian);
    ASSERT_FALSE(idle.empty()) << renderer_id;
    ASSERT_FALSE(fighting.empty()) << renderer_id;
    EXPECT_NE(idle.front(), fighting.front())
        << renderer_id
        << ": a cornered civilian swings the cudgel archetype, not bare fists";
  }
}

TEST(HumanoidPrepare, BuiltInBuildersUseMixedConstructionToolSets) {
  EXPECT_GT(render_builder_unique_tool_mesh_count(
                "troops/roman/builder", Game::Systems::NationID::RomanRepublic),
            1U);
  EXPECT_GT(render_builder_unique_tool_mesh_count("troops/carthage/builder",
                                                  Game::Systems::NationID::Carthage),
            1U);
}

TEST(HumanoidPrepare, BuiltInArchersUseDedicatedBowHoldClip) {
  using Render::Creature::AnimationStateId;
  auto& registry = Render::Creature::ArchetypeRegistry::instance();

  auto const roman_id = find_archetype_id("troops/roman/archer");
  ASSERT_NE(roman_id, Render::Creature::k_invalid_archetype);
  EXPECT_EQ(registry.bpat_clip(roman_id, AnimationStateId::Hold),
            Render::Creature::k_humanoid_hold_bow_clip);
  EXPECT_NE(registry.bpat_clip(roman_id, AnimationStateId::Hold),
            registry.bpat_clip(roman_id, AnimationStateId::AttackBow));

  auto const carthage_id = find_archetype_id("troops/carthage/archer");
  ASSERT_NE(carthage_id, Render::Creature::k_invalid_archetype);
  EXPECT_EQ(registry.bpat_clip(carthage_id, AnimationStateId::Hold),
            Render::Creature::k_humanoid_hold_bow_clip);
  EXPECT_NE(registry.bpat_clip(carthage_id, AnimationStateId::Hold),
            registry.bpat_clip(carthage_id, AnimationStateId::AttackBow));
}

TEST(HumanoidPrepare, RoleSpecificClipsSeparateArcherMeleeAndHeldAttacks) {
  using Render::Creature::ArchetypeRegistry;
  using Render::Creature::Pipeline::resolve_humanoid_animation_selection;
  using Render::Creature::Pipeline::UnitVisualSpec;

  UnitVisualSpec spec{};
  spec.archetype_id = ArchetypeRegistry::k_humanoid_base;
  spec.animation_manifest.melee_clip_override = Animation::k_humanoid_archer_melee_clip;

  Render::GL::HumanoidAnimationContext anim{};
  anim.inputs.is_attacking = true;
  anim.inputs.is_melee = true;
  anim.inputs.attack_family = Engine::Core::CombatAttackFamily::Sword;
  auto melee = resolve_humanoid_animation_selection(spec, anim, 7U);
  ASSERT_TRUE(melee.clip_id.has_value());
  EXPECT_EQ(*melee.clip_id, Animation::k_humanoid_archer_melee_clip);

  spec.animation_manifest.melee_clip_override = Animation::k_unmapped_clip;
  anim.inputs.is_in_hold_mode = true;
  anim.inputs.attack_family = Engine::Core::CombatAttackFamily::Spear;
  auto held_spear = resolve_humanoid_animation_selection(spec, anim, 7U);
  ASSERT_TRUE(held_spear.clip_id.has_value());
  EXPECT_EQ(*held_spear.clip_id, Animation::k_humanoid_hold_spear_attack_clip);

  anim.inputs.is_melee = false;
  anim.inputs.attack_family = Engine::Core::CombatAttackFamily::Bow;
  auto held_bow = resolve_humanoid_animation_selection(spec, anim, 7U);
  ASSERT_TRUE(held_bow.clip_id.has_value());
  EXPECT_EQ(*held_bow.clip_id, Animation::k_humanoid_hold_bow_attack_clip);
}

TEST(HumanoidPrepare, BuiltInArchersKeepBowVisibleWhileMoving) {
  EXPECT_GT(render_bow_mesh_count("troops/roman/archer",
                                  Game::Units::SpawnType::Archer,
                                  Game::Systems::NationID::RomanRepublic,
                                  false),
            0);
  EXPECT_EQ(render_runtime_mesh_count("troops/roman/archer",
                                      Game::Units::SpawnType::Archer,
                                      Game::Systems::NationID::RomanRepublic,
                                      false),
            0);
  EXPECT_GT(render_bow_mesh_count("troops/roman/archer",
                                  Game::Units::SpawnType::Archer,
                                  Game::Systems::NationID::RomanRepublic,
                                  true),
            0);
  EXPECT_EQ(render_runtime_mesh_count("troops/roman/archer",
                                      Game::Units::SpawnType::Archer,
                                      Game::Systems::NationID::RomanRepublic,
                                      true),
            0);
  EXPECT_GT(render_bow_mesh_count("troops/carthage/archer",
                                  Game::Units::SpawnType::Archer,
                                  Game::Systems::NationID::Carthage,
                                  false),
            0);
  EXPECT_EQ(render_runtime_mesh_count("troops/carthage/archer",
                                      Game::Units::SpawnType::Archer,
                                      Game::Systems::NationID::Carthage,
                                      false),
            0);
  EXPECT_GT(render_bow_mesh_count("troops/carthage/archer",
                                  Game::Units::SpawnType::Archer,
                                  Game::Systems::NationID::Carthage,
                                  true),
            0);
  EXPECT_EQ(render_runtime_mesh_count("troops/carthage/archer",
                                      Game::Units::SpawnType::Archer,
                                      Game::Systems::NationID::Carthage,
                                      true),
            0);
  EXPECT_GT(render_bow_mesh_count("troops/roman/horse_archer",
                                  Game::Units::SpawnType::HorseArcher,
                                  Game::Systems::NationID::RomanRepublic,
                                  false),
            0);
  EXPECT_EQ(render_runtime_mesh_count("troops/roman/horse_archer",
                                      Game::Units::SpawnType::HorseArcher,
                                      Game::Systems::NationID::RomanRepublic,
                                      false),
            0);
  EXPECT_GT(render_bow_mesh_count("troops/roman/horse_archer",
                                  Game::Units::SpawnType::HorseArcher,
                                  Game::Systems::NationID::RomanRepublic,
                                  true),
            0);
  EXPECT_EQ(render_runtime_mesh_count("troops/roman/horse_archer",
                                      Game::Units::SpawnType::HorseArcher,
                                      Game::Systems::NationID::RomanRepublic,
                                      true),
            0);
  EXPECT_GT(render_bow_mesh_count("troops/carthage/horse_archer",
                                  Game::Units::SpawnType::HorseArcher,
                                  Game::Systems::NationID::Carthage,
                                  false),
            0);
  EXPECT_EQ(render_runtime_mesh_count("troops/carthage/horse_archer",
                                      Game::Units::SpawnType::HorseArcher,
                                      Game::Systems::NationID::Carthage,
                                      false),
            0);
  EXPECT_GT(render_bow_mesh_count("troops/carthage/horse_archer",
                                  Game::Units::SpawnType::HorseArcher,
                                  Game::Systems::NationID::Carthage,
                                  true),
            0);
  EXPECT_EQ(render_runtime_mesh_count("troops/carthage/horse_archer",
                                      Game::Units::SpawnType::HorseArcher,
                                      Game::Systems::NationID::Carthage,
                                      true),
            0);
}

TEST(HumanoidPrepare, BuiltInFootArchersKeepBowBakedInHold) {
  EXPECT_EQ(render_direct_bow_mesh_count("troops/roman/archer",
                                         Game::Units::SpawnType::Archer,
                                         Game::Systems::NationID::RomanRepublic,
                                         true),
            0);
  EXPECT_EQ(render_direct_bow_mesh_count("troops/carthage/archer",
                                         Game::Units::SpawnType::Archer,
                                         Game::Systems::NationID::Carthage,
                                         true),
            0);
}

TEST(HumanoidPrepare, HumanoidBpatPlaybackFollowsArcherVisibleClipState) {
  using Render::Creature::AnimationStateId;
  using Render::Creature::Pipeline::humanoid_bpat_playback_for_anim;

  auto& registry = Render::Creature::ArchetypeRegistry::instance();
  auto const roman_id = find_archetype_id("troops/roman/archer");
  ASSERT_NE(roman_id, Render::Creature::k_invalid_archetype);

  Render::GL::HumanoidAnimationContext moving_anim{};
  moving_anim.inputs.movement_state = Render::Creature::MovementAnimationState::Walk;
  moving_anim.gait.state = Render::GL::HumanoidMotionState::Walk;
  moving_anim.gait.cycle_phase = 0.35F;
  auto const moving = humanoid_bpat_playback_for_anim(
      roman_id, Render::Creature::Bpat::k_species_humanoid, moving_anim);
  ASSERT_TRUE(moving.has_value());
  EXPECT_EQ(moving->clip_id, registry.bpat_clip(roman_id, AnimationStateId::Walk));

  Render::GL::HumanoidAnimationContext hold_anim{};
  hold_anim.inputs.is_in_hold_mode = true;
  hold_anim.inputs.hold_entry_progress = 1.0F;
  hold_anim.gait.state = Render::GL::HumanoidMotionState::Hold;
  hold_anim.gait.cycle_phase = 0.20F;
  auto const hold = humanoid_bpat_playback_for_anim(
      roman_id, Render::Creature::Bpat::k_species_humanoid, hold_anim);
  ASSERT_TRUE(hold.has_value());
  EXPECT_EQ(hold->clip_id, registry.bpat_clip(roman_id, AnimationStateId::Hold));
}

TEST(HumanoidPrepare, LocomotionOverridesStationaryActionFlagsForPlayback) {
  using Render::Creature::AnimationStateId;
  using Render::Creature::Pipeline::humanoid_bpat_playback_for_anim;

  auto& registry = Render::Creature::ArchetypeRegistry::instance();
  auto const archetype_id = Render::Creature::ArchetypeRegistry::k_humanoid_base;

  auto expect_walk = [&](Render::GL::AnimationInputs anim) {
    anim.movement_state = Render::Creature::MovementAnimationState::Walk;
    auto const resolved_pose = Render::Creature::resolve_pose(anim);

    EXPECT_EQ(resolved_pose.intent, Render::Creature::PoseIntent::Walk);
    EXPECT_EQ(resolved_pose.motion_state, Render::GL::HumanoidMotionState::Walk);

    Render::GL::HumanoidAnimationContext anim_ctx{};
    anim_ctx.inputs = anim;
    anim_ctx.gait.state = resolved_pose.motion_state;
    anim_ctx.gait.cycle_phase = 0.35F;

    auto const playback = humanoid_bpat_playback_for_anim(
        archetype_id, Render::Creature::Bpat::k_species_humanoid, anim_ctx);
    ASSERT_TRUE(playback.has_value());
    EXPECT_EQ(playback->clip_id,
              registry.bpat_clip(archetype_id, AnimationStateId::Walk));
  };

  Render::GL::AnimationInputs hold_anim{};
  hold_anim.is_in_hold_mode = true;
  hold_anim.hold_entry_progress = 1.0F;
  expect_walk(hold_anim);

  Render::GL::AnimationInputs exiting_hold_anim{};
  exiting_hold_anim.is_exiting_hold = true;
  exiting_hold_anim.hold_exit_progress = 0.5F;
  expect_walk(exiting_hold_anim);

  Render::GL::AnimationInputs construct_anim{};
  construct_anim.is_constructing = true;
  construct_anim.construction_progress = 0.4F;
  expect_walk(construct_anim);

  Render::GL::AnimationInputs healing_anim{};
  healing_anim.is_healing = true;
  expect_walk(healing_anim);

  Render::GL::AnimationInputs attacking_anim{};
  attacking_anim.is_attacking = true;
  attacking_anim.is_melee = true;
  attacking_anim.attack_family = Engine::Core::CombatAttackFamily::Spear;
  expect_walk(attacking_anim);
}

TEST(HumanoidPrepare, RiderDeathPlaybackUsesMountedDeathClips) {
  using Render::Creature::Pipeline::humanoid_bpat_playback_for_anim;

  Render::GL::HumanoidAnimationContext dying_anim{};
  dying_anim.inputs.is_dying = true;
  dying_anim.inputs.death_progress = 0.5F;
  auto const dying =
      humanoid_bpat_playback_for_anim(Render::Creature::ArchetypeRegistry::k_rider_base,
                                      Render::Creature::Bpat::k_species_humanoid,
                                      dying_anim);
  ASSERT_TRUE(dying.has_value());
  EXPECT_EQ(dying->clip_id, Render::Creature::k_humanoid_die_mounted_clip);

  Render::GL::HumanoidAnimationContext dead_anim{};
  dead_anim.inputs.is_dead = true;
  dead_anim.inputs.death_progress = 1.0F;
  auto const dead =
      humanoid_bpat_playback_for_anim(Render::Creature::ArchetypeRegistry::k_rider_base,
                                      Render::Creature::Bpat::k_species_humanoid,
                                      dead_anim);
  ASSERT_TRUE(dead.has_value());
  EXPECT_EQ(dead->clip_id, Render::Creature::k_humanoid_dead_mounted_clip);
}

TEST(HumanoidPrepare, SpearAttackPlaybackUsesSpearClipFamily) {
  using Render::Creature::AnimationStateId;
  using Render::Creature::Pipeline::humanoid_bpat_playback_for_anim;

  auto& registry = Render::Creature::ArchetypeRegistry::instance();
  auto const archetype_id = Render::Creature::ArchetypeRegistry::k_humanoid_base;

  Render::GL::HumanoidAnimationContext attack_anim{};
  attack_anim.inputs.is_attacking = true;
  attack_anim.inputs.is_melee = true;
  attack_anim.inputs.attack_family = Engine::Core::CombatAttackFamily::Spear;
  attack_anim.inputs.attack_variant = 1U;
  attack_anim.gait.state = Render::GL::HumanoidMotionState::Attacking;
  attack_anim.attack_phase = 0.35F;

  auto const attack = humanoid_bpat_playback_for_anim(
      archetype_id, Render::Creature::Bpat::k_species_humanoid, attack_anim);
  ASSERT_TRUE(attack.has_value());
  EXPECT_EQ(
      attack->clip_id,
      registry.resolve_bpat_clip(archetype_id, AnimationStateId::AttackSpear, 1U));
}

TEST(HumanoidPrepare, LegacySpearPlaybackHelperKeepsHoldBaseClip) {
  using Render::Creature::AnimationStateId;
  using Render::Creature::Pipeline::humanoid_bpat_playback_for_anim;

  Render::GL::EntityRendererRegistry renderer_registry;
  Render::GL::register_built_in_entity_renderers(renderer_registry);
  auto const renderer = renderer_registry.get("troops/roman/spearman");
  ASSERT_TRUE(static_cast<bool>(renderer));

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(1);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Spearman;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  ctx.entity = &entity;

  CountingSubmitter sink;
  renderer(ctx, sink);
  ASSERT_GT(sink.rigged_calls, 0);

  auto& registry = Render::Creature::ArchetypeRegistry::instance();
  auto const archetype_id = find_archetype_id("troops/roman/spearman");
  ASSERT_NE(archetype_id, Render::Creature::k_invalid_archetype);

  Render::GL::HumanoidAnimationContext attack_anim{};
  attack_anim.inputs.is_in_hold_mode = true;
  attack_anim.inputs.hold_entry_progress = 1.0F;
  attack_anim.inputs.is_attacking = true;
  attack_anim.inputs.is_melee = true;
  attack_anim.inputs.attack_family = Engine::Core::CombatAttackFamily::Spear;
  attack_anim.inputs.attack_variant = 1U;
  attack_anim.gait.state = Render::GL::HumanoidMotionState::Hold;
  attack_anim.attack_phase = 0.35F;

  auto const attack = humanoid_bpat_playback_for_anim(
      archetype_id, Render::Creature::Bpat::k_species_humanoid, attack_anim);
  ASSERT_TRUE(attack.has_value());
  EXPECT_EQ(
      attack->clip_id,
      registry.resolve_bpat_clip(archetype_id, AnimationStateId::AttackMelee, 0U));
}

TEST(HumanoidPrepare, LegacyArcherPlaybackHelperKeepsHoldBaseClip) {
  using Render::Creature::AnimationStateId;
  using Render::Creature::Pipeline::humanoid_bpat_playback_for_anim;

  Render::GL::EntityRendererRegistry renderer_registry;
  Render::GL::register_built_in_entity_renderers(renderer_registry);
  auto const renderer = renderer_registry.get("troops/roman/archer");
  ASSERT_TRUE(static_cast<bool>(renderer));

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(1);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Archer;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  ctx.entity = &entity;

  CountingSubmitter sink;
  renderer(ctx, sink);
  ASSERT_GT(sink.rigged_calls, 0);

  auto& registry = Render::Creature::ArchetypeRegistry::instance();
  auto const archetype_id = find_archetype_id("troops/roman/archer");
  ASSERT_NE(archetype_id, Render::Creature::k_invalid_archetype);

  Render::GL::HumanoidAnimationContext attack_anim{};
  attack_anim.inputs.is_in_hold_mode = true;
  attack_anim.inputs.hold_entry_progress = 1.0F;
  attack_anim.inputs.is_attacking = true;
  attack_anim.inputs.is_melee = false;
  attack_anim.inputs.attack_family = Engine::Core::CombatAttackFamily::Bow;
  attack_anim.gait.state = Render::GL::HumanoidMotionState::Hold;
  attack_anim.attack_phase = 0.35F;

  auto const attack = humanoid_bpat_playback_for_anim(
      archetype_id, Render::Creature::Bpat::k_species_humanoid, attack_anim);
  ASSERT_TRUE(attack.has_value());
  EXPECT_EQ(
      attack->clip_id,
      registry.resolve_bpat_clip(archetype_id, AnimationStateId::AttackRanged, 0U));
  EXPECT_EQ(attack->clip_id,
            registry.resolve_bpat_clip(archetype_id, AnimationStateId::Hold, 0U));
}

TEST(HumanoidPrepare, AttackRequestsUsePerSoldierVisualPhaseOffsets) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.has_seed_override = true;
  ctx.seed_override = 0x1234ABCDU;

  Engine::Core::StandaloneEntity entity_scratch(42);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(unit, nullptr);
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::Knight;
  unit->render_individuals_per_unit_override = 15;
  ctx.entity = &entity;

  Render::GL::AnimationInputs anim{};
  anim.time = 10.0F;
  anim.is_attacking = true;
  anim.is_melee = true;
  anim.attack_family = Engine::Core::CombatAttackFamily::Sword;
  anim.attack_variant = 0U;
  anim.finisher_attack = false;
  anim.combat_phase = Render::GL::CombatAnimPhase::WindUp;
  anim.combat_phase_progress = 0.5F;

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, anim, test_runtime(0U), prep);

  auto const& requests = prep.bodies.requests();
  ASSERT_GE(requests.size(), 15U);

  std::vector<float> attack_phases;
  std::unordered_set<Render::Creature::AnimationStateId> states;
  for (auto const& request : requests) {
    states.insert(request.state);
    if (request.state == Render::Creature::AnimationStateId::AttackSword) {
      attack_phases.push_back(request.phase);
    }
  }

  EXPECT_EQ(states.size(), 1U);
  EXPECT_EQ(states.count(Render::Creature::AnimationStateId::AttackSword), 1U);
  ASSERT_GE(attack_phases.size(), 2U);
  auto const [min_phase, max_phase] =
      std::minmax_element(attack_phases.begin(), attack_phases.end());
  ASSERT_NE(min_phase, attack_phases.end());
  ASSERT_NE(max_phase, attack_phases.end());
  EXPECT_GT(*max_phase - *min_phase, 0.001F);
}

auto add_walk_motion(Engine::Core::Entity& entity,
                     float speed,
                     float direction_x = 1.0F,
                     float direction_z = 0.0F)
    -> Engine::Core::MotionPresentationComponent* {
  auto* motion = entity.add_component<Engine::Core::MotionPresentationComponent>();
  if (motion == nullptr) {
    return nullptr;
  }
  motion->set_state(Engine::Core::MotionPresentationState::Walk);
  motion->has_velocity = speed > 0.0F;
  motion->has_navigation_intent = true;
  motion->direction_x = direction_x;
  motion->direction_z = direction_z;
  motion->velocity_x = direction_x * speed;
  motion->velocity_z = direction_z * speed;
  motion->speed = speed;
  return motion;
}

TEST(HumanoidPrepare, MovingCombatRecoveryUsesAttackClipInsteadOfWalkClip) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.animation_time = 10.0F;

  Engine::Core::StandaloneEntity entity_scratch(42);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  auto* movement = entity.add_component<Engine::Core::MovementComponent>();
  auto* combat_state = entity.add_component<Engine::Core::CombatStateComponent>();
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(combat_state, nullptr);
  ASSERT_NE(add_walk_motion(entity, unit->speed), nullptr);
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::Knight;
  MovementTestAccess::set_has_target(*movement, true);
  MovementTestAccess::set_target_x(*movement, 4.0F);
  MovementTestAccess::set_target_y(*movement, 0.0F);
  combat_state->animation_state = Engine::Core::CombatAnimationState::Recover;
  combat_state->attack_family = Engine::Core::CombatAttackFamily::Sword;
  combat_state->state_time = 0.18F;
  combat_state->state_duration = Engine::Core::CombatStateComponent::k_recover_duration;
  ctx.entity = &entity;

  auto const anim = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(Render::Creature::is_moving_animation(anim.movement_state));
  EXPECT_TRUE(anim.is_attacking);
  EXPECT_EQ(Render::Creature::resolve_pose_intent(anim),
            Render::Creature::PoseIntent::AttackMelee);
  EXPECT_EQ(anim.combat_phase, Render::GL::CombatAnimPhase::Recover);

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, anim, test_runtime(0U), prep);

  auto const& requests = prep.bodies.requests();
  ASSERT_FALSE(requests.empty());
  EXPECT_EQ(requests.front().state, Render::Creature::AnimationStateId::Walk);
}

TEST(HumanoidPrepare, CombatAdvancePreservesWalkClipWhileClosingDistance) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.animation_time = 10.0F;

  Engine::Core::StandaloneEntity entity_scratch(142);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  auto* movement = entity.add_component<Engine::Core::MovementComponent>();
  auto* combat_state = entity.add_component<Engine::Core::CombatStateComponent>();
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(combat_state, nullptr);
  ASSERT_NE(add_walk_motion(entity, unit->speed), nullptr);
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::Knight;
  MovementTestAccess::set_has_target(*movement, true);
  MovementTestAccess::set_target_x(*movement, 4.0F);
  MovementTestAccess::set_target_y(*movement, 0.0F);
  combat_state->animation_state = Engine::Core::CombatAnimationState::Advance;
  combat_state->attack_family = Engine::Core::CombatAttackFamily::Sword;
  combat_state->state_time = 0.05F;
  combat_state->state_duration = Engine::Core::CombatStateComponent::k_advance_duration;
  ctx.entity = &entity;

  auto const anim = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(Render::Creature::is_moving_animation(anim.movement_state));
  EXPECT_TRUE(anim.is_attacking);
  EXPECT_EQ(Render::Creature::resolve_pose_intent(anim),
            Render::Creature::PoseIntent::Walk);

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, anim, test_runtime(0U), prep);

  auto const& requests = prep.bodies.requests();
  ASSERT_FALSE(requests.empty());
  EXPECT_EQ(requests.front().state, Render::Creature::AnimationStateId::Walk);
}

TEST(HumanoidPrepare, FireballSpecialAttackResolvesExplicitCastIntent) {
  using Render::Creature::AnimationStateId;
  using Render::Creature::ArchetypeRegistry;
  using Render::Creature::Pipeline::resolve_humanoid_animation_selection;
  using Render::Creature::Pipeline::UnitVisualSpec;

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.animation_time = 8.0F;

  Engine::Core::StandaloneEntity entity_scratch(900);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  auto* attack = entity.add_component<Engine::Core::AttackComponent>();
  auto* combat_state = entity.add_component<Engine::Core::CombatStateComponent>();
  auto* special_attack = entity.add_component<Engine::Core::SpecialAttackComponent>();
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(attack, nullptr);
  ASSERT_NE(combat_state, nullptr);
  ASSERT_NE(special_attack, nullptr);
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::GravePriest;
  attack->current_mode = Engine::Core::AttackComponent::CombatMode::Ranged;
  combat_state->animation_state = Engine::Core::CombatAnimationState::Strike;
  combat_state->attack_family = Engine::Core::CombatAttackFamily::Bow;
  combat_state->state_time = 0.15F;
  combat_state->state_duration = Engine::Core::CombatStateComponent::k_strike_duration;
  special_attack->projectile_kind = Game::Systems::ProjectileKind::Fireball;
  special_attack->use_projectile_system = true;
  ctx.entity = &entity;

  auto const anim = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(anim.is_attacking);
  EXPECT_TRUE(anim.is_casting);
  EXPECT_EQ(anim.cast_kind, Render::GL::CastVisualKind::Fireball);
  EXPECT_EQ(Render::Creature::resolve_animation_intent(anim).action,
            Render::Creature::ActionIntent::Cast);
  EXPECT_EQ(Render::Creature::resolve_pose_intent(anim),
            Render::Creature::PoseIntent::Cast);
  UnitVisualSpec spec{};
  spec.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
  spec.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
  spec.archetype_id = ArchetypeRegistry::k_humanoid_base;
  Render::GL::HumanoidAnimationContext anim_ctx{};
  anim_ctx.inputs = anim;
  auto const selection = resolve_humanoid_animation_selection(spec, anim_ctx, 17U);
  EXPECT_EQ(selection.state, AnimationStateId::Cast);
  ASSERT_TRUE(selection.clip_id.has_value());
  EXPECT_EQ(*selection.clip_id,
            ArchetypeRegistry::instance().bpat_clip(spec.archetype_id,
                                                    AnimationStateId::Cast));
}

TEST(HumanoidPrepare, CursedArrowSpecialAttackStaysGenericRangedIntent) {
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.animation_time = 8.0F;

  Engine::Core::StandaloneEntity entity_scratch(901);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  auto* attack = entity.add_component<Engine::Core::AttackComponent>();
  auto* combat_state = entity.add_component<Engine::Core::CombatStateComponent>();
  auto* special_attack = entity.add_component<Engine::Core::SpecialAttackComponent>();
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(attack, nullptr);
  ASSERT_NE(combat_state, nullptr);
  ASSERT_NE(special_attack, nullptr);
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::SkeletonArcher;
  attack->current_mode = Engine::Core::AttackComponent::CombatMode::Ranged;
  combat_state->animation_state = Engine::Core::CombatAnimationState::Strike;
  combat_state->attack_family = Engine::Core::CombatAttackFamily::Bow;
  combat_state->state_time = 0.15F;
  combat_state->state_duration = Engine::Core::CombatStateComponent::k_strike_duration;
  special_attack->projectile_kind = Game::Systems::ProjectileKind::CursedArrow;
  special_attack->use_projectile_system = true;
  ctx.entity = &entity;

  auto const anim = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(anim.is_attacking);
  EXPECT_FALSE(anim.is_casting);
  EXPECT_EQ(Render::Creature::resolve_animation_intent(anim).action,
            Render::Creature::ActionIntent::AttackRanged);
  EXPECT_EQ(Render::Creature::resolve_pose_intent(anim),
            Render::Creature::PoseIntent::AttackRanged);
  EXPECT_EQ(Render::Creature::resolve_pose(anim).animation_state,
            Render::Creature::AnimationStateId::AttackBow);
}

TEST(HumanoidPrepare, MeleeLockKeepsStaleForcedDisplacementOffTheRootLayer) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.animation_time = 6.2F;

  Engine::Core::StandaloneEntity entity_scratch(143);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 2.0F, 12.0F);
  auto* attack = entity.add_component<Engine::Core::AttackComponent>();
  auto* motion = entity.add_component<Engine::Core::MotionPresentationComponent>();
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(attack, nullptr);
  ASSERT_NE(motion, nullptr);
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::Spearman;
  attack->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;
  attack->in_melee_lock = true;
  attack->melee_lock_target_id = 99;
  motion->initialized = true;
  motion->snapshot_valid = true;
  motion->set_state(Engine::Core::MotionPresentationState::Walk);
  motion->has_velocity = true;
  motion->source = Engine::Core::MotionPresentationSource::ForcedDisplacement;
  motion->direction_x = 1.0F;
  motion->direction_z = 0.0F;
  motion->velocity_x = 0.9F;
  motion->velocity_z = 0.0F;
  motion->speed = 0.9F;
  ctx.entity = &entity;

  auto const anim = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(Render::Creature::is_moving_animation(anim.movement_state));
  EXPECT_TRUE(anim.is_attacking);
  EXPECT_TRUE(anim.is_in_melee_lock);
  EXPECT_EQ(Render::Creature::resolve_pose_intent(anim),
            Render::Creature::PoseIntent::AttackSpear);

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, anim, test_runtime(0U), prep);

  auto const& requests = prep.bodies.requests();
  ASSERT_FALSE(requests.empty());
  EXPECT_EQ(requests.front().state, Render::Creature::AnimationStateId::Idle);

  EXPECT_EQ(requests.front().clip_id, Animation::k_humanoid_combat_ready_clip);
  EXPECT_FALSE(requests.front().upper_body_overlay.active());
}

TEST(HumanoidPrepare, CommandedMovementWithoutVelocityStillBuildsStride) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.animation_time = 3.0F;

  Engine::Core::StandaloneEntity entity_scratch(43);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 2.3F, 12.0F);
  auto* movement = entity.add_component<Engine::Core::MovementComponent>();
  auto* transform = entity.add_component<Engine::Core::TransformComponent>();
  auto* motion = entity.add_component<Engine::Core::MotionPresentationComponent>();
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(transform, nullptr);
  ASSERT_NE(motion, nullptr);
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::Knight;
  MovementTestAccess::set_has_target(*movement, true);
  MovementTestAccess::set_target_x(*movement, 6.0F);
  MovementTestAccess::set_target_y(*movement, 0.0F);
  MovementTestAccess::set_vx(*movement, 0.0F);
  MovementTestAccess::set_vz(*movement, 0.0F);
  transform->position = {0.0F, 0.0F, 0.0F};
  transform->rotation = {0.0F, 0.0F, 0.0F};
  transform->scale = {1.0F, 1.0F, 1.0F};
  motion->set_state(Engine::Core::MotionPresentationState::Walk);
  motion->has_navigation_intent = true;
  motion->has_movement_target = true;
  motion->movement_target_x = movement->get_target_x();
  motion->movement_target_z = movement->get_target_y();
  motion->speed = unit->speed;
  ctx.entity = &entity;

  auto const anim = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(Render::Creature::is_moving_animation(anim.movement_state));
  EXPECT_FALSE(anim.is_attacking);

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, anim, test_runtime(0U), prep);

  auto* humanoid_state =
      entity.get_component<Render::Creature::HumanoidAnimationStateComponent>();
  ASSERT_NE(humanoid_state, nullptr);
  ASSERT_FALSE(prep.bodies.requests().empty());
  EXPECT_EQ(prep.bodies.requests().front().state,
            Render::Creature::AnimationStateId::Walk);
  EXPECT_EQ(humanoid_state->locomotion.state, Render::GL::HumanoidMotionState::Walk);
  EXPECT_GT(humanoid_state->locomotion.filtered_speed, 0.0F);
  EXPECT_GT(humanoid_state->locomotion.locomotion_blend, 0.0F);
}

TEST(HumanoidPrepare, ActiveTargetMovementStillTriggersWalkAnimation) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.animation_time = 4.0F;

  Engine::Core::StandaloneEntity entity_scratch(44);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 2.2F, 12.0F);
  auto* movement = entity.add_component<Engine::Core::MovementComponent>();
  auto* transform = entity.add_component<Engine::Core::TransformComponent>();
  auto* motion = entity.add_component<Engine::Core::MotionPresentationComponent>();
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(transform, nullptr);
  ASSERT_NE(motion, nullptr);
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::Knight;
  MovementTestAccess::set_goal_x(*movement, 8.0F);
  MovementTestAccess::set_goal_y(*movement, 0.0F);
  MovementTestAccess::set_has_target(*movement, true);
  MovementTestAccess::set_target_x(*movement, 8.0F);
  MovementTestAccess::set_target_y(*movement, 0.0F);
  transform->position = {0.0F, 0.0F, 0.0F};
  motion->set_state(Engine::Core::MotionPresentationState::Walk);
  motion->has_navigation_intent = true;
  motion->has_movement_target = true;
  motion->movement_target_x = movement->get_goal_x();
  motion->movement_target_z = movement->get_goal_y();
  motion->speed = unit->speed;
  ctx.entity = &entity;

  auto const anim = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(Render::Creature::is_moving_animation(anim.movement_state));

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, anim, test_runtime(0U), prep);
  ASSERT_FALSE(prep.bodies.requests().empty());
  EXPECT_EQ(prep.bodies.requests().front().state,
            Render::Creature::AnimationStateId::Walk);
}

TEST(HumanoidPrepare, WaypointMovementStillTriggersWalkAnimation) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.animation_time = 4.5F;

  Engine::Core::StandaloneEntity entity_scratch(45);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 2.0F, 12.0F);
  auto* movement = entity.add_component<Engine::Core::MovementComponent>();
  auto* transform = entity.add_component<Engine::Core::TransformComponent>();
  auto* motion = entity.add_component<Engine::Core::MotionPresentationComponent>();
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(transform, nullptr);
  ASSERT_NE(motion, nullptr);
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::Knight;
  MovementTestAccess::set_path(*movement, {{5.0F, 0.0F}});
  MovementTestAccess::set_path_index(*movement, 0);
  MovementTestAccess::set_goal_x(*movement, 5.0F);
  MovementTestAccess::set_goal_y(*movement, 0.0F);
  transform->position = {0.0F, 0.0F, 0.0F};
  motion->set_state(Engine::Core::MotionPresentationState::Walk);
  motion->has_navigation_intent = true;
  motion->has_movement_target = true;
  motion->movement_target_x = movement->get_goal_x();
  motion->movement_target_z = movement->get_goal_y();
  motion->speed = unit->speed;
  ctx.entity = &entity;

  auto const anim = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(Render::Creature::is_moving_animation(anim.movement_state));

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, anim, test_runtime(0U), prep);
  ASSERT_FALSE(prep.bodies.requests().empty());
  EXPECT_EQ(prep.bodies.requests().front().state,
            Render::Creature::AnimationStateId::Walk);
}

TEST(HumanoidPrepare, VelocityOnlyMovementStillTriggersWalkAnimation) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.animation_time = 5.0F;

  Engine::Core::StandaloneEntity entity_scratch(46);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 2.0F, 12.0F);
  auto* movement = entity.add_component<Engine::Core::MovementComponent>();
  auto* transform = entity.add_component<Engine::Core::TransformComponent>();
  auto* motion = entity.add_component<Engine::Core::MotionPresentationComponent>();
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(transform, nullptr);
  ASSERT_NE(motion, nullptr);
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::Knight;
  MovementTestAccess::set_vx(*movement, 1.4F);
  MovementTestAccess::set_vz(*movement, 0.2F);
  transform->position = {0.0F, 0.0F, 0.0F};
  motion->set_state(Engine::Core::MotionPresentationState::Walk);
  motion->has_velocity = true;
  motion->velocity_x = movement->get_vx();
  motion->velocity_z = movement->get_vz();
  motion->speed = std::sqrt(movement->get_vx() * movement->get_vx() +
                            movement->get_vz() * movement->get_vz());
  ctx.entity = &entity;

  auto const anim = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(Render::Creature::is_moving_animation(anim.movement_state));

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, anim, test_runtime(0U), prep);
  ASSERT_FALSE(prep.bodies.requests().empty());
  EXPECT_EQ(prep.bodies.requests().front().state,
            Render::Creature::AnimationStateId::Walk);
}

TEST(HumanoidPrepare, ChaseIntentOutOfRangeTriggersWalkAnimation) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.animation_time = 5.5F;

  Engine::Core::World world;
  Engine::Core::StandaloneEntity attacker_scratch(47);
  Engine::Core::Entity& attacker = attacker_scratch.entity();
  auto* unit =
      attacker.add_component<Engine::Core::UnitComponent>(100, 100, 2.4F, 12.0F);
  auto* attack = attacker.add_component<Engine::Core::AttackComponent>();
  auto* attack_target = attacker.add_component<Engine::Core::AttackTargetComponent>();
  auto* transform = attacker.add_component<Engine::Core::TransformComponent>();
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(attack, nullptr);
  ASSERT_NE(attack_target, nullptr);
  ASSERT_NE(transform, nullptr);
  ASSERT_NE(add_walk_motion(attacker, unit->speed), nullptr);
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::Knight;
  attack->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;
  attack->range = 1.5F;
  attack_target->should_chase = true;
  transform->position = {0.0F, 0.0F, 0.0F};

  auto* enemy = world.create_entity();
  ASSERT_NE(enemy, nullptr);
  auto* enemy_unit =
      enemy->add_component<Engine::Core::UnitComponent>(100, 100, 1.8F, 12.0F);
  auto* enemy_transform = enemy->add_component<Engine::Core::TransformComponent>();
  ASSERT_NE(enemy_unit, nullptr);
  ASSERT_NE(enemy_transform, nullptr);
  enemy_unit->owner_id = 2;
  enemy_transform->position = {8.0F, 0.0F, 0.0F};
  attack_target->target_id = enemy->get_id();

  ctx.entity = &attacker;
  ctx.world = &world;

  auto const anim = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(Render::Creature::is_moving_animation(anim.movement_state));
  EXPECT_FALSE(anim.is_attacking);

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, anim, test_runtime(0U), prep);
  ASSERT_FALSE(prep.bodies.requests().empty());
  EXPECT_EQ(prep.bodies.requests().front().state,
            Render::Creature::AnimationStateId::Walk);
}

TEST(HumanoidPrepare, ChaseIntentInRangePreservesAttackInsteadOfWalk) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.animation_time = 6.0F;

  Engine::Core::World world;
  Engine::Core::StandaloneEntity attacker_scratch(48);
  Engine::Core::Entity& attacker = attacker_scratch.entity();
  auto* unit =
      attacker.add_component<Engine::Core::UnitComponent>(100, 100, 2.4F, 12.0F);
  auto* attack = attacker.add_component<Engine::Core::AttackComponent>();
  auto* attack_target = attacker.add_component<Engine::Core::AttackTargetComponent>();
  auto* transform = attacker.add_component<Engine::Core::TransformComponent>();
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(attack, nullptr);
  ASSERT_NE(attack_target, nullptr);
  ASSERT_NE(transform, nullptr);
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::Knight;
  attack->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;
  attack->range = 1.5F;
  attack->time_since_last = 0.05F;
  attack_target->should_chase = true;
  transform->position = {0.0F, 0.0F, 0.0F};

  auto* enemy = world.create_entity();
  ASSERT_NE(enemy, nullptr);
  auto* enemy_unit =
      enemy->add_component<Engine::Core::UnitComponent>(100, 100, 1.8F, 12.0F);
  auto* enemy_transform = enemy->add_component<Engine::Core::TransformComponent>();
  ASSERT_NE(enemy_unit, nullptr);
  ASSERT_NE(enemy_transform, nullptr);
  enemy_unit->owner_id = 2;
  enemy_transform->position = {1.0F, 0.0F, 0.0F};
  attack_target->target_id = enemy->get_id();

  ctx.entity = &attacker;
  ctx.world = &world;

  auto const anim = Render::GL::sample_anim_state(ctx);
  EXPECT_FALSE(Render::Creature::is_moving_animation(anim.movement_state));
  EXPECT_FALSE(anim.is_attacking);

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, anim, test_runtime(0U), prep);
  ASSERT_FALSE(prep.bodies.requests().empty());
  EXPECT_EQ(prep.bodies.requests().front().state,
            Render::Creature::AnimationStateId::Idle);
}

TEST(HumanoidPrepare, ActiveMoveSegmentInRangeStillTriggersWalkAnimation) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.animation_time = 6.1F;

  Engine::Core::World world;
  Engine::Core::StandaloneEntity attacker_scratch(148);
  Engine::Core::Entity& attacker = attacker_scratch.entity();
  auto* unit =
      attacker.add_component<Engine::Core::UnitComponent>(100, 100, 2.4F, 12.0F);
  auto* movement = attacker.add_component<Engine::Core::MovementComponent>();
  auto* attack = attacker.add_component<Engine::Core::AttackComponent>();
  auto* attack_target = attacker.add_component<Engine::Core::AttackTargetComponent>();
  auto* transform = attacker.add_component<Engine::Core::TransformComponent>();
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(attack, nullptr);
  ASSERT_NE(attack_target, nullptr);
  ASSERT_NE(transform, nullptr);
  ASSERT_NE(add_walk_motion(attacker, 0.05F), nullptr);
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::Spearman;
  attack->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;
  attack->range = 1.5F;
  attack_target->should_chase = true;
  MovementTestAccess::set_has_target(*movement, true);
  MovementTestAccess::set_target_x(*movement, 0.6F);
  MovementTestAccess::set_target_y(*movement, 0.0F);
  MovementTestAccess::set_goal_x(*movement, 0.6F);
  MovementTestAccess::set_goal_y(*movement, 0.0F);
  MovementTestAccess::set_vx(*movement, 0.05F);
  MovementTestAccess::set_vz(*movement, 0.0F);
  transform->position = {0.0F, 0.0F, 0.0F};

  auto* enemy = world.create_entity();
  ASSERT_NE(enemy, nullptr);
  auto* enemy_unit =
      enemy->add_component<Engine::Core::UnitComponent>(100, 100, 1.8F, 12.0F);
  auto* enemy_transform = enemy->add_component<Engine::Core::TransformComponent>();
  ASSERT_NE(enemy_unit, nullptr);
  ASSERT_NE(enemy_transform, nullptr);
  enemy_unit->owner_id = 2;
  enemy_transform->position = {1.0F, 0.0F, 0.0F};
  attack_target->target_id = enemy->get_id();

  ctx.entity = &attacker;
  ctx.world = &world;

  auto const anim = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(Render::Creature::is_moving_animation(anim.movement_state));
  EXPECT_FALSE(anim.is_attacking);

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, anim, test_runtime(0U), prep);
  ASSERT_FALSE(prep.bodies.requests().empty());
  EXPECT_EQ(prep.bodies.requests().front().state,
            Render::Creature::AnimationStateId::Walk);
}

TEST(HumanoidPrepare, MotionSnapshotDrivesWalkWithoutMovementComponentIntent) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.animation_time = 6.2F;

  Engine::Core::StandaloneEntity entity_scratch(149);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 2.4F, 12.0F);
  auto* transform = entity.add_component<Engine::Core::TransformComponent>();
  auto* motion = entity.add_component<Engine::Core::MotionPresentationComponent>();
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(transform, nullptr);
  ASSERT_NE(motion, nullptr);
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::Spearman;
  transform->position = {0.0F, 0.0F, 0.0F};
  motion->initialized = true;
  motion->snapshot_valid = true;
  motion->set_state(Engine::Core::MotionPresentationState::Walk);
  motion->has_velocity = true;
  motion->source = Engine::Core::MotionPresentationSource::ForcedDisplacement;
  motion->direction_x = 1.0F;
  motion->direction_z = 0.0F;
  motion->velocity_x = 1.2F;
  motion->velocity_z = 0.0F;
  motion->speed = 1.2F;

  ctx.entity = &entity;

  auto const anim = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(Render::Creature::is_moving_animation(anim.movement_state));
  EXPECT_TRUE(anim.visual_movement.is_authoritative);

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, anim, test_runtime(0U), prep);
  ASSERT_FALSE(prep.bodies.requests().empty());
  EXPECT_EQ(prep.bodies.requests().front().state,
            Render::Creature::AnimationStateId::Walk);
}

TEST(HumanoidPrepare, SampledMovementSnapshotDrivesPreparationAfterIntentClears) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.animation_time = 6.5F;

  Engine::Core::StandaloneEntity entity_scratch(49);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 2.3F, 12.0F);
  auto* movement = entity.add_component<Engine::Core::MovementComponent>();
  auto* transform = entity.add_component<Engine::Core::TransformComponent>();
  auto* motion = add_walk_motion(entity, unit->speed);
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(transform, nullptr);
  ASSERT_NE(motion, nullptr);
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::Knight;
  MovementTestAccess::set_has_target(*movement, true);
  MovementTestAccess::set_target_x(*movement, 7.0F);
  MovementTestAccess::set_target_y(*movement, 0.0F);
  transform->position = {0.0F, 0.0F, 0.0F};
  ctx.entity = &entity;

  auto const anim = Render::GL::sample_anim_state(ctx);
  ASSERT_TRUE(anim.visual_movement.is_authoritative);
  ASSERT_TRUE(Render::Creature::is_moving_animation(anim.movement_state));

  MovementTestAccess::set_has_target(*movement, false);
  MovementTestAccess::set_target_x(*movement, 0.0F);
  MovementTestAccess::set_target_y(*movement, 0.0F);
  MovementTestAccess::set_goal_x(*movement, 0.0F);
  MovementTestAccess::set_goal_y(*movement, 0.0F);

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, anim, test_runtime(0U), prep);

  auto* humanoid_state =
      entity.get_component<Render::Creature::HumanoidAnimationStateComponent>();
  ASSERT_NE(humanoid_state, nullptr);
  ASSERT_FALSE(prep.bodies.requests().empty());
  EXPECT_EQ(prep.bodies.requests().front().state,
            Render::Creature::AnimationStateId::Walk);
  EXPECT_EQ(humanoid_state->locomotion.state, Render::GL::HumanoidMotionState::Walk);
  EXPECT_GT(humanoid_state->locomotion.filtered_speed, 0.0F);
}

TEST(HumanoidPrepare, IdleAnimationOverrideSuppressesLiveMovementIntent) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.animation_time = 7.0F;

  Engine::Core::StandaloneEntity entity_scratch(50);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 2.0F, 12.0F);
  auto* movement = entity.add_component<Engine::Core::MovementComponent>();
  auto* transform = entity.add_component<Engine::Core::TransformComponent>();
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(transform, nullptr);
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::Knight;
  MovementTestAccess::set_has_target(*movement, true);
  MovementTestAccess::set_target_x(*movement, 9.0F);
  MovementTestAccess::set_target_y(*movement, 0.0F);
  transform->position = {0.0F, 0.0F, 0.0F};
  ctx.entity = &entity;

  Render::GL::AnimationInputs override_anim{};
  override_anim.time = ctx.animation_time;
  override_anim.movement_state = Render::Creature::MovementAnimationState::Idle;
  ctx.animation_override = &override_anim;

  auto const anim = Render::GL::sample_anim_state(ctx);
  ASSERT_TRUE(anim.visual_movement.is_authoritative);
  EXPECT_FALSE(Render::Creature::is_moving_animation(anim.movement_state));

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, anim, test_runtime(0U), prep);
  ASSERT_FALSE(prep.bodies.requests().empty());
  EXPECT_EQ(prep.bodies.requests().front().state,
            Render::Creature::AnimationStateId::Idle);
}

TEST(HumanoidPrepare, MovingAnimationOverrideBuildsStrideWithoutLiveIntent) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.animation_time = 7.5F;

  Engine::Core::StandaloneEntity entity_scratch(51);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 2.0F, 12.0F);
  auto* transform = entity.add_component<Engine::Core::TransformComponent>();
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(transform, nullptr);
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::Knight;
  transform->position = {0.0F, 0.0F, 0.0F};
  transform->rotation = {0.0F, 0.0F, 0.0F};
  ctx.entity = &entity;

  Render::GL::AnimationInputs override_anim{};
  override_anim.time = ctx.animation_time;
  override_anim.movement_state = Render::Creature::MovementAnimationState::Walk;
  ctx.animation_override = &override_anim;

  auto const anim = Render::GL::sample_anim_state(ctx);
  ASSERT_TRUE(anim.visual_movement.is_authoritative);
  EXPECT_TRUE(Render::Creature::is_moving_animation(anim.movement_state));
  EXPECT_GT(anim.visual_movement.speed_hint, 0.0F);

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, anim, test_runtime(0U), prep);

  ASSERT_FALSE(prep.bodies.requests().empty());
  EXPECT_EQ(prep.bodies.requests().front().state,
            Render::Creature::AnimationStateId::Walk);
}

TEST(HumanoidPrepare, MovingAnimationOverrideWithoutEntityBuildsAuthoritativeStride) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.animation_time = 8.0F;

  Render::GL::AnimationInputs override_anim{};
  override_anim.time = ctx.animation_time;
  override_anim.movement_state = Render::Creature::MovementAnimationState::Run;
  ctx.animation_override = &override_anim;

  auto const anim = Render::GL::sample_anim_state(ctx);
  ASSERT_TRUE(anim.visual_movement.is_authoritative);
  EXPECT_TRUE(Render::Creature::is_moving_animation(anim.movement_state));
  EXPECT_TRUE(Render::Creature::is_running_animation(anim.movement_state));
  EXPECT_GT(anim.visual_movement.speed_hint, 0.0F);

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, anim, test_runtime(0U), prep);
  ASSERT_FALSE(prep.bodies.requests().empty());
  EXPECT_EQ(prep.bodies.requests().front().state,
            Render::Creature::AnimationStateId::Run);
}

TEST(HumanoidPrepare, CommanderFpvAttacksKeepAuthoredClipPhaseMapping) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;

  Engine::Core::StandaloneEntity entity_scratch(42);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(unit, nullptr);
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::Knight;
  ctx.entity = &entity;

  Render::GL::AnimationInputs anim{};
  anim.time = 10.0F;
  anim.is_attacking = true;
  anim.is_melee = true;
  anim.attack_family = Engine::Core::CombatAttackFamily::Sword;
  anim.attack_variant = 0U;
  anim.combat_phase = Render::GL::CombatAnimPhase::WindUp;
  anim.combat_phase_progress = 0.5F;

  Render::Humanoid::HumanoidPreparation base_prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, anim, test_runtime(0U), base_prep);
  auto const& base_requests = base_prep.bodies.requests();
  ASSERT_FALSE(base_requests.empty());

  auto* commander = entity.add_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(commander, nullptr);
  commander->fpv_controlled = true;
  commander->combo_step = 1;
  anim.attack_variant = 1U;
  anim.finisher_attack = true;

  Render::Humanoid::HumanoidPreparation commander_prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, anim, test_runtime(0U), commander_prep);
  auto const& commander_requests = commander_prep.bodies.requests();
  ASSERT_FALSE(commander_requests.empty());

  EXPECT_FLOAT_EQ(commander_requests.front().phase, base_requests.front().phase);
}

TEST(HumanoidPrepare, StationaryCommanderGuardUsesHoldClipButMovingGuardKeepsWalkClip) {
  using Render::Creature::AnimationStateId;
  using Render::Creature::Pipeline::humanoid_bpat_playback_for_anim;

  Render::GL::EntityRendererRegistry renderer_registry;
  Render::GL::register_built_in_entity_renderers(renderer_registry);
  auto const renderer = renderer_registry.get("troops/roman/swordsman");
  ASSERT_TRUE(static_cast<bool>(renderer));
  if (renderer) {
    Render::GL::DrawContext ctx{};
    ctx.world_view = Render::WorldView::of_active_session();
    ctx.force_single_soldier = true;
    ctx.allow_template_cache = false;

    Engine::Core::StandaloneEntity entity_scratch(1);
    Engine::Core::Entity& entity = entity_scratch.entity();
    auto* unit =
        entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
    ASSERT_NE(unit, nullptr);
    unit->spawn_type = Game::Units::SpawnType::Knight;
    unit->nation_id = Game::Systems::NationID::RomanRepublic;
    ctx.entity = &entity;

    CountingSubmitter sink;
    renderer(ctx, sink);
    ASSERT_GT(sink.rigged_calls, 0);
  }

  auto& registry = Render::Creature::ArchetypeRegistry::instance();
  auto const roman_id = find_archetype_id("troops/roman/swordsman");
  ASSERT_NE(roman_id, Render::Creature::k_invalid_archetype);

  Render::GL::HumanoidAnimationContext guard_idle{};
  guard_idle.inputs.is_guarding = true;
  guard_idle.inputs.guard_pose_progress = 1.0F;
  guard_idle.gait.state = Render::GL::HumanoidMotionState::Idle;
  auto const idle_playback = humanoid_bpat_playback_for_anim(
      roman_id, Render::Creature::Bpat::k_species_humanoid, guard_idle);
  ASSERT_TRUE(idle_playback.has_value());
  EXPECT_EQ(idle_playback->clip_id,
            registry.bpat_clip(roman_id, AnimationStateId::Hold));

  Render::GL::HumanoidAnimationContext guard_walk{};
  guard_walk.inputs.is_guarding = true;
  guard_walk.inputs.guard_pose_progress = 1.0F;
  guard_walk.inputs.movement_state = Render::Creature::MovementAnimationState::Walk;
  guard_walk.gait.state = Render::GL::HumanoidMotionState::Walk;
  guard_walk.gait.cycle_phase = 0.35F;
  auto const walk_playback = humanoid_bpat_playback_for_anim(
      roman_id, Render::Creature::Bpat::k_species_humanoid, guard_walk);
  ASSERT_TRUE(walk_playback.has_value());
  EXPECT_EQ(walk_playback->clip_id,
            registry.bpat_clip(roman_id, AnimationStateId::Walk));
}

TEST(HumanoidPrepare, GuardStanceForSwordAssetKeepsSwordCreatureAsset) {
  class FixedSpecRenderer : public Render::GL::HumanoidRendererBase {
  public:
    explicit FixedSpecRenderer(Render::Creature::Pipeline::UnitVisualSpec spec)
        : Render::GL::HumanoidRendererBase(std::move(spec)) {}
  };

  auto const archetype_id = find_archetype_id("troops/roman/swordsman");
  ASSERT_NE(archetype_id, Render::Creature::k_invalid_archetype);

  Render::Creature::Pipeline::UnitVisualSpec spec{};
  spec.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
  spec.debug_name = "tests/guard_sword_asset_switch";
  spec.archetype_id = archetype_id;
  spec.creature_asset_id = Render::Creature::Pipeline::k_humanoid_sword_asset;
  FixedSpecRenderer const owner(spec);

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Render::GL::AnimationInputs anim{};
  anim.is_guarding = true;
  anim.guard_pose_progress = 1.0F;
  anim.shield_formation_pose = Render::GL::ShieldFormationPose::RomanFront;

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, anim, test_runtime(0U), prep);
  ASSERT_EQ(prep.bodies.requests().size(), 1U);
  EXPECT_EQ(prep.bodies.requests().front().state,
            Render::Creature::AnimationStateId::Hold);
  EXPECT_EQ(prep.bodies.requests().front().creature_asset_id,
            Render::Creature::Pipeline::k_humanoid_sword_asset);
}

TEST(HumanoidPrepare, StationaryGuardUsesTemporaryShieldArchetypeOnlyWhileGuarding) {
  constexpr std::uint8_t k_base_role = 6;
  std::array<Render::Creature::StaticAttachmentSpec, 1> attachments{
      Render::GL::roman_scutum_make_static_attachment(k_base_role),
  };
  auto& registry = Render::Creature::ArchetypeRegistry::instance();
  auto const base_archetype = registry.register_unit_archetype(
      "tests/guard_shield_base",
      Render::Creature::Pipeline::CreatureKind::Humanoid,
      attachments);
  ASSERT_NE(base_archetype, Render::Creature::k_invalid_archetype);

  class FixedSpecRenderer : public Render::GL::HumanoidRendererBase {
  public:
    explicit FixedSpecRenderer(Render::Creature::ArchetypeId archetype)
        : Render::GL::HumanoidRendererBase(make_spec(archetype)) {}

    static auto make_spec(Render::Creature::ArchetypeId archetype)
        -> Render::Creature::Pipeline::UnitVisualSpec {
      Render::Creature::Pipeline::UnitVisualSpec spec{};
      spec.debug_name = "tests/guard_shield_renderer";
      spec.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
      spec.archetype_id = archetype;
      spec.skip_default_facial_hair_archetype = true;
      return spec;
    }
  };

  FixedSpecRenderer const owner(base_archetype);
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Render::GL::AnimationInputs const base_anim{};
  Render::Humanoid::HumanoidPreparation base_prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, base_anim, test_runtime(0U), base_prep);
  ASSERT_EQ(base_prep.bodies.requests().size(), 1U);
  EXPECT_EQ(base_prep.bodies.requests().front().archetype, base_archetype);

  Render::GL::AnimationInputs guard_anim{};
  guard_anim.is_guarding = true;
  guard_anim.guard_pose_progress = 1.0F;
  Render::Humanoid::HumanoidPreparation guard_prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, guard_anim, test_runtime(0U), guard_prep);
  ASSERT_EQ(guard_prep.bodies.requests().size(), 1U);
  auto const guard_archetype = guard_prep.bodies.requests().front().archetype;
  EXPECT_NE(guard_archetype, base_archetype);

  auto const* base_desc = registry.get(base_archetype);
  auto const* guard_desc = registry.get(guard_archetype);
  ASSERT_NE(base_desc, nullptr);
  ASSERT_NE(guard_desc, nullptr);
  ASSERT_EQ(base_desc->bake_attachment_count, 1U);
  ASSERT_EQ(guard_desc->bake_attachment_count, 1U);
  EXPECT_FALSE(Render::Creature::static_attachment_equal(
      base_desc->bake_attachments.front(), guard_desc->bake_attachments.front()));

  Render::GL::AnimationInputs moving_guard_anim = guard_anim;
  moving_guard_anim.movement_state = Render::Creature::MovementAnimationState::Walk;
  Render::Humanoid::HumanoidPreparation moving_guard_prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, moving_guard_anim, test_runtime(0U), moving_guard_prep);
  ASSERT_EQ(moving_guard_prep.bodies.requests().size(), 1U);
  EXPECT_EQ(moving_guard_prep.bodies.requests().front().archetype, base_archetype);
}

TEST(HumanoidPrepare, GuardShieldFormationFacesRomanAndCarthageShieldsOutward) {
  class FixedSpecRenderer : public Render::GL::HumanoidRendererBase {
  public:
    explicit FixedSpecRenderer(Render::Creature::ArchetypeId archetype)
        : Render::GL::HumanoidRendererBase(make_spec(archetype)) {}

    static auto make_spec(Render::Creature::ArchetypeId archetype)
        -> Render::Creature::Pipeline::UnitVisualSpec {
      Render::Creature::Pipeline::UnitVisualSpec spec{};
      spec.debug_name = "tests/guard_shield_facing_renderer";
      spec.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
      spec.archetype_id = archetype;
      spec.skip_default_facial_hair_archetype = true;
      return spec;
    }
  };

  auto& registry = Render::Creature::ArchetypeRegistry::instance();

  auto const guard_forward_for_pose =
      [&](std::string_view debug_name,
          const Render::Creature::StaticAttachmentSpec& attachment,
          Render::GL::ShieldFormationPose pose) {
        std::array<Render::Creature::StaticAttachmentSpec, 1> attachments{attachment};
        auto const base_archetype = registry.register_unit_archetype(
            debug_name,
            Render::Creature::Pipeline::CreatureKind::Humanoid,
            attachments);
        EXPECT_NE(base_archetype, Render::Creature::k_invalid_archetype);
        if (base_archetype == Render::Creature::k_invalid_archetype) {
          return QVector3D{};
        }

        FixedSpecRenderer const owner(base_archetype);
        Render::GL::DrawContext ctx{};
        ctx.world_view = Render::WorldView::of_active_session();
        ctx.force_single_soldier = true;
        ctx.allow_template_cache = false;

        Render::GL::AnimationInputs anim{};
        anim.is_guarding = true;
        anim.guard_pose_progress = 1.0F;
        anim.shield_formation_pose = pose;

        Render::Humanoid::HumanoidPreparation prep;
        Render::Humanoid::prepare_humanoid_instances(
            owner, ctx, anim, test_runtime(0U), prep);
        EXPECT_EQ(prep.bodies.requests().size(), 1U);
        if (prep.bodies.requests().size() != 1U) {
          return QVector3D{};
        }

        auto const guard_archetype = prep.bodies.requests().front().archetype;
        auto const* guard_desc = registry.get(guard_archetype);
        EXPECT_NE(guard_desc, nullptr);
        if (guard_desc == nullptr) {
          return QVector3D{};
        }
        EXPECT_EQ(guard_desc->bake_attachment_count, 1U);
        if (guard_desc->bake_attachment_count != 1U) {
          return QVector3D{};
        }
        return guard_desc->bake_attachments.front()
            .local_offset.column(2)
            .toVector3D()
            .normalized();
      };

  QVector3D const roman_forward =
      guard_forward_for_pose("tests/guard_shield_facing_roman",
                             Render::GL::roman_scutum_make_static_attachment(0U),
                             Render::GL::ShieldFormationPose::RomanFront);
  EXPECT_GT(QVector3D::dotProduct(roman_forward, QVector3D(0.0F, 0.0F, 1.0F)), 0.6F);

  QVector3D const carthage_forward =
      guard_forward_for_pose("tests/guard_shield_facing_carthage",
                             Render::GL::carthage_shield_make_static_attachment(
                                 Render::GL::CarthageShieldConfig{}, 0U),
                             Render::GL::ShieldFormationPose::CarthageFront);
  EXPECT_GT(QVector3D::dotProduct(carthage_forward, QVector3D(0.0F, 0.0F, 1.0F)), 0.4F);
}

TEST(HumanoidPrepare, CommanderGuardReusesNationShieldFormationArchetype) {
  class FixedSpecRenderer : public Render::GL::HumanoidRendererBase {
  public:
    explicit FixedSpecRenderer(Render::Creature::ArchetypeId archetype)
        : Render::GL::HumanoidRendererBase(make_spec(archetype)) {}

    static auto make_spec(Render::Creature::ArchetypeId archetype)
        -> Render::Creature::Pipeline::UnitVisualSpec {
      Render::Creature::Pipeline::UnitVisualSpec spec{};
      spec.debug_name = "tests/commander_guard_shared_pose_renderer";
      spec.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
      spec.archetype_id = archetype;
      spec.skip_default_facial_hair_archetype = true;
      return spec;
    }
  };

  auto& registry = Render::Creature::ArchetypeRegistry::instance();

  auto const expect_commander_pose =
      [&](std::string_view debug_name,
          Game::Systems::NationID nation_id,
          const Render::Creature::StaticAttachmentSpec& attachment,
          Render::GL::ShieldFormationPose expected_pose) {
        std::array<Render::Creature::StaticAttachmentSpec, 1> attachments{attachment};
        auto const base_archetype = registry.register_unit_archetype(
            debug_name,
            Render::Creature::Pipeline::CreatureKind::Humanoid,
            attachments);
        ASSERT_NE(base_archetype, Render::Creature::k_invalid_archetype);

        FixedSpecRenderer const owner(base_archetype);

        Render::GL::DrawContext commander_ctx{};
        commander_ctx.force_single_soldier = true;
        commander_ctx.allow_template_cache = false;

        Engine::Core::StandaloneEntity commander_entity_scratch(1);
        Engine::Core::Entity& commander_entity = commander_entity_scratch.entity();
        auto* unit = commander_entity.add_component<Engine::Core::UnitComponent>(
            100, 100, 1.0F, 12.0F);
        ASSERT_NE(unit, nullptr);
        unit->spawn_type = Game::Units::SpawnType::RomanFieldCommander;
        unit->nation_id = nation_id;
        ASSERT_NE(commander_entity.add_component<Engine::Core::CommanderComponent>(),
                  nullptr);
        commander_ctx.entity = &commander_entity;

        Render::GL::AnimationInputs commander_anim{};
        commander_anim.is_guarding = true;
        commander_anim.guard_pose_progress = 1.0F;

        Render::Humanoid::HumanoidPreparation commander_prep;
        Render::Humanoid::prepare_humanoid_instances(
            owner, commander_ctx, commander_anim, test_runtime(0U), commander_prep);
        ASSERT_EQ(commander_prep.bodies.requests().size(), 1U);

        Render::GL::DrawContext explicit_ctx{};
        explicit_ctx.force_single_soldier = true;
        explicit_ctx.allow_template_cache = false;

        Render::GL::AnimationInputs explicit_anim = commander_anim;
        explicit_anim.shield_formation_pose = expected_pose;

        Render::Humanoid::HumanoidPreparation explicit_prep;
        Render::Humanoid::prepare_humanoid_instances(
            owner, explicit_ctx, explicit_anim, test_runtime(0U), explicit_prep);
        ASSERT_EQ(explicit_prep.bodies.requests().size(), 1U);

        EXPECT_EQ(commander_prep.bodies.requests().front().archetype,
                  explicit_prep.bodies.requests().front().archetype);
      };

  expect_commander_pose("tests/commander_guard_shared_pose_roman",
                        Game::Systems::NationID::RomanRepublic,
                        Render::GL::roman_scutum_make_static_attachment(0U),
                        Render::GL::ShieldFormationPose::RomanFront);
  expect_commander_pose("tests/commander_guard_shared_pose_carthage",
                        Game::Systems::NationID::Carthage,
                        Render::GL::carthage_shield_make_static_attachment(
                            Render::GL::CarthageShieldConfig{}, 0U),
                        Render::GL::ShieldFormationPose::CarthageFront);
}

TEST(HumanoidPrepare, ShieldBearingGuardFallsBackToNationFrontFormationPose) {
  class FixedSpecRenderer : public Render::GL::HumanoidRendererBase {
  public:
    explicit FixedSpecRenderer(Render::Creature::ArchetypeId archetype)
        : Render::GL::HumanoidRendererBase(make_spec(archetype)) {}

    static auto make_spec(Render::Creature::ArchetypeId archetype)
        -> Render::Creature::Pipeline::UnitVisualSpec {
      Render::Creature::Pipeline::UnitVisualSpec spec{};
      spec.debug_name = "tests/shield_guard_fallback_renderer";
      spec.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
      spec.archetype_id = archetype;
      spec.skip_default_facial_hair_archetype = true;
      return spec;
    }
  };

  auto& registry = Render::Creature::ArchetypeRegistry::instance();

  auto const expect_guard_fallback =
      [&](std::string_view debug_name,
          Game::Systems::NationID nation_id,
          const Render::Creature::StaticAttachmentSpec& attachment,
          Render::GL::ShieldFormationPose expected_pose) {
        std::array<Render::Creature::StaticAttachmentSpec, 1> attachments{attachment};
        auto const base_archetype = registry.register_unit_archetype(
            debug_name,
            Render::Creature::Pipeline::CreatureKind::Humanoid,
            attachments);
        ASSERT_NE(base_archetype, Render::Creature::k_invalid_archetype);

        FixedSpecRenderer const owner(base_archetype);

        Render::GL::DrawContext guard_ctx{};
        guard_ctx.force_single_soldier = true;
        guard_ctx.allow_template_cache = false;

        Engine::Core::StandaloneEntity entity_scratch(1);
        Engine::Core::Entity& entity = entity_scratch.entity();
        auto* unit =
            entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
        ASSERT_NE(unit, nullptr);
        unit->spawn_type = Game::Units::SpawnType::Knight;
        unit->nation_id = nation_id;
        guard_ctx.entity = &entity;

        Render::GL::AnimationInputs guard_anim{};
        guard_anim.is_guarding = true;
        guard_anim.guard_pose_progress = 1.0F;

        Render::Humanoid::HumanoidPreparation guard_prep;
        Render::Humanoid::prepare_humanoid_instances(
            owner, guard_ctx, guard_anim, test_runtime(0U), guard_prep);
        ASSERT_EQ(guard_prep.bodies.requests().size(), 1U);

        Render::GL::DrawContext explicit_ctx{};
        explicit_ctx.force_single_soldier = true;
        explicit_ctx.allow_template_cache = false;

        Render::GL::AnimationInputs explicit_anim = guard_anim;
        explicit_anim.shield_formation_pose = expected_pose;

        Render::Humanoid::HumanoidPreparation explicit_prep;
        Render::Humanoid::prepare_humanoid_instances(
            owner, explicit_ctx, explicit_anim, test_runtime(0U), explicit_prep);
        ASSERT_EQ(explicit_prep.bodies.requests().size(), 1U);

        EXPECT_EQ(guard_prep.bodies.requests().front().archetype,
                  explicit_prep.bodies.requests().front().archetype);
      };

  expect_guard_fallback("tests/shield_guard_fallback_roman",
                        Game::Systems::NationID::RomanRepublic,
                        Render::GL::roman_scutum_make_static_attachment(0U),
                        Render::GL::ShieldFormationPose::RomanFront);
  expect_guard_fallback("tests/shield_guard_fallback_carthage",
                        Game::Systems::NationID::Carthage,
                        Render::GL::carthage_shield_make_static_attachment(
                            Render::GL::CarthageShieldConfig{}, 0U),
                        Render::GL::ShieldFormationPose::CarthageFront);
}

TEST(HumanoidPrepare, FormationSamplingBlendsRomanInfantryShieldGuardInAndOut) {
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(81);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Knight;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  auto* persistent =
      entity.add_component<Render::Creature::HumanoidAnimationStateComponent>();
  ASSERT_NE(persistent, nullptr);
  auto* formation_mode = entity.add_component<Engine::Core::FormationModeComponent>();
  ASSERT_NE(formation_mode, nullptr);
  formation_mode->active = true;
  ctx.entity = &entity;

  ctx.animation_time = 1.0F;
  auto const enter_start = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(enter_start.is_guarding);
  EXPECT_FALSE(enter_start.is_exiting_guard);
  EXPECT_FLOAT_EQ(enter_start.guard_pose_progress, 0.0F);

  ctx.animation_time = 1.75F;
  auto const enter_mid = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(enter_mid.is_guarding);
  EXPECT_FALSE(enter_mid.is_exiting_guard);
  EXPECT_NEAR(enter_mid.guard_pose_progress,
              0.75F / Engine::Core::Defaults::k_guard_enter_duration,
              1.0e-3F);

  ctx.animation_time = 3.25F;
  auto const fully_braced = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(fully_braced.is_guarding);
  EXPECT_FALSE(fully_braced.is_exiting_guard);
  EXPECT_FLOAT_EQ(fully_braced.guard_pose_progress, 1.0F);

  formation_mode->active = false;
  ctx.animation_time = 4.25F;
  auto const exit_mid = Render::GL::sample_anim_state(ctx);
  EXPECT_FALSE(exit_mid.is_guarding);
  EXPECT_TRUE(exit_mid.is_exiting_guard);
  EXPECT_NEAR(exit_mid.guard_pose_progress,
              1.0F - (1.0F / Engine::Core::Defaults::k_guard_exit_duration),
              1.0e-3F);
}

TEST(HumanoidPrepare, HoldSamplingBlendsKneelInAndOutFromVisualState) {
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(82);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* persistent =
      entity.add_component<Render::Creature::HumanoidAnimationStateComponent>();
  ASSERT_NE(persistent, nullptr);
  auto* hold_mode = entity.add_component<Engine::Core::HoldModeComponent>();
  ASSERT_NE(hold_mode, nullptr);
  hold_mode->active = true;
  hold_mode->kneel_entry_progress = 1.0F;
  hold_mode->kneel_duration = 1.5F;
  hold_mode->stand_up_duration = 2.0F;
  ctx.entity = &entity;

  ctx.animation_time = 1.0F;
  auto const enter_start = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(enter_start.is_in_hold_mode);
  EXPECT_FALSE(enter_start.is_exiting_hold);
  EXPECT_FLOAT_EQ(enter_start.hold_entry_progress, 0.0F);

  ctx.animation_time = 1.75F;
  auto const enter_mid = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(enter_mid.is_in_hold_mode);
  EXPECT_FALSE(enter_mid.is_exiting_hold);
  EXPECT_NEAR(enter_mid.hold_entry_progress, 0.5F, 1.0e-3F);

  ctx.animation_time = 2.50F;
  auto const fully_knelt = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(fully_knelt.is_in_hold_mode);
  EXPECT_FALSE(fully_knelt.is_exiting_hold);
  EXPECT_FLOAT_EQ(fully_knelt.hold_entry_progress, 1.0F);

  hold_mode->begin_exit();
  ctx.animation_time = 3.50F;
  auto const exit_mid = Render::GL::sample_anim_state(ctx);
  EXPECT_FALSE(exit_mid.is_in_hold_mode);
  EXPECT_TRUE(exit_mid.is_exiting_hold);
  EXPECT_NEAR(exit_mid.hold_exit_progress, 0.5F, 1.0e-3F);
  EXPECT_NEAR(Render::GL::hold_transition_amount(exit_mid), 0.5F, 1.0e-3F);
}

TEST(HumanoidPrepare, HeldSpearAndBowAttacksKeepPersistentKneelingState) {
  std::array const families{Engine::Core::CombatAttackFamily::Spear,
                            Engine::Core::CombatAttackFamily::Bow};
  std::array const spawn_types{Game::Units::SpawnType::Spearman,
                               Game::Units::SpawnType::Archer};
  for (std::size_t index = 0; index < families.size(); ++index) {
    Render::GL::DrawContext ctx{};
    ctx.world_view = Render::WorldView::of_active_session();
    ctx.allow_template_cache = false;

    Engine::Core::StandaloneEntity entity_scratch(820 + index);
    Engine::Core::Entity& entity = entity_scratch.entity();
    auto* unit = entity.add_component<Engine::Core::UnitComponent>();
    ASSERT_NE(unit, nullptr);
    unit->spawn_type = spawn_types[index];
    ASSERT_NE(entity.add_component<Render::Creature::HumanoidAnimationStateComponent>(),
              nullptr);
    auto* hold_mode = entity.add_component<Engine::Core::HoldModeComponent>();
    ASSERT_NE(hold_mode, nullptr);
    hold_mode->active = true;
    hold_mode->kneel_entry_progress = 1.0F;
    hold_mode->kneel_duration = 1.0F;
    auto* combat_state = entity.add_component<Engine::Core::CombatStateComponent>();
    ASSERT_NE(combat_state, nullptr);
    ctx.entity = &entity;

    ctx.animation_time = 1.0F;
    (void)Render::GL::sample_anim_state(ctx);
    ctx.animation_time = 2.0F;
    auto const fully_knelt = Render::GL::sample_anim_state(ctx);
    ASSERT_TRUE(fully_knelt.is_in_hold_mode);
    ASSERT_FLOAT_EQ(fully_knelt.hold_entry_progress, 1.0F);

    combat_state->animation_state = Engine::Core::CombatAnimationState::Strike;
    combat_state->attack_family = families[index];
    combat_state->state_time = 0.05F;
    combat_state->state_duration =
        Engine::Core::CombatStateComponent::k_strike_duration;
    ctx.animation_time = 2.25F;
    auto const attacking = Render::GL::sample_anim_state(ctx);

    EXPECT_TRUE(attacking.is_attacking);
    EXPECT_EQ(attacking.attack_family, families[index]);
    EXPECT_TRUE(attacking.is_in_hold_mode);
    EXPECT_FALSE(attacking.is_exiting_hold);
    EXPECT_FLOAT_EQ(attacking.hold_entry_progress, 1.0F);
  }
}

TEST(HumanoidPrepare, FormationGuardDoesNotBuildHiddenKneelWhileAttacking) {
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(83);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Knight;
  unit->nation_id = Game::Systems::NationID::Carthage;
  ASSERT_NE(entity.add_component<Render::Creature::HumanoidAnimationStateComponent>(),
            nullptr);
  auto* formation_mode = entity.add_component<Engine::Core::FormationModeComponent>();
  ASSERT_NE(formation_mode, nullptr);
  formation_mode->active = true;
  auto* combat_state = entity.add_component<Engine::Core::CombatStateComponent>();
  ASSERT_NE(combat_state, nullptr);
  combat_state->animation_state = Engine::Core::CombatAnimationState::Strike;
  combat_state->attack_family = Engine::Core::CombatAttackFamily::Sword;
  combat_state->state_time = 0.05F;
  combat_state->state_duration = Engine::Core::CombatStateComponent::k_strike_duration;
  ctx.entity = &entity;

  ctx.animation_time = 1.0F;
  auto const attack_start = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(attack_start.is_attacking);
  EXPECT_FALSE(attack_start.is_guarding);
  EXPECT_FLOAT_EQ(attack_start.guard_pose_progress, 0.0F);

  ctx.animation_time = 2.5F;
  auto const attack_later = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(attack_later.is_attacking);
  EXPECT_FALSE(attack_later.is_guarding);
  EXPECT_FLOAT_EQ(attack_later.guard_pose_progress, 0.0F);

  combat_state->animation_state = Engine::Core::CombatAnimationState::Idle;
  ctx.animation_time = 2.75F;
  auto const guard_enter = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(guard_enter.is_guarding);
  EXPECT_FALSE(guard_enter.is_exiting_guard);
  EXPECT_GT(guard_enter.guard_pose_progress, 0.0F);
  EXPECT_LT(guard_enter.guard_pose_progress, 1.0F);
}

TEST(HumanoidPrepare, ExplicitSpearBraceFeedsGuardAnimationState) {
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(830);
  Engine::Core::Entity& entity = entity_scratch.entity();
  ASSERT_NE(entity.add_component<Render::Creature::HumanoidAnimationStateComponent>(),
            nullptr);
  auto* brace = entity.add_component<Engine::Core::SpearBraceComponent>();
  ASSERT_NE(brace, nullptr);
  brace->requested = true;
  brace->pose_progress = 0.5F;
  ctx.entity = &entity;

  ctx.animation_time = 1.0F;
  auto const enter_start = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(enter_start.is_guarding);
  EXPECT_FALSE(enter_start.is_exiting_guard);

  ctx.animation_time = 1.75F;
  auto const enter_mid = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(enter_mid.is_guarding);
  EXPECT_GT(enter_mid.guard_pose_progress, 0.0F);

  brace->requested = false;
  brace->active = false;
  ctx.animation_time = 2.0F;
  auto const exit = Render::GL::sample_anim_state(ctx);
  EXPECT_FALSE(exit.is_guarding);
  EXPECT_TRUE(exit.is_exiting_guard);
}

TEST(HumanoidPrepare, MeleeAttackSmoothlyExitsExistingHoldKneel) {
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(84);
  Engine::Core::Entity& entity = entity_scratch.entity();
  ASSERT_NE(entity.add_component<Render::Creature::HumanoidAnimationStateComponent>(),
            nullptr);
  auto* hold_mode = entity.add_component<Engine::Core::HoldModeComponent>();
  ASSERT_NE(hold_mode, nullptr);
  hold_mode->active = true;
  hold_mode->kneel_entry_progress = 1.0F;
  hold_mode->kneel_duration = 1.0F;
  hold_mode->stand_up_duration = 2.0F;
  auto* combat_state = entity.add_component<Engine::Core::CombatStateComponent>();
  ASSERT_NE(combat_state, nullptr);
  ctx.entity = &entity;

  ctx.animation_time = 1.0F;
  (void)Render::GL::sample_anim_state(ctx);
  ctx.animation_time = 2.0F;
  auto const fully_knelt = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(fully_knelt.is_in_hold_mode);
  EXPECT_FLOAT_EQ(Render::GL::hold_transition_amount(fully_knelt), 1.0F);

  combat_state->animation_state = Engine::Core::CombatAnimationState::Strike;
  combat_state->attack_family = Engine::Core::CombatAttackFamily::Sword;
  combat_state->state_time = 0.05F;
  combat_state->state_duration = Engine::Core::CombatStateComponent::k_strike_duration;
  ctx.animation_time = 2.5F;
  auto const melee_exit = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(melee_exit.is_attacking);
  EXPECT_FALSE(melee_exit.is_in_hold_mode);
  EXPECT_TRUE(melee_exit.is_exiting_hold);

  EXPECT_NEAR(Render::GL::hold_transition_amount(melee_exit), 0.84375F, 1.0e-3F);
}

TEST(HumanoidPrepare, FormationUsesRomanTopInteriorAndDistinctCarthageFrontShields) {
  constexpr std::uint8_t k_base_role = 6;
  std::array<Render::Creature::StaticAttachmentSpec, 1> attachments{
      Render::GL::roman_scutum_make_static_attachment(k_base_role),
  };
  auto& registry = Render::Creature::ArchetypeRegistry::instance();
  auto const base_archetype = registry.register_unit_archetype(
      "tests/formation_guard_shield_base",
      Render::Creature::Pipeline::CreatureKind::Humanoid,
      attachments);
  ASSERT_NE(base_archetype, Render::Creature::k_invalid_archetype);

  class FixedSpecRenderer : public Render::GL::HumanoidRendererBase {
  public:
    explicit FixedSpecRenderer(Render::Creature::ArchetypeId archetype)
        : Render::GL::HumanoidRendererBase(make_spec(archetype)) {}

    static auto make_spec(Render::Creature::ArchetypeId archetype)
        -> Render::Creature::Pipeline::UnitVisualSpec {
      Render::Creature::Pipeline::UnitVisualSpec spec{};
      spec.debug_name = "tests/formation_guard_shield_renderer";
      spec.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
      spec.archetype_id = archetype;
      spec.skip_default_facial_hair_archetype = true;
      return spec;
    }
  };

  auto collect_archetypes = [&](Game::Systems::NationID nation_id) {
    FixedSpecRenderer const owner(base_archetype);
    Render::GL::DrawContext ctx{};
    ctx.world_view = Render::WorldView::of_active_session();
    ctx.force_single_soldier = false;
    ctx.allow_template_cache = false;

    Engine::Core::StandaloneEntity entity_scratch(
        nation_id == Game::Systems::NationID::RomanRepublic ? 101 : 102);
    Engine::Core::Entity& entity = entity_scratch.entity();
    auto* unit =
        entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
    EXPECT_NE(unit, nullptr);
    if (unit == nullptr) {
      return std::unordered_set<Render::Creature::ArchetypeId>{};
    }
    unit->spawn_type = Game::Units::SpawnType::Knight;
    unit->nation_id = nation_id;
    unit->render_individuals_per_unit_override = 15;
    auto* formation_mode = entity.add_component<Engine::Core::FormationModeComponent>();
    EXPECT_NE(formation_mode, nullptr);
    if (formation_mode == nullptr) {
      return std::unordered_set<Render::Creature::ArchetypeId>{};
    }
    formation_mode->active = true;

    auto* layout = entity.add_component<Engine::Core::UnitLayoutStateComponent>();
    EXPECT_NE(layout, nullptr);
    if (layout == nullptr) {
      return std::unordered_set<Render::Creature::ArchetypeId>{};
    }
    layout->state =
        static_cast<std::uint8_t>(Game::Formation::UnitLayoutState::Defensive);
    layout->phase = static_cast<std::uint8_t>(Game::Formation::LayoutPhase::Formed);
    layout->transition_progress = 1.0F;
    ctx.entity = &entity;

    Render::GL::AnimationInputs guard_anim{};
    guard_anim.is_guarding = true;
    guard_anim.guard_pose_progress = 1.0F;
    Render::Humanoid::HumanoidPreparation prep;
    Render::Humanoid::prepare_humanoid_instances(
        owner, ctx, guard_anim, test_runtime(0U), prep);

    std::unordered_set<Render::Creature::ArchetypeId> archetypes;
    for (const auto& req : prep.bodies.requests()) {
      archetypes.insert(req.archetype);
    }
    return archetypes;
  };

  auto const roman_archetypes =
      collect_archetypes(Game::Systems::NationID::RomanRepublic);
  auto const carthage_archetypes =
      collect_archetypes(Game::Systems::NationID::Carthage);

  EXPECT_EQ(roman_archetypes.size(), 5U)
      << "a locked testudo faces shields front, left, right, rear and overhead";
  EXPECT_EQ(carthage_archetypes.size(), 3U)
      << "the shield wall faces front and to both flanks, and never roofs over";
  EXPECT_EQ(roman_archetypes.count(base_archetype), 0U);
  EXPECT_EQ(carthage_archetypes.count(base_archetype), 0U);
  for (auto const archetype : carthage_archetypes) {
    EXPECT_EQ(roman_archetypes.count(archetype), 0U)
        << "the two doctrines must not share a shield archetype";
  }
}

TEST(AnimationCoreGuardManifest, TheTestudoOwnsAContiguousBakedClipBlock) {

  EXPECT_EQ(Animation::k_humanoid_testudo_front_clip,
            Animation::k_humanoid_testudo_first_clip);
  EXPECT_EQ(Animation::k_humanoid_testudo_top_clip,
            Animation::k_humanoid_testudo_first_clip + 1U);
  EXPECT_EQ(Animation::k_humanoid_testudo_left_clip,
            Animation::k_humanoid_testudo_first_clip + 2U);
  EXPECT_EQ(Animation::k_humanoid_testudo_right_clip,
            Animation::k_humanoid_testudo_first_clip + 3U);
  EXPECT_EQ(Animation::k_humanoid_testudo_rear_clip,
            Animation::k_humanoid_testudo_first_clip + 4U);
  EXPECT_EQ(Animation::k_humanoid_testudo_clip_count, 5U);
  EXPECT_LT(Animation::k_humanoid_testudo_rear_clip, Animation::k_humanoid_clip_count)
      << "the testudo block must fit inside the baked clip table";
}

TEST(AnimationCoreGuardManifest, CarthageShieldWallOwnsDistinctBakedPoses) {
  EXPECT_EQ(Animation::k_humanoid_carthage_shield_wall_front_clip,
            Animation::k_humanoid_carthage_shield_wall_first_clip);
  EXPECT_EQ(Animation::k_humanoid_carthage_shield_wall_left_clip,
            Animation::k_humanoid_carthage_shield_wall_first_clip + 1U);
  EXPECT_EQ(Animation::k_humanoid_carthage_shield_wall_right_clip,
            Animation::k_humanoid_carthage_shield_wall_first_clip + 2U);
  EXPECT_EQ(Animation::k_humanoid_carthage_shield_wall_clip_count, 3U);
  EXPECT_LT(Animation::k_humanoid_carthage_shield_wall_right_clip,
            Animation::k_humanoid_clip_count);
}

TEST(AnimationCoreGuardManifest, EveryTestudoSlotGetsTheShieldFacingItsPositionNeeds) {
  using Animation::FormationShieldFacing;
  using Animation::resolve_formation_shield_facing;

  EXPECT_EQ(resolve_formation_shield_facing(3, 2, 4, 5), FormationShieldFacing::Front);
  EXPECT_EQ(resolve_formation_shield_facing(0, 2, 4, 5), FormationShieldFacing::Rear);
  EXPECT_EQ(resolve_formation_shield_facing(1, 0, 4, 5), FormationShieldFacing::Left);
  EXPECT_EQ(resolve_formation_shield_facing(2, 4, 4, 5), FormationShieldFacing::Right);
  EXPECT_EQ(resolve_formation_shield_facing(1, 2, 4, 5), FormationShieldFacing::Top);

  EXPECT_EQ(resolve_formation_shield_facing(0, 0, 1, 1), FormationShieldFacing::Front)
      << "a lone man presents to the front rather than roofing over himself";
}

TEST(AnimationCoreGuardManifest, TheShellFacesShieldsOutwardOnEverySide) {
  auto const pose_at = [](int row, int col, Animation::GuardShieldFamily family) {
    return Animation::resolve_humanoid_guard_shield_pose({
        .has_left_hand_shield = true,
        .infantry_formation_unit = true,
        .formation_active = true,
        .defensive_layout_locked = true,
        .shield_family = family,
        .row = row,
        .col = col,
        .rows = 4,
        .cols = 5,
    });
  };

  using Pose = Animation::ShieldFormationPose;
  auto const roman = Animation::GuardShieldFamily::Roman;
  EXPECT_EQ(pose_at(3, 2, roman), Pose::RomanFront);
  EXPECT_EQ(pose_at(1, 2, roman), Pose::RomanTop);
  EXPECT_EQ(pose_at(1, 0, roman), Pose::RomanLeft);
  EXPECT_EQ(pose_at(1, 4, roman), Pose::RomanRight);
  EXPECT_EQ(pose_at(0, 2, roman), Pose::RomanRear);

  auto const carthage = Animation::GuardShieldFamily::Carthage;
  EXPECT_EQ(pose_at(3, 2, carthage), Pose::CarthageFront);
  EXPECT_EQ(pose_at(1, 0, carthage), Pose::CarthageLeft);
  EXPECT_EQ(pose_at(1, 4, carthage), Pose::CarthageRight);
  EXPECT_EQ(pose_at(1, 2, carthage), Pose::CarthageFront)
      << "a shield wall has no roof; interior men keep facing forward";
  EXPECT_EQ(pose_at(0, 2, carthage), Pose::CarthageFront)
      << "a shield wall does not cover its own back";
}

TEST(AnimationCoreGuardManifest, ShieldFacingsTurnTheShieldToDifferentQuarters) {
  using Pose = Animation::ShieldFormationPose;
  auto const yaw = [](Pose pose) {
    return Animation::guard_shield_attachment_profile(pose).yaw_degrees;
  };
  auto const pitch = [](Pose pose) {
    return Animation::guard_shield_attachment_profile(pose).pitch_degrees;
  };

  EXPECT_NE(yaw(Pose::RomanLeft), yaw(Pose::RomanFront));
  EXPECT_NE(yaw(Pose::RomanRight), yaw(Pose::RomanFront));
  EXPECT_NE(yaw(Pose::RomanRight), yaw(Pose::RomanLeft));
  EXPECT_NE(yaw(Pose::RomanRear), yaw(Pose::RomanFront));
  EXPECT_LT(pitch(Pose::RomanTop), pitch(Pose::RomanFront))
      << "only the roof lays the shield toward horizontal";
  EXPECT_NE(yaw(Pose::CarthageLeft), yaw(Pose::CarthageRight));
}

TEST(HumanoidPrepare, RomanFormationFrontShieldMatchesDefaultRomanGuardFallback) {
  constexpr std::uint8_t k_base_role = 6;
  std::array<Render::Creature::StaticAttachmentSpec, 1> attachments{
      Render::GL::roman_scutum_make_static_attachment(k_base_role),
  };
  auto& registry = Render::Creature::ArchetypeRegistry::instance();
  auto const base_archetype = registry.register_unit_archetype(
      "tests/roman_formation_guard_face",
      Render::Creature::Pipeline::CreatureKind::Humanoid,
      attachments);
  ASSERT_NE(base_archetype, Render::Creature::k_invalid_archetype);

  class FixedSpecRenderer : public Render::GL::HumanoidRendererBase {
  public:
    explicit FixedSpecRenderer(Render::Creature::ArchetypeId archetype)
        : Render::GL::HumanoidRendererBase(make_spec(archetype)) {}

    static auto make_spec(Render::Creature::ArchetypeId archetype)
        -> Render::Creature::Pipeline::UnitVisualSpec {
      Render::Creature::Pipeline::UnitVisualSpec spec{};
      spec.debug_name = "tests/roman_formation_guard_face_renderer";
      spec.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
      spec.archetype_id = archetype;
      spec.skip_default_facial_hair_archetype = true;
      return spec;
    }
  };

  auto build_guard_attachment =
      [&](bool formation_active) -> const Render::Creature::StaticAttachmentSpec* {
    FixedSpecRenderer const owner(base_archetype);
    Render::GL::DrawContext ctx{};
    ctx.world_view = Render::WorldView::of_active_session();
    ctx.force_single_soldier = true;
    ctx.allow_template_cache = false;

    Engine::Core::StandaloneEntity entity_scratch(111 + (formation_active ? 1 : 0));
    Engine::Core::Entity& entity = entity_scratch.entity();
    auto* unit =
        entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
    EXPECT_NE(unit, nullptr);
    if (unit == nullptr) {
      return nullptr;
    }
    unit->spawn_type = Game::Units::SpawnType::Knight;
    unit->nation_id = Game::Systems::NationID::RomanRepublic;
    if (formation_active) {
      auto* formation_mode =
          entity.add_component<Engine::Core::FormationModeComponent>();
      EXPECT_NE(formation_mode, nullptr);
      if (formation_mode == nullptr) {
        return nullptr;
      }
      formation_mode->active = true;
    }
    ctx.entity = &entity;

    Render::GL::AnimationInputs anim{};
    anim.is_guarding = true;
    anim.guard_pose_progress = 1.0F;

    Render::Humanoid::HumanoidPreparation prep;
    Render::Humanoid::prepare_humanoid_instances(
        owner, ctx, anim, test_runtime(0U), prep);
    if (prep.bodies.requests().empty()) {
      return nullptr;
    }

    auto const* desc = registry.get(prep.bodies.requests().front().archetype);
    if (desc == nullptr || desc->bake_attachment_count != 1U) {
      return nullptr;
    }
    return &desc->bake_attachments.front();
  };

  auto const* default_attachment = build_guard_attachment(false);
  auto const* formation_attachment = build_guard_attachment(true);
  ASSERT_NE(default_attachment, nullptr);
  ASSERT_NE(formation_attachment, nullptr);

  QVector3D const default_forward =
      default_attachment->local_offset.mapVector(QVector3D(0.0F, 0.0F, 1.0F))
          .normalized();
  QVector3D const formation_forward =
      formation_attachment->local_offset.mapVector(QVector3D(0.0F, 0.0F, 1.0F))
          .normalized();
  EXPECT_GT(QVector3D::dotProduct(default_forward, formation_forward), 0.95F);
}

TEST(HumanoidPrepare, CarthageFormationFrontShieldTiltsOverBody) {
  constexpr std::uint8_t k_base_role = 6;
  std::array<Render::Creature::StaticAttachmentSpec, 1> attachments{
      Render::GL::carthage_shield_make_static_attachment({}, k_base_role),
  };
  auto& registry = Render::Creature::ArchetypeRegistry::instance();
  auto const base_archetype = registry.register_unit_archetype(
      "tests/carthage_formation_guard_tilt",
      Render::Creature::Pipeline::CreatureKind::Humanoid,
      attachments);
  ASSERT_NE(base_archetype, Render::Creature::k_invalid_archetype);

  class FixedSpecRenderer : public Render::GL::HumanoidRendererBase {
  public:
    explicit FixedSpecRenderer(Render::Creature::ArchetypeId archetype)
        : Render::GL::HumanoidRendererBase(make_spec(archetype)) {}

    static auto make_spec(Render::Creature::ArchetypeId archetype)
        -> Render::Creature::Pipeline::UnitVisualSpec {
      Render::Creature::Pipeline::UnitVisualSpec spec{};
      spec.debug_name = "tests/carthage_formation_guard_tilt_renderer";
      spec.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
      spec.archetype_id = archetype;
      spec.skip_default_facial_hair_archetype = true;
      return spec;
    }
  };

  auto build_front_attachment =
      [&](bool formation_active) -> const Render::Creature::StaticAttachmentSpec* {
    FixedSpecRenderer const owner(base_archetype);
    Render::GL::DrawContext ctx{};
    ctx.world_view = Render::WorldView::of_active_session();
    ctx.force_single_soldier = true;
    ctx.allow_template_cache = false;

    Engine::Core::StandaloneEntity entity_scratch(121 + (formation_active ? 1 : 0));
    Engine::Core::Entity& entity = entity_scratch.entity();
    auto* unit =
        entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
    EXPECT_NE(unit, nullptr);
    if (unit == nullptr) {
      return nullptr;
    }
    unit->spawn_type = Game::Units::SpawnType::Knight;
    unit->nation_id = Game::Systems::NationID::Carthage;
    if (formation_active) {
      auto* formation_mode =
          entity.add_component<Engine::Core::FormationModeComponent>();
      EXPECT_NE(formation_mode, nullptr);
      if (formation_mode == nullptr) {
        return nullptr;
      }
      formation_mode->active = true;
    }
    ctx.entity = &entity;

    Render::GL::AnimationInputs anim{};
    anim.is_guarding = true;
    anim.guard_pose_progress = 1.0F;

    Render::Humanoid::HumanoidPreparation prep;
    Render::Humanoid::prepare_humanoid_instances(
        owner, ctx, anim, test_runtime(0U), prep);
    if (prep.bodies.requests().empty()) {
      return nullptr;
    }

    auto const* desc = registry.get(prep.bodies.requests().front().archetype);
    if (desc == nullptr || desc->bake_attachment_count != 1U) {
      return nullptr;
    }
    return &desc->bake_attachments.front();
  };

  auto const* default_attachment = build_front_attachment(false);
  auto const* formation_attachment = build_front_attachment(true);
  ASSERT_NE(default_attachment, nullptr);
  ASSERT_NE(formation_attachment, nullptr);

  QVector3D const default_up =
      default_attachment->local_offset.mapVector(QVector3D(0.0F, 1.0F, 0.0F))
          .normalized();
  QVector3D const formation_up =
      formation_attachment->local_offset.mapVector(QVector3D(0.0F, 1.0F, 0.0F))
          .normalized();
  EXPECT_LT(default_up.z(), -0.15F);
  EXPECT_LT(formation_up.z(), -0.15F);
}

TEST(HumanoidPrepare, AmbientIdleContextSelectsCorrectIdleClipVariant) {
  using Render::Creature::AnimationStateId;
  using Render::Creature::Pipeline::humanoid_bpat_playback_for_anim;
  using Render::GL::AmbientIdleType;

  auto const archetype_id = Render::Creature::ArchetypeRegistry::k_humanoid_base;
  Render::GL::HumanoidAnimationContext idle_no_ambient{};
  idle_no_ambient.ambient_idle_type = AmbientIdleType::None;
  auto const no_ambient = humanoid_bpat_playback_for_anim(
      archetype_id, Render::Creature::Bpat::k_species_humanoid, idle_no_ambient);
  ASSERT_TRUE(no_ambient.has_value());
  EXPECT_EQ(no_ambient->clip_id, Render::Creature::k_humanoid_idle_clip);

  Render::GL::HumanoidAnimationContext idle_squat{};
  idle_squat.ambient_idle_type = AmbientIdleType::SitDown;
  idle_squat.ambient_idle_phase = 0.5F;
  auto const squat = humanoid_bpat_playback_for_anim(
      archetype_id, Render::Creature::Bpat::k_species_humanoid, idle_squat);
  ASSERT_TRUE(squat.has_value());
  EXPECT_EQ(squat->clip_id, Render::Creature::k_humanoid_idle_squat_clip);

  Render::GL::HumanoidAnimationContext idle_jump{};
  idle_jump.ambient_idle_type = AmbientIdleType::Jump;
  idle_jump.ambient_idle_phase = 0.5F;
  auto const jump = humanoid_bpat_playback_for_anim(
      archetype_id, Render::Creature::Bpat::k_species_humanoid, idle_jump);
  ASSERT_TRUE(jump.has_value());
  EXPECT_EQ(jump->clip_id, Render::Creature::k_humanoid_idle_jump_clip);

  Render::GL::HumanoidAnimationContext idle_weapon{};
  idle_weapon.ambient_idle_type = AmbientIdleType::RaiseWeapon;
  idle_weapon.ambient_idle_phase = 0.5F;
  auto const weapon = humanoid_bpat_playback_for_anim(
      archetype_id, Render::Creature::Bpat::k_species_humanoid, idle_weapon);
  ASSERT_TRUE(weapon.has_value());
  EXPECT_EQ(weapon->clip_id, Render::Creature::k_humanoid_idle_weapon_clip);

  Render::GL::HumanoidAnimationContext idle_weave{};
  idle_weave.ambient_idle_type = AmbientIdleType::ShiftWeight;
  idle_weave.ambient_idle_phase = 0.5F;
  auto const weave = humanoid_bpat_playback_for_anim(
      archetype_id, Render::Creature::Bpat::k_species_humanoid, idle_weave);
  ASSERT_TRUE(weave.has_value());
  EXPECT_EQ(weave->clip_id, Render::Creature::k_humanoid_idle_weave_clip);
}

TEST(HumanoidPrepare, RomanSwordsmanUsesRomanScutumRoleLayout) {
  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto renderer = registry.get("troops/roman/swordsman");
  ASSERT_TRUE(static_cast<bool>(renderer));

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(1);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Knight;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  ctx.entity = &entity;

  CountingSubmitter sink;
  renderer(ctx, sink);
  ASSERT_GT(sink.rigged_calls, 0);

  auto const archetype_id = find_archetype_id("troops/roman/swordsman");
  ASSERT_NE(archetype_id, Render::Creature::k_invalid_archetype);
  EXPECT_EQ(extra_role_color_count(archetype_id),
            Render::GL::k_historical_helmet_role_count +
                Render::GL::k_roman_greaves_role_count +
                Render::GL::k_roman_shoulder_cover_role_count +
                Render::GL::k_roman_scutum_role_count +
                Render::GL::k_roman_light_armor_role_count +
                Render::GL::k_sword_role_count + Render::GL::k_scabbard_role_count);
}

TEST(HumanoidPrepare, BuiltInRomanSwordsmanRemainsVisibleAfterGuardTransition) {
  Render::GL::reset_humanoid_runtime_context();

  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto renderer = registry.get("troops/roman/swordsman");
  ASSERT_TRUE(static_cast<bool>(renderer));

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;
  ctx.force_humanoid_lod = true;
  ctx.forced_humanoid_lod = Render::Creature::CreatureLOD::Full;

  Engine::Core::StandaloneEntity entity_scratch(91);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  auto* persistent =
      entity.add_component<Render::Creature::HumanoidAnimationStateComponent>();
  auto* formation_mode = entity.add_component<Engine::Core::FormationModeComponent>();
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(persistent, nullptr);
  ASSERT_NE(formation_mode, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Knight;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  formation_mode->active = true;
  ctx.entity = &entity;

  auto expect_visible = [&](float animation_time) {
    ctx.animation_time = animation_time;
    CountingSubmitter sink;
    renderer(ctx, sink);
    ASSERT_EQ(sink.rigged_calls, 2);
    ASSERT_EQ(sink.rigged_mesh_min_world_y.size(), 2U);
    EXPECT_TRUE(std::isfinite(sink.rigged_mesh_min_world_y.front()));
  };

  expect_visible(1.0F);
  expect_visible(1.75F);
  expect_visible(3.25F);
}

TEST(HumanoidPrepare, RomanArcherUsesRomanCloakRoleLayout) {
  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto renderer = registry.get("troops/roman/archer");
  ASSERT_TRUE(static_cast<bool>(renderer));

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(1);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Archer;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  ctx.entity = &entity;

  CountingSubmitter sink;
  renderer(ctx, sink);
  ASSERT_GT(sink.rigged_calls, 0);

  auto const archetype_id = find_archetype_id("troops/roman/archer");
  ASSERT_NE(archetype_id, Render::Creature::k_invalid_archetype);
  EXPECT_EQ(extra_role_color_count(archetype_id),
            Render::GL::k_historical_helmet_role_count +
                Render::GL::k_roman_greaves_role_count +
                Render::GL::k_quiver_role_count +
                Render::GL::k_roman_light_armor_role_count +
                Render::GL::k_bow_role_count + Render::GL::k_cloak_role_count);
}

TEST(HumanoidPrepare, RomanSpearmanUsesGreavesAndShoulderRoleLayout) {
  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto renderer = registry.get("troops/roman/spearman");
  ASSERT_TRUE(static_cast<bool>(renderer));

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(1);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Spearman;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  ctx.entity = &entity;

  CountingSubmitter sink;
  renderer(ctx, sink);
  ASSERT_GT(sink.rigged_calls, 0);

  auto const archetype_id = find_archetype_id("troops/roman/spearman");
  ASSERT_NE(archetype_id, Render::Creature::k_invalid_archetype);
  EXPECT_EQ(extra_role_color_count(archetype_id),
            Render::GL::k_roman_heavy_helmet_role_count +
                Render::GL::k_roman_greaves_role_count +
                Render::GL::k_roman_shoulder_cover_role_count +
                Render::GL::k_roman_light_armor_role_count +
                Render::GL::k_spear_role_count);
}

TEST(HumanoidPrepare, CarthageSwordsmanUsesCarthageShieldRoleLayout) {
  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto renderer = registry.get("troops/carthage/swordsman");
  ASSERT_TRUE(static_cast<bool>(renderer));

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(1);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Knight;
  unit->nation_id = Game::Systems::NationID::Carthage;
  ctx.entity = &entity;

  CountingSubmitter sink;
  renderer(ctx, sink);
  ASSERT_GT(sink.rigged_calls, 0);

  auto const archetype_id = find_archetype_id("troops/carthage/swordsman");
  ASSERT_NE(archetype_id, Render::Creature::k_invalid_archetype);
  EXPECT_EQ(extra_role_color_count(archetype_id),
            Render::GL::k_carthage_heavy_helmet_role_count +
                Render::GL::k_carthage_shield_role_count +
                Render::GL::k_carthage_shoulder_cover_role_count +
                Render::GL::k_armor_heavy_carthage_role_count +
                Render::GL::k_sword_role_count + Render::GL::k_scabbard_role_count);
}

TEST(HumanoidPrepare, RomanHealerUsesSenatorRobeRoleLayout) {
  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto renderer = registry.get("troops/roman/healer");
  ASSERT_TRUE(static_cast<bool>(renderer));

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(1);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Healer;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  ctx.entity = &entity;

  CountingSubmitter sink;
  renderer(ctx, sink);
  ASSERT_GT(sink.rigged_calls, 0);

  auto const archetype_id = find_archetype_id("troops/roman/healer");
  ASSERT_NE(archetype_id, Render::Creature::k_invalid_archetype);

  EXPECT_EQ(extra_role_color_count(archetype_id), k_roman_senator_role_count);
}

TEST(HumanoidPrepare, CarthageHealerUsesDarkMageRobeRoleLayout) {
  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto renderer = registry.get("troops/carthage/healer");
  ASSERT_TRUE(static_cast<bool>(renderer));

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(1);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Healer;
  unit->nation_id = Game::Systems::NationID::Carthage;
  ctx.entity = &entity;

  CountingSubmitter sink;
  renderer(ctx, sink);
  ASSERT_GT(sink.rigged_calls, 0);

  auto const archetype_id = find_archetype_id("troops/carthage/healer");
  ASSERT_NE(archetype_id, Render::Creature::k_invalid_archetype);

  EXPECT_EQ(extra_role_color_count(archetype_id), k_carthage_dark_mage_role_count);
}

TEST(HumanoidPrepare, RomanBuilderUsesSupportLoadoutRoleLayout) {
  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto renderer = registry.get("troops/roman/builder");
  ASSERT_TRUE(static_cast<bool>(renderer));

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(1);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Builder;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  ctx.entity = &entity;

  CountingSubmitter sink;
  renderer(ctx, sink);
  ASSERT_GT(sink.rigged_calls, 0);

  auto const archetype_id = find_archetype_id("troops/roman/builder");
  ASSERT_NE(archetype_id, Render::Creature::k_invalid_archetype);
  EXPECT_EQ(
      extra_role_color_count(archetype_id),
      k_roman_builder_tunic_role_count + Render::GL::k_roman_light_helmet_role_count +
          Render::GL::k_tool_belt_role_count + Render::GL::k_work_apron_role_count +
          Render::GL::k_arm_guards_role_count);
}

TEST(HumanoidPrepare, CarthageBuilderUsesSupportLoadoutRoleLayout) {
  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto renderer = registry.get("troops/carthage/builder");
  ASSERT_TRUE(static_cast<bool>(renderer));

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(1);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Builder;
  unit->nation_id = Game::Systems::NationID::Carthage;
  ctx.entity = &entity;

  CountingSubmitter sink;
  renderer(ctx, sink);
  ASSERT_GT(sink.rigged_calls, 0);

  auto const archetype_id = find_archetype_id("troops/carthage/builder");
  ASSERT_NE(archetype_id, Render::Creature::k_invalid_archetype);
  EXPECT_EQ(
      extra_role_color_count(archetype_id),
      k_carthage_builder_headwrap_role_count + k_carthage_builder_robes_role_count +
          Render::GL::k_tool_belt_role_count + Render::GL::k_work_apron_role_count +
          Render::GL::k_arm_guards_role_count);
}

TEST(HumanoidPrepare, RomanCivilianUsesDataLoadoutRoleLayout) {
  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto renderer = registry.get("troops/roman/civilian");
  ASSERT_TRUE(static_cast<bool>(renderer));

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(1);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Civilian;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  ctx.entity = &entity;

  CountingSubmitter sink;
  renderer(ctx, sink);
  ASSERT_GT(sink.rigged_calls, 0);

  auto const archetype_id = find_archetype_id("troops/roman/civilian");
  ASSERT_NE(archetype_id, Render::Creature::k_invalid_archetype);
  EXPECT_EQ(extra_role_color_count(archetype_id),
            k_roman_builder_tunic_role_count + k_roman_civilian_mantle_role_count +
                k_civilian_pack_role_count);
}

TEST(HumanoidPrepare, CarthageCivilianUsesDataLoadoutRoleLayout) {
  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto renderer = registry.get("troops/carthage/civilian");
  ASSERT_TRUE(static_cast<bool>(renderer));

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(1);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Civilian;
  unit->nation_id = Game::Systems::NationID::Carthage;
  ctx.entity = &entity;

  CountingSubmitter sink;
  renderer(ctx, sink);
  ASSERT_GT(sink.rigged_calls, 0);

  auto const archetype_id = find_archetype_id("troops/carthage/civilian");
  ASSERT_NE(archetype_id, Render::Creature::k_invalid_archetype);
  EXPECT_EQ(extra_role_color_count(archetype_id),
            k_carthage_builder_headwrap_role_count +
                k_carthage_builder_robes_role_count +
                k_carthage_civilian_sash_role_count + k_civilian_pack_role_count);
}

TEST(HumanoidPrepare, RomanCivilianGarmentsUseCompactAttachmentCount) {
  EXPECT_LE(render_runtime_mesh_count("troops/roman/civilian",
                                      Game::Units::SpawnType::Civilian,
                                      Game::Systems::NationID::RomanRepublic,
                                      false),
            16);
}

TEST(HumanoidPrepare, CarthageCivilianGarmentsUseCompactAttachmentCount) {
  EXPECT_LE(render_runtime_mesh_count("troops/carthage/civilian",
                                      Game::Units::SpawnType::Civilian,
                                      Game::Systems::NationID::Carthage,
                                      false),
            16);
}

TEST(HumanoidPrepare, RomanMountedSwordsmanUsesRomanScutumRoleLayout) {
  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto renderer = registry.get("troops/roman/horse_swordsman");
  ASSERT_TRUE(static_cast<bool>(renderer));

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(1);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::MountedKnight;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  ctx.entity = &entity;

  CountingSubmitter sink;
  renderer(ctx, sink);
  ASSERT_GT(sink.rigged_calls, 0);

  auto const archetype_id = find_archetype_id("troops/roman/horse_swordsman/rider");
  ASSERT_NE(archetype_id, Render::Creature::k_invalid_archetype);
  EXPECT_EQ(extra_role_color_count(archetype_id),
            Render::GL::k_roman_heavy_helmet_role_count +
                Render::GL::k_roman_shoulder_cover_role_count +
                Render::GL::k_roman_scutum_role_count +
                Render::GL::k_roman_heavy_armor_role_count +
                Render::GL::k_sword_role_count + Render::GL::k_scabbard_role_count);
}

TEST(HumanoidPrepare, CarthageMountedSwordsmanUsesCarthageShieldRoleLayout) {
  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto renderer = registry.get("troops/carthage/horse_swordsman");
  ASSERT_TRUE(static_cast<bool>(renderer));

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(1);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::MountedKnight;
  unit->nation_id = Game::Systems::NationID::Carthage;
  ctx.entity = &entity;

  CountingSubmitter sink;
  renderer(ctx, sink);
  ASSERT_GT(sink.rigged_calls, 0);

  auto const archetype_id = find_archetype_id("troops/carthage/horse_swordsman/rider");
  ASSERT_NE(archetype_id, Render::Creature::k_invalid_archetype);
  EXPECT_EQ(extra_role_color_count(archetype_id),
            Render::GL::k_carthage_heavy_helmet_role_count +
                Render::GL::k_carthage_shield_role_count +
                Render::GL::k_carthage_shoulder_cover_role_count +
                Render::GL::k_armor_heavy_carthage_role_count +
                Render::GL::k_sword_role_count + Render::GL::k_scabbard_role_count);
}

TEST(HumanoidPrepare, BowReadyRootRequestUsesSurfaceGroundingContract) {
  BowReadyRegressionRenderer const renderer;
  ScopedFlatTerrain const terrain(2.4F);

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(1);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Archer;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;

  auto* transform = entity.add_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);
  transform->position.x = 0.35F;
  transform->position.y = 6.0F;
  transform->position.z = -0.2F;
  transform->scale.x = 1.0F;
  transform->scale.y = 1.0F;
  transform->scale.z = 1.0F;

  ctx.entity = &entity;

  Render::GL::AnimationInputs const anim{};
  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      renderer, ctx, anim, test_runtime(0U), prep);

  ASSERT_EQ(prep.bodies.requests().size(), 1U);
  EXPECT_FALSE(prep.bodies.requests().front().world_already_grounded);
}

TEST(HumanoidPrepare, BowReadySubmittedSurfaceGroundingTouchesTerrain) {
  BowReadyRegressionRenderer const renderer;
  ScopedFlatTerrain const terrain(2.4F);

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(1);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Archer;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;

  auto* transform = entity.add_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);
  transform->position.x = 0.35F;
  transform->position.y = 6.0F;
  transform->position.z = -0.2F;
  transform->scale.x = 1.0F;
  transform->scale.y = 1.0F;
  transform->scale.z = 1.0F;

  ctx.entity = &entity;

  CountingSubmitter sink;
  renderer.render(ctx, sink);

  ASSERT_EQ(sink.rigged_calls, 1);
  ASSERT_EQ(sink.rigged_world_y.size(), 1U);
  EXPECT_NEAR(sink.rigged_world_y.front(), 2.4F, 0.1F);
}

TEST(HumanoidPrepare, CommanderJumpLiftSurvivesSurfaceGrounding) {
  ScopedFlatTerrain const terrain(1.5F);

  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(1);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Knight;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;

  auto* transform = entity.add_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);
  transform->position.x = 0.1F;
  transform->position.y = 6.0F;
  transform->position.z = -0.2F;

  auto* commander = entity.add_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(commander, nullptr);
  commander->jump_active = true;
  commander->jump_phase = 0.5F;
  commander->jump_height_offset = 0.35F;

  ctx.entity = &entity;

  Render::GL::AnimationInputs const anim{};
  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, anim, test_runtime(1U), prep);

  CountingSubmitter sink;
  Render::Creature::Pipeline::submit_preparation(prep, sink);

  ASSERT_EQ(sink.rigged_calls, 1);
  ASSERT_EQ(sink.rigged_world_y.size(), 1U);
  ASSERT_EQ(sink.rigged_mesh_min_world_y.size(), 1U);
  EXPECT_NEAR(sink.rigged_world_y.front(), 1.85F, 0.1F);
  EXPECT_GT(sink.rigged_mesh_min_world_y.front(), 1.55F);
}

TEST(HumanoidPrepare, RenderIndividualsOverrideLimitsPreparedSoldiers) {
  ScopedFlatTerrain const terrain(0.0F);

  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(1);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Archer;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  unit->render_individuals_per_unit_override = 3;

  auto* transform = entity.add_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);
  transform->position.x = 0.0F;
  transform->position.y = 0.0F;
  transform->position.z = 0.0F;
  transform->scale.x = 1.0F;
  transform->scale.y = 1.0F;
  transform->scale.z = 1.0F;

  ctx.entity = &entity;

  Render::GL::AnimationInputs const anim{};
  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, anim, test_runtime(0U), prep);

  EXPECT_EQ(prep.bodies.requests().size(), 3U);
}

TEST(HumanoidPrepare, ActiveSoldierCasualtiesRenderDeathRequests) {
  ScopedFlatTerrain const terrain(0.0F);

  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(1);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(60, 100, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Archer;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  unit->render_individuals_per_unit_override = 3;

  auto* transform = entity.add_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);
  transform->position.x = 0.0F;
  transform->position.y = 0.0F;
  transform->position.z = 0.0F;
  transform->scale.x = 1.0F;
  transform->scale.y = 1.0F;
  transform->scale.z = 1.0F;

  auto* casualties =
      entity.add_component<Engine::Core::SoldierCasualtyAnimationComponent>();
  ASSERT_NE(casualties, nullptr);
  casualties->entries.push_back({
      .slot_index = 0U,
      .profile = Engine::Core::DeathSequenceProfile::Infantry,
      .state = Engine::Core::DeathSequenceState::Dying,
      .state_time = 0.2F,
      .state_duration = 1.0F,
      .dead_hold_duration = 0.8F,
      .sequence_variant = 0U,
  });

  ctx.entity = &entity;

  Render::GL::AnimationInputs const anim{};
  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, anim, test_runtime(0U), prep);

  ASSERT_EQ(prep.bodies.requests().size(), 3U);
  EXPECT_EQ(std::count_if(prep.bodies.requests().begin(),
                          prep.bodies.requests().end(),
                          [](const auto& req) {
                            return req.state == Render::Creature::AnimationStateId::Die;
                          }),
            1);
}

TEST(HumanoidPrepare, LaunchedChargeCasualtyTravelsAndTumblesInSubmittedWorld) {
  ScopedFlatTerrain const terrain(0.0F);

  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.allow_template_cache = false;

  auto casualty_origin = [&owner, &ctx](bool launched) {
    Engine::Core::StandaloneEntity entity_scratch(1);
    Engine::Core::Entity& entity = entity_scratch.entity();
    auto* unit = entity.add_component<Engine::Core::UnitComponent>(75, 100, 0.0F, 0.0F);
    unit->spawn_type = Game::Units::SpawnType::Spearman;
    unit->nation_id = Game::Systems::NationID::Carthage;
    unit->render_individuals_per_unit_override = 4;

    auto* transform = entity.add_component<Engine::Core::TransformComponent>();
    transform->scale = {1.0F, 1.0F, 1.0F};

    auto* casualties =
        entity.add_component<Engine::Core::SoldierCasualtyAnimationComponent>();
    casualties->entries.push_back({
        .slot_index = 0U,
        .profile = Engine::Core::DeathSequenceProfile::Infantry,
        .state = Engine::Core::DeathSequenceState::Dying,
        .state_time = 0.45F,
        .state_duration = 1.0F,
        .dead_hold_duration = 0.8F,
        .sequence_variant = 0U,
        .launched = launched,
        .launch_velocity_x = launched ? 7.0F : 0.0F,
        .launch_velocity_y = launched ? 8.0F : 0.0F,
        .launch_velocity_z = launched ? 2.0F : 0.0F,
        .launch_pitch_speed = launched ? 250.0F : 0.0F,
        .launch_roll_speed = launched ? -110.0F : 0.0F,
    });
    ctx.entity = &entity;

    Render::Humanoid::HumanoidPreparation prep;
    Render::Humanoid::prepare_humanoid_instances(
        owner, ctx, Render::GL::AnimationInputs{}, test_runtime(0U), prep);

    auto const casualty =
        std::find_if(prep.bodies.requests().begin(),
                     prep.bodies.requests().end(),
                     [](auto const& request) { return request.instance_index == 0U; });
    EXPECT_NE(casualty, prep.bodies.requests().end());
    return std::make_pair(casualty->world.map(QVector3D{}),
                          casualty->world.mapVector(QVector3D(0.0F, 1.0F, 0.0F)));
  };

  auto const [resting_origin, resting_up] = casualty_origin(false);
  auto const [launched_origin, launched_up] = casualty_origin(true);

  float const travelled = std::hypot(launched_origin.x() - resting_origin.x(),
                                     launched_origin.z() - resting_origin.z());
  EXPECT_GT(travelled, 0.75F);
  EXPECT_GT(launched_origin.y(), 1.0F);
  EXPECT_LT(
      std::abs(QVector3D::dotProduct(launched_up.normalized(), QVector3D(0, 1, 0))),
      0.8F);
}

TEST(HumanoidPrepare, BowReadySubmittedVisibleGeometryTouchesTerrain) {
  BowReadyRegressionRenderer const renderer;
  ScopedFlatTerrain const terrain(2.4F);

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(1);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Archer;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;

  auto* transform = entity.add_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);
  transform->position.x = 0.35F;
  transform->position.y = 6.0F;
  transform->position.z = -0.2F;
  transform->scale.x = 1.0F;
  transform->scale.y = 1.0F;
  transform->scale.z = 1.0F;

  ctx.entity = &entity;

  CountingSubmitter sink;
  renderer.render(ctx, sink);

  ASSERT_EQ(sink.rigged_calls, 1);
  ASSERT_EQ(sink.rigged_world_y.size(), 1U);
  EXPECT_NEAR(sink.rigged_world_y.front(), 2.4F, 0.1F);
}

TEST(HumanoidPrepare, BuiltInArchersUseSurfaceGroundingInIdlePose) {
  ScopedFlatTerrain const terrain(2.4F);
  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);

  auto verify_archer = [&](const char* renderer_id, Game::Systems::NationID nation_id) {
    const auto renderer = registry.get(renderer_id);
    ASSERT_TRUE(static_cast<bool>(renderer));

    Render::GL::DrawContext ctx{};
    ctx.world_view = Render::WorldView::of_active_session();
    ctx.force_single_soldier = true;
    ctx.allow_template_cache = false;

    Engine::Core::StandaloneEntity entity_scratch(1);
    Engine::Core::Entity& entity = entity_scratch.entity();
    auto* unit =
        entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
    ASSERT_NE(unit, nullptr);
    unit->spawn_type = Game::Units::SpawnType::Archer;
    unit->nation_id = nation_id;

    auto* transform = entity.add_component<Engine::Core::TransformComponent>();
    ASSERT_NE(transform, nullptr);
    transform->position.x = 0.35F;
    transform->position.y = 6.0F;
    transform->position.z = -0.2F;
    transform->scale.x = 1.0F;
    transform->scale.y = 1.0F;
    transform->scale.z = 1.0F;

    ctx.entity = &entity;

    CountingSubmitter sink;
    renderer(ctx, sink);

    ASSERT_EQ(sink.rigged_calls, 2);
    ASSERT_EQ(sink.rigged_world_y.size(), 2U);
    EXPECT_NEAR(sink.rigged_world_y.front(), 2.4F, 0.1F);
  };

  verify_archer("troops/roman/archer", Game::Systems::NationID::RomanRepublic);
  verify_archer("troops/carthage/archer", Game::Systems::NationID::Carthage);
}

TEST(HumanoidPrepare, BuiltInArchersVisibleIdleGeometryTouchesTerrain) {
  ScopedFlatTerrain const terrain(2.4F);
  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);

  auto verify_archer = [&](const char* renderer_id, Game::Systems::NationID nation_id) {
    const auto renderer = registry.get(renderer_id);
    ASSERT_TRUE(static_cast<bool>(renderer));

    Render::GL::DrawContext ctx{};
    ctx.world_view = Render::WorldView::of_active_session();
    ctx.force_single_soldier = true;
    ctx.allow_template_cache = false;

    Engine::Core::StandaloneEntity entity_scratch(1);
    Engine::Core::Entity& entity = entity_scratch.entity();
    auto* unit =
        entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
    ASSERT_NE(unit, nullptr);
    unit->spawn_type = Game::Units::SpawnType::Archer;
    unit->nation_id = nation_id;

    auto* transform = entity.add_component<Engine::Core::TransformComponent>();
    ASSERT_NE(transform, nullptr);
    transform->position.x = 0.35F;
    transform->position.y = 6.0F;
    transform->position.z = -0.2F;
    transform->scale.x = 1.0F;
    transform->scale.y = 1.0F;
    transform->scale.z = 1.0F;

    ctx.entity = &entity;

    CountingSubmitter sink;
    renderer(ctx, sink);

    ASSERT_EQ(sink.rigged_calls, 2);
    ASSERT_EQ(sink.rigged_world_y.size(), 2U);
    EXPECT_NEAR(sink.rigged_world_y.front(), 2.4F, 0.1F);
  };

  verify_archer("troops/roman/archer", Game::Systems::NationID::RomanRepublic);
  verify_archer("troops/carthage/archer", Game::Systems::NationID::Carthage);
}

TEST(HumanoidPrepare, MixedBatchOnlySubmitsMainRows) {
  using namespace Render::Creature::Pipeline;

  CountingSubmitter sink;
  CreaturePreparationResult prep;
  for (int i = 0; i < 5; ++i) {
    Render::Creature::CreatureRenderRequest req{};
    req.archetype = Render::Creature::ArchetypeRegistry::k_humanoid_base;
    req.state = Render::Creature::AnimationStateId::Idle;
    req.lod = Render::Creature::CreatureLOD::Full;
    req.pass = (i % 2 == 0) ? RenderPassIntent::Main : RenderPassIntent::Shadow;
    req.seed = static_cast<std::uint32_t>(i + 1);
    req.world_already_grounded = true;
    prep.bodies.add_request(req);
  }
  const auto stats = submit_preparation(prep, sink);

  EXPECT_EQ(stats.entities_submitted, 3U);
}

TEST(HumanoidPrepare, DeriveUnitSeedHonorsOverride) {
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.has_seed_override = true;
  ctx.seed_override = 0xDEADBEEFU;
  EXPECT_EQ(Render::Creature::Pipeline::derive_unit_seed(ctx, nullptr), 0xDEADBEEFU);
}

TEST(HumanoidPrepare, DeriveUnitSeedDeterministicWithoutOverride) {
  Render::GL::DrawContext const ctx{};

  EXPECT_EQ(Render::Creature::Pipeline::derive_unit_seed(ctx, nullptr), 0U);
}

TEST(HumanoidPrepare, BuildSoldierLayoutIsDeterministic) {
  Render::Entity::FormationInstanceRequest inputs{};
  inputs.unit_layout = Game::Formation::UnitLayoutLibrary::instance().resolve(
      "rome", "close_order_infantry");
  inputs.idx = 3;
  inputs.row = 1;
  inputs.col = 1;
  inputs.rows = 2;
  inputs.cols = 2;
  inputs.formation_spacing = 1.25F;
  inputs.seed = 0x12345678U;
  inputs.force_single_soldier = false;
  inputs.melee_attack = true;
  inputs.animation_time = 2.5F;

  inputs.soldier_offsets = &Game::Formation::UnitLayoutSystem::instance();

  auto const first = Render::Entity::build_formation_instance(inputs);
  auto const second = Render::Entity::build_formation_instance(inputs);

  EXPECT_FLOAT_EQ(first.offset_x, second.offset_x);
  EXPECT_FLOAT_EQ(first.offset_z, second.offset_z);
  EXPECT_FLOAT_EQ(first.vertical_jitter, second.vertical_jitter);
  EXPECT_FLOAT_EQ(first.yaw_offset, second.yaw_offset);
  EXPECT_FLOAT_EQ(first.individuality.gait_phase_offset,
                  second.individuality.gait_phase_offset);
  EXPECT_EQ(first.inst_seed, second.inst_seed);
}

TEST(HumanoidPrepare, BuildSoldierLayoutLeavesSingleSoldierUnjittered) {
  Render::Entity::FormationInstanceRequest inputs{};
  inputs.unit_layout = Game::Formation::UnitLayoutLibrary::instance().resolve(
      "rome", "close_order_infantry");
  inputs.idx = 0;
  inputs.row = 0;
  inputs.col = 0;
  inputs.rows = 1;
  inputs.cols = 1;
  inputs.formation_spacing = 1.0F;
  inputs.seed = 0xCAFEBABEU;
  inputs.force_single_soldier = true;
  inputs.melee_attack = false;
  inputs.animation_time = 0.0F;

  auto const layout = Render::Entity::build_formation_instance(inputs);

  EXPECT_FLOAT_EQ(layout.offset_x, 0.0F);
  EXPECT_FLOAT_EQ(layout.offset_z, 0.0F);
  EXPECT_FLOAT_EQ(layout.vertical_jitter, 0.0F);
  EXPECT_FLOAT_EQ(layout.yaw_offset, 0.0F);
  EXPECT_FLOAT_EQ(layout.individuality.gait_phase_offset, 0.0F);
  EXPECT_EQ(layout.inst_seed, inputs.seed);
}

TEST(HumanoidPrepare, CavalryWedgeNarrowsTowardTheFrontRank) {
  auto const layout =
      Game::Formation::UnitLayoutLibrary::instance().resolve("rome", "cavalry_wedge");
  ASSERT_NE(layout, Game::Formation::k_invalid_layout);

  float const spacing = Render::GL::cavalry_formation_spacing(0.95F);
  auto const riders = Game::Formation::UnitLayoutSystem::instance().compute(
      layout, 9, 3, spacing, 0x12345678U);
  ASSERT_EQ(riders.size(), 9U);

  auto rank_extent = [&riders](std::size_t first, std::size_t last) {
    float widest = 0.0F;
    for (std::size_t i = first; i <= last; ++i) {
      widest = std::max(widest, std::abs(riders[i].offset_x));
    }
    return widest;
  };
  auto rank_depth = [&riders](std::size_t first, std::size_t last) {
    float sum = 0.0F;
    for (std::size_t i = first; i <= last; ++i) {
      sum += riders[i].offset_z;
    }
    return sum / static_cast<float>(last - first + 1U);
  };

  EXPECT_GT(rank_depth(0, 0) - rank_depth(1, 3), spacing * 0.5F);
  EXPECT_GT(rank_depth(1, 3) - rank_depth(4, 8), spacing * 0.5F);
  EXPECT_LT(rank_extent(0, 0), rank_extent(1, 3));
  EXPECT_LT(rank_extent(1, 3), rank_extent(4, 8));
}

TEST(HumanoidPrepare, BuildLocomotionStateIsDeterministicForRun) {
  Render::Humanoid::HumanoidLocomotionInputs inputs{};
  inputs.anim.time = 1.5F;
  inputs.anim.movement_state = Render::Creature::MovementAnimationState::Run;
  inputs.variation.walk_speed_mult = 1.10F;
  inputs.move_speed = 4.8F;
  inputs.locomotion_direction = QVector3D(0.0F, 0.0F, 1.0F);
  inputs.movement_target = QVector3D(3.0F, 0.0F, 8.0F);
  inputs.has_movement_target = true;
  inputs.animation_time = 1.5F;
  inputs.individuality.gait_phase_offset = 0.125F;

  auto const first = Render::Humanoid::build_humanoid_locomotion_state(inputs);
  auto const second = Render::Humanoid::build_humanoid_locomotion_state(inputs);

  EXPECT_EQ(first.gait.state, Render::GL::HumanoidMotionState::Run);
  EXPECT_EQ(first.gait.state, second.gait.state);
  EXPECT_FLOAT_EQ(first.gait.cycle_time, second.gait.cycle_time);
  EXPECT_FLOAT_EQ(first.gait.cycle_phase, second.gait.cycle_phase);
  EXPECT_FLOAT_EQ(first.gait.normalized_speed, second.gait.normalized_speed);
  EXPECT_FLOAT_EQ(first.gait.stride_distance, second.gait.stride_distance);
  EXPECT_EQ(first.has_movement_target, second.has_movement_target);
  EXPECT_GT(first.gait.cycle_time, 0.0F);
  EXPECT_GT(first.gait.stride_distance, 0.0F);
}

TEST(HumanoidPrepare, BuildLocomotionStateUsesAuthoritativeMovementState) {
  Render::Humanoid::HumanoidLocomotionInputs inputs{};
  inputs.anim.movement_state = Render::Creature::MovementAnimationState::Walk;
  inputs.variation.walk_speed_mult = 1.0F;
  inputs.entity_forward = QVector3D(0.0F, 0.0F, 1.0F);
  inputs.locomotion_direction = QVector3D(0.0F, 0.0F, 1.0F);
  inputs.animation_time = 1.0F;
  inputs.individuality.gait_phase_offset = 0.0F;

  inputs.move_speed = 2.30F;
  auto const walking = Render::Humanoid::build_humanoid_locomotion_state(inputs);
  EXPECT_EQ(walking.gait.state, Render::GL::HumanoidMotionState::Walk);

  inputs.move_speed = 3.30F;
  auto const speed_only = Render::Humanoid::build_humanoid_locomotion_state(inputs);
  EXPECT_EQ(speed_only.gait.state, Render::GL::HumanoidMotionState::Walk);

  inputs.move_speed = 1.20F;
  inputs.anim.movement_state = Render::Creature::MovementAnimationState::Run;
  auto const state_promoted = Render::Humanoid::build_humanoid_locomotion_state(inputs);
  EXPECT_EQ(state_promoted.gait.state, Render::GL::HumanoidMotionState::Run);
}

TEST(HumanoidPrepare, BuildLocomotionStateWalkCadenceTightensAsSpeedRises) {
  Render::Humanoid::HumanoidLocomotionInputs inputs{};
  inputs.anim.movement_state = Render::Creature::MovementAnimationState::Walk;
  inputs.variation.walk_speed_mult = 1.0F;
  inputs.entity_forward = QVector3D(0.0F, 0.0F, 1.0F);
  inputs.locomotion_direction = QVector3D(0.0F, 0.0F, 1.0F);
  inputs.animation_time = 1.0F;
  inputs.individuality.gait_phase_offset = 0.0F;

  inputs.move_speed = 1.25F;
  auto const slow_walk = Render::Humanoid::build_humanoid_locomotion_state(inputs);

  inputs.move_speed = 2.35F;
  auto const brisk_walk = Render::Humanoid::build_humanoid_locomotion_state(inputs);

  EXPECT_GT(slow_walk.gait.cycle_time, brisk_walk.gait.cycle_time);
  EXPECT_LT(slow_walk.gait.stride_distance, brisk_walk.gait.stride_distance);
}

TEST(HumanoidPrepare, BuildLocomotionStateAnimatesIdlePhaseOverTime) {
  Render::Humanoid::HumanoidLocomotionInputs inputs{};
  inputs.anim.movement_state = Render::Creature::MovementAnimationState::Idle;
  inputs.variation.walk_speed_mult = 1.0F;
  inputs.animation_time = 0.10F;
  inputs.individuality.gait_phase_offset = 0.125F;

  auto const first = Render::Humanoid::build_humanoid_locomotion_state(inputs);

  inputs.animation_time = 0.90F;
  auto const second = Render::Humanoid::build_humanoid_locomotion_state(inputs);

  EXPECT_FLOAT_EQ(first.gait.cycle_time, 1.6F);
  EXPECT_LT(first.gait.cycle_phase, second.gait.cycle_phase);
  EXPECT_NEAR(first.gait.cycle_phase, 0.140625F, 1.0e-6F);
  EXPECT_NEAR(second.gait.cycle_phase, 0.640625F, 1.0e-6F);
  EXPECT_FLOAT_EQ(first.gait.stride_distance, 0.0F);
  EXPECT_FLOAT_EQ(second.gait.stride_distance, 0.0F);
}

TEST(HumanoidPrepare, BuildLocomotionStateKeepsAdvancingPhaseDuringCadenceChanges) {
  auto wrapped_forward_delta = [](float start, float end) {
    float delta = end - start;
    if (delta < 0.0F) {
      delta += 1.0F;
    }
    return delta;
  };

  Render::Creature::HumanoidAnimationStateComponent persistent{};
  Render::Humanoid::HumanoidLocomotionInputs inputs{};
  inputs.anim.movement_state = Render::Creature::MovementAnimationState::Walk;
  inputs.variation.walk_speed_mult = 1.0F;
  inputs.entity_forward = QVector3D(0.0F, 0.0F, 1.0F);
  inputs.locomotion_direction = QVector3D(0.0F, 0.0F, 1.0F);
  inputs.individuality.gait_phase_offset = 0.125F;
  inputs.persistent_state = &persistent;
  inputs.allow_persistent_update = true;
  inputs.animation_time = 1.00F;
  inputs.move_speed = 1.25F;

  auto const first = Render::Humanoid::build_humanoid_locomotion_state(inputs);

  inputs.animation_time += 1.0F / 60.0F;
  inputs.move_speed = 2.35F;
  auto const second = Render::Humanoid::build_humanoid_locomotion_state(inputs);

  EXPECT_GT(wrapped_forward_delta(first.gait.cycle_phase, second.gait.cycle_phase),
            0.01F);
  EXPECT_LT(second.gait.cycle_time, first.gait.cycle_time);
}

TEST(HumanoidPrepare, BuildLocomotionStatePreservesPhaseAcrossWalkRunTransition) {
  auto wrapped_phase_delta = [](float a, float b) {
    float const direct = std::abs(a - b);
    return std::min(direct, 1.0F - direct);
  };

  Render::Creature::HumanoidAnimationStateComponent persistent{};
  Render::Humanoid::HumanoidLocomotionInputs inputs{};
  inputs.variation.walk_speed_mult = 1.0F;
  inputs.entity_forward = QVector3D(0.0F, 0.0F, 1.0F);
  inputs.locomotion_direction = QVector3D(0.0F, 0.0F, 1.0F);
  inputs.individuality.gait_phase_offset = 0.125F;
  inputs.persistent_state = &persistent;
  inputs.allow_persistent_update = true;

  inputs.anim.movement_state = Render::Creature::MovementAnimationState::Walk;
  inputs.animation_time = 1.50F;
  inputs.move_speed = 2.30F;
  auto const walking = Render::Humanoid::build_humanoid_locomotion_state(inputs);

  inputs.anim.movement_state = Render::Creature::MovementAnimationState::Run;
  inputs.animation_time += 1.0F / 60.0F;
  inputs.move_speed = 5.10F;
  auto const running = Render::Humanoid::build_humanoid_locomotion_state(inputs);

  EXPECT_EQ(walking.gait.state, Render::GL::HumanoidMotionState::Walk);
  EXPECT_EQ(running.gait.state, Render::GL::HumanoidMotionState::Run);
  EXPECT_LT(wrapped_phase_delta(walking.gait.cycle_phase, running.gait.cycle_phase),
            0.08F);
  EXPECT_GT(running.gait.run_blend, 0.0F);
}

TEST(HumanoidPrepare, BuildLocomotionStateSkipsPersistentWritesWhenUpdatesDisabled) {
  Render::Creature::HumanoidAnimationStateComponent persistent{};
  Render::Humanoid::HumanoidLocomotionInputs inputs{};
  inputs.variation.walk_speed_mult = 1.0F;
  inputs.entity_forward = QVector3D(0.0F, 0.0F, 1.0F);
  inputs.locomotion_direction = QVector3D(0.0F, 0.0F, 1.0F);
  inputs.individuality.gait_phase_offset = 0.125F;
  inputs.persistent_state = &persistent;
  inputs.allow_persistent_update = true;
  inputs.anim.movement_state = Render::Creature::MovementAnimationState::Walk;
  inputs.animation_time = 0.75F;
  inputs.move_speed = 2.30F;
  auto const seeded = Render::Humanoid::build_humanoid_locomotion_state(inputs);
  ASSERT_EQ(seeded.gait.state, Render::GL::HumanoidMotionState::Walk);

  auto const snapshot = persistent;

  inputs.allow_persistent_update = false;
  inputs.anim.movement_state = Render::Creature::MovementAnimationState::Run;
  inputs.animation_time += 1.0F / 60.0F;
  inputs.move_speed = 5.10F;
  auto const preview = Render::Humanoid::build_humanoid_locomotion_state(inputs);

  EXPECT_FLOAT_EQ(persistent.locomotion.phase, snapshot.locomotion.phase);
  EXPECT_FLOAT_EQ(persistent.locomotion.filtered_speed,
                  snapshot.locomotion.filtered_speed);
  EXPECT_FLOAT_EQ(persistent.locomotion.run_blend, snapshot.locomotion.run_blend);
  EXPECT_EQ(persistent.locomotion.state, snapshot.locomotion.state);
  EXPECT_GT(preview.gait.run_blend, snapshot.locomotion.run_blend);
}

TEST(HumanoidPrepare, TemplatePrewarmSamplingLeavesPersistentAnimationStateUntouched) {
  Engine::Core::StandaloneEntity entity_scratch(7);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* humanoid_state =
      entity.add_component<Render::Creature::HumanoidAnimationStateComponent>();
  ASSERT_NE(humanoid_state, nullptr);
  humanoid_state->initialized = true;
  humanoid_state->idle_duration = 2.0F;
  humanoid_state->last_sample_time = 4.0F;
  humanoid_state->locomotion.initialized = true;
  humanoid_state->locomotion.phase = 0.35F;

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.entity = &entity;
  ctx.animation_time = 6.0F;
  ctx.template_prewarm = true;

  auto const anim = Render::GL::sample_anim_state(ctx);

  EXPECT_FLOAT_EQ(anim.idle_duration, 4.0F);
  EXPECT_FLOAT_EQ(humanoid_state->idle_duration, 2.0F);
  EXPECT_FLOAT_EQ(humanoid_state->last_sample_time, 4.0F);
  EXPECT_FLOAT_EQ(humanoid_state->locomotion.phase, 0.35F);
}

TEST(HumanoidPrepare, LocomotionBlendSoftensEarlyStrideAmplitude) {
  Render::GL::VariationParams const variation =
      Render::GL::VariationParams::from_seed(12345U);

  Render::GL::HumanoidGaitDescriptor eased{};
  eased.state = Render::GL::HumanoidMotionState::Walk;
  eased.normalized_speed = 1.0F;
  eased.cycle_time = 0.92F / std::max(0.1F, variation.walk_speed_mult);
  eased.cycle_phase = 0.25F;
  eased.locomotion_blend = 0.20F;

  auto full = eased;
  full.locomotion_blend = 1.0F;

  Render::GL::HumanoidPose eased_pose{};
  Render::GL::HumanoidPose full_pose{};
  Render::GL::HumanoidRendererBase::compute_locomotion_pose(
      12345U, 0.25F, eased, variation, eased_pose);
  Render::GL::HumanoidRendererBase::compute_locomotion_pose(
      12345U, 0.25F, full, variation, full_pose);

  float const eased_stride = std::abs(eased_pose.foot_l.z() - eased_pose.foot_r.z());
  float const full_stride = std::abs(full_pose.foot_l.z() - full_pose.foot_r.z());
  EXPECT_GT(full_stride, eased_stride + 0.015F);
}

TEST(HumanoidPrepare, WalkPoseTransfersMoreWeightThanRun) {
  Render::GL::VariationParams const variation =
      Render::GL::VariationParams::from_seed(4242U);

  Render::GL::HumanoidGaitDescriptor walk{};
  walk.state = Render::GL::HumanoidMotionState::Walk;
  walk.normalized_speed = 1.0F;
  walk.cycle_time = 0.90F / std::max(0.1F, variation.walk_speed_mult);
  walk.cycle_phase = 0.12F;
  walk.locomotion_blend = 1.0F;
  walk.stride_distance = 2.35F * walk.cycle_time;

  auto run = walk;
  run.state = Render::GL::HumanoidMotionState::Run;
  run.run_blend = 1.0F;
  run.cycle_time = 0.52F / std::max(0.1F, variation.walk_speed_mult);
  run.stride_distance = 5.10F * run.cycle_time;

  Render::GL::HumanoidPose walk_pose{};
  Render::GL::HumanoidPose run_pose{};
  Render::GL::HumanoidRendererBase::compute_locomotion_pose(
      4242U, 0.12F, walk, variation, walk_pose);
  Render::GL::HumanoidRendererBase::compute_locomotion_pose(
      4242U, 0.12F, run, variation, run_pose);

  EXPECT_GT(std::abs(walk_pose.pelvis_pos.x()),
            std::abs(run_pose.pelvis_pos.x()) + 0.004F);
}

TEST(HumanoidPrepare, TurnSignalIntroducesUpperBodyTwistDuringLocomotion) {
  Render::GL::VariationParams const variation =
      Render::GL::VariationParams::from_seed(777U);

  Render::GL::HumanoidGaitDescriptor neutral{};
  neutral.state = Render::GL::HumanoidMotionState::Walk;
  neutral.normalized_speed = 1.0F;
  neutral.cycle_time = 0.92F / std::max(0.1F, variation.walk_speed_mult);
  neutral.cycle_phase = 0.25F;
  neutral.locomotion_blend = 1.0F;

  auto turning = neutral;
  turning.turn_amount = 0.8F;

  Render::GL::HumanoidPose neutral_pose{};
  Render::GL::HumanoidPose turning_pose{};
  Render::GL::HumanoidRendererBase::compute_locomotion_pose(
      777U, 0.25F, neutral, variation, neutral_pose);
  Render::GL::HumanoidRendererBase::compute_locomotion_pose(
      777U, 0.25F, turning, variation, turning_pose);

  float const neutral_twist = neutral_pose.shoulder_l.z() - neutral_pose.shoulder_r.z();
  float const turning_twist = turning_pose.shoulder_l.z() - turning_pose.shoulder_r.z();
  EXPECT_GT(turning_twist, neutral_twist + 0.01F);
  EXPECT_GT(turning_pose.pelvis_pos.x(), neutral_pose.pelvis_pos.x());
}

TEST(HumanoidPrepare, AccelerationShapesLeanStrideAndBrakingSink) {
  Render::GL::VariationParams const variation =
      Render::GL::VariationParams::from_seed(991U);

  Render::GL::HumanoidGaitDescriptor accelerating{};
  accelerating.state = Render::GL::HumanoidMotionState::Walk;
  accelerating.normalized_speed = 1.0F;
  accelerating.cycle_time = 0.92F / std::max(0.1F, variation.walk_speed_mult);
  accelerating.cycle_phase = 0.30F;
  accelerating.locomotion_blend = 1.0F;
  accelerating.acceleration = 1.5F;

  auto braking = accelerating;
  braking.acceleration = -1.5F;

  Render::GL::HumanoidPose accelerating_pose{};
  Render::GL::HumanoidPose braking_pose{};
  Render::GL::HumanoidRendererBase::compute_locomotion_pose(
      991U, 0.30F, accelerating, variation, accelerating_pose);
  Render::GL::HumanoidRendererBase::compute_locomotion_pose(
      991U, 0.30F, braking, variation, braking_pose);

  float const accelerating_stride =
      std::abs(accelerating_pose.foot_l.z() - accelerating_pose.foot_r.z());
  float const braking_stride =
      std::abs(braking_pose.foot_l.z() - braking_pose.foot_r.z());
  EXPECT_GT(accelerating_pose.shoulder_l.z(), braking_pose.shoulder_l.z() + 0.01F);
  EXPECT_GT(accelerating_pose.pelvis_pos.y(), braking_pose.pelvis_pos.y() + 0.005F);
  EXPECT_GT(accelerating_stride, braking_stride + 0.01F);
}

TEST(HumanoidPrepare, TurnSignalCreatesInnerOuterStrideAsymmetry) {
  Render::GL::VariationParams const variation =
      Render::GL::VariationParams::from_seed(555U);

  Render::GL::HumanoidGaitDescriptor neutral{};
  neutral.state = Render::GL::HumanoidMotionState::Walk;
  neutral.normalized_speed = 1.0F;
  neutral.cycle_time = 0.92F / std::max(0.1F, variation.walk_speed_mult);
  neutral.cycle_phase = 0.25F;
  neutral.locomotion_blend = 1.0F;

  Render::GL::HumanoidGaitDescriptor turning{};
  turning = neutral;
  turning.turn_amount = 0.8F;

  Render::GL::HumanoidPose neutral_pose{};
  Render::GL::HumanoidPose turning_pose{};
  Render::GL::HumanoidRendererBase::compute_locomotion_pose(
      555U, 0.25F, neutral, variation, neutral_pose);
  Render::GL::HumanoidRendererBase::compute_locomotion_pose(
      555U, 0.25F, turning, variation, turning_pose);

  auto const reach_ratio = [](const Render::GL::HumanoidPose& pose) {
    return std::abs(pose.foot_l.z()) / std::max(1.0e-4F, std::abs(pose.foot_r.z()));
  };
  EXPECT_GT(reach_ratio(turning_pose), reach_ratio(neutral_pose) * 1.05F);
  EXPECT_LT(turning_pose.foot_l.x(), turning_pose.foot_r.x());
}

TEST(HumanoidPrepare, RunPoseCanEnterFlightPhaseWhileWalkKeepsSupport) {
  Render::GL::VariationParams const variation =
      Render::GL::VariationParams::from_seed(6600U);

  Render::GL::HumanoidGaitDescriptor walk{};
  walk.state = Render::GL::HumanoidMotionState::Walk;
  walk.normalized_speed = 1.0F;
  walk.cycle_time = 0.90F / std::max(0.1F, variation.walk_speed_mult);
  walk.cycle_phase = 0.48F;
  walk.locomotion_blend = 1.0F;
  walk.stride_distance = 2.35F * walk.cycle_time;

  auto run = walk;
  run.state = Render::GL::HumanoidMotionState::Run;
  run.run_blend = 1.0F;
  run.cycle_time = 0.52F / std::max(0.1F, variation.walk_speed_mult);
  run.stride_distance = 5.10F * run.cycle_time;

  Render::GL::HumanoidPose walk_pose{};
  Render::GL::HumanoidPose run_pose{};
  Render::GL::HumanoidRendererBase::compute_locomotion_pose(
      6600U, 0.48F, walk, variation, walk_pose);
  Render::GL::HumanoidRendererBase::compute_locomotion_pose(
      6600U, 0.48F, run, variation, run_pose);

  float const walk_ground =
      Render::GL::HumanProportions::GROUND_Y + walk_pose.foot_y_offset;
  float const run_ground =
      Render::GL::HumanProportions::GROUND_Y + run_pose.foot_y_offset;

  auto const sole_y = [](float ankle_y, float pitch) {
    return ankle_y - Animation::humanoid_foot_contact_lift(pitch);
  };

  EXPECT_TRUE(
      sole_y(walk_pose.foot_l.y(), walk_pose.foot_pitch_l) <= walk_ground + 0.002F ||
      sole_y(walk_pose.foot_r.y(), walk_pose.foot_pitch_r) <= walk_ground + 0.002F);
  EXPECT_GT(sole_y(run_pose.foot_l.y(), run_pose.foot_pitch_l), run_ground + 0.004F);
  EXPECT_GT(sole_y(run_pose.foot_r.y(), run_pose.foot_pitch_r), run_ground + 0.004F);
}

TEST(HumanoidPrepare, TemplatePrewarmRenderLeavesHumanoidAnimationStateUntouched) {
  SnapshotPrewarmRenderer const renderer;
  Engine::Core::StandaloneEntity entity_scratch(9);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>();
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Knight;
  unit->owner_id = 1;
  unit->max_health = 100;
  unit->health = 100;

  auto* transform = entity.add_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);
  transform->position = {0.0F, 0.0F, 0.0F};
  transform->rotation = {0.0F, 0.0F, 0.0F};
  transform->scale = {1.0F, 1.0F, 1.0F};

  auto* humanoid_state =
      entity.add_component<Render::Creature::HumanoidAnimationStateComponent>();
  ASSERT_NE(humanoid_state, nullptr);
  humanoid_state->initialized = true;
  humanoid_state->idle_duration = 1.5F;
  humanoid_state->last_sample_time = 5.0F;
  humanoid_state->locomotion.initialized = true;
  humanoid_state->locomotion.phase = 0.42F;
  humanoid_state->locomotion.phase_bias = 0.11F;
  humanoid_state->locomotion.filtered_speed = 0.65F;
  humanoid_state->locomotion.filtered_acceleration = -0.25F;
  humanoid_state->locomotion.filtered_turn = 0.30F;

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.entity = &entity;
  ctx.template_prewarm = true;
  ctx.force_single_soldier = true;
  ctx.animation_time = 8.0F;

  Render::GL::TemplateRecorder recorder;
  renderer.render(ctx, recorder);

  EXPECT_FLOAT_EQ(humanoid_state->idle_duration, 1.5F);
  EXPECT_FLOAT_EQ(humanoid_state->last_sample_time, 5.0F);
  EXPECT_FLOAT_EQ(humanoid_state->locomotion.phase, 0.42F);
  EXPECT_FLOAT_EQ(humanoid_state->locomotion.phase_bias, 0.11F);
  EXPECT_FLOAT_EQ(humanoid_state->locomotion.filtered_speed, 0.65F);
  EXPECT_FLOAT_EQ(humanoid_state->locomotion.filtered_acceleration, -0.25F);
  EXPECT_FLOAT_EQ(humanoid_state->locomotion.filtered_turn, 0.30F);
}

TEST(HumanoidPrepare, WalkRunTransitionKeepsPlaybackGroundingCoherent) {
  auto const archetype_id = Render::Creature::ArchetypeRegistry::k_humanoid_base;

  Render::Creature::HumanoidAnimationStateComponent persistent{};
  Render::Humanoid::HumanoidLocomotionInputs inputs{};
  inputs.variation = Render::GL::VariationParams::from_seed(2222U);
  inputs.entity_forward = QVector3D(0.0F, 0.0F, 1.0F);
  inputs.locomotion_direction = QVector3D(0.0F, 0.0F, 1.0F);
  inputs.individuality.gait_phase_offset = 0.125F;
  inputs.persistent_state = &persistent;
  inputs.allow_persistent_update = true;
  inputs.anim.movement_state = Render::Creature::MovementAnimationState::Walk;
  inputs.animation_time = 1.40F;
  inputs.move_speed = 2.35F;

  auto const walking = Render::Humanoid::build_humanoid_locomotion_state(inputs);

  inputs.anim.movement_state = Render::Creature::MovementAnimationState::Run;
  inputs.animation_time += 1.0F / 60.0F;
  inputs.move_speed = 5.10F;
  auto const running = Render::Humanoid::build_humanoid_locomotion_state(inputs);

  Render::GL::HumanoidAnimationContext walk_ctx{};
  walk_ctx.inputs.movement_state = Render::Creature::MovementAnimationState::Walk;
  walk_ctx.gait.state = walking.gait.state;
  walk_ctx.gait = walking.gait;

  Render::GL::HumanoidAnimationContext run_ctx{};
  run_ctx.inputs.movement_state = Render::Creature::MovementAnimationState::Run;
  run_ctx.gait.state = running.gait.state;
  run_ctx.gait = running.gait;

  Render::GL::HumanoidPose walk_pose{};
  Render::GL::HumanoidPose run_pose{};
  Render::GL::HumanoidRendererBase::compute_locomotion_pose(
      2222U, inputs.animation_time, walking.gait, inputs.variation, walk_pose);
  Render::GL::HumanoidRendererBase::compute_locomotion_pose(
      2222U, inputs.animation_time, running.gait, inputs.variation, run_pose);

  auto const walk_contact = Render::Creature::Pipeline::grounded_humanoid_contact_y(
      archetype_id, Render::Creature::Bpat::k_species_humanoid, walk_pose, walk_ctx);
  auto const run_contact = Render::Creature::Pipeline::grounded_humanoid_contact_y(
      archetype_id, Render::Creature::Bpat::k_species_humanoid, run_pose, run_ctx);

  EXPECT_LT(std::abs(Render::Creature::Pipeline::humanoid_phase_for_anim(walk_ctx) -
                     Render::Creature::Pipeline::humanoid_phase_for_anim(run_ctx)),
            0.08F);
  EXPECT_LT(std::abs(walk_contact - run_contact), 0.12F);
}

TEST(HumanoidPrepare, IdleArchersKeepNeutralBowReadyPhase) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(42);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Archer;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  ctx.entity = &entity;

  Render::GL::AnimationInputs anim{};
  anim.time = 3.0F;

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, anim, test_runtime(0U), prep);

  auto const& requests = prep.bodies.requests();
  ASSERT_EQ(requests.size(), 1U);
  EXPECT_EQ(requests.front().state, Render::Creature::AnimationStateId::Idle);
  EXPECT_FLOAT_EQ(requests.front().phase, 0.5F);
}

TEST(HumanoidPrepare, FormationAmbientIdlesStaggerAndRotatePerSoldier) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.allow_template_cache = false;
  ctx.force_humanoid_lod = true;
  ctx.forced_humanoid_lod = Render::Creature::CreatureLOD::Full;

  Engine::Core::StandaloneEntity entity_scratch(4242);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 2.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Knight;
  unit->render_individuals_per_unit_override = 12;
  entity.add_component<Engine::Core::TransformComponent>();
  ctx.entity = &entity;

  std::array<std::unordered_set<std::uint8_t>, 12> variants_by_soldier;
  std::unordered_set<std::uint8_t> concurrently_active_variants;
  bool saw_staggered_activation = false;
  bool saw_distinct_concurrent_variants = false;

  constexpr float k_step = Animation::k_ambient_max_step;
  for (std::uint32_t frame = 0U; frame < 1600U; ++frame) {
    Render::GL::AnimationInputs anim{};
    anim.time = static_cast<float>(frame) * k_step;
    anim.idle_duration = anim.time;
    ctx.animation_time = anim.time;

    Render::Humanoid::HumanoidPreparation prep;
    Render::Humanoid::prepare_humanoid_instances(
        owner, ctx, anim, test_runtime(frame), prep);

    std::size_t active_count = 0U;
    concurrently_active_variants.clear();
    for (auto const& request : prep.bodies.requests()) {
      auto variant = request.clip_variant;
      if (request.full_body_blend.active()) {
        variant = request.full_body_blend.clip_variant;
      }
      if (variant == 0U) {
        continue;
      }
      ++active_count;
      ASSERT_LT(request.instance_index, variants_by_soldier.size());
      variants_by_soldier[request.instance_index].insert(variant);
      concurrently_active_variants.insert(variant);
    }
    saw_staggered_activation =
        saw_staggered_activation ||
        (active_count > 0U && active_count < variants_by_soldier.size());
    saw_distinct_concurrent_variants =
        saw_distinct_concurrent_variants || concurrently_active_variants.size() >= 2U;
  }

  EXPECT_TRUE(saw_staggered_activation);
  EXPECT_TRUE(saw_distinct_concurrent_variants);
  for (std::size_t soldier = 0; soldier < variants_by_soldier.size(); ++soldier) {
    EXPECT_EQ(variants_by_soldier[soldier].size(), 4U)
        << "soldier " << soldier << " did not rotate through every ambient idle";
  }
}

TEST(HumanoidPrepare, PopulationLodKeepsRepresentativesAcrossFormationFootprint) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.allow_template_cache = false;
  ctx.max_rendered_individuals = 4;

  Engine::Core::StandaloneEntity entity_scratch(43);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Knight;
  unit->render_individuals_per_unit_override = 16;
  entity.add_component<Engine::Core::TransformComponent>();
  ctx.entity = &entity;

  Render::GL::AnimationInputs const anim{};
  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, anim, test_runtime(0U), prep);

  ASSERT_EQ(prep.bodies.requests().size(), 4U);
  EXPECT_EQ(prep.bodies.requests().front().instance_index, 0U);
  EXPECT_EQ(prep.bodies.requests().back().instance_index, 15U);
}

TEST(HumanoidPrepare, SwordAttackRecoveryStaysOnOutgoingClipBeforeIdle) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(42);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Knight;
  auto* persistent =
      entity.add_component<Render::Creature::HumanoidAnimationStateComponent>();
  ASSERT_NE(persistent, nullptr);
  ctx.entity = &entity;

  Render::GL::AnimationInputs attack_anim{};
  attack_anim.time = 2.5F;
  attack_anim.is_attacking = true;
  attack_anim.is_melee = true;
  attack_anim.attack_family = Engine::Core::CombatAttackFamily::Sword;
  attack_anim.combat_phase = Render::GL::CombatAnimPhase::Recover;
  attack_anim.combat_phase_progress = 0.35F;

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, attack_anim, test_runtime(0U), prep);

  auto const& attack_requests = prep.bodies.requests();
  ASSERT_EQ(attack_requests.size(), 1U);
  EXPECT_EQ(attack_requests.front().state,
            Render::Creature::AnimationStateId::AttackSword);
  float const attack_phase = attack_requests.front().phase;

  Render::GL::AnimationInputs recover_anim = attack_anim;
  recover_anim.time += 0.06F;
  recover_anim.is_attacking = false;
  recover_anim.combat_phase = Render::GL::CombatAnimPhase::Idle;
  recover_anim.combat_phase_progress = 0.0F;

  prep.clear();
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, recover_anim, test_runtime(1U), prep);

  auto const& recover_requests = prep.bodies.requests();
  ASSERT_EQ(recover_requests.size(), 1U);
  EXPECT_EQ(recover_requests.front().state,
            Render::Creature::AnimationStateId::AttackSword);
  EXPECT_GT(recover_requests.front().phase, attack_phase);

  Render::GL::AnimationInputs idle_anim = recover_anim;
  idle_anim.time += 0.28F;

  prep.clear();
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, idle_anim, test_runtime(2U), prep);

  auto const& idle_requests = prep.bodies.requests();
  ASSERT_EQ(idle_requests.size(), 1U);
  EXPECT_EQ(idle_requests.front().state,
            Render::Creature::AnimationStateId::AttackSword);
  EXPECT_TRUE(persistent->combat_visual.active);

  Render::GL::AnimationInputs settled_anim = idle_anim;
  settled_anim.time += 0.20F;

  prep.clear();
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, settled_anim, test_runtime(3U), prep);

  auto const& settled_requests = prep.bodies.requests();
  ASSERT_EQ(settled_requests.size(), 1U);
  EXPECT_EQ(settled_requests.front().state, Render::Creature::AnimationStateId::Idle);
}

TEST(HumanoidPrepare,
     PreparedSingleSoldierRequestProjectsToTerrainBeforeSubmitContactGrounding) {
  ScopedFlatTerrain const terrain(2.75F);

  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.model.translate(0.4F, 8.0F, -0.2F);
  ctx.force_single_soldier = true;

  Render::GL::AnimationInputs anim{};
  anim.time = 0.0F;
  anim.movement_state = Render::Creature::MovementAnimationState::Idle;
  anim.is_attacking = false;
  anim.is_melee = false;
  anim.is_in_hold_mode = false;
  anim.is_exiting_hold = false;
  anim.hold_exit_progress = 0.0F;
  anim.combat_phase = Render::GL::CombatAnimPhase::Idle;
  anim.combat_phase_progress = 0.0F;
  anim.attack_variant = 0;
  anim.is_hit_reacting = false;
  anim.hit_reaction_intensity = 0.0F;
  anim.is_healing = false;
  anim.healing_target_dx = 0.0F;
  anim.healing_target_dz = 0.0F;
  anim.is_constructing = false;
  anim.construction_progress = 0.0F;

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, anim, test_runtime(1U), prep);

  auto const requests = prep.bodies.requests();
  ASSERT_EQ(requests.size(), 1U);
  EXPECT_FALSE(requests[0].world_already_grounded);
  EXPECT_NEAR(requests[0].world.map(QVector3D(0.0F, 0.0F, 0.0F)).y(), 2.75F, 0.0001F);
}

TEST(HumanoidPrepare,
     PreparedMovingSingleSoldierVisibleGeometryTouchesTerrainAfterSubmit) {
  ScopedFlatTerrain const terrain(1.5F);

  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.model.translate(-0.1F, 6.0F, 0.3F);
  ctx.force_single_soldier = true;

  Render::GL::AnimationInputs anim{};
  anim.time = 0.23F;
  anim.movement_state = Render::Creature::MovementAnimationState::Walk;

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, anim, test_runtime(1U), prep);

  CountingSubmitter sink;
  Render::Creature::Pipeline::submit_preparation(prep, sink);

  ASSERT_EQ(sink.rigged_calls, 1);
  ASSERT_EQ(sink.rigged_world_y.size(), 1U);
  EXPECT_NEAR(sink.rigged_world_y.front(), 1.5F, 0.1F);
}

TEST(HumanoidPrepare,
     InfantryMovementStateDrivesRequestEvenWhenLegacyBooleansAreStale) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(77);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Spearman;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  ctx.entity = &entity;

  Render::GL::AnimationInputs anim{};
  anim.time = 0.25F;
  anim.movement_state = Render::Creature::MovementAnimationState::Idle;
  anim.movement_state = Render::Creature::MovementAnimationState::Walk;

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, anim, test_runtime(1U), prep);

  auto const requests = prep.bodies.requests();
  ASSERT_FALSE(requests.empty());
  EXPECT_EQ(requests.front().state, Render::Creature::AnimationStateId::Walk);
}

TEST(HumanoidPrepare,
     RaceWindowInitializedSnapshotDrivesWalkEvenWhenSnapshotValidFalse) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.animation_time = 11.0F;

  Engine::Core::StandaloneEntity entity_scratch(901);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 2.2F, 12.0F);
  auto* transform = entity.add_component<Engine::Core::TransformComponent>();
  auto* motion = entity.add_component<Engine::Core::MotionPresentationComponent>();
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(transform, nullptr);
  ASSERT_NE(motion, nullptr);
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::Spearman;
  transform->position = {0.0F, 0.0F, 0.0F};

  motion->initialized = true;
  motion->snapshot_valid = false;
  motion->set_state(Engine::Core::MotionPresentationState::Walk);
  motion->has_navigation_intent = true;
  motion->direction_x = 0.0F;
  motion->direction_z = 1.0F;
  motion->speed = 2.2F;
  ctx.entity = &entity;

  auto const anim = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(Render::Creature::is_moving_animation(anim.movement_state))
      << "initialized snapshot must drive walk even when snapshot_valid=false";
  EXPECT_TRUE(anim.visual_movement.is_authoritative);

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, anim, test_runtime(0U), prep);
  ASSERT_FALSE(prep.bodies.requests().empty());
  EXPECT_EQ(prep.bodies.requests().front().state,
            Render::Creature::AnimationStateId::Walk)
      << "infantry must play Walk clip during race window, not slide in Idle";
}

TEST(HumanoidPrepare, RaceWindowSnapshotRunFlagDrivesRunningEvenWithoutLiveStamina) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.animation_time = 12.0F;

  Engine::Core::StandaloneEntity entity_scratch(902);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 4.0F, 12.0F);
  auto* transform = entity.add_component<Engine::Core::TransformComponent>();
  auto* motion = entity.add_component<Engine::Core::MotionPresentationComponent>();
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(transform, nullptr);
  ASSERT_NE(motion, nullptr);
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::Spearman;
  transform->position = {0.0F, 0.0F, 0.0F};
  motion->initialized = true;
  motion->snapshot_valid = false;
  motion->set_state(Engine::Core::MotionPresentationState::Run);
  motion->direction_x = 0.0F;
  motion->direction_z = 1.0F;
  motion->speed = 4.0F;
  ctx.entity = &entity;

  auto const anim = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(Render::Creature::is_moving_animation(anim.movement_state));
  EXPECT_TRUE(Render::Creature::is_running_animation(anim.movement_state))
      << "is_running must be sourced from initialized snapshot, not live stamina";

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, anim, test_runtime(0U), prep);
  ASSERT_FALSE(prep.bodies.requests().empty());
  EXPECT_EQ(prep.bodies.requests().front().state,
            Render::Creature::AnimationStateId::Run);
}

TEST(HumanoidPrepare, HitReactionDoesNotMoveOrSquashFormationRoot) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(421);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  auto* transform = entity.add_component<Engine::Core::TransformComponent>();
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(transform, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Knight;
  transform->position = {2.0F, 0.0F, 3.0F};
  ctx.entity = &entity;

  Render::GL::AnimationInputs anim{};
  anim.movement_state = Render::Creature::MovementAnimationState::Idle;

  Render::Humanoid::HumanoidPreparation baseline_prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, anim, test_runtime(1U), baseline_prep);
  auto const baseline_requests = baseline_prep.bodies.requests();
  ASSERT_EQ(baseline_requests.size(), 1U);
  QVector3D const baseline_origin =
      baseline_requests.front().world.map(QVector3D(0.0F, 0.0F, 0.0F));
  QVector3D const baseline_head =
      baseline_requests.front().world.map(QVector3D(0.0F, 1.0F, 0.0F));

  anim.is_hit_reacting = true;
  anim.hit_reaction_intensity = 1.0F;
  anim.hit_recoil_x = 0.2F;
  anim.hit_recoil_z = 0.0F;

  Render::Humanoid::HumanoidPreparation hit_prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, anim, test_runtime(1U), hit_prep);
  auto const hit_requests = hit_prep.bodies.requests();
  ASSERT_EQ(hit_requests.size(), 1U);
  QVector3D const hit_origin =
      hit_requests.front().world.map(QVector3D(0.0F, 0.0F, 0.0F));
  QVector3D const hit_head =
      hit_requests.front().world.map(QVector3D(0.0F, 1.0F, 0.0F));

  EXPECT_NEAR(hit_origin.x(), baseline_origin.x(), 0.0001F);
  EXPECT_NEAR(hit_origin.z(), baseline_origin.z(), 0.0001F);

  float const baseline_head_height = baseline_head.y() - baseline_origin.y();
  float const hit_head_height = hit_head.y() - hit_origin.y();
  EXPECT_NEAR(hit_head_height, baseline_head_height, 0.0001F);
}

TEST(HumanoidPrepare, FogHiddenMemberIsRejectedBeforeBodyPreparation) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(422);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  auto* transform = entity.add_component<Engine::Core::TransformComponent>();
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(transform, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Knight;
  transform->position = {0.0F, 0.0F, 0.0F};
  ctx.entity = &entity;

  Game::Map::VisibilityService::Snapshot snapshot;
  snapshot.initialized = true;
  snapshot.width = 3;
  snapshot.height = 3;
  snapshot.tile_size = 1.0F;
  snapshot.half_width = 1.0F;
  snapshot.half_height = 1.0F;
  snapshot.cells.assign(
      9U, static_cast<std::uint8_t>(Game::Map::VisibilityState::Explored));
  Render::GL::SubmissionVisibilityPolicy visibility_policy;
  visibility_policy.reset(nullptr, &snapshot);
  ctx.submission_visibility = &visibility_policy;
  ctx.submission_fog_mode = Render::GL::SubmissionFogMode::VisibleOnly;

  auto& runtime = test_runtime(0U);
  runtime.reset_stats();
  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, Render::GL::AnimationInputs{}, runtime, prep);

  EXPECT_TRUE(prep.bodies.requests().empty());
  EXPECT_EQ(runtime.stats.soldiers_skipped_fog, 1U);
}

TEST(HumanoidPrepare, SoldierUsesCentralFrustumGuardBandAtScreenEdge) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::StandaloneEntity entity_scratch(423);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  auto* transform = entity.add_component<Engine::Core::TransformComponent>();
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(transform, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Knight;
  transform->position = {3.8F, 0.0F, 0.0F};
  ctx.entity = &entity;

  Render::GL::Camera camera;
  camera.set_perspective(60.0F, 1.0F, 0.1F, 100.0F);
  camera.look_at(
      QVector3D(0.0F, 0.0F, 5.0F), QVector3D(0.0F, 0.0F, 0.0F), QVector3D(0, 1, 0));
  ASSERT_FALSE(camera.is_in_frustum(
      QVector3D(transform->position.x, transform->position.y, transform->position.z),
      0.6F));

  Render::GL::SubmissionVisibilityPolicy visibility_policy;
  visibility_policy.reset(&camera, nullptr);
  ctx.camera = &camera;
  ctx.submission_visibility = &visibility_policy;
  ctx.submission_fog_mode = Render::GL::SubmissionFogMode::Ignore;

  Render::GL::reset_humanoid_render_stats();
  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, Render::GL::AnimationInputs{}, test_runtime(0U), prep);

  EXPECT_EQ(prep.bodies.requests().size(), 1U);
  EXPECT_EQ(Render::GL::get_humanoid_render_stats().soldiers_skipped_frustum, 0U);
}

} // namespace
