

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <numbers>
#include <unordered_set>
#include <vector>

#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "game/map/terrain.h"
#include "game/map/terrain_service.h"
#include "render/creature/animation_state_components.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/bpat/bpat_format.h"
#include "render/creature/humanoid_clip_ids.h"
#include "render/creature/pipeline/creature_pipeline.h"
#include "render/creature/pipeline/creature_render_state.h"
#include "render/creature/pipeline/humanoid_animation_selection.h"
#include "render/creature/pipeline/preparation_common.h"
#include "render/creature/pipeline/prepared_submit.h"
#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/creature/pose_intent.h"
#include "render/creature/render_request.h"
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
#include "render/humanoid/cache_control.h"
#include "render/humanoid/formation_calculator.h"
#include "render/humanoid/humanoid_renderer_base.h"
#include "render/humanoid/humanoid_spec.h"
#include "render/humanoid/pose_controller.h"
#include "render/humanoid/prepare.h"
#include "render/humanoid/skeleton.h"
#include "render/rigged_mesh.h"
#include "render/submitter.h"
#include "render/template_cache.h"

namespace {

constexpr std::uint32_t k_roman_healer_tunic_role_count = 6;
constexpr std::uint32_t k_roman_builder_tunic_role_count = 2;
constexpr std::uint32_t k_roman_civilian_mantle_role_count = 2;
constexpr std::uint32_t k_carthage_builder_headwrap_role_count = 1;
constexpr std::uint32_t k_carthage_builder_robes_role_count = 2;
constexpr std::uint32_t k_carthage_civilian_sash_role_count = 2;

int g_pose_layer_invocations = 0;
float g_pose_layer_last_head_x = 0.0F;

void counting_pose_layer(const Render::Creature::Pipeline::HumanoidPoseLayerContext&,
                         Render::GL::HumanoidPose& io_pose) {
  ++g_pose_layer_invocations;
  io_pose.head_pos.setX(io_pose.head_pos.x() + 0.25F);
  g_pose_layer_last_head_x = io_pose.head_pos.x();
}

class CountingSubmitter : public Render::GL::ISubmitter {
public:
  int rigged_calls{0};
  int meshes{0};
  std::vector<std::uint32_t> role_color_counts;
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
  void selection_ring(const QMatrix4x4&, float, float, const QVector3D&) override {}
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
    ctx.force_single_soldier = true;
    ctx.allow_template_cache = false;

    Engine::Core::Entity entity(1);
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
      movement->has_target = true;
      movement->target_x = 2.0F;
      movement->target_y = 0.0F;
      movement->vx = 1.0F;
      movement->vz = 0.0F;
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
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(1);
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
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(1);
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
    movement->has_target = true;
    movement->target_x = 2.0F;
    movement->target_y = 0.0F;
    movement->vx = 1.0F;
    movement->vz = 0.0F;
  }
  ctx.entity = &entity;

  CountingSubmitter sink;
  renderer(ctx, sink);

  EXPECT_GT(sink.rigged_calls, 0);
  return sink.meshes;
}

auto find_archetype_id(std::string_view debug_name) -> Render::Creature::ArchetypeId {
  auto& registry = Render::Creature::ArchetypeRegistry::instance();
  for (std::size_t i = 0; i < registry.size(); ++i) {
    auto const id = static_cast<Render::Creature::ArchetypeId>(i);
    auto const* desc = registry.get(id);
    if (desc != nullptr && desc->debug_name == debug_name) {
      return id;
    }
  }
  return Render::Creature::k_invalid_archetype;
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

auto render_archer_idle_bone_palette(const char* renderer_id) -> const QMatrix4x4* {
  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto renderer = registry.get(renderer_id);
  EXPECT_TRUE(static_cast<bool>(renderer));
  if (!renderer) {
    return nullptr;
  }

  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;
  Engine::Core::Entity entity(1);
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  EXPECT_NE(unit, nullptr);
  if (unit == nullptr) {
    return nullptr;
  }
  unit->spawn_type = Game::Units::SpawnType::Archer;
  unit->nation_id =
      std::string_view(renderer_id).find("carthage") != std::string_view::npos
          ? Game::Systems::NationID::Carthage
          : Game::Systems::NationID::RomanRepublic;
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
  ctx.allow_template_cache = false;
  ctx.force_humanoid_lod = true;
  ctx.forced_humanoid_lod = Render::Creature::CreatureLOD::Full;

  Engine::Core::Entity entity(1);
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
  movement->has_target = true;
  movement->target_x = 6.0F;
  movement->target_y = 0.0F;
  movement->vx = 1.0F;
  movement->vz = 0.0F;
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

  Render::GL::advance_pose_cache_frame();
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
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(1);
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

auto render_builder_unique_role_color_count(
    const char* renderer_id, Game::Systems::NationID nation_id) -> std::size_t {
  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto renderer = registry.get(renderer_id);
  EXPECT_TRUE(static_cast<bool>(renderer));
  if (!renderer) {
    return 0U;
  }

  Render::GL::DrawContext ctx{};
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(1);
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

  std::unordered_set<std::uint32_t> unique_counts;
  for (auto const count : sink.role_color_counts) {
    unique_counts.insert(count);
  }
  return unique_counts.size();
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
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(2);
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
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(1);
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
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(1);
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
  auto
  visual_spec() const -> const Render::Creature::Pipeline::UnitVisualSpec& override {
    static const auto spec = [] {
      Render::Creature::Pipeline::UnitVisualSpec s{};
      s.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
      s.debug_name = "tests/beard_renderer";
      s.owned_legacy_slots =
          Render::Creature::Pipeline::LegacySlotMask::ArmorOverlay |
          Render::Creature::Pipeline::LegacySlotMask::ShoulderDecorations |
          Render::Creature::Pipeline::LegacySlotMask::Attachments;
      return s;
    }();
    return spec;
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
  auto
  visual_spec() const -> const Render::Creature::Pipeline::UnitVisualSpec& override {
    static const auto spec = [] {
      using Render::Creature::AnimationStateId;
      using Render::Creature::ArchetypeDescriptor;
      using Render::Creature::ArchetypeRegistry;

      auto& registry = ArchetypeRegistry::instance();
      auto const* base_desc = registry.get(ArchetypeRegistry::k_humanoid_base);
      EXPECT_NE(base_desc, nullptr);

      Render::Creature::Pipeline::UnitVisualSpec s{};
      s.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
      s.debug_name = "tests/bow_ready_regression_renderer";
      s.owned_legacy_slots = Render::Creature::Pipeline::LegacySlotMask::AllHumanoid;
      if (base_desc == nullptr) {
        return s;
      }

      ArchetypeDescriptor desc = *base_desc;
      desc.debug_name = "tests/bow_ready_regression_archetype";
      auto const attack_bow_clip =
          desc.bpat_clip[static_cast<std::size_t>(AnimationStateId::AttackBow)];
      desc.bpat_clip[static_cast<std::size_t>(AnimationStateId::Idle)] =
          attack_bow_clip;
      desc.bpat_clip[static_cast<std::size_t>(AnimationStateId::Hold)] =
          attack_bow_clip;
      s.archetype_id = registry.register_archetype(desc);
      return s;
    }();
    return spec;
  }
};

class SnapshotPrewarmRenderer : public Render::GL::HumanoidRendererBase {
public:
  auto
  visual_spec() const -> const Render::Creature::Pipeline::UnitVisualSpec& override {
    static const auto spec = [] {
      Render::Creature::Pipeline::UnitVisualSpec s{};
      s.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
      s.debug_name = "tests/snapshot_prewarm_renderer";
      s.archetype_id = Render::Creature::ArchetypeRegistry::k_humanoid_base;
      return s;
    }();
    return spec;
  }
};

TEST(HumanoidPrepare, PassIntentFromCtxIsShadowOnPrewarm) {
  Render::GL::DrawContext ctx{};
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
  Engine::Core::Entity entity(1);
  auto* unit = entity.add_component<Engine::Core::UnitComponent>();
  unit->spawn_type = Game::Units::SpawnType::Knight;
  unit->owner_id = 1;
  unit->max_health = 100;
  unit->health = 100;
  auto* transform = entity.add_component<Engine::Core::TransformComponent>();
  transform->position = {0.0F, 0.0F, 0.0F};
  transform->rotation = {0.0F, 0.0F, 0.0F};
  transform->scale = {1.0F, 1.0F, 1.0F};
  auto* renderable = entity.add_component<Engine::Core::RenderableComponent>("", "");
  renderable->renderer_id = "tests/snapshot_prewarm_renderer";
  renderable->visible = true;

  Render::GL::DrawContext ctx{};
  ctx.entity = &entity;
  ctx.template_prewarm = true;
  ctx.force_single_soldier = true;

  Render::GL::TemplateRecorder recorder;
  recorder.snapshot_mesh_cache().clear();
  renderer.render(ctx, recorder);

  EXPECT_GT(recorder.snapshot_mesh_cache().size(), 0U);
  EXPECT_TRUE(recorder.commands().empty());
}

TEST(HumanoidPrepare, PoseLayerRunsOnlyOnTemplatePrewarmRuntimePath) {
  class PoseLayerRenderer final : public Render::GL::HumanoidRendererBase {
  public:
    auto
    visual_spec() const -> const Render::Creature::Pipeline::UnitVisualSpec& override {
      static const auto spec = [] {
        Render::Creature::Pipeline::UnitVisualSpec s{};
        s.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
        s.debug_name = "tests/pose_layer_renderer";
        s.archetype_id = Render::Creature::ArchetypeRegistry::k_humanoid_base;
        s.animation_manifest.pose_layer = &counting_pose_layer;
        return s;
      }();
      return spec;
    }
  };

  PoseLayerRenderer const renderer;
  Render::GL::AnimationInputs const anim{};
  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  g_pose_layer_invocations = 0;
  g_pose_layer_last_head_x = 0.0F;

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(renderer, ctx, anim, 0U, prep);
  EXPECT_EQ(g_pose_layer_invocations, 0);

  ctx.template_prewarm = true;
  prep.clear();
  Render::Humanoid::prepare_humanoid_instances(renderer, ctx, anim, 0U, prep);
  EXPECT_EQ(g_pose_layer_invocations, 1);
  EXPECT_GT(g_pose_layer_last_head_x, 0.0F);
}

TEST(HumanoidPrepare, FacialHairUsesBakedArchetypeWithoutPostBodyDraw) {
  BeardRenderer const renderer;
  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;
  Render::GL::AnimationInputs const anim{};

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(renderer, ctx, anim, 0, prep);

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

TEST(HumanoidPrepare, BuiltInArchersUseBowReadyIdleClip) {
  using Render::Creature::AnimationStateId;
  auto& registry = Render::Creature::ArchetypeRegistry::instance();

  auto const* roman_idle_palette =
      render_archer_idle_bone_palette("troops/roman/archer");
  auto const roman_id = find_archetype_id("troops/roman/archer");
  ASSERT_NE(roman_id, Render::Creature::k_invalid_archetype);
  EXPECT_EQ(registry.bpat_clip(roman_id, AnimationStateId::Idle),
            registry.bpat_clip(roman_id, AnimationStateId::AttackBow));
  EXPECT_EQ(roman_idle_palette, request_idle_bone_palette(roman_id, 0.5F));
  EXPECT_NE(roman_idle_palette, request_idle_bone_palette(roman_id, 0.0F));

  auto const* carthage_idle_palette =
      render_archer_idle_bone_palette("troops/carthage/archer");
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
        : spec_(spec) {}

    auto
    visual_spec() const -> const Render::Creature::Pipeline::UnitVisualSpec& override {
      return spec_;
    }

  private:
    Render::Creature::Pipeline::UnitVisualSpec spec_{};
  };

  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  auto const warm_renderer = registry.get("troops/roman/swordsman");
  ASSERT_TRUE(static_cast<bool>(warm_renderer));
  if (warm_renderer) {
    Render::GL::DrawContext warm_ctx{};
    warm_ctx.allow_template_cache = false;
    Engine::Core::Entity warm_entity(997);
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
  spec.owned_legacy_slots = Render::Creature::Pipeline::LegacySlotMask::AllHumanoid;
  spec.archetype_id = archetype_id;
  spec.creature_asset_id = Render::Creature::Pipeline::k_humanoid_sword_asset;
  FixedSpecRenderer owner(spec);

  Engine::Core::Entity entity(1);
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
  movement->has_target = true;
  movement->target_x = 6.0F;
  movement->target_y = 0.0F;
  movement->vx = 1.0F;
  movement->vz = 0.0F;
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
    ctx.allow_template_cache = false;
    ctx.force_humanoid_lod = true;
    ctx.forced_humanoid_lod = Render::Creature::CreatureLOD::Full;
    ctx.animation_time = animation_time;
    ctx.entity = &entity;
    auto const anim = Render::GL::sample_anim_state(ctx);
    Render::Humanoid::HumanoidPreparation prep;
    Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 0U, prep);
    EXPECT_FALSE(prep.bodies.requests().empty());
    return prep.bodies.requests().front();
  };

  auto const first = request_for_time(0.10F);
  Render::GL::advance_pose_cache_frame();
  auto const second = request_for_time(0.70F);

  EXPECT_EQ(first.state, Render::Creature::AnimationStateId::Walk);
  EXPECT_EQ(second.state, Render::Creature::AnimationStateId::Walk);
  EXPECT_NE(first.phase, second.phase);
}

TEST(HumanoidPrepare, MultiSoldierCombatFallbackOffsetsAttackPhasePerSoldier) {
  class FixedSpecRenderer final : public Render::GL::HumanoidRendererBase {
  public:
    explicit FixedSpecRenderer(Render::Creature::Pipeline::UnitVisualSpec spec)
        : spec_(spec) {}

    auto
    visual_spec() const -> const Render::Creature::Pipeline::UnitVisualSpec& override {
      return spec_;
    }

  private:
    Render::Creature::Pipeline::UnitVisualSpec spec_{};
  };

  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  auto const warm_renderer = registry.get("troops/roman/swordsman");
  ASSERT_TRUE(static_cast<bool>(warm_renderer));
  if (warm_renderer) {
    Render::GL::DrawContext warm_ctx{};
    warm_ctx.allow_template_cache = false;
    Engine::Core::Entity warm_entity(996);
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
  spec.owned_legacy_slots = Render::Creature::Pipeline::LegacySlotMask::AllHumanoid;
  spec.archetype_id = archetype_id;
  spec.creature_asset_id = Render::Creature::Pipeline::k_humanoid_sword_asset;
  FixedSpecRenderer const owner(spec);

  Engine::Core::Entity entity(2);
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Knight;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  unit->render_individuals_per_unit_override = 15;

  Render::GL::DrawContext ctx{};
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
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 0U, prep);

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

  EXPECT_GE(states.size(), 3U);
  EXPECT_EQ(states.count(Render::Creature::AnimationStateId::AttackSword), 1U);
  EXPECT_EQ(states.count(Render::Creature::AnimationStateId::Hold), 1U);
  EXPECT_EQ(states.count(Render::Creature::AnimationStateId::Walk), 1U);

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
  EXPECT_GT(spread, 0.05F);
  EXPECT_LT(spread, 0.18F);
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
  using Render::GL::FormationCalculatorFactory;
  auto const* calculator = FormationCalculatorFactory::get_calculator(
      FormationCalculatorFactory::Nation::Roman,
      FormationCalculatorFactory::UnitCategory::BuilderConstruction);
  ASSERT_NE(calculator, nullptr);

  float const spacing = 2.0F;
  constexpr int total = 8;
  for (int idx = 0; idx < total; ++idx) {
    auto const offset =
        calculator->calculate_offset(idx, 0, idx, 1, total, spacing, 0x12345678U);
    float const yaw_rad = offset.yaw_offset * (std::numbers::pi_v<float> / 180.0F);
    float const cos_yaw = std::cos(yaw_rad);
    float const sin_yaw = std::sin(yaw_rad);

    float const world_x = cos_yaw * offset.offset_x + sin_yaw * offset.offset_z;
    float const world_z = -sin_yaw * offset.offset_x + cos_yaw * offset.offset_z;

    float const face_x = sin_yaw;
    float const face_z = cos_yaw;

    float const world_r = std::sqrt(world_x * world_x + world_z * world_z);
    ASSERT_GT(world_r, 0.001F);
    float const inward_x = -world_x / world_r;
    float const inward_z = -world_z / world_r;
    EXPECT_NEAR(face_x, inward_x, 0.01F) << "idx=" << idx;
    EXPECT_NEAR(face_z, inward_z, 0.01F) << "idx=" << idx;
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

TEST(HumanoidPrepare, HitReactionSelectionStillUsesIdleClipFamily) {
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

  auto const selection = resolve_humanoid_animation_selection(spec, anim, 19U);

  EXPECT_EQ(selection.pose.intent, Render::Creature::PoseIntent::HitReaction);
  EXPECT_EQ(selection.state, AnimationStateId::Idle);
  EXPECT_FLOAT_EQ(selection.phase, 0.61F);
  EXPECT_EQ(selection.clip_variant, 0U);
  ASSERT_TRUE(selection.clip_id.has_value());
  EXPECT_EQ(*selection.clip_id,
            ArchetypeRegistry::instance().bpat_clip(ArchetypeRegistry::k_humanoid_base,
                                                    AnimationStateId::Idle));
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
  anim.inputs.combat_visual.attack_family = Engine::Core::CombatAttackFamily::Sword;

  auto const selection = resolve_humanoid_animation_selection(spec, anim, 29U);

  EXPECT_EQ(selection.state, AnimationStateId::Walk);
  EXPECT_TRUE(selection.upper_body_overlay.active());
  EXPECT_EQ(selection.upper_body_overlay.mode, PlaybackLayerMode::UpperBodyOverlay);
  EXPECT_NE(selection.upper_body_overlay.state, selection.state);
  EXPECT_GT(selection.upper_body_overlay.weight, 0.7F);
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
  anim.inputs.combat_visual.attack_family = Engine::Core::CombatAttackFamily::Sword;

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
  anim.inputs.combat_visual.attack_family = Engine::Core::CombatAttackFamily::Sword;

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
  ASSERT_GT(overlay_palette.size(), hand_index);
  ASSERT_GT(attack_palette.size(), hand_index);
  ASSERT_GT(walk_palette.size(), hand_index);

  EXPECT_LT(max_matrix_delta(overlay_palette[foot_index], walk_palette[foot_index]),
            1.0e-5F);
  EXPECT_GT(max_matrix_delta(overlay_palette[foot_index], attack_palette[foot_index]),
            1.0e-3F);
  EXPECT_LT(max_matrix_delta(overlay_palette[hand_index], attack_palette[hand_index]),
            1.0e-5F);
  EXPECT_GT(max_matrix_delta(overlay_palette[hand_index], walk_palette[hand_index]),
            1.0e-3F);
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
        : spec_(spec) {}

    auto
    visual_spec() const -> const Render::Creature::Pipeline::UnitVisualSpec& override {
      return spec_;
    }

  private:
    Render::Creature::Pipeline::UnitVisualSpec spec_{};
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
  spec.owned_legacy_slots = Render::Creature::Pipeline::LegacySlotMask::AllHumanoid;
  spec.archetype_id = archetype_id;
  spec.animation_manifest.variant_table = &k_variant_table;
  FixedSpecRenderer const owner(spec);

  Engine::Core::Entity entity(17);
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
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 0U, prep);

  ASSERT_EQ(prep.bodies.requests().size(), 16U);

  std::array<bool, 4> seen_roles{false, false, false, false};
  for (auto const& req : prep.bodies.requests()) {
    auto const role_index =
        Render::Creature::Pipeline::seeded_variant_index(req.seed, 4U);
    ASSERT_LT(role_index, seen_roles.size());
    seen_roles[role_index] = true;

    switch (role_index) {
    case 0U:
      EXPECT_EQ(req.state, AnimationStateId::AttackSword);
      EXPECT_EQ(req.clip_variant, 0U);
      break;
    case 1U:
      EXPECT_EQ(req.state, AnimationStateId::AttackSword);
      EXPECT_EQ(req.clip_variant, 1U);
      break;
    case 2U:
      EXPECT_EQ(req.state, AnimationStateId::AttackSword);
      EXPECT_EQ(req.clip_variant, 2U);
      break;
    case 3U:
      EXPECT_EQ(req.state, AnimationStateId::Hold);
      EXPECT_EQ(req.clip_variant, 0U);
      EXPECT_GT(req.phase, 0.99F);
      break;
    default:
      FAIL() << "unexpected construction role index";
      break;
    }
  }

  EXPECT_TRUE(std::all_of(
      seen_roles.begin(), seen_roles.end(), [](bool seen) { return seen; }));
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

TEST(HumanoidPrepare, BuiltInBuildersUseMixedConstructionToolSets) {
  EXPECT_GT(render_builder_unique_role_color_count(
                "troops/roman/builder", Game::Systems::NationID::RomanRepublic),
            1U);
  EXPECT_GT(render_builder_unique_role_color_count("troops/carthage/builder",
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

TEST(HumanoidPrepare, HoldModeSpearAttackPlaybackUsesHoldClip) {
  using Render::Creature::AnimationStateId;
  using Render::Creature::Pipeline::humanoid_bpat_playback_for_anim;

  Render::GL::EntityRendererRegistry renderer_registry;
  Render::GL::register_built_in_entity_renderers(renderer_registry);
  auto const renderer = renderer_registry.get("troops/roman/spearman");
  ASSERT_TRUE(static_cast<bool>(renderer));

  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(1);
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

TEST(HumanoidPrepare, HoldModeArcherAttackPlaybackUsesBowHoldClip) {
  using Render::Creature::AnimationStateId;
  using Render::Creature::Pipeline::humanoid_bpat_playback_for_anim;

  Render::GL::EntityRendererRegistry renderer_registry;
  Render::GL::register_built_in_entity_renderers(renderer_registry);
  auto const renderer = renderer_registry.get("troops/roman/archer");
  ASSERT_TRUE(static_cast<bool>(renderer));

  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(1);
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
  ctx.has_seed_override = true;
  ctx.seed_override = 0x1234ABCDU;

  Engine::Core::Entity entity(42);
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
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 0U, prep);

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

  EXPECT_GE(states.size(), 3U);
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
  ctx.force_single_soldier = true;
  ctx.animation_time = 10.0F;

  Engine::Core::Entity entity(42);
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  auto* movement = entity.add_component<Engine::Core::MovementComponent>();
  auto* combat_state = entity.add_component<Engine::Core::CombatStateComponent>();
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(combat_state, nullptr);
  ASSERT_NE(add_walk_motion(entity, unit->speed), nullptr);
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::Knight;
  movement->has_target = true;
  movement->target_x = 4.0F;
  movement->target_y = 0.0F;
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
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 0U, prep);

  auto const& requests = prep.bodies.requests();
  ASSERT_FALSE(requests.empty());
  EXPECT_EQ(requests.front().state, Render::Creature::AnimationStateId::AttackSword);
}

TEST(HumanoidPrepare, CombatAdvancePreservesWalkClipWhileClosingDistance) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.animation_time = 10.0F;

  Engine::Core::Entity entity(142);
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  auto* movement = entity.add_component<Engine::Core::MovementComponent>();
  auto* combat_state = entity.add_component<Engine::Core::CombatStateComponent>();
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(combat_state, nullptr);
  ASSERT_NE(add_walk_motion(entity, unit->speed), nullptr);
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::Knight;
  movement->has_target = true;
  movement->target_x = 4.0F;
  movement->target_y = 0.0F;
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
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 0U, prep);

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
  ctx.animation_time = 8.0F;

  Engine::Core::Entity entity(900);
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
  ctx.animation_time = 8.0F;

  Engine::Core::Entity entity(901);
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

TEST(HumanoidPrepare, MeleeLockForcedDisplacementUsesAttackClipInsteadOfWalk) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.animation_time = 6.2F;

  Engine::Core::Entity entity(143);
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
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 0U, prep);

  auto const& requests = prep.bodies.requests();
  ASSERT_FALSE(requests.empty());
  EXPECT_EQ(requests.front().state, Render::Creature::AnimationStateId::AttackSpear);
}

TEST(HumanoidPrepare, CommandedMovementWithoutVelocityStillBuildsStride) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.animation_time = 3.0F;

  Engine::Core::Entity entity(43);
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
  movement->has_target = true;
  movement->target_x = 6.0F;
  movement->target_y = 0.0F;
  movement->vx = 0.0F;
  movement->vz = 0.0F;
  transform->position = {0.0F, 0.0F, 0.0F};
  transform->rotation = {0.0F, 0.0F, 0.0F};
  transform->scale = {1.0F, 1.0F, 1.0F};
  motion->set_state(Engine::Core::MotionPresentationState::Walk);
  motion->has_navigation_intent = true;
  motion->has_movement_target = true;
  motion->movement_target_x = movement->target_x;
  motion->movement_target_z = movement->target_y;
  motion->speed = unit->speed;
  ctx.entity = &entity;

  auto const anim = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(Render::Creature::is_moving_animation(anim.movement_state));
  EXPECT_FALSE(anim.is_attacking);

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 0U, prep);

  auto* humanoid_state =
      entity.get_component<Render::Creature::HumanoidAnimationStateComponent>();
  ASSERT_NE(humanoid_state, nullptr);
  ASSERT_FALSE(prep.bodies.requests().empty());
  EXPECT_EQ(prep.bodies.requests().front().state,
            Render::Creature::AnimationStateId::Walk);
  EXPECT_EQ(humanoid_state->locomotion_state, Render::GL::HumanoidMotionState::Walk);
  EXPECT_GT(humanoid_state->filtered_speed, 0.0F);
  EXPECT_GT(humanoid_state->locomotion_blend, 0.0F);
}

TEST(HumanoidPrepare, PathPendingMovementStillTriggersWalkAnimation) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.animation_time = 4.0F;

  Engine::Core::Entity entity(44);
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
  movement->goal_x = 8.0F;
  movement->goal_y = 0.0F;
  movement->path_pending = true;
  movement->pending_request_id = 77U;
  movement->time_since_last_path_request = 0.05F;
  transform->position = {0.0F, 0.0F, 0.0F};
  motion->set_state(Engine::Core::MotionPresentationState::Walk);
  motion->has_navigation_intent = true;
  motion->has_movement_target = true;
  motion->movement_target_x = movement->goal_x;
  motion->movement_target_z = movement->goal_y;
  motion->speed = unit->speed;
  ctx.entity = &entity;

  auto const anim = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(Render::Creature::is_moving_animation(anim.movement_state));

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 0U, prep);
  ASSERT_FALSE(prep.bodies.requests().empty());
  EXPECT_EQ(prep.bodies.requests().front().state,
            Render::Creature::AnimationStateId::Walk);
}

TEST(HumanoidPrepare, QueuedWaypointMovementStillTriggersWalkAnimation) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.animation_time = 4.5F;

  Engine::Core::Entity entity(45);
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
  movement->path.emplace_back(5.0F, 0.0F);
  movement->path_index = 0;
  movement->goal_x = 5.0F;
  movement->goal_y = 0.0F;
  transform->position = {0.0F, 0.0F, 0.0F};
  motion->set_state(Engine::Core::MotionPresentationState::Walk);
  motion->has_navigation_intent = true;
  motion->has_movement_target = true;
  motion->movement_target_x = movement->goal_x;
  motion->movement_target_z = movement->goal_y;
  motion->speed = unit->speed;
  ctx.entity = &entity;

  auto const anim = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(Render::Creature::is_moving_animation(anim.movement_state));

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 0U, prep);
  ASSERT_FALSE(prep.bodies.requests().empty());
  EXPECT_EQ(prep.bodies.requests().front().state,
            Render::Creature::AnimationStateId::Walk);
}

TEST(HumanoidPrepare, VelocityOnlyMovementStillTriggersWalkAnimation) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.animation_time = 5.0F;

  Engine::Core::Entity entity(46);
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
  movement->vx = 1.4F;
  movement->vz = 0.2F;
  transform->position = {0.0F, 0.0F, 0.0F};
  motion->set_state(Engine::Core::MotionPresentationState::Walk);
  motion->has_velocity = true;
  motion->velocity_x = movement->vx;
  motion->velocity_z = movement->vz;
  motion->speed = std::sqrt(movement->vx * movement->vx + movement->vz * movement->vz);
  ctx.entity = &entity;

  auto const anim = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(Render::Creature::is_moving_animation(anim.movement_state));

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 0U, prep);
  ASSERT_FALSE(prep.bodies.requests().empty());
  EXPECT_EQ(prep.bodies.requests().front().state,
            Render::Creature::AnimationStateId::Walk);
}

TEST(HumanoidPrepare, ChaseIntentOutOfRangeTriggersWalkAnimation) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.animation_time = 5.5F;

  Engine::Core::World world;
  Engine::Core::Entity attacker(47);
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
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 0U, prep);
  ASSERT_FALSE(prep.bodies.requests().empty());
  EXPECT_EQ(prep.bodies.requests().front().state,
            Render::Creature::AnimationStateId::Walk);
}

TEST(HumanoidPrepare, ChaseIntentInRangePreservesAttackInsteadOfWalk) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.animation_time = 6.0F;

  Engine::Core::World world;
  Engine::Core::Entity attacker(48);
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
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 0U, prep);
  ASSERT_FALSE(prep.bodies.requests().empty());
  EXPECT_EQ(prep.bodies.requests().front().state,
            Render::Creature::AnimationStateId::Idle);
}

TEST(HumanoidPrepare, ActiveMoveSegmentInRangeStillTriggersWalkAnimation) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.animation_time = 6.1F;

  Engine::Core::World world;
  Engine::Core::Entity attacker(148);
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
  movement->has_target = true;
  movement->target_x = 0.6F;
  movement->target_y = 0.0F;
  movement->goal_x = 0.6F;
  movement->goal_y = 0.0F;
  movement->vx = 0.05F;
  movement->vz = 0.0F;
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
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 0U, prep);
  ASSERT_FALSE(prep.bodies.requests().empty());
  EXPECT_EQ(prep.bodies.requests().front().state,
            Render::Creature::AnimationStateId::Walk);
}

TEST(HumanoidPrepare, MotionSnapshotDrivesWalkWithoutMovementComponentIntent) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.animation_time = 6.2F;

  Engine::Core::Entity entity(149);
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
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 0U, prep);
  ASSERT_FALSE(prep.bodies.requests().empty());
  EXPECT_EQ(prep.bodies.requests().front().state,
            Render::Creature::AnimationStateId::Walk);
}

TEST(HumanoidPrepare, SampledMovementSnapshotDrivesPreparationAfterIntentClears) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.animation_time = 6.5F;

  Engine::Core::Entity entity(49);
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
  movement->has_target = true;
  movement->target_x = 7.0F;
  movement->target_y = 0.0F;
  transform->position = {0.0F, 0.0F, 0.0F};
  ctx.entity = &entity;

  auto const anim = Render::GL::sample_anim_state(ctx);
  ASSERT_TRUE(anim.visual_movement.is_authoritative);
  ASSERT_TRUE(Render::Creature::is_moving_animation(anim.movement_state));

  movement->has_target = false;
  movement->target_x = 0.0F;
  movement->target_y = 0.0F;
  movement->goal_x = 0.0F;
  movement->goal_y = 0.0F;

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 0U, prep);

  auto* humanoid_state =
      entity.get_component<Render::Creature::HumanoidAnimationStateComponent>();
  ASSERT_NE(humanoid_state, nullptr);
  ASSERT_FALSE(prep.bodies.requests().empty());
  EXPECT_EQ(prep.bodies.requests().front().state,
            Render::Creature::AnimationStateId::Walk);
  EXPECT_EQ(humanoid_state->locomotion_state, Render::GL::HumanoidMotionState::Walk);
  EXPECT_GT(humanoid_state->filtered_speed, 0.0F);
}

TEST(HumanoidPrepare, IdleAnimationOverrideSuppressesLiveMovementIntent) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.animation_time = 7.0F;

  Engine::Core::Entity entity(50);
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 2.0F, 12.0F);
  auto* movement = entity.add_component<Engine::Core::MovementComponent>();
  auto* transform = entity.add_component<Engine::Core::TransformComponent>();
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(transform, nullptr);
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::Knight;
  movement->has_target = true;
  movement->target_x = 9.0F;
  movement->target_y = 0.0F;
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
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 0U, prep);
  ASSERT_FALSE(prep.bodies.requests().empty());
  EXPECT_EQ(prep.bodies.requests().front().state,
            Render::Creature::AnimationStateId::Idle);
}

TEST(HumanoidPrepare, MovingAnimationOverrideBuildsStrideWithoutLiveIntent) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.animation_time = 7.5F;

  Engine::Core::Entity entity(51);
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
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 0U, prep);

  ASSERT_FALSE(prep.bodies.requests().empty());
  EXPECT_EQ(prep.bodies.requests().front().state,
            Render::Creature::AnimationStateId::Walk);
}

TEST(HumanoidPrepare, MovingAnimationOverrideWithoutEntityBuildsAuthoritativeStride) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
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
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 0U, prep);
  ASSERT_FALSE(prep.bodies.requests().empty());
  EXPECT_EQ(prep.bodies.requests().front().state,
            Render::Creature::AnimationStateId::Run);
}

TEST(HumanoidPrepare, CommanderFpvAttacksUseExpandedAttackPhaseMapping) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;

  Engine::Core::Entity entity(42);
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
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 0U, base_prep);
  auto const& base_requests = base_prep.bodies.requests();
  ASSERT_FALSE(base_requests.empty());

  auto* commander = entity.add_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(commander, nullptr);
  commander->fpv_controlled = true;
  commander->combo_step = 1;
  anim.attack_variant = 1U;
  anim.finisher_attack = true;

  Render::Humanoid::HumanoidPreparation commander_prep;
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 0U, commander_prep);
  auto const& commander_requests = commander_prep.bodies.requests();
  ASSERT_FALSE(commander_requests.empty());

  EXPECT_GT(commander_requests.front().phase, base_requests.front().phase);
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
    ctx.force_single_soldier = true;
    ctx.allow_template_cache = false;

    Engine::Core::Entity entity(1);
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
    explicit FixedSpecRenderer(Render::Creature::ArchetypeId archetype) {
      spec_.debug_name = "tests/guard_shield_renderer";
      spec_.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
      spec_.archetype_id = archetype;
      spec_.skip_default_facial_hair_archetype = true;
    }

    auto
    visual_spec() const -> const Render::Creature::Pipeline::UnitVisualSpec& override {
      return spec_;
    }

  private:
    Render::Creature::Pipeline::UnitVisualSpec spec_{};
  };

  FixedSpecRenderer const owner(base_archetype);
  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Render::GL::AnimationInputs const base_anim{};
  Render::Humanoid::HumanoidPreparation base_prep;
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, base_anim, 0U, base_prep);
  ASSERT_EQ(base_prep.bodies.requests().size(), 1U);
  EXPECT_EQ(base_prep.bodies.requests().front().archetype, base_archetype);

  Render::GL::AnimationInputs guard_anim{};
  guard_anim.is_guarding = true;
  guard_anim.guard_pose_progress = 1.0F;
  Render::Humanoid::HumanoidPreparation guard_prep;
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, guard_anim, 0U, guard_prep);
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
      owner, ctx, moving_guard_anim, 0U, moving_guard_prep);
  ASSERT_EQ(moving_guard_prep.bodies.requests().size(), 1U);
  EXPECT_EQ(moving_guard_prep.bodies.requests().front().archetype, base_archetype);
}

TEST(HumanoidPrepare, FormationSamplingBlendsRomanInfantryShieldGuardInAndOut) {
  Render::GL::DrawContext ctx{};
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(81);
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
  EXPECT_NEAR(enter_mid.guard_pose_progress, 0.5F, 1.0e-3F);

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
  EXPECT_NEAR(exit_mid.guard_pose_progress, 0.5F, 1.0e-3F);
}

TEST(HumanoidPrepare, HoldSamplingBlendsKneelInAndOutFromVisualState) {
  Render::GL::DrawContext ctx{};
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(82);
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

TEST(HumanoidPrepare, FormationGuardDoesNotBuildHiddenKneelWhileAttacking) {
  Render::GL::DrawContext ctx{};
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(83);
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

TEST(HumanoidPrepare, MeleeAttackSmoothlyExitsExistingHoldKneel) {
  Render::GL::DrawContext ctx{};
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(84);
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
  EXPECT_NEAR(Render::GL::hold_transition_amount(melee_exit), 0.75F, 1.0e-3F);
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
    explicit FixedSpecRenderer(Render::Creature::ArchetypeId archetype) {
      spec_.debug_name = "tests/formation_guard_shield_renderer";
      spec_.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
      spec_.archetype_id = archetype;
      spec_.skip_default_facial_hair_archetype = true;
    }

    auto
    visual_spec() const -> const Render::Creature::Pipeline::UnitVisualSpec& override {
      return spec_;
    }

  private:
    Render::Creature::Pipeline::UnitVisualSpec spec_{};
  };

  auto collect_archetypes = [&](Game::Systems::NationID nation_id) {
    FixedSpecRenderer const owner(base_archetype);
    Render::GL::DrawContext ctx{};
    ctx.force_single_soldier = false;
    ctx.allow_template_cache = false;

    Engine::Core::Entity entity(
        nation_id == Game::Systems::NationID::RomanRepublic ? 101 : 102);
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
    ctx.entity = &entity;

    Render::GL::AnimationInputs guard_anim{};
    guard_anim.is_guarding = true;
    guard_anim.guard_pose_progress = 1.0F;
    Render::Humanoid::HumanoidPreparation prep;
    Render::Humanoid::prepare_humanoid_instances(owner, ctx, guard_anim, 0U, prep);

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

  ASSERT_EQ(roman_archetypes.size(), 2U);
  ASSERT_EQ(carthage_archetypes.size(), 1U);
  EXPECT_EQ(roman_archetypes.count(base_archetype), 0U);
  EXPECT_EQ(carthage_archetypes.count(base_archetype), 0U);
  EXPECT_EQ(roman_archetypes.count(*carthage_archetypes.begin()), 0U);
}

TEST(HumanoidPrepare, RomanFormationFrontShieldFlipsFaceOutwardComparedToDefaultGuard) {
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
    explicit FixedSpecRenderer(Render::Creature::ArchetypeId archetype) {
      spec_.debug_name = "tests/roman_formation_guard_face_renderer";
      spec_.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
      spec_.archetype_id = archetype;
      spec_.skip_default_facial_hair_archetype = true;
    }

    auto
    visual_spec() const -> const Render::Creature::Pipeline::UnitVisualSpec& override {
      return spec_;
    }

  private:
    Render::Creature::Pipeline::UnitVisualSpec spec_{};
  };

  auto build_guard_attachment =
      [&](bool formation_active) -> const Render::Creature::StaticAttachmentSpec* {
    FixedSpecRenderer const owner(base_archetype);
    Render::GL::DrawContext ctx{};
    ctx.force_single_soldier = true;
    ctx.allow_template_cache = false;

    Engine::Core::Entity entity(111 + (formation_active ? 1 : 0));
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
    Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 0U, prep);
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
  EXPECT_LT(QVector3D::dotProduct(default_forward, formation_forward), -0.95F);
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
    explicit FixedSpecRenderer(Render::Creature::ArchetypeId archetype) {
      spec_.debug_name = "tests/carthage_formation_guard_tilt_renderer";
      spec_.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
      spec_.archetype_id = archetype;
      spec_.skip_default_facial_hair_archetype = true;
    }

    auto
    visual_spec() const -> const Render::Creature::Pipeline::UnitVisualSpec& override {
      return spec_;
    }

  private:
    Render::Creature::Pipeline::UnitVisualSpec spec_{};
  };

  auto build_front_attachment =
      [&](bool formation_active) -> const Render::Creature::StaticAttachmentSpec* {
    FixedSpecRenderer const owner(base_archetype);
    Render::GL::DrawContext ctx{};
    ctx.force_single_soldier = true;
    ctx.allow_template_cache = false;

    Engine::Core::Entity entity(121 + (formation_active ? 1 : 0));
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
    Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 0U, prep);
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
  EXPECT_LT(std::abs(default_up.z()), 0.05F);
  EXPECT_GT(formation_up.z(), 0.15F);
}

TEST(HumanoidPrepare, AmbientIdleContextSelectsCorrectIdleClipVariant) {
  using Render::Creature::AnimationStateId;
  using Render::Creature::Pipeline::humanoid_bpat_playback_for_anim;
  using Render::GL::AmbientIdleType;

  auto const archetype_id = Render::Creature::ArchetypeRegistry::k_humanoid_base;
  auto& registry = Render::Creature::ArchetypeRegistry::instance();

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
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(1);
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
            Render::GL::k_roman_heavy_helmet_role_count +
                Render::GL::k_roman_greaves_role_count +
                Render::GL::k_roman_shoulder_cover_role_count +
                Render::GL::k_roman_scutum_role_count +
                Render::GL::k_roman_heavy_armor_role_count +
                Render::GL::k_sword_role_count + Render::GL::k_scabbard_role_count);
}

TEST(HumanoidPrepare, RomanArcherUsesRomanCloakRoleLayout) {
  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto renderer = registry.get("troops/roman/archer");
  ASSERT_TRUE(static_cast<bool>(renderer));

  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(1);
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
            Render::GL::k_roman_light_helmet_role_count +
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
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(1);
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
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(1);
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

TEST(HumanoidPrepare, RomanHealerUsesSupportLoadoutRoleLayout) {
  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto renderer = registry.get("troops/roman/healer");
  ASSERT_TRUE(static_cast<bool>(renderer));

  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(1);
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
  EXPECT_EQ(extra_role_color_count(archetype_id),
            k_roman_healer_tunic_role_count +
                Render::GL::k_roman_light_armor_role_count);
}

TEST(HumanoidPrepare, CarthageHealerUsesSupportLoadoutRoleLayout) {
  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto renderer = registry.get("troops/carthage/healer");
  ASSERT_TRUE(static_cast<bool>(renderer));

  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(1);
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
  EXPECT_EQ(extra_role_color_count(archetype_id),
            Render::GL::k_armor_light_carthage_role_count);
}

TEST(HumanoidPrepare, RomanBuilderUsesSupportLoadoutRoleLayout) {
  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto renderer = registry.get("troops/roman/builder");
  ASSERT_TRUE(static_cast<bool>(renderer));

  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(1);
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
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(1);
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
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(1);
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
            k_roman_builder_tunic_role_count + k_roman_civilian_mantle_role_count);
}

TEST(HumanoidPrepare, CarthageCivilianUsesDataLoadoutRoleLayout) {
  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto renderer = registry.get("troops/carthage/civilian");
  ASSERT_TRUE(static_cast<bool>(renderer));

  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(1);
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
                k_carthage_civilian_sash_role_count);
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
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(1);
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
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(1);
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
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(1);
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
  Render::Humanoid::prepare_humanoid_instances(renderer, ctx, anim, 0U, prep);

  ASSERT_EQ(prep.bodies.requests().size(), 1U);
  EXPECT_FALSE(prep.bodies.requests().front().world_already_grounded);
}

TEST(HumanoidPrepare, BowReadySubmittedSurfaceGroundingTouchesTerrain) {
  BowReadyRegressionRenderer const renderer;
  ScopedFlatTerrain const terrain(2.4F);

  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(1);
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
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(1);
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
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 1U, prep);

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
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(1);
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
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 0U, prep);

  EXPECT_EQ(prep.bodies.requests().size(), 3U);
}

TEST(HumanoidPrepare, ActiveSoldierCasualtiesRenderDeathRequests) {
  ScopedFlatTerrain const terrain(0.0F);

  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(1);
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
      .slot_index = 2U,
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
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 0U, prep);

  ASSERT_EQ(prep.bodies.requests().size(), 3U);
  EXPECT_EQ(std::count_if(prep.bodies.requests().begin(),
                          prep.bodies.requests().end(),
                          [](const auto& req) {
                            return req.state == Render::Creature::AnimationStateId::Die;
                          }),
            1);
}

TEST(HumanoidPrepare, BowReadySubmittedVisibleGeometryTouchesTerrain) {
  BowReadyRegressionRenderer const renderer;
  ScopedFlatTerrain const terrain(2.4F);

  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(1);
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
    ctx.force_single_soldier = true;
    ctx.allow_template_cache = false;

    Engine::Core::Entity entity(1);
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

    ASSERT_EQ(sink.rigged_calls, 1);
    ASSERT_EQ(sink.rigged_world_y.size(), 1U);
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
    ctx.force_single_soldier = true;
    ctx.allow_template_cache = false;

    Engine::Core::Entity entity(1);
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

    ASSERT_EQ(sink.rigged_calls, 1);
    ASSERT_EQ(sink.rigged_world_y.size(), 1U);
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
  ctx.has_seed_override = true;
  ctx.seed_override = 0xDEADBEEFU;
  EXPECT_EQ(Render::Creature::Pipeline::derive_unit_seed(ctx, nullptr), 0xDEADBEEFU);
}

TEST(HumanoidPrepare, DeriveUnitSeedDeterministicWithoutOverride) {
  Render::GL::DrawContext const ctx{};

  EXPECT_EQ(Render::Creature::Pipeline::derive_unit_seed(ctx, nullptr), 0U);
}

TEST(HumanoidPrepare, BuildSoldierLayoutIsDeterministic) {
  using Render::GL::FormationCalculatorFactory;
  auto const* calculator = FormationCalculatorFactory::get_calculator(
      FormationCalculatorFactory::Nation::Roman,
      FormationCalculatorFactory::UnitCategory::Infantry);
  ASSERT_NE(calculator, nullptr);

  Render::Humanoid::SoldierLayoutInputs inputs{};
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

  auto const first = Render::Humanoid::build_soldier_layout(*calculator, inputs);
  auto const second = Render::Humanoid::build_soldier_layout(*calculator, inputs);

  EXPECT_FLOAT_EQ(first.offset_x, second.offset_x);
  EXPECT_FLOAT_EQ(first.offset_z, second.offset_z);
  EXPECT_FLOAT_EQ(first.vertical_jitter, second.vertical_jitter);
  EXPECT_FLOAT_EQ(first.yaw_offset, second.yaw_offset);
  EXPECT_FLOAT_EQ(first.phase_offset, second.phase_offset);
  EXPECT_EQ(first.inst_seed, second.inst_seed);
}

TEST(HumanoidPrepare, BuildSoldierLayoutLeavesSingleSoldierUnjittered) {
  using Render::GL::FormationCalculatorFactory;
  auto const* calculator = FormationCalculatorFactory::get_calculator(
      FormationCalculatorFactory::Nation::Roman,
      FormationCalculatorFactory::UnitCategory::Infantry);
  ASSERT_NE(calculator, nullptr);

  Render::Humanoid::SoldierLayoutInputs inputs{};
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

  auto const layout = Render::Humanoid::build_soldier_layout(*calculator, inputs);

  EXPECT_FLOAT_EQ(layout.offset_x, 0.0F);
  EXPECT_FLOAT_EQ(layout.offset_z, 0.0F);
  EXPECT_FLOAT_EQ(layout.vertical_jitter, 0.0F);
  EXPECT_FLOAT_EQ(layout.yaw_offset, 0.0F);
  EXPECT_FLOAT_EQ(layout.phase_offset, 0.0F);
  EXPECT_EQ(layout.inst_seed, inputs.seed);
}

TEST(HumanoidPrepare, CavalryFormationStaggersRowsAndAlternatesRankYaw) {
  using Render::GL::FormationCalculatorFactory;
  auto const* calculator = FormationCalculatorFactory::get_calculator(
      FormationCalculatorFactory::Nation::Roman,
      FormationCalculatorFactory::UnitCategory::Cavalry);
  ASSERT_NE(calculator, nullptr);

  float const spacing = Render::GL::cavalry_formation_spacing(0.95F);

  auto const rear_left =
      calculator->calculate_offset(0, 0, 0, 3, 3, spacing, 0x12345678U);
  auto const middle_left =
      calculator->calculate_offset(3, 1, 0, 3, 3, spacing, 0x12345678U);
  auto const front_left =
      calculator->calculate_offset(6, 2, 0, 3, 3, spacing, 0x12345678U);

  EXPECT_GT(middle_left.offset_x, rear_left.offset_x + spacing * 0.25F);
  EXPECT_GT(middle_left.offset_z - rear_left.offset_z, spacing * 1.10F);
  EXPECT_GT(front_left.offset_z - middle_left.offset_z, spacing * 1.10F);
  EXPECT_FLOAT_EQ(front_left.yaw_offset, 0.0F);
  EXPECT_FLOAT_EQ(middle_left.yaw_offset, -5.0F);
  EXPECT_FLOAT_EQ(rear_left.yaw_offset, 5.0F);
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
  inputs.phase_offset = 0.125F;

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
  inputs.phase_offset = 0.0F;

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
  inputs.phase_offset = 0.0F;

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
  inputs.phase_offset = 0.125F;

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
  inputs.phase_offset = 0.125F;
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
  inputs.phase_offset = 0.125F;
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
  inputs.phase_offset = 0.125F;
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

  EXPECT_FLOAT_EQ(persistent.locomotion_phase, snapshot.locomotion_phase);
  EXPECT_FLOAT_EQ(persistent.filtered_speed, snapshot.filtered_speed);
  EXPECT_FLOAT_EQ(persistent.run_blend, snapshot.run_blend);
  EXPECT_EQ(persistent.locomotion_state, snapshot.locomotion_state);
  EXPECT_GT(preview.gait.run_blend, snapshot.run_blend);
}

TEST(HumanoidPrepare, TemplatePrewarmSamplingLeavesPersistentAnimationStateUntouched) {
  Engine::Core::Entity entity(7);
  auto* humanoid_state =
      entity.add_component<Render::Creature::HumanoidAnimationStateComponent>();
  ASSERT_NE(humanoid_state, nullptr);
  humanoid_state->initialized = true;
  humanoid_state->idle_duration = 2.0F;
  humanoid_state->last_sample_time = 4.0F;
  humanoid_state->locomotion_initialized = true;
  humanoid_state->locomotion_phase = 0.35F;

  Render::GL::DrawContext ctx{};
  ctx.entity = &entity;
  ctx.animation_time = 6.0F;
  ctx.template_prewarm = true;

  auto const anim = Render::GL::sample_anim_state(ctx);

  EXPECT_FLOAT_EQ(anim.idle_duration, 4.0F);
  EXPECT_FLOAT_EQ(humanoid_state->idle_duration, 2.0F);
  EXPECT_FLOAT_EQ(humanoid_state->last_sample_time, 4.0F);
  EXPECT_FLOAT_EQ(humanoid_state->locomotion_phase, 0.35F);
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

  float const neutral_twist =
      std::abs(neutral_pose.shoulder_l.z() - neutral_pose.shoulder_r.z());
  float const turning_twist =
      std::abs(turning_pose.shoulder_l.z() - turning_pose.shoulder_r.z());
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

  float const neutral_asymmetry =
      std::abs(std::abs(neutral_pose.foot_l.z()) - std::abs(neutral_pose.foot_r.z()));
  float const turning_asymmetry =
      std::abs(std::abs(turning_pose.foot_l.z()) - std::abs(turning_pose.foot_r.z()));
  EXPECT_GT(turning_asymmetry, neutral_asymmetry + 0.001F);
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

  EXPECT_TRUE(walk_pose.foot_l.y() <= walk_ground + 0.002F ||
              walk_pose.foot_r.y() <= walk_ground + 0.002F);
  EXPECT_GT(run_pose.foot_l.y(), run_ground + 0.004F);
  EXPECT_GT(run_pose.foot_r.y(), run_ground + 0.004F);
}

TEST(HumanoidPrepare, TemplatePrewarmRenderLeavesHumanoidAnimationStateUntouched) {
  SnapshotPrewarmRenderer const renderer;
  Engine::Core::Entity entity(9);
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
  humanoid_state->locomotion_initialized = true;
  humanoid_state->locomotion_phase = 0.42F;
  humanoid_state->locomotion_phase_bias = 0.11F;
  humanoid_state->filtered_speed = 0.65F;
  humanoid_state->filtered_acceleration = -0.25F;
  humanoid_state->filtered_turn = 0.30F;

  Render::GL::DrawContext ctx{};
  ctx.entity = &entity;
  ctx.template_prewarm = true;
  ctx.force_single_soldier = true;
  ctx.animation_time = 8.0F;

  Render::GL::TemplateRecorder recorder;
  renderer.render(ctx, recorder);

  EXPECT_FLOAT_EQ(humanoid_state->idle_duration, 1.5F);
  EXPECT_FLOAT_EQ(humanoid_state->last_sample_time, 5.0F);
  EXPECT_FLOAT_EQ(humanoid_state->locomotion_phase, 0.42F);
  EXPECT_FLOAT_EQ(humanoid_state->locomotion_phase_bias, 0.11F);
  EXPECT_FLOAT_EQ(humanoid_state->filtered_speed, 0.65F);
  EXPECT_FLOAT_EQ(humanoid_state->filtered_acceleration, -0.25F);
  EXPECT_FLOAT_EQ(humanoid_state->filtered_turn, 0.30F);
}

TEST(HumanoidPrepare, WalkRunTransitionKeepsPlaybackGroundingCoherent) {
  auto const archetype_id = Render::Creature::ArchetypeRegistry::k_humanoid_base;

  Render::Creature::HumanoidAnimationStateComponent persistent{};
  Render::Humanoid::HumanoidLocomotionInputs inputs{};
  inputs.variation = Render::GL::VariationParams::from_seed(2222U);
  inputs.entity_forward = QVector3D(0.0F, 0.0F, 1.0F);
  inputs.locomotion_direction = QVector3D(0.0F, 0.0F, 1.0F);
  inputs.phase_offset = 0.125F;
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
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(42);
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Archer;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  ctx.entity = &entity;

  Render::GL::AnimationInputs anim{};
  anim.time = 3.0F;

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 0U, prep);

  auto const& requests = prep.bodies.requests();
  ASSERT_EQ(requests.size(), 1U);
  EXPECT_EQ(requests.front().state, Render::Creature::AnimationStateId::Idle);
  EXPECT_FLOAT_EQ(requests.front().phase, 0.5F);
}

TEST(HumanoidPrepare, SwordAttackRecoveryStaysOnOutgoingClipBeforeIdle) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(42);
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
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, attack_anim, 0U, prep);

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
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, recover_anim, 1U, prep);

  auto const& recover_requests = prep.bodies.requests();
  ASSERT_EQ(recover_requests.size(), 1U);
  EXPECT_EQ(recover_requests.front().state,
            Render::Creature::AnimationStateId::AttackSword);
  EXPECT_GT(recover_requests.front().phase, attack_phase);

  Render::GL::AnimationInputs idle_anim = recover_anim;
  idle_anim.time += 0.28F;

  prep.clear();
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, idle_anim, 2U, prep);

  auto const& idle_requests = prep.bodies.requests();
  ASSERT_EQ(idle_requests.size(), 1U);
  EXPECT_EQ(idle_requests.front().state,
            Render::Creature::AnimationStateId::AttackSword);
  EXPECT_TRUE(persistent->combat_visual.active);

  Render::GL::AnimationInputs settled_anim = idle_anim;
  settled_anim.time += 0.20F;

  prep.clear();
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, settled_anim, 3U, prep);

  auto const& settled_requests = prep.bodies.requests();
  ASSERT_EQ(settled_requests.size(), 1U);
  EXPECT_EQ(settled_requests.front().state, Render::Creature::AnimationStateId::Idle);
}

TEST(HumanoidPrepare,
     PreparedSingleSoldierRequestProjectsToTerrainBeforeSubmitContactGrounding) {
  ScopedFlatTerrain const terrain(2.75F);

  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
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
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 1U, prep);

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
  ctx.model.translate(-0.1F, 6.0F, 0.3F);
  ctx.force_single_soldier = true;

  Render::GL::AnimationInputs anim{};
  anim.time = 0.23F;
  anim.movement_state = Render::Creature::MovementAnimationState::Walk;

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 1U, prep);

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
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(77);
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
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 1U, prep);

  auto const requests = prep.bodies.requests();
  ASSERT_FALSE(requests.empty());
  EXPECT_EQ(requests.front().state, Render::Creature::AnimationStateId::Walk);
}

// Regression: render-thread race window (snapshot_valid=false, initialized=true).
// MotionPresentationComponent state is written only by
// finalize_motion_presentation_frame under entity_mutex, so reading it under the
// render lock is always safe even when snapshot_valid=false. The removed legacy
// fallback path read MovementComponent fields that the movement system may write
// without holding entity_mutex.
// The fix gates on initialized instead of snapshot_valid so the snapshot is
// preferred throughout the race window.
TEST(HumanoidPrepare,
     RaceWindowInitializedSnapshotDrivesWalkEvenWhenSnapshotValidFalse) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.animation_time = 11.0F;

  Engine::Core::Entity entity(901);
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 2.2F, 12.0F);
  auto* transform = entity.add_component<Engine::Core::TransformComponent>();
  auto* motion = entity.add_component<Engine::Core::MotionPresentationComponent>();
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(transform, nullptr);
  ASSERT_NE(motion, nullptr);
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::Spearman;
  transform->position = {0.0F, 0.0F, 0.0F};
  // Simulate race window: begin set snapshot_valid=false but finalize already
  // ran in the previous tick and populated the motion snapshot correctly.
  motion->initialized = true;
  motion->snapshot_valid = false; // as-if in the begin→finalize race window
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
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 0U, prep);
  ASSERT_FALSE(prep.bodies.requests().empty());
  EXPECT_EQ(prep.bodies.requests().front().state,
            Render::Creature::AnimationStateId::Walk)
      << "infantry must play Walk clip during race window, not slide in Idle";
}

TEST(HumanoidPrepare, RaceWindowSnapshotRunFlagDrivesRunningEvenWithoutLiveStamina) {
  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.animation_time = 12.0F;

  Engine::Core::Entity entity(902);
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

  // No StaminaComponent — the run flag must come from the snapshot alone.
  auto const anim = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(Render::Creature::is_moving_animation(anim.movement_state));
  EXPECT_TRUE(Render::Creature::is_running_animation(anim.movement_state))
      << "is_running must be sourced from initialized snapshot, not live stamina";

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 0U, prep);
  ASSERT_FALSE(prep.bodies.requests().empty());
  EXPECT_EQ(prep.bodies.requests().front().state,
            Render::Creature::AnimationStateId::Run);
}

} // namespace
