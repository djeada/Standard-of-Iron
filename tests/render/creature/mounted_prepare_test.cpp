// Phase A regression — mounted prepare module.
//
// Exercises render/entity/mounted_prepare.{h,cpp}: prepare_mounted_rows
// must produce a horse mount row + humanoid rider row carrying the supplied
// pass intent.

#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/units/spawn_type.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/bpat/bpat_format.h"
#include "render/creature/bpat/bpat_registry.h"
#include "render/creature/pipeline/creature_render_state.h"
#include "render/creature/pipeline/prepared_submit.h"
#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/entity/horse_spearman_renderer_base.h"
#include "render/entity/mounted_humanoid_renderer_base.h"
#include "render/entity/mounted_knight_renderer_base.h"
#include "render/entity/mounted_prepare.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/horse/horse_spec.h"
#include "render/humanoid/pose_cache_components.h"
#include "render/humanoid/prepare.h"
#include "render/humanoid/skeleton.h"
#include "render/submitter.h"
#include "render/template_cache.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <gtest/gtest.h>
#include <optional>
#include <unordered_map>

namespace {

class NullSubmitter : public Render::GL::ISubmitter {
public:
  int rigged_calls{0};
  int meshes{0};
  void mesh(Render::GL::Mesh *, const QMatrix4x4 &, const QVector3D &,
            Render::GL::Texture *, float, int) override {
    ++meshes;
  }
  void rigged(const Render::GL::RiggedCreatureCmd &) override {
    ++rigged_calls;
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

class InspectableMountedKnightRenderer
    : public Render::GL::MountedKnightRendererBase {
public:
  using Render::GL::MountedHumanoidRendererBase::resolve_mount_render_state;
  using Render::GL::MountedKnightRendererBase::MountedKnightRendererBase;
};

auto find_assets_dir() -> std::string {
  for (auto const *candidate :
       {"assets/creatures", "../assets/creatures", "../../assets/creatures"}) {
    std::filesystem::path p{candidate};
    if (std::filesystem::exists(p / "humanoid.bpat")) {
      return std::filesystem::absolute(p).string();
    }
  }
  return {};
}

auto rider_root_matrix(Render::Creature::ArchetypeId archetype_id,
                       Render::Creature::AnimationStateId state, float phase,
                       std::uint8_t clip_variant) -> std::optional<QMatrix4x4> {
  using Render::Creature::ArchetypeDescriptor;
  auto const base_clip =
      Render::Creature::ArchetypeRegistry::instance().bpat_clip(archetype_id,
                                                                state);
  if (base_clip == ArchetypeDescriptor::kUnmappedClip) {
    return std::nullopt;
  }

  auto const clip_id = static_cast<std::uint16_t>(base_clip + clip_variant);
  auto const *blob = Render::Creature::Bpat::BpatRegistry::instance().blob(
      Render::Creature::Bpat::kSpeciesHumanoid);
  if (blob == nullptr || clip_id >= blob->clip_count()) {
    return std::nullopt;
  }

  auto const clip = blob->clip(clip_id);
  if (clip.frame_count == 0U) {
    return std::nullopt;
  }

  float normalized_phase = phase;
  if (!clip.loops && normalized_phase >= 1.0F) {
    normalized_phase = 1.0F;
  } else {
    normalized_phase -= std::floor(normalized_phase);
    if (normalized_phase < 0.0F) {
      normalized_phase += 1.0F;
    }
  }

  int frame_idx = (!clip.loops && normalized_phase >= 1.0F)
                      ? static_cast<int>(clip.frame_count - 1U)
                      : static_cast<int>(normalized_phase *
                                         static_cast<float>(clip.frame_count));
  frame_idx = std::clamp(frame_idx, 0, static_cast<int>(clip.frame_count) - 1);
  auto const palette = blob->frame_palette_view(
      clip.frame_offset + static_cast<std::uint32_t>(frame_idx));
  auto const root_index =
      static_cast<std::size_t>(Render::Humanoid::HumanoidBone::Root);
  if (palette.size() <= root_index) {
    return std::nullopt;
  }
  return palette[root_index];
}

TEST(MountedPrepare, ProducesHorseMountAndHumanoidRiderRows) {
  using namespace Render::Creature::Pipeline;

  MountedSpec mounted{};
  mounted.mount.kind = CreatureKind::Horse;
  mounted.mount.debug_name = "test/horse";
  mounted.rider.kind = CreatureKind::Humanoid;
  mounted.rider.debug_name = "test/rider";

  Render::Horse::HorseSpecPose mount_pose{};
  Render::GL::HorseVariant mount_variant{};
  Render::GL::HumanoidPose rider_pose{};
  Render::GL::HumanoidVariant rider_variant{};
  Render::GL::HumanoidAnimationContext rider_anim{};
  QMatrix4x4 mount_world;
  QMatrix4x4 rider_world;

  auto set = Render::GL::prepare_mounted_rows(
      mounted, mount_world, rider_world, mount_pose, mount_variant, rider_pose,
      rider_variant, rider_anim, /*seed*/ 99,
      Render::Creature::CreatureLOD::Full, RenderPassIntent::Shadow);

  EXPECT_EQ(set.mount_row.spec.kind, CreatureKind::Horse);
  EXPECT_EQ(set.rider_row.spec.kind, CreatureKind::Humanoid);
  EXPECT_EQ(set.mount_row.pass, RenderPassIntent::Shadow);
  EXPECT_EQ(set.rider_row.pass, RenderPassIntent::Shadow);
  EXPECT_EQ(set.mount_row.seed, 99u);
  EXPECT_EQ(set.rider_row.seed, 99u);
}

TEST(MountedPrepare, ShadowPairProducesNoDrawCalls) {
  Render::GL::MountedKnightRendererConfig cfg;
  cfg.has_sword = false;
  cfg.has_cavalry_shield = false;

  Render::GL::MountedKnightRendererBase renderer(cfg);
  Render::GL::DrawContext ctx{};
  ctx.template_prewarm = true;
  ctx.force_single_soldier = true;
  Render::GL::AnimationInputs anim{};

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(renderer, ctx, anim, 0, prep);

  NullSubmitter sink;
  const auto stats = Render::Creature::Pipeline::submit_preparation(prep, sink);

  EXPECT_EQ(stats.entities_submitted, 0u);
  EXPECT_EQ(sink.rigged_calls, 0);
  EXPECT_EQ(sink.meshes, 0);
}

TEST(MountedPrepare, MainPairProducesTwoEntitySubmissions) {
  Render::GL::MountedKnightRendererConfig cfg;
  cfg.has_sword = false;
  cfg.has_cavalry_shield = false;

  Render::GL::MountedKnightRendererBase renderer(cfg);
  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  Render::GL::AnimationInputs anim{};

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(renderer, ctx, anim, 0, prep);

  NullSubmitter sink;
  const auto stats = Render::Creature::Pipeline::submit_preparation(prep, sink);

  EXPECT_EQ(stats.entities_submitted, 2u);
}

TEST(MountedPrepare, TemplatePrewarmRenderWarmsMountedSnapshotCache) {
  Render::GL::MountedKnightRendererConfig cfg;
  cfg.has_sword = false;
  cfg.has_cavalry_shield = false;

  Render::GL::MountedKnightRendererBase renderer(cfg);
  Engine::Core::Entity entity(1);
  auto *unit = entity.add_component<Engine::Core::UnitComponent>();
  unit->spawn_type = Game::Units::SpawnType::MountedKnight;
  unit->owner_id = 1;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  unit->max_health = 100;
  unit->health = 100;
  auto *transform = entity.add_component<Engine::Core::TransformComponent>();
  transform->position = {0.0F, 0.0F, 0.0F};
  transform->rotation = {0.0F, 0.0F, 0.0F};
  transform->scale = {1.0F, 1.0F, 1.0F};
  auto *renderable =
      entity.add_component<Engine::Core::RenderableComponent>("", "");
  renderable->visible = true;

  Render::GL::DrawContext ctx{};
  ctx.entity = &entity;
  ctx.template_prewarm = true;
  ctx.force_single_soldier = true;

  Render::GL::TemplateRecorder recorder;
  recorder.snapshot_mesh_cache().clear();
  renderer.render(ctx, recorder);

  EXPECT_GE(recorder.snapshot_mesh_cache().size(), 2u);
  EXPECT_TRUE(recorder.commands().empty());
}

TEST(MountedPrepare, MountedHumanoidPreparationQueuesRiderAndHorseBodies) {
  using namespace Render::Creature::Pipeline;

  Render::GL::MountedKnightRendererConfig cfg;
  cfg.has_sword = false;
  cfg.has_cavalry_shield = false;

  Render::GL::MountedKnightRendererBase renderer(cfg);
  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  Render::GL::AnimationInputs anim{};

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(renderer, ctx, anim, 0, prep);

  ASSERT_EQ(prep.bodies.size(), 2u);
  int rider_requests = 0;
  int horse_requests = 0;
  float rider_world_y = 0.0F;
  float horse_world_y = 0.0F;
  for (const auto &req : prep.bodies.requests()) {
    auto const species =
        Render::Creature::ArchetypeRegistry::instance().species(req.archetype);
    if (species == CreatureKind::Humanoid) {
      ++rider_requests;
      rider_world_y = req.world.column(3).y();
    }
    if (species == CreatureKind::Horse) {
      ++horse_requests;
      horse_world_y = req.world.column(3).y();
    }
  }

  EXPECT_EQ(rider_requests, 1);
  EXPECT_EQ(horse_requests, 1);
  EXPECT_GT(std::abs(rider_world_y - horse_world_y), 0.01F);
  EXPECT_EQ(prep.bodies.requests().size(), 2u);
  EXPECT_TRUE(owns_slot(renderer.visual_spec().owned_legacy_slots,
                        LegacySlotMask::Attachments));
}

TEST(MountedPrepare, SubmitPreparationDrawsRiderFromPreparedPose) {
  using namespace Render::Creature::Pipeline;

  Render::GL::MountedKnightRendererConfig cfg;
  cfg.has_sword = false;
  cfg.has_cavalry_shield = false;

  Render::GL::MountedKnightRendererBase renderer(cfg);
  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  Render::GL::AnimationInputs anim{};

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(renderer, ctx, anim, 0, prep);

  NullSubmitter sink;
  Render::Creature::Pipeline::submit_preparation(prep, sink);

  EXPECT_GT(sink.rigged_calls, 0);
}

TEST(MountedPrepare, MountedRiderRequestUsesAbsoluteSeatWorld) {
  Render::GL::MountedKnightRendererConfig cfg;
  cfg.has_sword = false;
  cfg.has_cavalry_shield = false;

  Render::GL::MountedKnightRendererBase renderer(cfg);
  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  Render::GL::AnimationInputs anim{};

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(renderer, ctx, anim, 0, prep);

  auto const &requests = prep.bodies.requests();
  ASSERT_EQ(requests.size(), 2u);

  auto const horse_req =
      std::find_if(requests.begin(), requests.end(), [](const auto &req) {
        return Render::Creature::ArchetypeRegistry::instance().species(
                   req.archetype) ==
               Render::Creature::Pipeline::CreatureKind::Horse;
      });
  auto const rider_req =
      std::find_if(requests.begin(), requests.end(), [](const auto &req) {
        return Render::Creature::ArchetypeRegistry::instance().species(
                   req.archetype) ==
               Render::Creature::Pipeline::CreatureKind::Humanoid;
      });
  ASSERT_NE(horse_req, requests.end());
  ASSERT_NE(rider_req, requests.end());
  bool invertible = false;
  QMatrix4x4 const rider_from_horse =
      horse_req->world.inverted(&invertible) * rider_req->world;
  ASSERT_TRUE(invertible);
  EXPECT_GT(rider_from_horse.column(3).toVector3D().length(), 0.0F);
}

TEST(MountedPrepare, MountedUnitGroupsRiderAndHorseBySharedWorldKey) {
  using Render::Creature::Pipeline::CreatureKind;

  Render::GL::MountedKnightRendererConfig cfg;
  cfg.has_sword = false;
  cfg.has_cavalry_shield = false;

  Render::GL::MountedKnightRendererBase renderer(cfg);
  Render::GL::DrawContext ctx{};
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(1);
  auto *unit =
      entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::MountedKnight;
  ctx.entity = &entity;

  Render::GL::AnimationInputs anim{};
  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(renderer, ctx, anim, 0, prep);

  auto const &requests = prep.bodies.requests();
  auto &registry = Render::Creature::ArchetypeRegistry::instance();
  struct GroupCounts {
    std::size_t horse{0};
    std::size_t rider{0};
  };
  std::unordered_map<Render::Creature::WorldKey, GroupCounts> groups;
  std::size_t horse_count = 0;
  std::size_t rider_count = 0;

  for (const auto &req : requests) {
    auto &group = groups[req.world_key];
    if (registry.species(req.archetype) == CreatureKind::Horse) {
      ++horse_count;
      ++group.horse;
    } else if (registry.species(req.archetype) == CreatureKind::Humanoid) {
      ++rider_count;
      ++group.rider;
    }
  }

  EXPECT_GT(horse_count, 1u);
  EXPECT_EQ(rider_count, horse_count);
  EXPECT_EQ(groups.size(), horse_count);
  for (const auto &[key, counts] : groups) {
    EXPECT_NE(key, 0U);
    EXPECT_EQ(counts.horse, 1u);
    EXPECT_EQ(counts.rider, 1u);
  }
}

TEST(MountedPrepare, MountedRiderRootAttachesToHorseSeatFrame) {
  auto const root = find_assets_dir();
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found";
  }
  auto &bpat = Render::Creature::Bpat::BpatRegistry::instance();
  ASSERT_TRUE(bpat.load_species(Render::Creature::Bpat::kSpeciesHumanoid,
                                root + "/humanoid.bpat"));

  Render::GL::MountedKnightRendererConfig cfg;
  cfg.has_sword = false;
  cfg.has_cavalry_shield = false;

  InspectableMountedKnightRenderer renderer(cfg);
  Render::GL::DrawContext ctx{};
  ctx.allow_template_cache = false;
  ctx.force_single_soldier = true;

  Engine::Core::Entity entity(1);
  auto *unit =
      entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::MountedKnight;
  ctx.entity = &entity;

  Render::GL::AnimationInputs anim{};
  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(renderer, ctx, anim, 0, prep);

  auto const &requests = prep.bodies.requests();
  auto const horse_req =
      std::find_if(requests.begin(), requests.end(), [](const auto &req) {
        return Render::Creature::ArchetypeRegistry::instance().species(
                   req.archetype) ==
               Render::Creature::Pipeline::CreatureKind::Horse;
      });
  auto const rider_req =
      std::find_if(requests.begin(), requests.end(), [](const auto &req) {
        return Render::Creature::ArchetypeRegistry::instance().species(
                   req.archetype) ==
               Render::Creature::Pipeline::CreatureKind::Humanoid;
      });
  ASSERT_NE(horse_req, requests.end());
  ASSERT_NE(rider_req, requests.end());

  Render::GL::HumanoidVariant variant{};
  std::uint32_t const seed =
      static_cast<std::uint32_t>(unit->owner_id * 2654435761U) ^
      static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&entity) &
                                 0xFFFFFFFFU);
  renderer.get_variant(ctx, seed, variant);

  Render::GL::HumanoidAnimationContext anim_ctx{};
  anim_ctx.inputs = anim;
  Render::GL::HorseProfile profile{};
  Render::GL::HorseDimensions dims{};
  Render::GL::MountedAttachmentFrame mount{};
  Render::GL::HorseMotionSample motion{};
  renderer.resolve_mount_render_state(ctx, seed, variant, anim_ctx, true,
                                      profile, dims, mount, motion);

  auto const root_matrix =
      rider_root_matrix(rider_req->archetype, rider_req->state,
                        rider_req->phase, rider_req->clip_variant);
  ASSERT_TRUE(root_matrix.has_value());

  bool invertible = false;
  QMatrix4x4 const rider_from_horse =
      horse_req->world.inverted(&invertible) * rider_req->world;
  ASSERT_TRUE(invertible);
  QMatrix4x4 const anchored_root = rider_from_horse * *root_matrix;
  QMatrix4x4 expected_seat;
  expected_seat.setColumn(0, QVector4D(mount.seat_right, 0.0F));
  expected_seat.setColumn(1, QVector4D(mount.seat_up, 0.0F));
  expected_seat.setColumn(2, QVector4D(mount.seat_forward, 0.0F));
  expected_seat.setColumn(3, QVector4D(mount.seat_position, 1.0F));

  for (int column = 0; column < 4; ++column) {
    auto const actual = anchored_root.column(column);
    auto const expected = expected_seat.column(column);
    EXPECT_NEAR(actual.x(), expected.x(), 1.0e-4F);
    EXPECT_NEAR(actual.y(), expected.y(), 1.0e-4F);
    EXPECT_NEAR(actual.z(), expected.z(), 1.0e-4F);
    EXPECT_NEAR(actual.w(), expected.w(), 1.0e-4F);
  }
}

TEST(MountedPrepare, MountedKnightKeepsConfiguredRiderArchetype) {
  Render::GL::MountedKnightRendererConfig cfg;
  cfg.has_sword = false;
  cfg.has_cavalry_shield = false;
  cfg.rider_archetype_id = 77U;

  Render::GL::MountedKnightRendererBase renderer(cfg);
  auto const &mounted = renderer.mounted_visual_spec();

  EXPECT_EQ(mounted.rider.archetype_id, 77U);
}

TEST(MountedPrepare, HorseSpearmanKeepsConfiguredRiderArchetype) {
  Render::GL::HorseSpearmanRendererConfig cfg;
  cfg.has_spear = false;
  cfg.has_shield = false;
  cfg.rider_archetype_id = 91U;

  Render::GL::HorseSpearmanRendererBase renderer(cfg);
  auto const &mounted = renderer.mounted_visual_spec();

  EXPECT_EQ(mounted.rider.archetype_id, 91U);
}

TEST(MountedPrepare, StaleLayoutCacheVersionForcesMountedFormationRefresh) {
  Render::GL::MountedKnightRendererConfig cfg;
  cfg.has_sword = false;
  cfg.has_cavalry_shield = false;

  Render::GL::MountedKnightRendererBase renderer(cfg);
  Render::GL::DrawContext ctx{};
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(1);
  auto *unit =
      entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::MountedKnight;
  ctx.entity = &entity;

  Render::GL::AnimationInputs anim{};
  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(renderer, ctx, anim, 0, prep);

  auto *layout_cache =
      entity.get_component<Render::Humanoid::HumanoidLayoutCacheComponent>();
  ASSERT_NE(layout_cache, nullptr);
  ASSERT_GT(layout_cache->soldiers.size(), 1u);

  for (auto &soldier : layout_cache->soldiers) {
    soldier.offset_x = 0.0F;
    soldier.offset_z = 0.0F;
    soldier.yaw_offset = 0.0F;
  }
  layout_cache->layout_version = 0U;
  layout_cache->valid = true;

  prep.clear();
  Render::Humanoid::prepare_humanoid_instances(renderer, ctx, anim, 1, prep);

  ASSERT_NE(layout_cache->layout_version, 0U);
  bool found_non_grid_offset = false;
  for (auto const &soldier : layout_cache->soldiers) {
    if (std::abs(soldier.offset_x) > 0.001F ||
        std::abs(soldier.offset_z) > 0.001F ||
        std::abs(soldier.yaw_offset) > 0.001F) {
      found_non_grid_offset = true;
      break;
    }
  }
  EXPECT_TRUE(found_non_grid_offset);
}

} // namespace
