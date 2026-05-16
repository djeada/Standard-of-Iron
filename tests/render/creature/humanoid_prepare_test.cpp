

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
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
#include "render/creature/pipeline/preparation_common.h"
#include "render/creature/pipeline/prepared_submit.h"
#include "render/creature/pipeline/unit_visual_spec.h"
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

class CountingSubmitter : public Render::GL::ISubmitter {
public:
  int rigged_calls{0};
  int meshes{0};
  std::vector<std::uint32_t> role_color_counts;
  std::vector<float> rigged_world_y;
  std::vector<float> rigged_mesh_min_world_y;
  const QMatrix4x4* last_bone_palette{nullptr};
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
  moving_anim.inputs.is_moving = true;
  moving_anim.motion_state = Render::GL::HumanoidMotionState::Walk;
  moving_anim.gait.state = Render::GL::HumanoidMotionState::Walk;
  moving_anim.gait.cycle_phase = 0.35F;
  auto const moving = humanoid_bpat_playback_for_anim(
      roman_id, Render::Creature::Bpat::k_species_humanoid, moving_anim);
  ASSERT_TRUE(moving.has_value());
  EXPECT_EQ(moving->clip_id, registry.bpat_clip(roman_id, AnimationStateId::Walk));

  Render::GL::HumanoidAnimationContext hold_anim{};
  hold_anim.inputs.is_in_hold_mode = true;
  hold_anim.inputs.hold_entry_progress = 1.0F;
  hold_anim.motion_state = Render::GL::HumanoidMotionState::Hold;
  hold_anim.gait.state = Render::GL::HumanoidMotionState::Hold;
  hold_anim.gait.cycle_phase = 0.20F;
  auto const hold = humanoid_bpat_playback_for_anim(
      roman_id, Render::Creature::Bpat::k_species_humanoid, hold_anim);
  ASSERT_TRUE(hold.has_value());
  EXPECT_EQ(hold->clip_id, registry.bpat_clip(roman_id, AnimationStateId::Hold));
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
  attack_anim.motion_state = Render::GL::HumanoidMotionState::Attacking;
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
  attack_anim.motion_state = Render::GL::HumanoidMotionState::Hold;
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
  attack_anim.motion_state = Render::GL::HumanoidMotionState::Hold;
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
  unit->render_individuals_per_unit_override = 4;
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
  ASSERT_GE(requests.size(), 4U);

  float min_phase = requests.front().phase;
  float max_phase = requests.front().phase;
  for (auto const& request : requests) {
    EXPECT_EQ(request.state, Render::Creature::AnimationStateId::AttackSword);
    min_phase = std::min(min_phase, request.phase);
    max_phase = std::max(max_phase, request.phase);
  }

  EXPECT_GT(max_phase - min_phase, 0.001F);
  EXPECT_LT(max_phase - min_phase, 0.08F);
}

TEST(HumanoidPrepare, MovingCombatRecoveryUsesWalkClipInsteadOfAttackClip) {
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
  EXPECT_TRUE(anim.is_moving);
  EXPECT_FALSE(anim.is_attacking);
  EXPECT_EQ(anim.combat_phase, Render::GL::CombatAnimPhase::Idle);

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 0U, prep);

  auto const& requests = prep.bodies.requests();
  ASSERT_FALSE(requests.empty());
  EXPECT_EQ(requests.front().state, Render::Creature::AnimationStateId::Walk);
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
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(transform, nullptr);
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
  ctx.entity = &entity;

  auto const anim = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(anim.is_moving);
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
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(transform, nullptr);
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::Knight;
  movement->goal_x = 8.0F;
  movement->goal_y = 0.0F;
  movement->path_pending = true;
  movement->pending_request_id = 77U;
  movement->time_since_last_path_request = 0.05F;
  transform->position = {0.0F, 0.0F, 0.0F};
  ctx.entity = &entity;

  auto const anim = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(anim.is_moving);

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
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(transform, nullptr);
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::Knight;
  movement->path.emplace_back(5.0F, 0.0F);
  movement->path_index = 0;
  movement->goal_x = 5.0F;
  movement->goal_y = 0.0F;
  transform->position = {0.0F, 0.0F, 0.0F};
  ctx.entity = &entity;

  auto const anim = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(anim.is_moving);

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
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(transform, nullptr);
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::Knight;
  movement->vx = 1.4F;
  movement->vz = 0.2F;
  transform->position = {0.0F, 0.0F, 0.0F};
  ctx.entity = &entity;

  auto const anim = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(anim.is_moving);

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
  EXPECT_TRUE(anim.is_moving);
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
  EXPECT_FALSE(anim.is_moving);
  EXPECT_TRUE(anim.is_attacking);

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 0U, prep);
  ASSERT_FALSE(prep.bodies.requests().empty());
  EXPECT_NE(prep.bodies.requests().front().state,
            Render::Creature::AnimationStateId::Walk);
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
  EXPECT_TRUE(anim.is_moving);
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
  motion->snapshot_valid = true;
  motion->is_moving = true;
  motion->has_velocity = true;
  motion->source = Engine::Core::MotionPresentationSource::ForcedDisplacement;
  motion->direction_x = 1.0F;
  motion->direction_z = 0.0F;
  motion->velocity_x = 1.2F;
  motion->velocity_z = 0.0F;
  motion->speed = 1.2F;

  ctx.entity = &entity;

  auto const anim = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(anim.is_moving);
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
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(transform, nullptr);
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::Knight;
  movement->has_target = true;
  movement->target_x = 7.0F;
  movement->target_y = 0.0F;
  transform->position = {0.0F, 0.0F, 0.0F};
  ctx.entity = &entity;

  auto const anim = Render::GL::sample_anim_state(ctx);
  ASSERT_TRUE(anim.visual_movement.is_authoritative);
  ASSERT_TRUE(anim.is_moving);

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
  override_anim.is_moving = false;
  ctx.animation_override = &override_anim;

  auto const anim = Render::GL::sample_anim_state(ctx);
  ASSERT_TRUE(anim.visual_movement.is_authoritative);
  EXPECT_FALSE(anim.is_moving);

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
  override_anim.is_moving = true;
  ctx.animation_override = &override_anim;

  auto const anim = Render::GL::sample_anim_state(ctx);
  ASSERT_TRUE(anim.visual_movement.is_authoritative);
  EXPECT_TRUE(anim.is_moving);
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
  override_anim.is_moving = true;
  override_anim.is_running = true;
  ctx.animation_override = &override_anim;

  auto const anim = Render::GL::sample_anim_state(ctx);
  ASSERT_TRUE(anim.visual_movement.is_authoritative);
  EXPECT_TRUE(anim.is_moving);
  EXPECT_TRUE(anim.is_running);
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
  guard_idle.motion_state = Render::GL::HumanoidMotionState::Idle;
  guard_idle.gait.state = Render::GL::HumanoidMotionState::Idle;
  auto const idle_playback = humanoid_bpat_playback_for_anim(
      roman_id, Render::Creature::Bpat::k_species_humanoid, guard_idle);
  ASSERT_TRUE(idle_playback.has_value());
  EXPECT_EQ(idle_playback->clip_id,
            registry.bpat_clip(roman_id, AnimationStateId::Hold));

  Render::GL::HumanoidAnimationContext guard_walk{};
  guard_walk.inputs.is_guarding = true;
  guard_walk.inputs.guard_pose_progress = 1.0F;
  guard_walk.inputs.is_moving = true;
  guard_walk.motion_state = Render::GL::HumanoidMotionState::Walk;
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
  moving_guard_anim.is_moving = true;
  Render::Humanoid::HumanoidPreparation moving_guard_prep;
  Render::Humanoid::prepare_humanoid_instances(
      owner, ctx, moving_guard_anim, 0U, moving_guard_prep);
  ASSERT_EQ(moving_guard_prep.bodies.requests().size(), 1U);
  EXPECT_EQ(moving_guard_prep.bodies.requests().front().archetype, base_archetype);
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
  inputs.anim.is_moving = true;
  inputs.anim.is_running = true;
  inputs.variation.walk_speed_mult = 1.10F;
  inputs.move_speed = 4.8F;
  inputs.locomotion_direction = QVector3D(0.0F, 0.0F, 1.0F);
  inputs.movement_target = QVector3D(3.0F, 0.0F, 8.0F);
  inputs.has_movement_target = true;
  inputs.animation_time = 1.5F;
  inputs.phase_offset = 0.125F;

  auto const first = Render::Humanoid::build_humanoid_locomotion_state(inputs);
  auto const second = Render::Humanoid::build_humanoid_locomotion_state(inputs);

  EXPECT_EQ(first.motion_state, Render::GL::HumanoidMotionState::Run);
  EXPECT_EQ(first.motion_state, second.motion_state);
  EXPECT_FLOAT_EQ(first.gait.cycle_time, second.gait.cycle_time);
  EXPECT_FLOAT_EQ(first.gait.cycle_phase, second.gait.cycle_phase);
  EXPECT_FLOAT_EQ(first.gait.normalized_speed, second.gait.normalized_speed);
  EXPECT_FLOAT_EQ(first.gait.stride_distance, second.gait.stride_distance);
  EXPECT_EQ(first.has_movement_target, second.has_movement_target);
  EXPECT_GT(first.gait.cycle_time, 0.0F);
  EXPECT_GT(first.gait.stride_distance, 0.0F);
}

TEST(HumanoidPrepare, BuildLocomotionStateUsesSharedWalkRunClassifier) {
  Render::Humanoid::HumanoidLocomotionInputs inputs{};
  inputs.anim.is_moving = true;
  inputs.anim.is_running = false;
  inputs.variation.walk_speed_mult = 1.0F;
  inputs.entity_forward = QVector3D(0.0F, 0.0F, 1.0F);
  inputs.locomotion_direction = QVector3D(0.0F, 0.0F, 1.0F);
  inputs.animation_time = 1.0F;
  inputs.phase_offset = 0.0F;

  inputs.move_speed = 2.30F;
  auto const walking = Render::Humanoid::build_humanoid_locomotion_state(inputs);
  EXPECT_EQ(walking.motion_state, Render::GL::HumanoidMotionState::Walk);

  inputs.move_speed = 3.30F;
  auto const speed_promoted = Render::Humanoid::build_humanoid_locomotion_state(inputs);
  EXPECT_EQ(speed_promoted.motion_state, Render::GL::HumanoidMotionState::Run);

  inputs.move_speed = 1.20F;
  inputs.anim.is_running = true;
  auto const flag_promoted = Render::Humanoid::build_humanoid_locomotion_state(inputs);
  EXPECT_EQ(flag_promoted.motion_state, Render::GL::HumanoidMotionState::Run);
}

TEST(HumanoidPrepare, BuildLocomotionStateAnimatesIdlePhaseOverTime) {
  Render::Humanoid::HumanoidLocomotionInputs inputs{};
  inputs.anim.is_moving = false;
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

  inputs.anim.is_moving = true;
  inputs.anim.is_running = false;
  inputs.animation_time = 1.50F;
  inputs.move_speed = 2.30F;
  auto const walking = Render::Humanoid::build_humanoid_locomotion_state(inputs);

  inputs.anim.is_running = true;
  inputs.animation_time += 1.0F / 60.0F;
  inputs.move_speed = 5.10F;
  auto const running = Render::Humanoid::build_humanoid_locomotion_state(inputs);

  EXPECT_EQ(walking.motion_state, Render::GL::HumanoidMotionState::Walk);
  EXPECT_EQ(running.motion_state, Render::GL::HumanoidMotionState::Run);
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
  inputs.anim.is_moving = true;
  inputs.animation_time = 0.75F;
  inputs.move_speed = 2.30F;
  auto const seeded = Render::Humanoid::build_humanoid_locomotion_state(inputs);
  ASSERT_EQ(seeded.motion_state, Render::GL::HumanoidMotionState::Walk);

  auto const snapshot = persistent;

  inputs.allow_persistent_update = false;
  inputs.anim.is_running = true;
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
  inputs.anim.is_moving = true;
  inputs.anim.is_running = false;
  inputs.animation_time = 1.40F;
  inputs.move_speed = 2.35F;

  auto const walking = Render::Humanoid::build_humanoid_locomotion_state(inputs);

  inputs.anim.is_running = true;
  inputs.animation_time += 1.0F / 60.0F;
  inputs.move_speed = 5.10F;
  auto const running = Render::Humanoid::build_humanoid_locomotion_state(inputs);

  Render::GL::HumanoidAnimationContext walk_ctx{};
  walk_ctx.inputs.is_moving = true;
  walk_ctx.motion_state = walking.motion_state;
  walk_ctx.gait = walking.gait;

  Render::GL::HumanoidAnimationContext run_ctx{};
  run_ctx.inputs.is_moving = true;
  run_ctx.inputs.is_running = true;
  run_ctx.motion_state = running.motion_state;
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

TEST(HumanoidPrepare,
     PreparedSingleSoldierRequestProjectsToTerrainBeforeSubmitContactGrounding) {
  ScopedFlatTerrain const terrain(2.75F);

  Render::GL::HumanoidRendererBase const owner;
  Render::GL::DrawContext ctx{};
  ctx.model.translate(0.4F, 8.0F, -0.2F);
  ctx.force_single_soldier = true;

  Render::GL::AnimationInputs anim{};
  anim.time = 0.0F;
  anim.is_moving = false;
  anim.is_running = false;
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
  anim.is_moving = true;
  anim.is_running = false;

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 1U, prep);

  CountingSubmitter sink;
  Render::Creature::Pipeline::submit_preparation(prep, sink);

  ASSERT_EQ(sink.rigged_calls, 1);
  ASSERT_EQ(sink.rigged_world_y.size(), 1U);
  EXPECT_NEAR(sink.rigged_world_y.front(), 1.5F, 0.1F);
}

// Regression: render-thread race window (snapshot_valid=false, initialized=true).
// motion->is_moving is written only by finalize_motion_presentation_frame under
// entity_mutex, so reading it under the render lock is always safe even when
// snapshot_valid=false.  The legacy fallback path reads MovementComponent fields
// that the movement system may write without holding entity_mutex — a data race.
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
  motion->is_moving = true;
  motion->has_navigation_intent = true;
  motion->direction_x = 0.0F;
  motion->direction_z = 1.0F;
  motion->speed = 2.2F;
  ctx.entity = &entity;

  auto const anim = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(anim.is_moving)
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
  motion->is_moving = true;
  motion->is_running = true;
  motion->direction_x = 0.0F;
  motion->direction_z = 1.0F;
  motion->speed = 4.0F;
  ctx.entity = &entity;

  // No StaminaComponent — the run flag must come from the snapshot alone.
  auto const anim = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(anim.is_moving);
  EXPECT_TRUE(anim.is_running)
      << "is_running must be sourced from initialized snapshot, not live stamina";

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 0U, prep);
  ASSERT_FALSE(prep.bodies.requests().empty());
  EXPECT_EQ(prep.bodies.requests().front().state,
            Render::Creature::AnimationStateId::Run);
}

} // namespace
