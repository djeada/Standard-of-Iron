// Stage 15.5e — elephant Full + Reduced LOD switchover regression.
//
// Mirror of horse_full_switchover_test.cpp for the elephant.

#include "game/core/component.h"
#include "game/core/entity.h"
#include "render/bone_palette_arena.h"
#include "render/creature/spec.h"
#include "render/draw_queue.h"
#include "render/elephant/elephant_renderer_base.h"
#include "render/elephant/elephant_spec.h"
#include "render/rigged_mesh.h"
#include "render/rigged_mesh_cache.h"
#include "render/scene_renderer.h"
#include "render/submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <cmath>
#include <gtest/gtest.h>
#include <vector>

namespace {

using Render::GL::DrawPartCmdIndex;
using Render::GL::MeshCmdIndex;
using Render::GL::RiggedCreatureCmd;
using Render::GL::RiggedCreatureCmdIndex;

class RecordingRenderer : public Render::GL::Renderer {
public:
  std::vector<RiggedCreatureCmd> rigged_calls;

  void rigged(const RiggedCreatureCmd &cmd) override {
    rigged_calls.push_back(cmd);
  }
};

Render::Elephant::ElephantSpecPose baseline_pose() {
  Render::GL::ElephantDimensions const dims =
      Render::GL::make_elephant_dimensions(0U);
  Render::GL::ElephantGait gait{};
  Render::Elephant::ElephantSpecPose pose{};
  Render::Elephant::make_elephant_spec_pose_reduced(
      dims, gait, Render::Elephant::ElephantReducedMotion{}, pose);
  return pose;
}

TEST(ElephantFullSwitchover, FullLodBakesMoreGeometryThanReduced) {
  auto const &spec = Render::Elephant::elephant_creature_spec();
  auto bind = Render::Elephant::elephant_bind_palette();
  EXPECT_EQ(bind.size(), Render::Elephant::kElephantBoneCount);

  ASSERT_GT(spec.lod_full.primitives.size(), 0U);
  ASSERT_GT(spec.lod_reduced.primitives.size(), 0U);
  EXPECT_EQ(spec.lod_full.primitives.size(), 50U);
  EXPECT_EQ(spec.lod_reduced.primitives.size(), 12U);

  Render::GL::RiggedMeshCache cache;
  const auto *full_entry =
      cache.get_or_bake(spec, Render::Creature::CreatureLOD::Full, bind);
  const auto *reduced_entry =
      cache.get_or_bake(spec, Render::Creature::CreatureLOD::Reduced, bind);
  ASSERT_NE(full_entry, nullptr);
  ASSERT_NE(reduced_entry, nullptr);
  ASSERT_NE(full_entry->mesh, nullptr);
  ASSERT_NE(reduced_entry->mesh, nullptr);
  EXPECT_GT(full_entry->mesh->vertex_count(), 0U);
  EXPECT_GT(full_entry->mesh->vertex_count(),
            reduced_entry->mesh->vertex_count());
}

TEST(ElephantFullSwitchover, BonePaletteEvaluationIsFinite) {
  auto pose = baseline_pose();
  std::array<QMatrix4x4, Render::Elephant::kElephantBoneCount> buf{};
  std::uint32_t const n = Render::Elephant::compute_elephant_bone_palette(
      pose, std::span<QMatrix4x4>(buf.data(), buf.size()));
  EXPECT_EQ(n, Render::Elephant::kElephantBoneCount);
  for (std::uint32_t i = 0; i < n; ++i) {
    for (int r = 0; r < 4; ++r) {
      for (int c = 0; c < 4; ++c) {
        EXPECT_TRUE(std::isfinite(buf[i](r, c)));
      }
    }
  }
}

TEST(ElephantFullSwitchover, RiggedCmdEmittedForFullLod) {
  auto const &spec = Render::Elephant::elephant_creature_spec();
  auto bind = Render::Elephant::elephant_bind_palette();

  Render::GL::RiggedMeshCache cache;
  Render::GL::BonePaletteArena arena;
  const auto *entry =
      cache.get_or_bake(spec, Render::Creature::CreatureLOD::Full, bind);
  ASSERT_NE(entry, nullptr);

  auto pose = baseline_pose();
  std::array<QMatrix4x4, Render::Elephant::kElephantBoneCount> palette_buf{};
  Render::Elephant::compute_elephant_bone_palette(
      pose, std::span<QMatrix4x4>(palette_buf.data(), palette_buf.size()));

  auto slot_h = arena.allocate_palette();
  QMatrix4x4 *slot = slot_h.cpu;
  ASSERT_NE(slot, nullptr);
  for (std::size_t i = 0; i < entry->inverse_bind.size(); ++i) {
    slot[i] = palette_buf[i] * entry->inverse_bind[i];
  }

  Render::GL::DrawQueue queue;
  Render::GL::QueueSubmitter sub(&queue);

  RiggedCreatureCmd cmd{};
  cmd.mesh = entry->mesh.get();
  cmd.bone_palette = slot;
  cmd.bone_count = static_cast<std::uint32_t>(entry->inverse_bind.size());
  cmd.color = QVector3D(0.45F, 0.40F, 0.38F);
  cmd.alpha = 1.0F;
  cmd.variation_scale = QVector3D(1.0F, 1.0F, 1.0F);
  sub.rigged(cmd);

  ASSERT_EQ(queue.size(), 1U);
  EXPECT_EQ(queue.items()[0].index(), RiggedCreatureCmdIndex);
  std::size_t parts = 0, meshes = 0;
  for (const auto &c : queue.items()) {
    if (c.index() == DrawPartCmdIndex)
      ++parts;
    else if (c.index() == MeshCmdIndex)
      ++meshes;
  }
  EXPECT_EQ(parts, 0U);
  EXPECT_EQ(meshes, 0U);
}

TEST(ElephantFullSwitchover, ReducedAndFullShareBindPalette) {
  auto const &spec = Render::Elephant::elephant_creature_spec();
  auto bind = Render::Elephant::elephant_bind_palette();

  Render::GL::RiggedMeshCache cache;
  const auto *full =
      cache.get_or_bake(spec, Render::Creature::CreatureLOD::Full, bind);
  const auto *reduced =
      cache.get_or_bake(spec, Render::Creature::CreatureLOD::Reduced, bind);
  ASSERT_NE(full, nullptr);
  ASSERT_NE(reduced, nullptr);
  EXPECT_NE(full->mesh.get(), reduced->mesh.get());
  EXPECT_EQ(cache.size(), 2U);
}

TEST(ElephantFullSwitchover, CacheEnabledRenderStillSubmitsRiggedBody) {
  Render::GL::ElephantRendererBase elephant;
  RecordingRenderer renderer;

  Engine::Core::Entity entity(9);
  auto *transform = entity.add_component<Engine::Core::TransformComponent>();
  transform->position = {0.0F, 0.0F, 0.0F};
  transform->rotation = {0.0F, 0.0F, 0.0F};
  transform->scale = {1.0F, 1.0F, 1.0F};
  auto *unit =
      entity.add_component<Engine::Core::UnitComponent>(800, 800, 1.0F, 12.0F);
  unit->spawn_type = Game::Units::SpawnType::Elephant;
  unit->owner_id = 1;

  Render::GL::AnimationInputs anim{};
  anim.time = 0.0F;

  Render::GL::DrawContext ctx{};
  ctx.entity = &entity;
  ctx.renderer_id = "cache_enabled_elephant_body_regression";
  ctx.allow_template_cache = true;
  ctx.force_horse_lod = true;
  ctx.forced_horse_lod = Render::GL::HorseLOD::Full;
  ctx.has_seed_override = true;
  ctx.seed_override = 77U;

  auto profile = Render::GL::make_elephant_profile(
      77U, QVector3D(0.45F, 0.16F, 0.10F), QVector3D(0.55F, 0.50F, 0.42F));

  elephant.render(ctx, anim, profile, nullptr, nullptr, renderer,
                  Render::GL::HorseLOD::Full);

  ASSERT_EQ(renderer.rigged_calls.size(), 1U);
  EXPECT_NE(renderer.rigged_calls[0].mesh, nullptr);
  EXPECT_GT(renderer.rigged_calls[0].bone_count, 0U);
  EXPECT_NE(renderer.rigged_calls[0].bone_palette, nullptr);
}

TEST(ElephantFullSwitchover, TemplatePrewarmRenderProducesNoDraws) {
  Render::GL::ElephantRendererBase elephant;
  RecordingRenderer renderer;

  Engine::Core::Entity entity(12);
  auto *transform = entity.add_component<Engine::Core::TransformComponent>();
  transform->position = {0.0F, 0.0F, 0.0F};
  transform->rotation = {0.0F, 0.0F, 0.0F};
  transform->scale = {1.0F, 1.0F, 1.0F};
  auto *unit =
      entity.add_component<Engine::Core::UnitComponent>(800, 800, 1.0F, 12.0F);
  unit->spawn_type = Game::Units::SpawnType::Elephant;
  unit->owner_id = 1;

  Render::GL::AnimationInputs anim{};
  anim.time = 0.0F;

  Render::GL::DrawContext ctx{};
  ctx.entity = &entity;
  ctx.force_horse_lod = true;
  ctx.forced_horse_lod = Render::GL::HorseLOD::Full;
  ctx.has_seed_override = true;
  ctx.seed_override = 77U;
  ctx.template_prewarm = true;

  auto profile = Render::GL::make_elephant_profile(
      77U, QVector3D(0.45F, 0.16F, 0.10F), QVector3D(0.55F, 0.50F, 0.42F));

  elephant.render(ctx, anim, profile, nullptr, nullptr, renderer,
                  Render::GL::HorseLOD::Full);
  EXPECT_TRUE(renderer.rigged_calls.empty());
}

} // namespace

namespace more {

TEST(ElephantReducedSwitchover, ReducedBakeNonEmpty) {
  auto const &spec = Render::Elephant::elephant_creature_spec();
  auto bind = Render::Elephant::elephant_bind_palette();
  Render::GL::RiggedMeshCache cache;
  const auto *e =
      cache.get_or_bake(spec, Render::Creature::CreatureLOD::Reduced, bind);
  ASSERT_NE(e, nullptr);
  ASSERT_NE(e->mesh, nullptr);
  EXPECT_GT(e->mesh->vertex_count(), 0U);
  EXPECT_EQ(e->inverse_bind.size(), Render::Elephant::kElephantBoneCount);
}

TEST(ElephantReducedSwitchover, BindPaletteIsCached) {
  auto a = Render::Elephant::elephant_bind_palette();
  auto b = Render::Elephant::elephant_bind_palette();
  EXPECT_EQ(a.data(), b.data());
}

TEST(ElephantFullSwitchover, VariationScaleSharedMeshDifferentUnits) {
  auto const &spec = Render::Elephant::elephant_creature_spec();
  auto bind = Render::Elephant::elephant_bind_palette();
  Render::GL::RiggedMeshCache cache;
  Render::GL::BonePaletteArena arena;
  const auto *entry =
      cache.get_or_bake(spec, Render::Creature::CreatureLOD::Full, bind);
  ASSERT_NE(entry, nullptr);

  Render::GL::DrawQueue queue;
  Render::GL::QueueSubmitter sub(&queue);
  for (int u = 0; u < 2; ++u) {
    auto slot_h = arena.allocate_palette();
    QMatrix4x4 *slot = slot_h.cpu;
    for (std::size_t i = 0; i < entry->inverse_bind.size(); ++i) {
      slot[i] = entry->inverse_bind[i].inverted() * entry->inverse_bind[i];
    }
    RiggedCreatureCmd cmd{};
    cmd.mesh = entry->mesh.get();
    cmd.bone_palette = slot;
    cmd.bone_count = static_cast<std::uint32_t>(entry->inverse_bind.size());
    cmd.color = QVector3D(0.5F, 0.4F, 0.4F);
    cmd.alpha = 1.0F;
    cmd.variation_scale =
        (u == 0) ? QVector3D(1.0F, 1.0F, 1.0F) : QVector3D(1.05F, 0.95F, 1.0F);
    sub.rigged(cmd);
  }
  ASSERT_EQ(queue.size(), 2U);
  const auto &c0 = std::get<RiggedCreatureCmdIndex>(queue.items()[0]);
  const auto &c1 = std::get<RiggedCreatureCmdIndex>(queue.items()[1]);
  EXPECT_EQ(c0.mesh, c1.mesh);
  EXPECT_NE(c0.variation_scale, c1.variation_scale);
}

TEST(ElephantFullSwitchover, FullPartGraphFirstPrimIsBodyEllipsoid) {
  auto const &spec = Render::Elephant::elephant_creature_spec();
  ASSERT_EQ(spec.lod_full.primitives.size(), 50U);
  EXPECT_EQ(spec.lod_full.primitives[0].shape,
            Render::Creature::PrimitiveShape::OrientedSphere);
}

} // namespace more
