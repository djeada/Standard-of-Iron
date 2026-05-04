

#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/map/terrain.h"
#include "game/map/terrain_service.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/pipeline/creature_pipeline.h"
#include "render/creature/pipeline/creature_render_state.h"
#include "render/creature/pipeline/preparation_common.h"
#include "render/creature/pipeline/prepared_submit.h"
#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/creature/render_request.h"
#include "render/entity/registry.h"
#include "render/equipment/armor/armor_heavy_carthage.h"
#include "render/equipment/armor/carthage_shoulder_cover.h"
#include "render/equipment/armor/roman_armor.h"
#include "render/equipment/armor/roman_greaves.h"
#include "render/equipment/armor/roman_shoulder_cover.h"
#include "render/equipment/helmets/carthage_heavy_helmet.h"
#include "render/equipment/helmets/roman_heavy_helmet.h"
#include "render/equipment/weapons/roman_scutum.h"
#include "render/equipment/weapons/shield_carthage.h"
#include "render/equipment/weapons/sword_renderer.h"
#include "render/gl/backend.h"
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

#include <QMatrix4x4>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QSurfaceFormat>
#include <QVector3D>
#include <algorithm>
#include <gtest/gtest.h>
#include <limits>
#include <vector>

namespace {

class CountingSubmitter : public Render::GL::ISubmitter {
public:
  int rigged_calls{0};
  int meshes{0};
  std::vector<std::uint32_t> role_color_counts;
  std::vector<float> rigged_world_y;
  std::vector<float> rigged_mesh_min_world_y;
  const QMatrix4x4 *last_bone_palette{nullptr};
  void mesh(Render::GL::Mesh *, const QMatrix4x4 &, const QVector3D &,
            Render::GL::Texture *, float, int) override {
    ++meshes;
  }
  void rigged(const Render::GL::RiggedCreatureCmd &cmd) override {
    ++rigged_calls;
    role_color_counts.push_back(cmd.role_color_count);
    rigged_world_y.push_back(cmd.world.column(3).y());
    float min_world_y = std::numeric_limits<float>::infinity();
    if (cmd.mesh != nullptr && cmd.bone_palette != nullptr) {
      for (const auto &vertex : cmd.mesh->get_vertices()) {
        QVector4D bone_local(vertex.position_bone_local[0],
                             vertex.position_bone_local[1],
                             vertex.position_bone_local[2], 1.0F);
        QVector4D skinned_local(0.0F, 0.0F, 0.0F, 0.0F);
        for (int i = 0; i < 4; ++i) {
          float const weight = vertex.bone_weights[i];
          if (weight <= 0.0F) {
            continue;
          }
          auto const bone_index =
              static_cast<std::uint32_t>(vertex.bone_indices[i]);
          if (bone_index >= cmd.bone_count) {
            continue;
          }
          skinned_local += (cmd.bone_palette[bone_index] * bone_local) * weight;
        }
        min_world_y = std::min(min_world_y,
                               cmd.world.map(skinned_local.toVector3D()).y());
      }
    }
    rigged_mesh_min_world_y.push_back(min_world_y);
    last_bone_palette = cmd.bone_palette;
  }
  void cylinder(const QVector3D &, const QVector3D &, float, const QVector3D &,
                float) override {}
  void selection_ring(const QMatrix4x4 &, float, float,
                      const QVector3D &) override {}
  void grid(const QMatrix4x4 &, const QVector3D &, float, float,
            float) override {}
  void selection_smoke(const QMatrix4x4 &, const QVector3D &, float) override {}
  void healing_beam(const QVector3D &, const QVector3D &, const QVector3D &,
                    float, float, float, float) override {}
  void healer_aura(const QVector3D &, const QVector3D &, float, float,
                   float) override {}
  void combat_dust(const QVector3D &, const QVector3D &, float, float,
                   float) override {}
  void stone_impact(const QVector3D &, const QVector3D &, float, float,
                    float) override {}
  void mode_indicator(const QMatrix4x4 &, int, const QVector3D &,
                      float) override {}
};

auto render_archer_role_color_count(const char *renderer_id) -> std::uint32_t {
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

auto render_bow_mesh_count(const char *renderer_id,
                           Game::Units::SpawnType spawn_type,
                           Game::Systems::NationID nation_id,
                           bool moving) -> int {
  struct RenderCounts {
    int meshes{0};
    int rigged_calls{0};
    std::vector<std::uint32_t> role_color_counts;
  };

  auto render_counts = [&](const char *id, Game::Units::SpawnType type,
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
    auto *unit = entity.add_component<Engine::Core::UnitComponent>(100, 100,
                                                                   1.0F, 12.0F);
    EXPECT_NE(unit, nullptr);
    if (unit == nullptr) {
      return {};
    }
    unit->spawn_type = type;
    unit->nation_id = nation;
    if (is_moving) {
      auto *movement = entity.add_component<Engine::Core::MovementComponent>();
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

auto render_runtime_mesh_count(const char *renderer_id,
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
  auto *unit =
      entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  EXPECT_NE(unit, nullptr);
  if (unit == nullptr) {
    return 0;
  }
  unit->spawn_type = spawn_type;
  unit->nation_id = nation_id;
  if (moving) {
    auto *movement = entity.add_component<Engine::Core::MovementComponent>();
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

auto find_archetype_id(std::string_view debug_name)
    -> Render::Creature::ArchetypeId {
  auto &registry = Render::Creature::ArchetypeRegistry::instance();
  for (std::size_t i = 0; i < registry.size(); ++i) {
    auto const id = static_cast<Render::Creature::ArchetypeId>(i);
    auto const *desc = registry.get(id);
    if (desc != nullptr && desc->debug_name == debug_name) {
      return id;
    }
  }
  return Render::Creature::kInvalidArchetype;
}

auto extra_role_color_count(Render::Creature::ArchetypeId archetype_id)
    -> std::uint32_t {
  auto &registry = Render::Creature::ArchetypeRegistry::instance();
  auto const *desc = registry.get(archetype_id);
  EXPECT_NE(desc, nullptr);
  if (desc == nullptr) {
    return 0U;
  }
  EXPECT_GE(desc->extra_role_color_fn_count, 1U);
  if (desc->extra_role_color_fn_count == 0U ||
      desc->extra_role_color_fns[0] == nullptr) {
    return 0U;
  }

  Render::GL::HumanoidVariant variant{};
  variant.palette.cloth = QVector3D(0.8F, 0.1F, 0.1F);
  variant.palette.leather = QVector3D(0.4F, 0.28F, 0.16F);
  variant.palette.metal = QVector3D(0.7F, 0.7F, 0.72F);
  std::array<QVector3D, 64> roles{};
  return desc->extra_role_color_fns[0](&variant, roles.data(), 0U,
                                       roles.size());
}

auto render_archer_idle_bone_palette(const char *renderer_id)
    -> const QMatrix4x4 * {
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
  auto *unit =
      entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
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

auto request_idle_bone_palette(Render::Creature::ArchetypeId archetype_id,
                               float phase) -> const QMatrix4x4 * {
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
  CreaturePipeline pipeline;
  std::array<CreatureRenderRequest, 1> requests{req};
  pipeline.submit_requests(requests, sink);

  EXPECT_GT(sink.rigged_calls, 0);
  return sink.last_bone_palette;
}

struct ScopedFlatTerrain {
  explicit ScopedFlatTerrain(float height) {
    auto &terrain = Game::Map::TerrainService::instance();
    std::vector<float> heights(9, height);
    std::vector<Game::Map::TerrainType> terrain_types(
        9, Game::Map::TerrainType::Flat);
    terrain.restore_from_serialized(3, 3, 1.0F, heights, terrain_types, {}, {},
                                    {}, Game::Map::BiomeSettings{});
  }

  ~ScopedFlatTerrain() { Game::Map::TerrainService::instance().clear(); }
};

class BeardRenderer : public Render::GL::HumanoidRendererBase {
public:
  auto visual_spec() const
      -> const Render::Creature::Pipeline::UnitVisualSpec & override {
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

  void get_variant(const Render::GL::DrawContext &, std::uint32_t,
                   Render::GL::HumanoidVariant &v) const override {
    v.facial_hair.style = Render::GL::FacialHairStyle::FullBeard;
    v.facial_hair.color = QVector3D(0.20F, 0.14F, 0.10F);
    v.facial_hair.greyness = 0.10F;
  }
};

struct ScopedOffscreenGlContext {
  ScopedOffscreenGlContext() {
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setVersion(3, 3);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);

    surface.setFormat(format);
    surface.create();

    context.setFormat(format);
    if (!context.create()) {
      return;
    }
    valid = context.makeCurrent(&surface);
  }

  ~ScopedOffscreenGlContext() {
    if (valid) {
      context.doneCurrent();
    }
  }

  [[nodiscard]] auto is_valid() const noexcept -> bool { return valid; }

  QOffscreenSurface surface;
  QOpenGLContext context;
  bool valid{false};
};

class BowReadyRegressionRenderer : public Render::GL::HumanoidRendererBase {
public:
  auto visual_spec() const
      -> const Render::Creature::Pipeline::UnitVisualSpec & override {
    static const auto spec = [] {
      using Render::Creature::AnimationStateId;
      using Render::Creature::ArchetypeDescriptor;
      using Render::Creature::ArchetypeRegistry;

      auto &registry = ArchetypeRegistry::instance();
      auto const *base_desc = registry.get(ArchetypeRegistry::kHumanoidBase);
      EXPECT_NE(base_desc, nullptr);

      Render::Creature::Pipeline::UnitVisualSpec s{};
      s.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
      s.debug_name = "tests/bow_ready_regression_renderer";
      s.owned_legacy_slots =
          Render::Creature::Pipeline::LegacySlotMask::AllHumanoid;
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
  auto visual_spec() const
      -> const Render::Creature::Pipeline::UnitVisualSpec & override {
    static const auto spec = [] {
      Render::Creature::Pipeline::UnitVisualSpec s{};
      s.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
      s.debug_name = "tests/snapshot_prewarm_renderer";
      s.archetype_id = Render::Creature::ArchetypeRegistry::kHumanoidBase;
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
  req.archetype = Render::Creature::ArchetypeRegistry::kHumanoidBase;
  req.state = Render::Creature::AnimationStateId::Idle;
  req.lod = Render::Creature::CreatureLOD::Full;
  req.pass = RenderPassIntent::Shadow;
  req.seed = 42U;
  req.world_already_grounded = true;
  prep.bodies.add_request(req);
  const auto stats = submit_preparation(prep, sink);

  EXPECT_EQ(stats.entities_submitted, 0u);
  EXPECT_EQ(sink.rigged_calls, 0);
  EXPECT_EQ(sink.meshes, 0);
}

TEST(HumanoidPrepare, MainRowStillSubmitsOneRiggedCall) {
  using namespace Render::Creature::Pipeline;

  CountingSubmitter sink;
  CreaturePreparationResult prep;
  Render::Creature::CreatureRenderRequest req{};
  req.archetype = Render::Creature::ArchetypeRegistry::kHumanoidBase;
  req.state = Render::Creature::AnimationStateId::Idle;
  req.lod = Render::Creature::CreatureLOD::Full;
  req.pass = RenderPassIntent::Main;
  req.seed = 7U;
  req.world_already_grounded = true;
  prep.bodies.add_request(req);
  const auto stats = submit_preparation(prep, sink);

  EXPECT_EQ(stats.entities_submitted, 1u);
  EXPECT_GT(sink.rigged_calls + sink.meshes, 0);
}

TEST(HumanoidPrepare, MainPassShadowDrawDoesNotRequireRendererShaderState) {
  ScopedOffscreenGlContext gl_context;
  if (!gl_context.is_valid()) {
    GTEST_SKIP() << "OpenGL offscreen context unavailable";
  }

  ScopedFlatTerrain terrain(0.0F);

  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto renderer = registry.get("troops/roman/swordsman");
  ASSERT_TRUE(static_cast<bool>(renderer));

  Render::GL::Backend backend;
  backend.initialize();

  Render::GL::DrawContext ctx{};
  ctx.backend = &backend;
  ctx.resources = backend.resources();
  ctx.allow_template_cache = true;
  ctx.force_single_soldier = true;

  Engine::Core::Entity entity(1);
  auto *unit =
      entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Knight;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  auto *transform = entity.add_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);
  transform->position = {0.0F, 0.0F, 0.0F};
  transform->scale = {1.0F, 1.0F, 1.0F};
  ctx.entity = &entity;

  CountingSubmitter sink;
  renderer(ctx, sink);

  EXPECT_GE(sink.rigged_calls, 1);
  EXPECT_GE(sink.meshes, 1)
      << "main-pass shadow draw should reach a plain ISubmitter";
}

TEST(HumanoidPrepare, TemplatePrewarmRenderWarmsSnapshotCache) {
  SnapshotPrewarmRenderer renderer;
  Engine::Core::Entity entity(1);
  auto *unit = entity.add_component<Engine::Core::UnitComponent>();
  unit->spawn_type = Game::Units::SpawnType::Knight;
  unit->owner_id = 1;
  unit->max_health = 100;
  unit->health = 100;
  auto *transform = entity.add_component<Engine::Core::TransformComponent>();
  transform->position = {0.0F, 0.0F, 0.0F};
  transform->rotation = {0.0F, 0.0F, 0.0F};
  transform->scale = {1.0F, 1.0F, 1.0F};
  auto *renderable =
      entity.add_component<Engine::Core::RenderableComponent>("", "");
  renderable->renderer_id = "tests/snapshot_prewarm_renderer";
  renderable->visible = true;

  Render::GL::DrawContext ctx{};
  ctx.entity = &entity;
  ctx.template_prewarm = true;
  ctx.force_single_soldier = true;

  Render::GL::TemplateRecorder recorder;
  recorder.snapshot_mesh_cache().clear();
  renderer.render(ctx, recorder);

  EXPECT_GT(recorder.snapshot_mesh_cache().size(), 0u);
  EXPECT_TRUE(recorder.commands().empty());
}

TEST(HumanoidPrepare, FacialHairUsesBakedArchetypeWithoutPostBodyDraw) {
  BeardRenderer renderer;
  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;
  Render::GL::AnimationInputs anim{};

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(renderer, ctx, anim, 0, prep);

  ASSERT_EQ(prep.bodies.requests().size(), 1u);
  EXPECT_TRUE(prep.post_body_draws.empty());

  auto const &req = prep.bodies.requests().front();
  EXPECT_NE(req.archetype, Render::Creature::ArchetypeRegistry::kHumanoidBase);

  auto const *desc =
      Render::Creature::ArchetypeRegistry::instance().get(req.archetype);
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
  auto &registry = Render::Creature::ArchetypeRegistry::instance();

  auto const *roman_idle_palette =
      render_archer_idle_bone_palette("troops/roman/archer");
  auto const roman_id = find_archetype_id("troops/roman/archer");
  ASSERT_NE(roman_id, Render::Creature::kInvalidArchetype);
  EXPECT_EQ(registry.bpat_clip(roman_id, AnimationStateId::Idle),
            registry.bpat_clip(roman_id, AnimationStateId::AttackBow));
  EXPECT_EQ(roman_idle_palette, request_idle_bone_palette(roman_id, 0.5F));
  EXPECT_NE(roman_idle_palette, request_idle_bone_palette(roman_id, 0.0F));

  auto const *carthage_idle_palette =
      render_archer_idle_bone_palette("troops/carthage/archer");
  auto const carthage_id = find_archetype_id("troops/carthage/archer");
  ASSERT_NE(carthage_id, Render::Creature::kInvalidArchetype);
  EXPECT_EQ(registry.bpat_clip(carthage_id, AnimationStateId::Idle),
            registry.bpat_clip(carthage_id, AnimationStateId::AttackBow));
  EXPECT_EQ(carthage_idle_palette,
            request_idle_bone_palette(carthage_id, 0.5F));
  EXPECT_NE(carthage_idle_palette,
            request_idle_bone_palette(carthage_id, 0.0F));
}

TEST(HumanoidPrepare, BuiltInArchersKeepBowVisibleWhileMoving) {
  EXPECT_GT(render_bow_mesh_count(
                "troops/roman/archer", Game::Units::SpawnType::Archer,
                Game::Systems::NationID::RomanRepublic, false),
            0);
  EXPECT_EQ(render_runtime_mesh_count(
                "troops/roman/archer", Game::Units::SpawnType::Archer,
                Game::Systems::NationID::RomanRepublic, false),
            0);
  EXPECT_GT(render_bow_mesh_count("troops/roman/archer",
                                  Game::Units::SpawnType::Archer,
                                  Game::Systems::NationID::RomanRepublic, true),
            0);
  EXPECT_EQ(render_runtime_mesh_count(
                "troops/roman/archer", Game::Units::SpawnType::Archer,
                Game::Systems::NationID::RomanRepublic, true),
            0);
  EXPECT_GT(render_bow_mesh_count("troops/carthage/archer",
                                  Game::Units::SpawnType::Archer,
                                  Game::Systems::NationID::Carthage, false),
            0);
  EXPECT_EQ(render_runtime_mesh_count("troops/carthage/archer",
                                      Game::Units::SpawnType::Archer,
                                      Game::Systems::NationID::Carthage, false),
            0);
  EXPECT_GT(render_bow_mesh_count("troops/carthage/archer",
                                  Game::Units::SpawnType::Archer,
                                  Game::Systems::NationID::Carthage, true),
            0);
  EXPECT_EQ(render_runtime_mesh_count("troops/carthage/archer",
                                      Game::Units::SpawnType::Archer,
                                      Game::Systems::NationID::Carthage, true),
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
                                  Game::Systems::NationID::RomanRepublic, true),
            0);
  EXPECT_EQ(render_runtime_mesh_count("troops/roman/horse_archer",
                                      Game::Units::SpawnType::HorseArcher,
                                      Game::Systems::NationID::RomanRepublic,
                                      true),
            0);
  EXPECT_GT(render_bow_mesh_count("troops/carthage/horse_archer",
                                  Game::Units::SpawnType::HorseArcher,
                                  Game::Systems::NationID::Carthage, false),
            0);
  EXPECT_EQ(render_runtime_mesh_count("troops/carthage/horse_archer",
                                      Game::Units::SpawnType::HorseArcher,
                                      Game::Systems::NationID::Carthage, false),
            0);
  EXPECT_GT(render_bow_mesh_count("troops/carthage/horse_archer",
                                  Game::Units::SpawnType::HorseArcher,
                                  Game::Systems::NationID::Carthage, true),
            0);
  EXPECT_EQ(render_runtime_mesh_count("troops/carthage/horse_archer",
                                      Game::Units::SpawnType::HorseArcher,
                                      Game::Systems::NationID::Carthage, true),
            0);
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
  auto *unit =
      entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Knight;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  ctx.entity = &entity;

  CountingSubmitter sink;
  renderer(ctx, sink);
  ASSERT_GT(sink.rigged_calls, 0);

  auto const archetype_id = find_archetype_id("troops/roman/swordsman");
  ASSERT_NE(archetype_id, Render::Creature::kInvalidArchetype);
  EXPECT_EQ(extra_role_color_count(archetype_id),
            Render::GL::kRomanHeavyHelmetRoleCount +
                Render::GL::kRomanGreavesRoleCount +
                Render::GL::kRomanShoulderCoverRoleCount +
                Render::GL::k_roman_scutum_role_count +
                Render::GL::kRomanHeavyArmorRoleCount +
                Render::GL::kSwordRoleCount + Render::GL::kScabbardRoleCount);
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
  auto *unit =
      entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Knight;
  unit->nation_id = Game::Systems::NationID::Carthage;
  ctx.entity = &entity;

  CountingSubmitter sink;
  renderer(ctx, sink);
  ASSERT_GT(sink.rigged_calls, 0);

  auto const archetype_id = find_archetype_id("troops/carthage/swordsman");
  ASSERT_NE(archetype_id, Render::Creature::kInvalidArchetype);
  EXPECT_EQ(extra_role_color_count(archetype_id),
            Render::GL::kCarthageHeavyHelmetRoleCount +
                Render::GL::k_carthage_shield_role_count +
                Render::GL::kCarthageShoulderCoverRoleCount +
                Render::GL::kArmorHeavyCarthageRoleCount +
                Render::GL::kSwordRoleCount + Render::GL::kScabbardRoleCount);
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
  auto *unit =
      entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::MountedKnight;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  ctx.entity = &entity;

  CountingSubmitter sink;
  renderer(ctx, sink);
  ASSERT_GT(sink.rigged_calls, 0);

  auto const archetype_id =
      find_archetype_id("troops/roman/horse_swordsman/rider");
  ASSERT_NE(archetype_id, Render::Creature::kInvalidArchetype);
  EXPECT_EQ(extra_role_color_count(archetype_id),
            Render::GL::kRomanHeavyHelmetRoleCount +
                Render::GL::kRomanShoulderCoverRoleCount +
                Render::GL::k_roman_scutum_role_count +
                Render::GL::kRomanHeavyArmorRoleCount +
                Render::GL::kSwordRoleCount + Render::GL::kScabbardRoleCount);
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
  auto *unit =
      entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::MountedKnight;
  unit->nation_id = Game::Systems::NationID::Carthage;
  ctx.entity = &entity;

  CountingSubmitter sink;
  renderer(ctx, sink);
  ASSERT_GT(sink.rigged_calls, 0);

  auto const archetype_id =
      find_archetype_id("troops/carthage/horse_swordsman/rider");
  ASSERT_NE(archetype_id, Render::Creature::kInvalidArchetype);
  EXPECT_EQ(extra_role_color_count(archetype_id),
            Render::GL::kCarthageHeavyHelmetRoleCount +
                Render::GL::k_carthage_shield_role_count +
                Render::GL::kCarthageShoulderCoverRoleCount +
                Render::GL::kArmorHeavyCarthageRoleCount +
                Render::GL::kSwordRoleCount + Render::GL::kScabbardRoleCount);
}

TEST(HumanoidPrepare, BowReadyRootRequestUsesSurfaceGroundingContract) {
  BowReadyRegressionRenderer renderer;
  ScopedFlatTerrain terrain(2.4F);

  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(1);
  auto *unit =
      entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Archer;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;

  auto *transform = entity.add_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);
  transform->position.x = 0.35F;
  transform->position.y = 6.0F;
  transform->position.z = -0.2F;
  transform->scale.x = 1.0F;
  transform->scale.y = 1.0F;
  transform->scale.z = 1.0F;

  ctx.entity = &entity;

  Render::GL::AnimationInputs anim{};
  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(renderer, ctx, anim, 0U, prep);

  ASSERT_EQ(prep.bodies.requests().size(), 1u);
  EXPECT_FALSE(prep.bodies.requests().front().world_already_grounded);
}

TEST(HumanoidPrepare, BowReadySubmittedSurfaceGroundingTouchesTerrain) {
  BowReadyRegressionRenderer renderer;
  ScopedFlatTerrain terrain(2.4F);

  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(1);
  auto *unit =
      entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Archer;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;

  auto *transform = entity.add_component<Engine::Core::TransformComponent>();
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
  ASSERT_EQ(sink.rigged_world_y.size(), 1u);
  EXPECT_NEAR(sink.rigged_world_y.front(), 2.4F, 0.1F);
}

TEST(HumanoidPrepare, RenderIndividualsOverrideLimitsPreparedSoldiers) {
  ScopedFlatTerrain terrain(0.0F);

  Render::GL::HumanoidRendererBase owner;
  Render::GL::DrawContext ctx{};
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(1);
  auto *unit =
      entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Archer;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  unit->render_individuals_per_unit_override = 3;

  auto *transform = entity.add_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);
  transform->position.x = 0.0F;
  transform->position.y = 0.0F;
  transform->position.z = 0.0F;
  transform->scale.x = 1.0F;
  transform->scale.y = 1.0F;
  transform->scale.z = 1.0F;

  ctx.entity = &entity;

  Render::GL::AnimationInputs anim{};
  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 0U, prep);

  EXPECT_EQ(prep.bodies.requests().size(), 3u);
}

TEST(HumanoidPrepare, BowReadySubmittedVisibleGeometryTouchesTerrain) {
  BowReadyRegressionRenderer renderer;
  ScopedFlatTerrain terrain(2.4F);

  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(1);
  auto *unit =
      entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Archer;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;

  auto *transform = entity.add_component<Engine::Core::TransformComponent>();
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
  ASSERT_EQ(sink.rigged_world_y.size(), 1u);
  EXPECT_NEAR(sink.rigged_world_y.front(), 2.4F, 0.1F);
}

TEST(HumanoidPrepare, BuiltInArchersUseSurfaceGroundingInIdlePose) {
  ScopedFlatTerrain terrain(2.4F);
  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);

  auto verify_archer = [&](const char *renderer_id,
                           Game::Systems::NationID nation_id) {
    const auto renderer = registry.get(renderer_id);
    ASSERT_TRUE(static_cast<bool>(renderer));

    Render::GL::DrawContext ctx{};
    ctx.force_single_soldier = true;
    ctx.allow_template_cache = false;

    Engine::Core::Entity entity(1);
    auto *unit =
        entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
    ASSERT_NE(unit, nullptr);
    unit->spawn_type = Game::Units::SpawnType::Archer;
    unit->nation_id = nation_id;

    auto *transform = entity.add_component<Engine::Core::TransformComponent>();
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
    ASSERT_EQ(sink.rigged_world_y.size(), 1u);
    EXPECT_NEAR(sink.rigged_world_y.front(), 2.4F, 0.1F);
  };

  verify_archer("troops/roman/archer", Game::Systems::NationID::RomanRepublic);
  verify_archer("troops/carthage/archer", Game::Systems::NationID::Carthage);
}

TEST(HumanoidPrepare, BuiltInArchersVisibleIdleGeometryTouchesTerrain) {
  ScopedFlatTerrain terrain(2.4F);
  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);

  auto verify_archer = [&](const char *renderer_id,
                           Game::Systems::NationID nation_id) {
    const auto renderer = registry.get(renderer_id);
    ASSERT_TRUE(static_cast<bool>(renderer));

    Render::GL::DrawContext ctx{};
    ctx.force_single_soldier = true;
    ctx.allow_template_cache = false;

    Engine::Core::Entity entity(1);
    auto *unit =
        entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
    ASSERT_NE(unit, nullptr);
    unit->spawn_type = Game::Units::SpawnType::Archer;
    unit->nation_id = nation_id;

    auto *transform = entity.add_component<Engine::Core::TransformComponent>();
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
    ASSERT_EQ(sink.rigged_world_y.size(), 1u);
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
    req.archetype = Render::Creature::ArchetypeRegistry::kHumanoidBase;
    req.state = Render::Creature::AnimationStateId::Idle;
    req.lod = Render::Creature::CreatureLOD::Full;
    req.pass = (i % 2 == 0) ? RenderPassIntent::Main : RenderPassIntent::Shadow;
    req.seed = static_cast<std::uint32_t>(i + 1);
    req.world_already_grounded = true;
    prep.bodies.add_request(req);
  }
  const auto stats = submit_preparation(prep, sink);

  EXPECT_EQ(stats.entities_submitted, 3u);
}

TEST(HumanoidPrepare, DeriveUnitSeedHonorsOverride) {
  Render::GL::DrawContext ctx{};
  ctx.has_seed_override = true;
  ctx.seed_override = 0xDEADBEEFu;
  EXPECT_EQ(Render::Creature::Pipeline::derive_unit_seed(ctx, nullptr),
            0xDEADBEEFu);
}

TEST(HumanoidPrepare, DeriveUnitSeedDeterministicWithoutOverride) {
  Render::GL::DrawContext ctx{};

  EXPECT_EQ(Render::Creature::Pipeline::derive_unit_seed(ctx, nullptr), 0u);
}

TEST(HumanoidPrepare, BuildSoldierLayoutIsDeterministic) {
  using Render::GL::FormationCalculatorFactory;
  auto const *calculator = FormationCalculatorFactory::get_calculator(
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

  auto const first =
      Render::Humanoid::build_soldier_layout(*calculator, inputs);
  auto const second =
      Render::Humanoid::build_soldier_layout(*calculator, inputs);

  EXPECT_FLOAT_EQ(first.offset_x, second.offset_x);
  EXPECT_FLOAT_EQ(first.offset_z, second.offset_z);
  EXPECT_FLOAT_EQ(first.vertical_jitter, second.vertical_jitter);
  EXPECT_FLOAT_EQ(first.yaw_offset, second.yaw_offset);
  EXPECT_FLOAT_EQ(first.phase_offset, second.phase_offset);
  EXPECT_EQ(first.inst_seed, second.inst_seed);
}

TEST(HumanoidPrepare, BuildSoldierLayoutLeavesSingleSoldierUnjittered) {
  using Render::GL::FormationCalculatorFactory;
  auto const *calculator = FormationCalculatorFactory::get_calculator(
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

  auto const layout =
      Render::Humanoid::build_soldier_layout(*calculator, inputs);

  EXPECT_FLOAT_EQ(layout.offset_x, 0.0F);
  EXPECT_FLOAT_EQ(layout.offset_z, 0.0F);
  EXPECT_FLOAT_EQ(layout.vertical_jitter, 0.0F);
  EXPECT_FLOAT_EQ(layout.yaw_offset, 0.0F);
  EXPECT_FLOAT_EQ(layout.phase_offset, 0.0F);
  EXPECT_EQ(layout.inst_seed, inputs.seed);
}

TEST(HumanoidPrepare, CavalryFormationStaggersRowsAndAlternatesRankYaw) {
  using Render::GL::FormationCalculatorFactory;
  auto const *calculator = FormationCalculatorFactory::get_calculator(
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

TEST(HumanoidPrepare, ResolveFormationStateUsesUnitHealthAndMountedSpawn) {
  Engine::Core::Entity entity(1);
  auto *unit =
      entity.add_component<Engine::Core::UnitComponent>(25, 100, 1.0F, 12.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::HorseSpearman;
  unit->render_individuals_per_unit_override = 8;

  Render::GL::HumanoidRendererBase owner;
  Render::GL::DrawContext ctx{};
  ctx.entity = &entity;
  Render::GL::AnimationInputs anim{};

  Render::Creature::Pipeline::HumanoidFormationStateInputs inputs{};
  inputs.owner = &owner;
  inputs.ctx = &ctx;
  inputs.anim = &anim;
  inputs.unit = unit;

  auto const state =
      Render::Creature::Pipeline::resolve_humanoid_formation_state(inputs);

  EXPECT_TRUE(state.mounted);
  EXPECT_EQ(state.formation.individuals_per_unit, 8);
  EXPECT_EQ(state.visible_count, 2);
  EXPECT_GE(state.rows, 1);
  EXPECT_GE(state.cols, 1);
}

TEST(
    HumanoidPrepare,
    PreparedSingleSoldierRequestProjectsToTerrainBeforeSubmitContactGrounding) {
  ScopedFlatTerrain terrain(2.75F);

  Render::GL::HumanoidRendererBase owner;
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
  ASSERT_EQ(requests.size(), 1u);
  EXPECT_FALSE(requests[0].world_already_grounded);
  EXPECT_NEAR(requests[0].world.map(QVector3D(0.0F, 0.0F, 0.0F)).y(), 2.75F,
              0.0001F);
}

TEST(HumanoidPrepare,
     PreparedMovingSingleSoldierVisibleGeometryTouchesTerrainAfterSubmit) {
  ScopedFlatTerrain terrain(1.5F);

  Render::GL::HumanoidRendererBase owner;
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
  ASSERT_EQ(sink.rigged_world_y.size(), 1u);
  EXPECT_NEAR(sink.rigged_world_y.front(), 1.5F, 0.1F);
}

} // namespace
