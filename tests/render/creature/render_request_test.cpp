

#include "render/creature/archetype_registry.h"
#include "render/creature/bpat/bpat_format.h"
#include "render/creature/bpat/bpat_registry.h"
#include "render/creature/pipeline/creature_asset.h"
#include "render/creature/pipeline/creature_pipeline.h"
#include "render/creature/pipeline/prepared_submit.h"
#include "render/creature/render_request.h"
#include "render/elephant/elephant_spec.h"
#include "render/horse/horse_spec.h"
#include "render/humanoid/humanoid_spec.h"
#include "render/humanoid/skeleton.h"
#include "render/submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <gtest/gtest.h>
#include <vector>

namespace {

using Render::Creature::AnimationStateId;
using Render::Creature::ArchetypeDescriptor;
using Render::Creature::ArchetypeRegistry;
using Render::Creature::CreatureRenderRequest;
using Render::Creature::Pipeline::CreatureKind;
using Render::Creature::Pipeline::CreaturePipeline;
using namespace Render::Creature::Bpat;

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

auto palette_contact_y(CreatureKind kind,
                       std::span<const QMatrix4x4> palette) -> float {
  auto const bind_adjusted_y = [&](std::size_t index,
                                   std::span<const QMatrix4x4> bind_palette) {
    return (palette[index] * bind_palette[index].inverted()).column(3).y();
  };
  switch (kind) {
  case CreatureKind::Humanoid:
    return std::min(bind_adjusted_y(static_cast<std::size_t>(
                                        Render::Humanoid::HumanoidBone::FootL),
                                    Render::Humanoid::humanoid_bind_palette()),
                    bind_adjusted_y(static_cast<std::size_t>(
                                        Render::Humanoid::HumanoidBone::FootR),
                                    Render::Humanoid::humanoid_bind_palette()));
  case CreatureKind::Horse:
    return std::min(
        std::min(bind_adjusted_y(
                     static_cast<std::size_t>(Render::Horse::HorseBone::FootFL),
                     Render::Horse::horse_bind_palette()),
                 bind_adjusted_y(
                     static_cast<std::size_t>(Render::Horse::HorseBone::FootFR),
                     Render::Horse::horse_bind_palette())),
        std::min(bind_adjusted_y(
                     static_cast<std::size_t>(Render::Horse::HorseBone::FootBL),
                     Render::Horse::horse_bind_palette()),
                 bind_adjusted_y(
                     static_cast<std::size_t>(Render::Horse::HorseBone::FootBR),
                     Render::Horse::horse_bind_palette())));
  case CreatureKind::Elephant:
    return std::min(
        std::min(bind_adjusted_y(static_cast<std::size_t>(
                                     Render::Elephant::ElephantBone::FootFL),
                                 Render::Elephant::elephant_bind_palette()),
                 bind_adjusted_y(static_cast<std::size_t>(
                                     Render::Elephant::ElephantBone::FootFR),
                                 Render::Elephant::elephant_bind_palette())),
        std::min(bind_adjusted_y(static_cast<std::size_t>(
                                     Render::Elephant::ElephantBone::FootBL),
                                 Render::Elephant::elephant_bind_palette()),
                 bind_adjusted_y(static_cast<std::size_t>(
                                     Render::Elephant::ElephantBone::FootBR),
                                 Render::Elephant::elephant_bind_palette())));
  case CreatureKind::Mounted:
    return 0.0F;
  }
  return 0.0F;
}

class CountingSubmitter : public Render::GL::ISubmitter {
public:
  std::uint32_t mesh_calls{0};
  std::uint32_t rigged_calls{0};
  std::vector<float> rigged_world_y;
  std::vector<float> rigged_world_z;
  std::vector<QMatrix4x4> last_bone_palette;

  void mesh(Render::GL::Mesh *, const QMatrix4x4 &, const QVector3D &,
            Render::GL::Texture *, float, int) override {
    ++mesh_calls;
  }
  void rigged(const Render::GL::RiggedCreatureCmd &cmd) override {
    ++rigged_calls;
    rigged_world_y.push_back(cmd.world.column(3).y());
    rigged_world_z.push_back(cmd.world.column(3).z());
    last_bone_palette.assign(cmd.bone_palette,
                             cmd.bone_palette + cmd.bone_count);
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

TEST(ArchetypeRegistryBaseline, HasThreeSpeciesArchetypes) {
  const auto &reg = ArchetypeRegistry::instance();
  ASSERT_GE(reg.size(), 3U);

  EXPECT_EQ(reg.species(ArchetypeRegistry::kHumanoidBase),
            CreatureKind::Humanoid);
  EXPECT_EQ(reg.species(ArchetypeRegistry::kHorseBase), CreatureKind::Horse);
  EXPECT_EQ(reg.species(ArchetypeRegistry::kElephantBase),
            CreatureKind::Elephant);
}

TEST(ArchetypeRegistryBaseline, IdleClipIsZeroForEverySpecies) {
  const auto &reg = ArchetypeRegistry::instance();
  EXPECT_EQ(
      reg.bpat_clip(ArchetypeRegistry::kHumanoidBase, AnimationStateId::Idle),
      0U);
  EXPECT_EQ(
      reg.bpat_clip(ArchetypeRegistry::kHorseBase, AnimationStateId::Idle), 0U);
  EXPECT_EQ(
      reg.bpat_clip(ArchetypeRegistry::kElephantBase, AnimationStateId::Idle),
      0U);
}

TEST(ArchetypeRegistryBaseline, GameplayStatesUseSnapshotCoverage) {
  const auto &reg = ArchetypeRegistry::instance();
  EXPECT_TRUE(reg.is_snapshot(ArchetypeRegistry::kHumanoidBase,
                              AnimationStateId::Idle));
  EXPECT_TRUE(reg.is_snapshot(ArchetypeRegistry::kHumanoidBase,
                              AnimationStateId::Walk));
  EXPECT_TRUE(
      reg.is_snapshot(ArchetypeRegistry::kHumanoidBase, AnimationStateId::Run));
  EXPECT_TRUE(reg.is_snapshot(ArchetypeRegistry::kHumanoidBase,
                              AnimationStateId::Hold));
  EXPECT_TRUE(reg.is_snapshot(ArchetypeRegistry::kHumanoidBase,
                              AnimationStateId::AttackSword));
  EXPECT_TRUE(reg.is_snapshot(ArchetypeRegistry::kHumanoidBase,
                              AnimationStateId::AttackBow));
  EXPECT_TRUE(reg.is_snapshot(ArchetypeRegistry::kHumanoidBase,
                              AnimationStateId::Dead));
  EXPECT_FALSE(
      reg.is_snapshot(ArchetypeRegistry::kHumanoidBase, AnimationStateId::Die));
  EXPECT_FALSE(reg.is_snapshot(ArchetypeRegistry::kHumanoidBase,
                               AnimationStateId::RidingIdle));
  EXPECT_TRUE(
      reg.is_snapshot(ArchetypeRegistry::kHorseBase, AnimationStateId::Run));
  EXPECT_TRUE(reg.is_snapshot(ArchetypeRegistry::kElephantBase,
                              AnimationStateId::Walk));
  EXPECT_TRUE(reg.is_snapshot(ArchetypeRegistry::kRiderBase,
                              AnimationStateId::RidingIdle));
  EXPECT_TRUE(reg.is_snapshot(ArchetypeRegistry::kRiderBase,
                              AnimationStateId::RidingCharge));
}

TEST(ArchetypeRegistryBaseline, UnknownArchetypeReturnsUnmappedClip) {
  const auto &reg = ArchetypeRegistry::instance();
  EXPECT_EQ(reg.bpat_clip(9999, AnimationStateId::Idle),
            ArchetypeDescriptor::kUnmappedClip);
  EXPECT_EQ(reg.get(9999), nullptr);
}

TEST(SubmitRequests, EmptySpanProducesZeroStats) {
  CreaturePipeline pipeline;
  CountingSubmitter sink;
  const auto stats = pipeline.submit_requests({}, sink);
  EXPECT_EQ(stats.entities_submitted, 0U);
  EXPECT_EQ(sink.rigged_calls, 0U);
  EXPECT_EQ(sink.mesh_calls, 0U);
}

TEST(SubmitPreparation, RequestOnlyPreparationStillDrawsBodies) {
  auto const root = find_assets_dir();
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found";
  }

  auto &reg = BpatRegistry::instance();
  ASSERT_TRUE(reg.load_species(kSpeciesHumanoid, root + "/humanoid.bpat"));

  Render::Creature::Pipeline::CreaturePreparationResult prep;
  CreatureRenderRequest req{};
  req.archetype = ArchetypeRegistry::kHumanoidBase;
  req.state = AnimationStateId::Idle;
  req.lod = Render::Creature::CreatureLOD::Full;
  prep.bodies.add_request(req);

  CountingSubmitter sink;
  Render::Creature::Pipeline::submit_preparation(prep, sink);

  EXPECT_EQ(sink.rigged_calls, 1U);
  EXPECT_EQ(sink.mesh_calls, 0U);
}

TEST(SubmitPreparation, ShadowRequestsAreSkipped) {
  auto const root = find_assets_dir();
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found";
  }

  auto &reg = BpatRegistry::instance();
  ASSERT_TRUE(reg.load_species(kSpeciesHumanoid, root + "/humanoid.bpat"));

  Render::Creature::Pipeline::CreaturePreparationResult prep;
  CreatureRenderRequest req{};
  req.archetype = ArchetypeRegistry::kHumanoidBase;
  req.state = AnimationStateId::Idle;
  req.lod = Render::Creature::CreatureLOD::Full;
  req.pass = Render::Creature::Pipeline::RenderPassIntent::Shadow;
  prep.bodies.add_request(req);

  CountingSubmitter sink;
  Render::Creature::Pipeline::submit_preparation(prep, sink);

  EXPECT_EQ(sink.rigged_calls, 0U);
  EXPECT_EQ(sink.mesh_calls, 0U);
}

TEST(SubmitPreparation, ExplicitMainRequestDoesNotDependOnShadowRows) {
  auto const root = find_assets_dir();
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found";
  }

  auto &reg = BpatRegistry::instance();
  ASSERT_TRUE(reg.load_species(kSpeciesHumanoid, root + "/humanoid.bpat"));

  Render::Creature::Pipeline::CreaturePreparationResult prep;
  Render::Creature::Pipeline::CreatureGraphOutput output{};
  output.spec.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
  output.lod = Render::Creature::CreatureLOD::Full;
  output.pass_intent = Render::Creature::Pipeline::RenderPassIntent::Shadow;

  Render::GL::HumanoidPose pose{};
  Render::GL::HumanoidVariant variant{};
  Render::GL::HumanoidAnimationContext anim{};
  prep.bodies.add_humanoid(output, pose, variant, anim);

  CreatureRenderRequest req{};
  req.archetype = ArchetypeRegistry::kHumanoidBase;
  req.state = AnimationStateId::Idle;
  req.lod = Render::Creature::CreatureLOD::Full;
  req.pass = Render::Creature::Pipeline::RenderPassIntent::Main;
  prep.bodies.add_request(req);

  CountingSubmitter sink;
  Render::Creature::Pipeline::submit_preparation(prep, sink);

  EXPECT_EQ(sink.rigged_calls, 1U);
  EXPECT_EQ(sink.mesh_calls, 0U);
}

TEST(SubmitRequests, BillboardLodCountsButDoesNotDraw) {
  CreaturePipeline pipeline;
  CountingSubmitter sink;

  CreatureRenderRequest req{};
  req.archetype = ArchetypeRegistry::kHumanoidBase;
  req.state = AnimationStateId::Idle;
  req.lod = Render::Creature::CreatureLOD::Billboard;
  req.world.setToIdentity();

  std::array<CreatureRenderRequest, 1> reqs{req};
  const auto stats = pipeline.submit_requests(reqs, sink);

  EXPECT_EQ(stats.entities_submitted, 1U);
  EXPECT_EQ(stats.lod_billboard, 1U);
  EXPECT_EQ(sink.rigged_calls, 0U);
}

TEST(SubmitRequests, UnknownArchetypeIsSkippedSafely) {
  CreaturePipeline pipeline;
  CountingSubmitter sink;

  CreatureRenderRequest req{};
  req.archetype = 9999;
  req.state = AnimationStateId::Idle;
  req.lod = Render::Creature::CreatureLOD::Full;

  std::array<CreatureRenderRequest, 1> reqs{req};
  const auto stats = pipeline.submit_requests(reqs, sink);

  EXPECT_EQ(stats.entities_submitted, 1U);
  EXPECT_EQ(sink.rigged_calls, 0U);
}

TEST(SubmitRequests, BatchOfMixedRequestsCountsCorrectly) {
  CreaturePipeline pipeline;
  CountingSubmitter sink;

  std::array<CreatureRenderRequest, 4> reqs{};
  reqs[0].archetype = ArchetypeRegistry::kHumanoidBase;
  reqs[0].state = AnimationStateId::Walk;
  reqs[0].lod = Render::Creature::CreatureLOD::Full;
  reqs[1].archetype = ArchetypeRegistry::kHorseBase;
  reqs[1].state = AnimationStateId::Run;
  reqs[1].lod = Render::Creature::CreatureLOD::Minimal;
  reqs[2].archetype = ArchetypeRegistry::kElephantBase;
  reqs[2].state = AnimationStateId::Idle;
  reqs[2].lod = Render::Creature::CreatureLOD::Minimal;
  reqs[3].archetype = ArchetypeRegistry::kHumanoidBase;
  reqs[3].state = AnimationStateId::Hold;
  reqs[3].lod = Render::Creature::CreatureLOD::Billboard;

  const auto stats = pipeline.submit_requests(reqs, sink);
  EXPECT_EQ(stats.entities_submitted, 4U);
  EXPECT_EQ(stats.lod_full, 1U);
  EXPECT_EQ(stats.lod_minimal, 2U);
  EXPECT_EQ(stats.lod_billboard, 1U);
}

TEST(SubmitRequests, AbsoluteWorldKeepsMountedPairsSeparatedInsideOneUnit) {
  CreaturePipeline pipeline;
  CountingSubmitter sink;

  std::array<CreatureRenderRequest, 4> reqs{};

  reqs[0].archetype = ArchetypeRegistry::kHorseBase;
  reqs[0].state = AnimationStateId::Run;
  reqs[0].lod = Render::Creature::CreatureLOD::Full;
  reqs[0].entity_id = 77U;
  reqs[0].seed = 1001U;
  reqs[0].world_already_grounded = true;
  reqs[0].world.translate(0.0F, 1.0F, 0.0F);

  reqs[1].archetype = ArchetypeRegistry::kHorseBase;
  reqs[1].state = AnimationStateId::Run;
  reqs[1].lod = Render::Creature::CreatureLOD::Full;
  reqs[1].entity_id = 77U;
  reqs[1].seed = 2002U;
  reqs[1].world_already_grounded = true;
  reqs[1].world.translate(0.0F, 2.0F, 0.0F);

  reqs[2].archetype = ArchetypeRegistry::kHumanoidBase;
  reqs[2].state = AnimationStateId::Run;
  reqs[2].lod = Render::Creature::CreatureLOD::Full;
  reqs[2].entity_id = 77U;
  reqs[2].seed = 1001U;
  reqs[2].world_already_grounded = true;
  reqs[2].world.translate(0.0F, 1.75F, 0.10F);

  reqs[3].archetype = ArchetypeRegistry::kHumanoidBase;
  reqs[3].state = AnimationStateId::Run;
  reqs[3].lod = Render::Creature::CreatureLOD::Full;
  reqs[3].entity_id = 77U;
  reqs[3].seed = 2002U;
  reqs[3].world_already_grounded = true;
  reqs[3].world.translate(0.0F, 2.75F, 0.25F);

  const auto stats = pipeline.submit_requests(reqs, sink);

  EXPECT_EQ(stats.entities_submitted, 4U);
  EXPECT_EQ(sink.rigged_calls, 4U);
  std::sort(sink.rigged_world_y.begin(), sink.rigged_world_y.end());
  ASSERT_EQ(sink.rigged_world_y.size(), 4u);
  EXPECT_NEAR(sink.rigged_world_y[1] - sink.rigged_world_y[0], 0.75F, 1.0e-4F);
  EXPECT_NEAR(sink.rigged_world_y[3] - sink.rigged_world_y[2], 0.75F, 1.0e-4F);
  EXPECT_NEAR(sink.rigged_world_y[2] - sink.rigged_world_y[0], 1.0F, 1.0e-4F);
  EXPECT_TRUE(
      std::any_of(sink.rigged_world_z.begin(), sink.rigged_world_z.end(),
                  [](float z) { return std::fabs(z - 0.10F) < 1.0e-4F; }));
  EXPECT_TRUE(
      std::any_of(sink.rigged_world_z.begin(), sink.rigged_world_z.end(),
                  [](float z) { return std::fabs(z - 0.25F) < 1.0e-4F; }));
}

TEST(SubmitRequests, RootCreaturesUseClipFootContactForWorldHeight) {
  auto const root = find_assets_dir();
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found";
  }

  auto &reg = BpatRegistry::instance();
  ASSERT_TRUE(reg.load_species(kSpeciesHumanoid, root + "/humanoid.bpat"));
  ASSERT_TRUE(reg.load_species(kSpeciesHorse, root + "/horse.bpat"));
  ASSERT_TRUE(reg.load_species(kSpeciesElephant, root + "/elephant.bpat"));

  auto const *humanoid_blob = reg.blob(kSpeciesHumanoid);
  auto const *horse_blob = reg.blob(kSpeciesHorse);
  auto const *elephant_blob = reg.blob(kSpeciesElephant);
  ASSERT_NE(humanoid_blob, nullptr);
  ASSERT_NE(horse_blob, nullptr);
  ASSERT_NE(elephant_blob, nullptr);

  CreaturePipeline pipeline;
  CountingSubmitter sink;

  std::array<CreatureRenderRequest, 3> reqs{};
  reqs[0].archetype = ArchetypeRegistry::kHumanoidBase;
  reqs[0].state = AnimationStateId::Idle;
  reqs[0].lod = Render::Creature::CreatureLOD::Full;
  reqs[1].archetype = ArchetypeRegistry::kHorseBase;
  reqs[1].state = AnimationStateId::Idle;
  reqs[1].lod = Render::Creature::CreatureLOD::Full;
  reqs[2].archetype = ArchetypeRegistry::kElephantBase;
  reqs[2].state = AnimationStateId::Idle;
  reqs[2].lod = Render::Creature::CreatureLOD::Full;

  const auto stats = pipeline.submit_requests(reqs, sink);

  ASSERT_EQ(stats.entities_submitted, 3U);
  ASSERT_EQ(sink.rigged_calls, 3U);
  ASSERT_EQ(sink.rigged_world_y.size(), 3u);

  float const humanoid_contact = palette_contact_y(
      CreatureKind::Humanoid, humanoid_blob->frame_palette_view(0));
  float const horse_contact =
      palette_contact_y(CreatureKind::Horse, horse_blob->frame_palette_view(0));
  float const elephant_contact = palette_contact_y(
      CreatureKind::Elephant, elephant_blob->frame_palette_view(0));

  EXPECT_NEAR(sink.rigged_world_y[0], -humanoid_contact, 1.0e-4F);
  EXPECT_NEAR(sink.rigged_world_y[1], -horse_contact, 1.0e-4F);
  EXPECT_NEAR(sink.rigged_world_y[2], -elephant_contact, 1.0e-4F);
}

TEST(SubmitRequests, GroundedRootCreaturesPreserveProvidedWorldHeight) {
  auto const root = find_assets_dir();
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found";
  }

  auto &reg = BpatRegistry::instance();
  ASSERT_TRUE(reg.load_species(kSpeciesHumanoid, root + "/humanoid.bpat"));

  CreaturePipeline pipeline;
  CountingSubmitter sink;

  CreatureRenderRequest req{};
  req.archetype = ArchetypeRegistry::kHumanoidBase;
  req.state = AnimationStateId::Idle;
  req.lod = Render::Creature::CreatureLOD::Full;
  req.world.translate(0.0F, 2.75F, 0.0F);
  req.world_already_grounded = true;

  std::array<CreatureRenderRequest, 1> reqs{req};
  const auto stats = pipeline.submit_requests(reqs, sink);

  ASSERT_EQ(stats.entities_submitted, 1U);
  ASSERT_EQ(sink.rigged_calls, 1U);
  ASSERT_EQ(sink.rigged_world_y.size(), 1u);
  EXPECT_NEAR(sink.rigged_world_y[0], 2.75F, 1.0e-4F);
}

TEST(SubmitRequests, ExplicitSwordAssetUsesSwordReadyHumanoidPalette) {
  auto const root = find_assets_dir();
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found";
  }

  auto &reg = BpatRegistry::instance();
  ASSERT_TRUE(reg.load_species(kSpeciesHumanoid, root + "/humanoid.bpat"));
  ASSERT_TRUE(
      reg.load_species(kSpeciesHumanoidSword, root + "/humanoid_sword.bpat"));

  auto make_request = [](Render::Creature::Pipeline::CreatureAssetId asset_id) {
    CreatureRenderRequest req{};
    req.archetype = ArchetypeRegistry::kHumanoidBase;
    req.state = AnimationStateId::Walk;
    req.phase = 0.25F;
    req.lod = Render::Creature::CreatureLOD::Full;
    req.creature_asset_id = asset_id;
    return req;
  };

  CreaturePipeline pipeline;
  CountingSubmitter default_sink;
  std::array<CreatureRenderRequest, 1> default_reqs{
      make_request(Render::Creature::Pipeline::kHumanoidAsset)};
  pipeline.submit_requests(default_reqs, default_sink);

  CountingSubmitter sword_sink;
  std::array<CreatureRenderRequest, 1> sword_reqs{
      make_request(Render::Creature::Pipeline::kHumanoidSwordAsset)};
  pipeline.submit_requests(sword_reqs, sword_sink);

  ASSERT_EQ(default_sink.rigged_calls, 1U);
  ASSERT_EQ(sword_sink.rigged_calls, 1U);
  ASSERT_GT(default_sink.last_bone_palette.size(),
            static_cast<std::size_t>(Render::Humanoid::HumanoidBone::HandR));
  ASSERT_EQ(default_sink.last_bone_palette.size(),
            sword_sink.last_bone_palette.size());

  auto const hand_r_index =
      static_cast<std::size_t>(Render::Humanoid::HumanoidBone::HandR);
  QVector3D const default_hand_r =
      default_sink.last_bone_palette[hand_r_index].column(3).toVector3D();
  QVector3D const sword_hand_r =
      sword_sink.last_bone_palette[hand_r_index].column(3).toVector3D();
  EXPECT_GT(sword_hand_r.z(), default_hand_r.z());
  EXPECT_GT((sword_hand_r - default_hand_r).length(), 0.05F);
}

TEST(ArchetypeRegistry, RegisterArchetypeAssignsStableId) {
  auto &reg = ArchetypeRegistry::instance();
  const auto baseline_size = reg.size();

  ArchetypeDescriptor desc{};
  desc.debug_name = "test_archer";
  desc.species = CreatureKind::Humanoid;
  desc.bpat_clip[static_cast<std::size_t>(AnimationStateId::Idle)] = 0U;
  desc.bpat_clip[static_cast<std::size_t>(AnimationStateId::AttackRanged)] = 3U;
  desc.role_count = 6;

  const auto new_id = reg.register_archetype(desc);
  ASSERT_NE(new_id, Render::Creature::kInvalidArchetype);
  EXPECT_EQ(reg.size(), baseline_size + 1U);

  const auto *fetched = reg.get(new_id);
  ASSERT_NE(fetched, nullptr);
  EXPECT_EQ(fetched->id, new_id);
  EXPECT_EQ(fetched->species, CreatureKind::Humanoid);
  EXPECT_EQ(reg.bpat_clip(new_id, AnimationStateId::AttackRanged), 3U);
}

} // namespace
