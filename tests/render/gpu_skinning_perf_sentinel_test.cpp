// Stage 16.5 — submit-phase perf sentinel.
//
// A regression smoke that runs 200 humanoid Full-LOD rigged submits
// (cache + arena + RiggedCreatureCmd) and asserts the wall-clock stays
// under a generous absolute budget. This is NOT a strict 30%-vs-pre-15.5
// gate (that comparison requires reverting prior commits and is not
// reproducible from this commit alone); it is a forward-looking
// guardrail that future changes can't quietly explode the submit hot
// path on the now-baked GPU-skinning route.
//
// Headless: no GL context, so the arena's GPU upload path no-ops and
// rigged_mesh.cpp logs a warning on first submit. The CPU work
// (skeleton evaluate → palette compose → DrawQueue submit) IS measured,
// which is exactly what the audit step is meant to keep honest.
//
// Threshold rationale: 200 submits × ~ tens of µs per humanoid is
// expected to land under 50 ms even on a slow CI worker; the assertion
// uses 250 ms to absorb startup / RiggedMesh bake + lots of headroom.

#include "render/bone_palette_arena.h"
#include "render/draw_queue.h"
#include "render/humanoid/humanoid_renderer_base.h"
#include "render/humanoid/humanoid_spec.h"
#include "render/humanoid/skeleton.h"
#include "render/rigged_mesh_cache.h"
#include "render/submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <chrono>
#include <cstdint>
#include <gtest/gtest.h>

namespace {

constexpr int k_units = 200;
constexpr long long k_budget_us_total = 250'000; // 250 ms ceiling

TEST(GpuSkinningPerfSentinel, TwoHundredHumanoidSubmitsUnderBudget) {
  using namespace Render::GL;
  using namespace Render::Humanoid;

  RiggedMeshCache cache;
  BonePaletteArena arena;
  auto const &spec = humanoid_creature_spec();
  auto bind = humanoid_bind_palette();
  ASSERT_NE(cache.get_or_bake(spec, Render::Creature::CreatureLOD::Full, bind),
            nullptr);

  HumanoidPose pose{};
  VariationParams var{};
  var.height_scale = 1.0F;
  var.bulk_scale = 1.0F;
  var.stance_width = 1.0F;
  var.arm_swing_amp = 1.0F;
  var.walk_speed_mult = 1.0F;
  HumanoidRendererBase::compute_locomotion_pose(0U, 0.0F, false, var, pose);

  HumanoidVariant variant{};
  variant.palette.cloth = QVector3D(0.5F, 0.5F, 0.6F);

  DrawQueue queue;

  std::array<QMatrix4x4, kBoneCount> palette_buf{};
  std::uint32_t const nbones = compute_bone_palette(
      pose, std::span<QMatrix4x4>(palette_buf.data(), palette_buf.size()));
  ASSERT_EQ(nbones, kBoneCount);
  auto const *entry =
      cache.get_or_bake(spec, Render::Creature::CreatureLOD::Full, bind);
  ASSERT_NE(entry, nullptr);

  arena.reset_frame();
  auto const t0 = std::chrono::steady_clock::now();
  for (int i = 0; i < k_units; ++i) {
    auto slot = arena.allocate_palette();
    for (std::size_t b = 0; b < nbones; ++b) {
      slot.cpu[b] = palette_buf[b] * entry->inverse_bind[b];
    }
    QMatrix4x4 world;
    world.translate(static_cast<float>(i), 0.0F, 0.0F);

    RiggedCreatureCmd cmd{};
    cmd.mesh = entry->mesh.get();
    cmd.world = world;
    cmd.bone_palette = slot.cpu;
    cmd.palette_ubo = slot.ubo;
    cmd.palette_offset = static_cast<std::uint32_t>(slot.offset);
    cmd.bone_count = nbones;
    cmd.color = variant.palette.cloth;
    queue.submit(cmd);
  }
  queue.sort_for_batching();
  auto const t1 = std::chrono::steady_clock::now();

  auto const elapsed_us =
      std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
  ASSERT_EQ(queue.size(), static_cast<std::size_t>(k_units));
  EXPECT_EQ(arena.allocations_in_flight(), static_cast<std::size_t>(k_units));
  EXPECT_LT(elapsed_us, k_budget_us_total)
      << "200-unit rigged submit phase exceeded budget; recorded " << elapsed_us
      << " us";

  ::testing::Test::RecordProperty("perf_us_total",
                                  static_cast<int>(elapsed_us));
  std::printf(
      "[perf] 200x humanoid Full submit phase: %lld us (%.2f us/unit)\n",
      static_cast<long long>(elapsed_us),
      static_cast<double>(elapsed_us) / k_units);
}

} // namespace
