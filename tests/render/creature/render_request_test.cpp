// Verifies the high-level CreatureRenderRequest interface — the new
// game→render contract that carries no anatomy, sockets, or pose data.
//
// These tests exercise:
//   - ArchetypeRegistry's baseline (humanoid/horse/elephant) bindings.
//   - CreaturePipeline::submit_requests() routing logic: every request
//     bumps entities_submitted, billboard LODs and unknown archetypes
//     never produce rigged draw calls.

#include "render/creature/archetype_registry.h"
#include "render/creature/pipeline/creature_pipeline.h"
#include "render/creature/render_request.h"
#include "render/submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <array>
#include <gtest/gtest.h>
#include <vector>

namespace {

using Render::Creature::AnimationStateId;
using Render::Creature::ArchetypeDescriptor;
using Render::Creature::ArchetypeRegistry;
using Render::Creature::CreatureRenderRequest;
using Render::Creature::Pipeline::CreatureKind;
using Render::Creature::Pipeline::CreaturePipeline;

class CountingSubmitter : public Render::GL::ISubmitter {
public:
  std::uint32_t mesh_calls{0};
  std::uint32_t rigged_calls{0};

  void mesh(Render::GL::Mesh *, const QMatrix4x4 &, const QVector3D &,
            Render::GL::Texture *, float, int) override {
    ++mesh_calls;
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

TEST(ArchetypeRegistryBaseline, IdleAndDeadAreSnapshotStates) {
  const auto &reg = ArchetypeRegistry::instance();
  EXPECT_TRUE(reg.is_snapshot(ArchetypeRegistry::kHumanoidBase,
                              AnimationStateId::Idle));
  EXPECT_TRUE(reg.is_snapshot(ArchetypeRegistry::kHumanoidBase,
                              AnimationStateId::Dead));
  EXPECT_FALSE(reg.is_snapshot(ArchetypeRegistry::kHumanoidBase,
                               AnimationStateId::Walk));
  EXPECT_FALSE(
      reg.is_snapshot(ArchetypeRegistry::kHumanoidBase, AnimationStateId::Run));
}

TEST(ArchetypeRegistryBaseline, UnknownArchetypeReturnsUnmappedClip) {
  const auto &reg = ArchetypeRegistry::instance();
  EXPECT_EQ(reg.bpat_clip(/*invalid id*/ 9999, AnimationStateId::Idle),
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
  reqs[1].lod = Render::Creature::CreatureLOD::Reduced;
  reqs[2].archetype = ArchetypeRegistry::kElephantBase;
  reqs[2].state = AnimationStateId::Idle;
  reqs[2].lod = Render::Creature::CreatureLOD::Minimal;
  reqs[3].archetype = ArchetypeRegistry::kHumanoidBase;
  reqs[3].state = AnimationStateId::Hold;
  reqs[3].lod = Render::Creature::CreatureLOD::Billboard;

  const auto stats = pipeline.submit_requests(reqs, sink);
  EXPECT_EQ(stats.entities_submitted, 4U);
  EXPECT_EQ(stats.lod_full, 1U);
  EXPECT_EQ(stats.lod_reduced, 1U);
  EXPECT_EQ(stats.lod_minimal, 1U);
  EXPECT_EQ(stats.lod_billboard, 1U);
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
