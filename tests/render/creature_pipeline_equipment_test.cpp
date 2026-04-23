// Phase D — equipment-loadout tests for CreaturePipeline.
//
// These tests assert that:
//   1. A unit with a non-empty humanoid equipment loadout routes
//      submission through the new pipeline (legacy_submit fires once
//      per record, in order). The legacy `draw_*` virtuals on the
//      spec are NOT consulted by the pipeline path.
//   2. Socket math: `compose_equipment_world(world, record, topology,
//      palette)` composes `unit_world * socket_transform * local_offset`
//      against a synthetic two-bone topology + palette.
//   3. Horse tack loadout: a horse spec with three legacy horse-tack
//      records issues three equipment draws via the horse shim.

#include "render/creature/pipeline/creature_frame.h"
#include "render/creature/pipeline/creature_pipeline.h"
#include "render/creature/pipeline/equipment_registry.h"
#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/creature/skeleton.h"
#include "render/entity/registry.h"
#include "render/equipment/equipment_submit.h"
#include "render/equipment/horse/i_horse_equipment_renderer.h"
#include "render/equipment/i_equipment_renderer.h"
#include "render/equipment/weapons/bow_renderer.h"
#include "render/equipment/weapons/shield_renderer.h"
#include "render/equipment/weapons/spear_renderer.h"
#include "render/equipment/weapons/sword_renderer.h"
#include "render/horse/horse_renderer_base.h"
#include "render/horse/horse_spec.h"
#include "render/humanoid/humanoid_renderer_base.h"
#include "render/humanoid/skeleton.h"
#include "render/submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <array>
#include <gtest/gtest.h>
#include <vector>

namespace {

using namespace Render::Creature::Pipeline;
using Render::Creature::CreatureLOD;

auto fake_mesh() -> Render::GL::Mesh * {
  static int sentinel = 0;
  return reinterpret_cast<Render::GL::Mesh *>(&sentinel);
}

class NullSubmitter : public Render::GL::ISubmitter {
public:
  int mesh_calls{0};
  int cylinder_calls{0};

  void mesh(Render::GL::Mesh *, const QMatrix4x4 &, const QVector3D &,
            Render::GL::Texture *, float, int) override {
    ++mesh_calls;
  }
  void cylinder(const QVector3D &, const QVector3D &, float, const QVector3D &,
                float) override {
    ++cylinder_calls;
  }
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

class ModelRecordingSubmitter : public Render::GL::ISubmitter {
public:
  std::vector<QMatrix4x4> models;

  void mesh(Render::GL::Mesh *, const QMatrix4x4 &model, const QVector3D &,
            Render::GL::Texture *, float, int) override {
    models.push_back(model);
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

// Test double — counts invocations and pushes a single mesh primitive
// into the batch so submit_equipment_batch downstream is exercised.
class CountingHumanoidEquipment : public Render::GL::IEquipmentRenderer {
public:
  mutable int calls{0};

  void render(const Render::GL::DrawContext &, const Render::GL::BodyFrames &,
              const Render::GL::HumanoidPalette &,
              const Render::GL::HumanoidAnimationContext &,
              Render::GL::EquipmentBatch &batch) override {
    ++calls;
    Render::GL::EquipmentMeshPrim prim{};
    batch.meshes.push_back(prim);
  }
};

class CountingHorseEquipment : public Render::GL::IHorseEquipmentRenderer {
public:
  mutable int calls{0};

  void render(const Render::GL::DrawContext &,
              const Render::GL::HorseBodyFrames &,
              const Render::GL::HorseVariant &,
              const Render::GL::HorseAnimationContext &,
              Render::GL::EquipmentBatch &batch) const override {
    ++calls;
    Render::GL::EquipmentMeshPrim prim{};
    batch.meshes.push_back(prim);
  }
};

TEST(PhaseDEquipment, HumanoidLoadoutDispatchesShimRecordsInOrder) {
  // Build a spec with three legacy-shim humanoid equipment records,
  // each backed by its own counting test double. Submit a single
  // humanoid row through the pipeline and assert each shim fired
  // exactly once. The spec carries no PoseHookFn / VariantHookFn so
  // we are also implicitly asserting that the pipeline does not
  // consult any legacy `draw_*` virtual to walk the loadout.
  CountingHumanoidEquipment helmet;
  CountingHumanoidEquipment armor;
  CountingHumanoidEquipment weapon;

  std::array<EquipmentRecord, 3> records{
      make_legacy_equipment_record(helmet),
      make_legacy_equipment_record(armor),
      make_legacy_equipment_record(weapon),
  };

  std::array<UnitVisualSpec, 1> specs{};
  specs[0].kind = CreatureKind::Humanoid;
  specs[0].equipment =
      std::span<const EquipmentRecord>{records.data(), records.size()};
  specs[0].owned_legacy_slots = LegacySlotMask::AllHumanoid;

  Render::GL::DrawContext ctx{};
  Render::GL::HumanoidPose pose{};
  Render::GL::HumanoidVariant variant{};
  Render::GL::HumanoidAnimationContext anim{};

  CreatureFrame frame;
  frame.push_humanoid(/*id*/ 0, QMatrix4x4{}, /*spec*/ 0, /*seed*/ 1,
                      CreatureLOD::Full, pose, variant, anim);
  // Phase D — populate the legacy_ctx sidecar so the shim trampoline
  // fires (it early-outs on null ctx).
  ASSERT_FALSE(frame.legacy_ctx.empty());
  frame.legacy_ctx.back() = &ctx;

  CreaturePipeline pipeline;
  FrameContext fctx;
  NullSubmitter sink;

  const auto stats = pipeline.submit(fctx, specs, frame, sink);

  EXPECT_EQ(helmet.calls, 1);
  EXPECT_EQ(armor.calls, 1);
  EXPECT_EQ(weapon.calls, 1);
  EXPECT_EQ(stats.equipment_submitted, 3u);
}

TEST(PhaseDEquipment, ComposeEquipmentWorldAppliesSocketAndOffset) {
  // Build a synthetic two-bone topology: Root at origin, Hand offset
  // along +X. Construct a palette where the Hand bone's world
  // transform is a pure translation to (3, 0, 0). Verify that
  // compose_equipment_world(world, record, topology, palette) yields
  // unit_world * hand_world * local_offset.
  using Render::Creature::BoneDef;
  using Render::Creature::BoneIndex;
  using Render::Creature::SkeletonTopology;
  using Render::Creature::SocketDef;
  using Render::Creature::SocketIndex;

  static constexpr std::array<BoneDef, 2> bones{
      BoneDef{"Root", Render::Creature::kInvalidBone},
      BoneDef{"Hand", BoneIndex{0}},
  };
  static constexpr std::array<SocketDef, 1> sockets{
      SocketDef{"hand", BoneIndex{1}, QVector3D{0.0F, 0.0F, 0.0F}},
  };

  SkeletonTopology topo{};
  topo.bones = std::span<const BoneDef>{bones.data(), bones.size()};
  topo.sockets = std::span<const SocketDef>{sockets.data(), sockets.size()};

  std::array<QMatrix4x4, 2> palette{};
  palette[0].setToIdentity();
  palette[1].setToIdentity();
  palette[1].translate(3.0F, 0.0F, 0.0F);

  EquipmentRecord rec{};
  rec.socket = SocketIndex{0};
  rec.local_offset.setToIdentity();
  rec.local_offset.translate(0.0F, 0.5F, 0.0F);

  QMatrix4x4 unit_world;
  unit_world.setToIdentity();
  unit_world.translate(10.0F, 0.0F, 0.0F);

  const QMatrix4x4 composed = compose_equipment_world(
      unit_world, rec, &topo,
      std::span<const QMatrix4x4>{palette.data(), palette.size()});

  // Expected: translate by (10,0,0), then by hand-bone (3,0,0), then
  // by record (0, 0.5, 0) => column-3 = (13, 0.5, 0, 1).
  const QVector4D origin = composed.column(3);
  EXPECT_NEAR(origin.x(), 13.0F, 1e-4F);
  EXPECT_NEAR(origin.y(), 0.5F, 1e-4F);
  EXPECT_NEAR(origin.z(), 0.0F, 1e-4F);
  EXPECT_NEAR(origin.w(), 1.0F, 1e-4F);

  // Sanity: with no topology the helper degrades to unit_world * offset.
  const QMatrix4x4 fallback = compose_equipment_world(unit_world, rec);
  const QVector4D fallback_origin = fallback.column(3);
  EXPECT_NEAR(fallback_origin.x(), 10.0F, 1e-4F);
  EXPECT_NEAR(fallback_origin.y(), 0.5F, 1e-4F);
}

TEST(PhaseDEquipment, HorseTackLoadoutIssuesThreeShimDraws) {
  CountingHorseEquipment reins;
  CountingHorseEquipment blanket;
  CountingHorseEquipment saddle;

  std::array<EquipmentRecord, 3> records{
      make_legacy_horse_equipment_record(reins),
      make_legacy_horse_equipment_record(blanket),
      make_legacy_horse_equipment_record(saddle),
  };

  std::array<UnitVisualSpec, 1> specs{};
  specs[0].kind = CreatureKind::Horse;
  specs[0].equipment =
      std::span<const EquipmentRecord>{records.data(), records.size()};
  specs[0].owned_legacy_slots = LegacySlotMask::HorseAttachments;

  Render::GL::DrawContext ctx{};
  Render::GL::HorseBodyFrames horse_frames{};
  Render::GL::HorseVariant horse_variant{};
  Render::Horse::HorseSpecPose horse_pose{};

  CreatureFrame frame;
  frame.push_horse(/*id*/ 0, QMatrix4x4{}, /*spec*/ 0, /*seed*/ 7,
                   CreatureLOD::Full, horse_pose, horse_variant);
  // Wire the row's horse sidecars + DrawContext so the horse shim
  // trampoline can fire.
  ASSERT_FALSE(frame.legacy_ctx.empty());
  frame.legacy_ctx.back() = &ctx;
  frame.horse_frames.back() = &horse_frames;
  frame.horse_variant.back() = horse_variant;
  // horse_anim default-initialised is fine.

  CreaturePipeline pipeline;
  FrameContext fctx;
  NullSubmitter sink;

  const auto stats = pipeline.submit(fctx, specs, frame, sink);

  EXPECT_EQ(reins.calls, 1);
  EXPECT_EQ(blanket.calls, 1);
  EXPECT_EQ(saddle.calls, 1);
  EXPECT_EQ(stats.equipment_submitted, 3u);
}

// Phase D — stateless weapon record factory smoke test. Drives a
// SwordRenderConfig + ShieldRenderConfig + SpearRenderConfig +
// BowRenderConfig through the pipeline via the new
// `make_stateless_*_record` factories and verifies that:
//   * each record is dispatched (equipment_submitted counter +
//     non-zero mesh emissions through the sink),
//   * no legacy IEquipmentRenderer instance is involved in the path
//     (the records carry only a const-config pointer in legacy_user),
//   * dispatch is reentrant: invoking the same loadout twice
//     produces identical mesh-emission counts (no leftover state).
TEST(PhaseDEquipment, StatelessWeaponRecordsDispatchWithoutInstanceState) {
  static const Render::GL::SwordRenderConfig sword_cfg{};
  static const Render::GL::ShieldRenderConfig shield_cfg{};
  static const Render::GL::SpearRenderConfig spear_cfg{};
  static const Render::GL::BowRenderConfig bow_cfg = []() {
    Render::GL::BowRenderConfig c;
    c.bow_top_y = 1.4F;
    c.bow_bot_y = 0.4F;
    return c;
  }();

  std::array<EquipmentRecord, 4> records{
      make_stateless_sword_record(sword_cfg),
      make_stateless_shield_record(shield_cfg),
      make_stateless_spear_record(spear_cfg),
      make_stateless_bow_record(bow_cfg),
  };
  for (const auto &r : records) {
    EXPECT_NE(r.legacy_submit, nullptr);
    EXPECT_NE(r.legacy_user, nullptr);
  }

  std::array<UnitVisualSpec, 1> specs{};
  specs[0].kind = CreatureKind::Humanoid;
  specs[0].equipment =
      std::span<const EquipmentRecord>{records.data(), records.size()};
  specs[0].owned_legacy_slots = LegacySlotMask::AllHumanoid;

  Render::GL::DrawContext ctx{};
  Render::GL::HumanoidPose pose{};
  pose.body_frames.hand_r.origin = {0.30F, 1.10F, 0.20F};
  pose.body_frames.hand_l.origin = {-0.30F, 1.10F, 0.20F};
  pose.body_frames.hand_r.right = {1.0F, 0.0F, 0.0F};
  pose.body_frames.hand_l.right = {-1.0F, 0.0F, 0.0F};
  Render::GL::HumanoidVariant variant{};
  Render::GL::HumanoidAnimationContext anim{};

  CreatureFrame frame;
  frame.push_humanoid(/*id*/ 0, QMatrix4x4{}, /*spec*/ 0, /*seed*/ 1,
                      CreatureLOD::Full, pose, variant, anim);
  ASSERT_FALSE(frame.legacy_ctx.empty());
  frame.legacy_ctx.back() = &ctx;

  CreaturePipeline pipeline;
  FrameContext fctx;
  NullSubmitter sink;

  const auto stats1 = pipeline.submit(fctx, specs, frame, sink);
  EXPECT_EQ(stats1.equipment_submitted, 4u);
  const int meshes_after_first = sink.mesh_calls;
  EXPECT_GT(meshes_after_first, 0);

  // Reentrance check — same loadout, fresh frame, identical inputs.
  CreatureFrame frame2;
  frame2.push_humanoid(/*id*/ 0, QMatrix4x4{}, /*spec*/ 0, /*seed*/ 1,
                       CreatureLOD::Full, pose, variant, anim);
  frame2.legacy_ctx.back() = &ctx;
  NullSubmitter sink2;
  const auto stats2 = pipeline.submit(fctx, specs, frame2, sink2);
  EXPECT_EQ(stats2.equipment_submitted, 4u);
  EXPECT_EQ(sink2.mesh_calls, meshes_after_first)
      << "stateless record dispatch must be deterministic across frames";
}

// Phase E — closure-based dispatch test. The same EquipmentRecord
// must yield different draw calls for different per-entity seeds when
// the closure derives its config from `sub.seed`.
TEST(PhaseEEquipment, PayloadRecordDispatchesPerEntityFromSeed) {
  struct FakeRenderer {
    struct Config {
      std::uint32_t derived_seed{0};
    };
    static void submit(const Config &, const Render::GL::DrawContext &,
                       const Render::GL::BodyFrames &,
                       const Render::GL::HumanoidPalette &,
                       const Render::GL::HumanoidAnimationContext &,
                       Render::GL::EquipmentBatch &) {}
  };

  std::vector<std::uint32_t> observed_seeds;
  auto payload_fn = [&observed_seeds](std::uint32_t seed) {
    observed_seeds.push_back(seed);
    return FakeRenderer::Config{seed};
  };

  std::array<EquipmentRecord, 1> records{
      make_payload_record<FakeRenderer>(payload_fn),
  };
  ASSERT_TRUE(static_cast<bool>(records[0].dispatch));

  std::array<UnitVisualSpec, 1> specs{};
  specs[0].kind = CreatureKind::Humanoid;
  specs[0].equipment =
      std::span<const EquipmentRecord>{records.data(), records.size()};

  Render::GL::DrawContext ctx{};
  Render::GL::HumanoidPose pose{};
  Render::GL::HumanoidVariant variant{};
  Render::GL::HumanoidAnimationContext anim{};

  // Three rows with distinct seeds 7, 11, 23. Use Billboard LOD so
  // body submission is skipped; the equipment loop still runs.
  CreatureFrame frame;
  const std::array<std::uint32_t, 3> seeds{7U, 11U, 23U};
  for (std::uint32_t s : seeds) {
    frame.push_humanoid(/*id*/ s, QMatrix4x4{}, /*spec*/ 0, /*seed*/ s,
                        CreatureLOD::Billboard, pose, variant, anim);
    frame.legacy_ctx.back() = &ctx;
  }

  CreaturePipeline pipeline;
  FrameContext fctx;
  NullSubmitter sink;

  const auto stats = pipeline.submit(fctx, specs, frame, sink);

  EXPECT_EQ(stats.equipment_submitted, 3u);
  ASSERT_EQ(observed_seeds.size(), 3U);
  EXPECT_EQ(observed_seeds[0], 7U);
  EXPECT_EQ(observed_seeds[1], 11U);
  EXPECT_EQ(observed_seeds[2], 23U);
}

TEST(PhaseEEquipment, PayloadRecordSameSeedSameOutput) {
  struct FakeRenderer {
    struct Config {
      std::uint32_t observed_seed{0};
    };
    static void submit(const Config &, const Render::GL::DrawContext &,
                       const Render::GL::BodyFrames &,
                       const Render::GL::HumanoidPalette &,
                       const Render::GL::HumanoidAnimationContext &,
                       Render::GL::EquipmentBatch &) {}
  };

  int payload_calls = 0;
  auto payload_fn = [&payload_calls](std::uint32_t seed) {
    ++payload_calls;
    return FakeRenderer::Config{seed};
  };

  std::array<EquipmentRecord, 1> records{
      make_payload_record<FakeRenderer>(payload_fn),
  };

  std::array<UnitVisualSpec, 1> specs{};
  specs[0].kind = CreatureKind::Humanoid;
  specs[0].equipment =
      std::span<const EquipmentRecord>{records.data(), records.size()};

  Render::GL::DrawContext ctx{};
  Render::GL::HumanoidPose pose{};
  Render::GL::HumanoidVariant variant{};
  Render::GL::HumanoidAnimationContext anim{};

  CreatureFrame frame;
  frame.push_humanoid(/*id*/ 0, QMatrix4x4{}, /*spec*/ 0, /*seed*/ 42,
                      CreatureLOD::Billboard, pose, variant, anim);
  frame.legacy_ctx.back() = &ctx;

  CreaturePipeline pipeline;
  FrameContext fctx;
  NullSubmitter sink;

  pipeline.submit(fctx, specs, frame, sink);
  pipeline.submit(fctx, specs, frame, sink);

  EXPECT_EQ(payload_calls, 2)
      << "payload_fn must be invoked once per entity per submit()";
}

// Phase E — pipeline must thread the row's HumanoidPose into the
// EquipmentSubmitContext so closures performing procedural draws
// (healer robes, builder tunic) can read raw bone transforms.
TEST(PhaseEEquipment, DispatchContextCarriesHumanoidPose) {
  const Render::GL::HumanoidPose *observed_pose = nullptr;
  const Render::GL::BodyFrames *observed_frames = nullptr;

  EquipmentRecord rec{};
  rec.dispatch = [&observed_pose,
                  &observed_frames](const EquipmentSubmitContext &sub,
                                    Render::GL::EquipmentBatch &) {
    observed_pose = sub.pose;
    observed_frames = sub.frames;
  };

  std::array<EquipmentRecord, 1> records{rec};
  std::array<UnitVisualSpec, 1> specs{};
  specs[0].kind = CreatureKind::Humanoid;
  specs[0].equipment =
      std::span<const EquipmentRecord>{records.data(), records.size()};

  Render::GL::DrawContext ctx{};
  Render::GL::HumanoidPose pose{};
  Render::GL::HumanoidVariant variant{};
  Render::GL::HumanoidAnimationContext anim{};

  CreatureFrame frame;
  frame.push_humanoid(/*id*/ 0, QMatrix4x4{}, /*spec*/ 0, /*seed*/ 1,
                      CreatureLOD::Billboard, pose, variant, anim);
  frame.legacy_ctx.back() = &ctx;

  CreaturePipeline pipeline;
  FrameContext fctx;
  NullSubmitter sink;
  pipeline.submit(fctx, specs, frame, sink);

  ASSERT_NE(observed_pose, nullptr)
      << "EquipmentSubmitContext.pose must be populated for humanoid rows";
  EXPECT_EQ(observed_frames, &observed_pose->body_frames)
      << "frames sidecar must alias pose->body_frames";
}

TEST(PhaseEEquipment, DispatchContextPosePopulatedOnHorseRows) {
  const Render::GL::HumanoidPose *observed_pose = nullptr;

  EquipmentRecord rec{};
  rec.dispatch = [&observed_pose](const EquipmentSubmitContext &sub,
                                  Render::GL::EquipmentBatch &) {
    observed_pose = sub.pose;
  };

  std::array<EquipmentRecord, 1> records{rec};
  std::array<UnitVisualSpec, 1> specs{};
  specs[0].kind = CreatureKind::Horse;
  specs[0].equipment =
      std::span<const EquipmentRecord>{records.data(), records.size()};

  Render::GL::DrawContext ctx{};
  Render::Horse::HorseSpecPose hpose{};
  Render::GL::HorseVariant hvariant{};

  CreatureFrame frame;
  frame.push_horse(/*id*/ 0, QMatrix4x4{}, /*spec*/ 0, /*seed*/ 1,
                   CreatureLOD::Billboard, hpose, hvariant);
  frame.legacy_ctx.back() = &ctx;

  CreaturePipeline pipeline;
  FrameContext fctx;
  NullSubmitter sink;
  pipeline.submit(fctx, specs, frame, sink);

  ASSERT_NE(observed_pose, nullptr)
      << "EquipmentSubmitContext.pose must be non-null on horse rows too "
         "(blank pose) so closures need not branch";
}

TEST(PhaseEEquipment, SocketBoundEquipmentUsesResolvedSkeletonSocket) {
  std::array<EquipmentRecord, 1> no_socket{};
  no_socket[0].static_mesh = fake_mesh();
  no_socket[0].local_offset.setToIdentity();
  no_socket[0].local_offset.translate(0.0F, 0.5F, 0.0F);

  std::array<EquipmentRecord, 1> with_socket = no_socket;
  with_socket[0].socket =
      static_cast<Render::Creature::SocketIndex>(Render::Humanoid::HumanoidSocket::HandR);

  std::array<UnitVisualSpec, 2> specs{};
  specs[0].kind = CreatureKind::Humanoid;
  specs[0].equipment = no_socket;
  specs[1].kind = CreatureKind::Humanoid;
  specs[1].equipment = with_socket;

  Render::GL::DrawContext ctx{};
  Render::GL::HumanoidPose pose{};
  Render::GL::HumanoidVariant variant{};
  Render::GL::HumanoidAnimationContext anim{};

  QMatrix4x4 world;
  world.setToIdentity();
  world.translate(10.0F, 0.0F, 0.0F);

  CreatureFrame frame;
  frame.push_humanoid(/*id*/ 0, world, /*spec*/ 0, /*seed*/ 1,
                      CreatureLOD::Billboard, pose, variant, anim);
  frame.push_humanoid(/*id*/ 1, world, /*spec*/ 1, /*seed*/ 1,
                      CreatureLOD::Billboard, pose, variant, anim);
  frame.legacy_ctx[0] = &ctx;
  frame.legacy_ctx[1] = &ctx;

  CreaturePipeline pipeline;
  FrameContext fctx;
  ModelRecordingSubmitter sink;
  const auto stats = pipeline.submit(fctx, specs, frame, sink);

  EXPECT_EQ(stats.equipment_submitted, 2u);
  ASSERT_EQ(sink.models.size(), 2u);
  const QVector3D a = sink.models[0].column(3).toVector3D();
  const QVector3D b = sink.models[1].column(3).toVector3D();
  EXPECT_NEAR(a.x(), 10.0F, 1e-4F);
  EXPECT_NEAR(a.y(), 0.5F, 1e-4F);
  EXPECT_NEAR(a.z(), 0.0F, 1e-4F);
  EXPECT_NE(a, b);
}

} // namespace
