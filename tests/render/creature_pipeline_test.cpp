// Phase A — CreaturePipeline unit tests.
//
// These tests exercise the new pipeline shell with synthetic
// UnitVisualSpec / CreatureFrame inputs. They intentionally avoid
// touching the legacy HumanoidRendererBase / IEquipmentRenderer paths
// so they validate the new contract in isolation; visual parity with
// the imperative orchestrator is covered by the existing
// rigged_visual_parity_test.cpp.

#include "render/creature/pipeline/creature_frame.h"
#include "render/creature/pipeline/creature_pipeline.h"
#include "render/creature/pipeline/equipment_registry.h"
#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <array>
#include <gtest/gtest.h>

namespace {

using namespace Render::Creature::Pipeline;
using Render::Creature::CreatureLOD;

// Minimal ISubmitter that ignores everything — Phase A submit() does
// not yet touch the submitter interface, but we still want to exercise
// the call path so future phases inherit a working harness.
class NullSubmitter : public Render::GL::ISubmitter {
public:
  void mesh(Render::GL::Mesh *, const QMatrix4x4 &, const QVector3D &,
            Render::GL::Texture *, float, int) override {}
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

TEST(CreaturePipelineFrame, EmptyFrameIsConsistent) {
  CreatureFrame f;
  EXPECT_TRUE(f.empty());
  EXPECT_EQ(f.size(), 0u);
  EXPECT_TRUE(frame_columns_consistent(f));
}

TEST(CreaturePipelineFrame, PushKeepsColumnsAligned) {
  CreatureFrame f;
  f.reserve(2);
  f.push(/*id*/ 7, QMatrix4x4{}, /*spec*/ 0, /*seed*/ 42, CreatureLOD::Full,
         PoseState{});
  f.push(/*id*/ 8, QMatrix4x4{}, /*spec*/ 0, /*seed*/ 43, CreatureLOD::Reduced,
         PoseState{});
  EXPECT_EQ(f.size(), 2u);
  EXPECT_TRUE(frame_columns_consistent(f));
  f.clear();
  EXPECT_TRUE(f.empty());
  EXPECT_TRUE(frame_columns_consistent(f));
}

TEST(CreaturePipelineLod, PicksByDistance) {
  FrameContext ctx;
  ctx.view_distance_full = 10.0F;
  ctx.view_distance_reduced = 20.0F;
  ctx.view_distance_minimal = 40.0F;
  EXPECT_EQ(pick_lod(ctx, 0.0F), CreatureLOD::Full);
  EXPECT_EQ(pick_lod(ctx, 9.99F), CreatureLOD::Full);
  EXPECT_EQ(pick_lod(ctx, 10.0F), CreatureLOD::Reduced);
  EXPECT_EQ(pick_lod(ctx, 19.99F), CreatureLOD::Reduced);
  EXPECT_EQ(pick_lod(ctx, 20.0F), CreatureLOD::Minimal);
  EXPECT_EQ(pick_lod(ctx, 39.99F), CreatureLOD::Minimal);
  EXPECT_EQ(pick_lod(ctx, 40.0F), CreatureLOD::Billboard);
  EXPECT_EQ(pick_lod(ctx, 1000.0F), CreatureLOD::Billboard);
}

TEST(CreaturePipelineProcess, CountsLodBuckets) {
  CreaturePipeline pipeline;
  FrameContext ctx;
  std::array<UnitVisualSpec, 1> specs{};
  specs[0].kind = CreatureKind::Humanoid;

  CreatureFrame f;
  f.push(1, QMatrix4x4{}, 0, 0, CreatureLOD::Full, PoseState{});
  f.push(2, QMatrix4x4{}, 0, 0, CreatureLOD::Full, PoseState{});
  f.push(3, QMatrix4x4{}, 0, 0, CreatureLOD::Reduced, PoseState{});
  f.push(4, QMatrix4x4{}, 0, 0, CreatureLOD::Minimal, PoseState{});
  f.push(5, QMatrix4x4{}, 0, 0, CreatureLOD::Billboard, PoseState{});

  const auto stats = pipeline.process(ctx, specs, f);
  EXPECT_EQ(stats.lod_full, 2u);
  EXPECT_EQ(stats.lod_reduced, 1u);
  EXPECT_EQ(stats.lod_minimal, 1u);
  EXPECT_EQ(stats.lod_billboard, 1u);
}

TEST(CreaturePipelineProcess, EmptyFrameYieldsZeroStats) {
  CreaturePipeline pipeline;
  FrameContext ctx;
  std::array<UnitVisualSpec, 1> specs{};
  CreatureFrame f;
  const auto stats = pipeline.process(ctx, specs, f);
  EXPECT_EQ(stats.lod_full, 0u);
  EXPECT_EQ(stats.entities_submitted, 0u);
}

TEST(CreaturePipelineSubmit, CountsEntitiesAndInlineEquipment) {
  CreaturePipeline pipeline;
  FrameContext ctx;
  NullSubmitter sink;

  // Two equipment records inlined on the spec — exercises the
  // span-on-spec path.
  std::array<EquipmentRecord, 2> records{};
  records[0].material_id = 1;
  records[0].tint_role = TintRole::Cloth;
  records[1].material_id = 2;
  records[1].tint_role = TintRole::Metal;

  std::array<UnitVisualSpec, 1> specs{};
  specs[0].equipment =
      std::span<const EquipmentRecord>{records.data(), records.size()};

  CreatureFrame f;
  f.push(1, QMatrix4x4{}, 0, 0, CreatureLOD::Full, PoseState{});
  f.push(2, QMatrix4x4{}, 0, 0, CreatureLOD::Reduced, PoseState{});

  const auto stats = pipeline.submit(ctx, specs, f, sink);
  EXPECT_EQ(stats.entities_submitted, 2u);
  EXPECT_EQ(stats.equipment_submitted, 4u);
}

TEST(EquipmentRegistry, RegisterFindGet) {
  auto &reg = EquipmentRegistry::instance();
  reg.clear();

  std::array<EquipmentRecord, 1> rec{};
  rec[0].material_id = 7;
  rec[0].tint_role = TintRole::Wood;

  const auto id = reg.register_loadout(
      "test/spec_a", std::span<const EquipmentRecord>{rec.data(), rec.size()});
  EXPECT_NE(id, kInvalidSpec);
  EXPECT_EQ(reg.find("test/spec_a"), id);
  EXPECT_EQ(reg.find("test/missing"), kInvalidSpec);
  ASSERT_EQ(reg.get(id).size(), 1u);
  EXPECT_EQ(reg.get(id)[0].material_id, 7);
  EXPECT_EQ(reg.get(id)[0].tint_role, TintRole::Wood);
  EXPECT_TRUE(reg.get(kInvalidSpec).empty());
}

TEST(EquipmentRegistry, ReregisterUpdatesInPlace) {
  auto &reg = EquipmentRegistry::instance();
  reg.clear();

  std::array<EquipmentRecord, 1> a{};
  a[0].material_id = 1;
  std::array<EquipmentRecord, 2> b{};
  b[0].material_id = 11;
  b[1].material_id = 12;

  const auto id1 = reg.register_loadout("k", a);
  const auto id2 = reg.register_loadout("k", b);
  EXPECT_EQ(id1, id2);
  ASSERT_EQ(reg.get(id1).size(), 2u);
  EXPECT_EQ(reg.get(id1)[1].material_id, 12);
}

TEST(CreaturePipelineSubmit, FallsBackToRegistryWhenSpecHasNoInlineEquipment) {
  auto &reg = EquipmentRegistry::instance();
  reg.clear();

  std::array<EquipmentRecord, 3> rec{};
  rec[0].material_id = 1;
  rec[1].material_id = 2;
  rec[2].material_id = 3;
  const auto id = reg.register_loadout("unit/x", rec);

  std::array<UnitVisualSpec, 4> specs{};
  ASSERT_LT(static_cast<std::size_t>(id), specs.size());
  // Leave specs[id].equipment empty — pipeline must consult the
  // registry by spec_id.

  CreaturePipeline pipeline;
  FrameContext ctx;
  NullSubmitter sink;

  CreatureFrame f;
  f.push(1, QMatrix4x4{}, id, 0, CreatureLOD::Full, PoseState{});

  const auto stats = pipeline.submit(ctx, specs, f, sink);
  EXPECT_EQ(stats.entities_submitted, 1u);
  EXPECT_EQ(stats.equipment_submitted, 3u);
}

} // namespace
