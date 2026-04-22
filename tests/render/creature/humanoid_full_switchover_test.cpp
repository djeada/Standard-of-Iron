// Stage 15.5d — humanoid Full LOD switchover regression.
//
// Verifies that the Full LOD baked PartGraph (k_full_parts, 41
// primitives) bakes into a single RiggedMesh and that
// `submit_humanoid_full_rigged` emits exactly ONE RiggedCreatureCmd
// with a valid bone palette — just like the Reduced LOD test does but
// keyed into the Full LOD bake.
//
// What this test cannot exercise (same caveat as the Reduced test):
// the actual GPU draw / pipeline flush. Those require a live GL
// context and are covered by the offscreen smoke test that launches
// the real binary.

#include "game/core/component.h"
#include "game/core/entity.h"
#include "render/bone_palette_arena.h"
#include "render/creature/part_graph.h"
#include "render/creature/spec.h"
#include "render/draw_queue.h"
#include "render/humanoid/humanoid_renderer_base.h"
#include "render/humanoid/humanoid_spec.h"
#include "render/humanoid/skeleton.h"
#include "render/rigged_mesh.h"
#include "render/rigged_mesh_cache.h"
#include "render/scene_renderer.h"
#include "render/submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

namespace {

using Render::GL::DrawPartCmdIndex;
using Render::GL::HumanoidPose;
using Render::GL::HumanoidVariant;
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

TEST(HumanoidFullSwitchover, FullLodBakesAndEmitsOneRiggedCmd) {
  auto const &spec = Render::Humanoid::humanoid_creature_spec();
  auto bind = Render::Humanoid::humanoid_bind_palette();
  EXPECT_EQ(bind.size(), Render::Humanoid::kBoneCount);

  // Full LOD PartGraph must be populated (41 prims — see k_full_parts).
  ASSERT_GT(spec.lod_full.primitives.size(), 0U)
      << "Stage 15.5d populates Full LOD PartGraph";

  Render::GL::RiggedMeshCache cache;
  Render::GL::BonePaletteArena arena;

  const auto *entry =
      cache.get_or_bake(spec, Render::Creature::CreatureLOD::Full, bind);
  ASSERT_NE(entry, nullptr);
  ASSERT_NE(entry->mesh, nullptr);
  EXPECT_GT(entry->mesh->vertex_count(), 0U)
      << "Full humanoid bake produced empty geometry";
  EXPECT_EQ(entry->inverse_bind.size(), Render::Humanoid::kBoneCount);

  // Full bake should have strictly more vertices than Reduced (6 vs
  // 41 primitives) — this is a quick sanity check that the static
  // Full PartGraph actually drove the bake and we didn't accidentally
  // reuse the Reduced bake.
  const auto *reduced_entry =
      cache.get_or_bake(spec, Render::Creature::CreatureLOD::Reduced, bind);
  ASSERT_NE(reduced_entry, nullptr);
  EXPECT_GT(entry->mesh->vertex_count(), reduced_entry->mesh->vertex_count())
      << "Full LOD bake must contain more geometry than Reduced";

  // Build a current pose (idle at t=0 — identical to bind pose).
  HumanoidPose pose{};
  Render::GL::VariationParams var{};
  var.height_scale = 1.0F;
  var.bulk_scale = 1.0F;
  var.stance_width = 1.0F;
  var.arm_swing_amp = 1.0F;
  var.walk_speed_mult = 1.0F;
  Render::GL::HumanoidRendererBase::compute_locomotion_pose(0U, 0.0F, false,
                                                            var, pose);

  std::array<QMatrix4x4, Render::Humanoid::kBoneCount> palette_buf{};
  std::uint32_t const nbones = Render::Humanoid::compute_bone_palette(
      pose, std::span<QMatrix4x4>(palette_buf.data(), palette_buf.size()));
  EXPECT_EQ(nbones, Render::Humanoid::kBoneCount);

  auto arena_slot_h = arena.allocate_palette();
  QMatrix4x4 *arena_slot = arena_slot_h.cpu;
  ASSERT_NE(arena_slot, nullptr);
  for (std::size_t i = 0; i < nbones; ++i) {
    arena_slot[i] = palette_buf[i] * entry->inverse_bind[i];
  }

  Render::GL::DrawQueue queue;
  Render::GL::QueueSubmitter sub(&queue);

  RiggedCreatureCmd cmd{};
  cmd.mesh = entry->mesh.get();
  cmd.world = QMatrix4x4{};
  cmd.bone_palette = arena_slot;
  cmd.bone_count = nbones;
  cmd.color = QVector3D(0.55F, 0.30F, 0.30F);
  cmd.alpha = 1.0F;

  sub.rigged(cmd);

  // Exactly one command, exactly one RiggedCreatureCmd. The humanoid
  // body emits no DrawPartCmd/MeshCmd — armor/attachments/helmet go
  // through separate code paths not exercised here.
  ASSERT_EQ(queue.size(), 1U);
  EXPECT_EQ(queue.items()[0].index(), RiggedCreatureCmdIndex);

  std::size_t part_count = 0;
  std::size_t mesh_count = 0;
  for (const auto &c : queue.items()) {
    if (c.index() == DrawPartCmdIndex) {
      ++part_count;
    } else if (c.index() == MeshCmdIndex) {
      ++mesh_count;
    }
  }
  EXPECT_EQ(part_count, 0U);
  EXPECT_EQ(mesh_count, 0U);

  const auto &emitted = std::get<RiggedCreatureCmdIndex>(queue.items()[0]);
  EXPECT_EQ(emitted.mesh, entry->mesh.get());
  EXPECT_GT(emitted.bone_count, 0U);
  EXPECT_EQ(emitted.bone_palette, arena_slot);

  // Idle pose at t=0 matches the bind pose, so every skinning matrix
  // should be numerically identity (same invariant the Reduced test
  // enforces). If the bind or bake drifts, this catches it.
  for (std::uint32_t i = 0; i < emitted.bone_count; ++i) {
    const QMatrix4x4 &m = emitted.bone_palette[i];
    for (int r = 0; r < 4; ++r) {
      for (int cc = 0; cc < 4; ++cc) {
        float const expected = (r == cc) ? 1.0F : 0.0F;
        EXPECT_NEAR(m(r, cc), expected, 1e-3F)
            << "bone " << i << " [" << r << "," << cc << "]";
      }
    }
  }

  queue.sort_for_batching();
  EXPECT_EQ(queue.get_sorted(0).index(), RiggedCreatureCmdIndex);
}

TEST(HumanoidFullSwitchover, CacheEnabledRenderStillSubmitsRiggedBody) {
  Render::GL::HumanoidRendererBase humanoid;
  RecordingRenderer renderer;

  Engine::Core::Entity entity(7);
  auto *transform = entity.add_component<Engine::Core::TransformComponent>();
  transform->position = {0.0F, 0.0F, 0.0F};
  transform->rotation = {0.0F, 0.0F, 0.0F};
  transform->scale = {1.0F, 1.0F, 1.0F};
  auto *unit =
      entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  unit->spawn_type = Game::Units::SpawnType::Archer;
  unit->owner_id = 1;

  Render::GL::AnimationInputs anim{};
  anim.time = 0.0F;

  Render::GL::DrawContext ctx{};
  ctx.entity = &entity;
  ctx.renderer_id = "cache_enabled_humanoid_body_regression";
  ctx.allow_template_cache = true;
  ctx.force_single_soldier = true;
  ctx.force_humanoid_lod = true;
  ctx.forced_humanoid_lod = Render::GL::HumanoidLOD::Full;
  ctx.animation_override = &anim;
  ctx.has_seed_override = true;
  ctx.seed_override = 42U;

  humanoid.render(ctx, renderer);

  ASSERT_EQ(renderer.rigged_calls.size(), 1U);
  EXPECT_NE(renderer.rigged_calls[0].mesh, nullptr);
  EXPECT_GT(renderer.rigged_calls[0].bone_count, 0U);
  EXPECT_NE(renderer.rigged_calls[0].bone_palette, nullptr);
  EXPECT_EQ(renderer.rigged_calls[0].role_color_count,
            Render::Humanoid::kHumanoidRoleCount);
  EXPECT_EQ(renderer.rigged_calls[0].role_colors[0],
            renderer.rigged_calls[0].color);
  EXPECT_NE(renderer.rigged_calls[0].role_colors[1],
            renderer.rigged_calls[0].role_colors[0]);
}

TEST(HumanoidFullSwitchover, CacheDistinguishesLodsAndBucket) {
  auto const &spec = Render::Humanoid::humanoid_creature_spec();
  auto bind = Render::Humanoid::humanoid_bind_palette();

  Render::GL::RiggedMeshCache cache;
  const auto *full0 = cache.get_or_bake(
      spec, Render::Creature::CreatureLOD::Full, bind, /*variant_bucket=*/0);
  const auto *full0_again = cache.get_or_bake(
      spec, Render::Creature::CreatureLOD::Full, bind, /*variant_bucket=*/0);
  const auto *full5 = cache.get_or_bake(
      spec, Render::Creature::CreatureLOD::Full, bind, /*variant_bucket=*/5);
  const auto *reduced0 = cache.get_or_bake(
      spec, Render::Creature::CreatureLOD::Reduced, bind, /*variant_bucket=*/0);

  ASSERT_NE(full0, nullptr);
  ASSERT_NE(full5, nullptr);
  ASSERT_NE(reduced0, nullptr);
  EXPECT_EQ(full0, full0_again) << "same key hits same entry";
  EXPECT_NE(full0, full5) << "variant_bucket keys must differ";
  EXPECT_NE(full0, reduced0) << "LOD key must differ";
  EXPECT_EQ(cache.size(), 3U);
}

TEST(HumanoidFullSwitchover, BonePaletteMatricesAreFinite) {
  HumanoidPose pose{};
  Render::GL::VariationParams var{};
  var.height_scale = 1.0F;
  var.bulk_scale = 1.0F;
  var.stance_width = 1.0F;
  var.arm_swing_amp = 1.0F;
  var.walk_speed_mult = 1.0F;
  // Non-zero time to exercise a real (non-bind) pose.
  Render::GL::HumanoidRendererBase::compute_locomotion_pose(0xABCDU, 0.42F,
                                                            true, var, pose);

  std::array<QMatrix4x4, Render::Humanoid::kBoneCount> palette_buf{};
  std::uint32_t const nbones = Render::Humanoid::compute_bone_palette(
      pose, std::span<QMatrix4x4>(palette_buf.data(), palette_buf.size()));
  EXPECT_GT(nbones, 0U);
  for (std::uint32_t i = 0; i < nbones; ++i) {
    const QMatrix4x4 &m = palette_buf[i];
    for (int r = 0; r < 4; ++r) {
      for (int c = 0; c < 4; ++c) {
        EXPECT_TRUE(std::isfinite(m(r, c)))
            << "bone " << i << " [" << r << "," << c << "] not finite";
      }
    }
  }
}

TEST(HumanoidFullSwitchover, IdlePoseUsesRelaxedHumanStance) {
  using HP = Render::GL::HumanProportions;

  HumanoidPose pose{};
  Render::GL::VariationParams var{};
  var.height_scale = 1.0F;
  var.bulk_scale = 1.0F;
  var.stance_width = 1.0F;
  var.arm_swing_amp = 1.0F;
  var.walk_speed_mult = 1.0F;

  Render::GL::HumanoidRendererBase::compute_locomotion_pose(0x1234U, 0.0F,
                                                            false, var, pose);

  float const shoulder_span = pose.shoulder_r.x() - pose.shoulder_l.x();
  float const foot_span = pose.foot_r.x() - pose.foot_l.x();

  EXPECT_GT(foot_span, shoulder_span * 0.70F);
  EXPECT_LT(foot_span, shoulder_span * 1.00F);
  EXPECT_LT(pose.hand_l.y(), pose.shoulder_l.y());
  EXPECT_LT(pose.hand_r.y(), pose.shoulder_r.y());
  EXPECT_GT(std::abs(pose.hand_l.x()), std::abs(pose.shoulder_l.x()) * 0.60F);
  EXPECT_GT(std::abs(pose.hand_r.x()), std::abs(pose.shoulder_r.x()) * 0.60F);
  EXPECT_LT(std::abs(pose.hand_l.x()), std::abs(pose.shoulder_l.x()) * 0.95F);
  EXPECT_LT(std::abs(pose.hand_r.x()), std::abs(pose.shoulder_r.x()) * 0.95F);
  EXPECT_GT(pose.head_pos.z(), pose.pelvis_pos.z());
  EXPECT_LT(pose.head_pos.z() - pose.neck_base.z(), 0.02F);
  EXPECT_LT(std::abs(pose.hand_l.z() - pose.pelvis_pos.z()), 0.08F);
  EXPECT_LT(std::abs(pose.hand_r.z() - pose.pelvis_pos.z()), 0.08F);
  EXPECT_GE(pose.foot_l.y(), HP::GROUND_Y);
  EXPECT_GE(pose.foot_r.y(), HP::GROUND_Y);
}

TEST(HumanoidFullSwitchover, MovingPoseAddsGroundedSupportAndCounterSwing) {
  using HP = Render::GL::HumanProportions;

  HumanoidPose pose{};
  Render::GL::VariationParams var{};
  var.height_scale = 1.0F;
  var.bulk_scale = 1.0F;
  var.stance_width = 1.0F;
  var.arm_swing_amp = 1.0F;
  var.walk_speed_mult = 1.0F;

  Render::GL::HumanoidRendererBase::compute_locomotion_pose(0U, 0.2F, true, var,
                                                            pose);

  float const ground = HP::GROUND_Y + pose.foot_y_offset;
  float const left_foot_rel = pose.foot_l.z() - pose.pelvis_pos.z();
  float const right_foot_rel = pose.foot_r.z() - pose.pelvis_pos.z();
  float const left_hand_rel = pose.hand_l.z() - pose.shoulder_l.z();
  float const right_hand_rel = pose.hand_r.z() - pose.shoulder_r.z();

  EXPECT_NEAR(std::min(pose.foot_l.y(), pose.foot_r.y()), ground, 0.01F);
  EXPECT_GT(std::abs(pose.foot_l.y() - pose.foot_r.y()), 0.03F);
  EXPECT_GT(pose.shoulder_l.z(), pose.pelvis_pos.z());
  EXPECT_GT(pose.shoulder_r.z(), pose.pelvis_pos.z());
  EXPECT_LT(left_hand_rel * left_foot_rel, 0.0F);
  EXPECT_LT(right_hand_rel * right_foot_rel, 0.0F);
}

} // namespace
