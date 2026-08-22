

#include <gtest/gtest.h>
#include <type_traits>

#include "render/creature/runtime_bake_guard.h"
#include "render/creature/spec.h"
#include "render/elephant/elephant_spec.h"
#include "render/horse/horse_spec.h"
#include "render/humanoid/asset/humanoid_spec.h"
#include "render/rigged_mesh.h"
#include "render/rigged_mesh_cache.h"

namespace {

using Render::Creature::CreatureLOD;
using Render::GL::RiggedMeshCache;
using Render::GL::RiggedVertex;

class RuntimeBakeGuardReset {
public:
  RuntimeBakeGuardReset() { Render::Creature::set_runtime_bake_forbidden(false); }
  ~RuntimeBakeGuardReset() { Render::Creature::set_runtime_bake_forbidden(false); }
};

TEST(RiggedMeshCache, RepeatedCallsForSameKeyReturnSameEntry) {
  RiggedMeshCache cache;
  auto const& spec = Render::Humanoid::humanoid_creature_spec();
  auto const bind = Render::Humanoid::humanoid_bind_palette();

  EXPECT_EQ(cache.size(), 0U);

  const auto* first = cache.get_or_bake(spec, CreatureLOD::Full, bind);
  ASSERT_NE(first, nullptr);
  EXPECT_EQ(cache.size(), 1U);

  const auto* second = cache.get_or_bake(spec, CreatureLOD::Full, bind);
  EXPECT_EQ(first, second) << "second get_or_bake must hit the cache";
  EXPECT_EQ(first->mesh.get(), second->mesh.get())
      << "the underlying mesh must not be re-baked";
  EXPECT_EQ(cache.size(), 1U) << "cache must not grow when re-asked for the same key";

  for (int i = 0; i < 64; ++i) {
    EXPECT_EQ(cache.get_or_bake(spec, CreatureLOD::Full, bind), first);
  }
  EXPECT_EQ(cache.size(), 1U);
}

TEST(RiggedMeshCache, PerUnitVariantBucketDefaultsToZeroAndDeduplicates) {

  RiggedMeshCache cache;
  auto const& spec = Render::Humanoid::humanoid_creature_spec();
  auto const bind = Render::Humanoid::humanoid_bind_palette();

  const auto* via_default = cache.get_or_bake(spec, CreatureLOD::Full, bind);
  const auto* via_explicit_zero = cache.get_or_bake(spec, CreatureLOD::Full, bind, 0);
  const auto* via_visual_variant = cache.get_or_bake(spec, CreatureLOD::Full, bind, 7);
  EXPECT_EQ(via_default, via_explicit_zero);
  EXPECT_EQ(via_default, via_visual_variant)
      << "draw-time visual variants do not change baked geometry";
  EXPECT_EQ(cache.size(), 1U);
}

TEST(RiggedMeshCache, FullAndMinimalBakeIndependentlyButOnlyOnceEach) {
  RiggedMeshCache cache;
  auto const& spec = Render::Humanoid::humanoid_creature_spec();
  auto const bind = Render::Humanoid::humanoid_bind_palette();

  const auto* full = cache.get_or_bake(spec, CreatureLOD::Full, bind);
  const auto* minimal = cache.get_or_bake(spec, CreatureLOD::Minimal, bind);

  ASSERT_NE(full, nullptr);
  ASSERT_NE(minimal, nullptr);
  EXPECT_NE(full, minimal);
  EXPECT_EQ(cache.size(), 2U);

  EXPECT_EQ(cache.get_or_bake(spec, CreatureLOD::Full, bind), full);
  EXPECT_EQ(cache.get_or_bake(spec, CreatureLOD::Minimal, bind), minimal);
  EXPECT_EQ(cache.size(), 2U);
}

TEST(RiggedMeshCache, MeshVariantsShareOneSpeciesSkinAtlas) {
  RiggedMeshCache cache;
  auto const& spec = Render::Humanoid::humanoid_creature_spec();
  auto const bind = Render::Humanoid::humanoid_bind_palette();
  constexpr std::uint32_t k_species = 17U;

  const auto* first = cache.get_or_bake_prehashed(
      spec, CreatureLOD::Full, bind, 0U, {}, 0x12U, 1U, k_species);
  const auto* second = cache.get_or_bake_prehashed(
      spec, CreatureLOD::Full, bind, 7U, {}, 0x34U, 2U, k_species);
  const auto* minimal = cache.get_or_bake_prehashed(
      spec, CreatureLOD::Minimal, bind, 0U, {}, 0x56U, 3U, k_species);

  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  ASSERT_NE(minimal, nullptr);
  ASSERT_NE(first->skin_atlas, nullptr);
  EXPECT_EQ(first->skin_atlas, second->skin_atlas);
  EXPECT_EQ(first->skin_atlas, minimal->skin_atlas);
  EXPECT_EQ(first->mesh.get(), second->mesh.get());
  EXPECT_NE(first->mesh.get(), minimal->mesh.get());
}

TEST(RiggedMeshCache, DifferentAnimationSpeciesDoNotShareSkinAtlases) {
  RiggedMeshCache cache;
  auto const& spec = Render::Humanoid::humanoid_creature_spec();
  auto const bind = Render::Humanoid::humanoid_bind_palette();

  const auto* first = cache.get_or_bake_prehashed(
      spec, CreatureLOD::Full, bind, 0U, {}, 0x12U, 1U, 17U);
  const auto* second = cache.get_or_bake_prehashed(
      spec, CreatureLOD::Full, bind, 0U, {}, 0x12U, 1U, 18U);

  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  ASSERT_NE(first->skin_atlas, nullptr);
  ASSERT_NE(second->skin_atlas, nullptr);
  EXPECT_NE(first->skin_atlas, second->skin_atlas);
}

TEST(RiggedMeshCache, DifferentSpeciesProduceDistinctEntries) {
  RiggedMeshCache cache;

  auto const& humanoid = Render::Humanoid::humanoid_creature_spec();
  auto const& horse = Render::Horse::horse_creature_spec();
  auto const& elephant = Render::Elephant::elephant_creature_spec();

  const auto* h = cache.get_or_bake(
      humanoid, CreatureLOD::Full, Render::Humanoid::humanoid_bind_palette());
  const auto* r =
      cache.get_or_bake(horse, CreatureLOD::Full, Render::Horse::horse_bind_palette());
  const auto* e = cache.get_or_bake(
      elephant, CreatureLOD::Full, Render::Elephant::elephant_bind_palette());

  ASSERT_NE(h, nullptr);
  ASSERT_NE(r, nullptr);
  ASSERT_NE(e, nullptr);
  EXPECT_NE(h, r);
  EXPECT_NE(r, e);
  EXPECT_NE(h, e);
  EXPECT_EQ(cache.size(), 3U);

  for (int i = 0; i < 32; ++i) {
    cache.get_or_bake(
        humanoid, CreatureLOD::Full, Render::Humanoid::humanoid_bind_palette());
    cache.get_or_bake(horse, CreatureLOD::Full, Render::Horse::horse_bind_palette());
    cache.get_or_bake(
        elephant, CreatureLOD::Full, Render::Elephant::elephant_bind_palette());
  }
  EXPECT_EQ(cache.size(), 3U)
      << "spawning many units of each species must not re-bake meshes";
}

TEST(RiggedMeshCache, PartitionsExactRigidAndBlendedTriangleRanges) {
  RiggedMeshCache cache;
  const auto* humanoid = cache.get_or_bake(Render::Humanoid::humanoid_creature_spec(),
                                           CreatureLOD::Full,
                                           Render::Humanoid::humanoid_bind_palette());
  const auto* horse = cache.get_or_bake(Render::Horse::horse_creature_spec(),
                                        CreatureLOD::Full,
                                        Render::Horse::horse_bind_palette());

  ASSERT_NE(humanoid, nullptr);
  ASSERT_NE(humanoid->mesh, nullptr);
  ASSERT_NE(horse, nullptr);
  ASSERT_NE(horse->mesh, nullptr);
  EXPECT_GT(humanoid->mesh->rigid_index_count(), 0U);
  EXPECT_LT(humanoid->mesh->rigid_index_count(), humanoid->mesh->index_count());
  EXPECT_EQ(humanoid->mesh->rigid_index_count() % 3U, 0U);
  EXPECT_FALSE(humanoid->mesh->rigid_skinning());
  for (std::size_t i = 0; i < humanoid->mesh->index_count(); ++i) {
    const auto vertex_index = humanoid->mesh->get_indices()[i];
    ASSERT_LT(vertex_index, humanoid->mesh->get_vertices().size());
    const auto& weights = humanoid->mesh->get_vertices()[vertex_index].bone_weights;
    const bool rigid = weights == std::array<float, 4>{1.0F, 0.0F, 0.0F, 0.0F};
    if (i < humanoid->mesh->rigid_index_count()) {
      EXPECT_TRUE(rigid);
    }
  }
  EXPECT_FALSE(horse->mesh->rigid_skinning());
}

TEST(RiggedMeshCache, BakedVertexFormatCarriesRoleIndexButNoPerUnitColour) {

  static_assert(sizeof(RiggedVertex) ==
                    sizeof(float) * (3 + 3 + 2 + 4) + sizeof(std::uint8_t) * 8,
                "RiggedVertex may contain stable authoring metadata like a "
                "role index, but per-unit colour still belongs in draw-time "
                "uniforms, never in baked vertices.");

  RiggedVertex v{};
  static_assert(std::is_same_v<decltype(v.position_bone_local), std::array<float, 3>>);
  static_assert(std::is_same_v<decltype(v.normal_bone_local), std::array<float, 3>>);
  static_assert(std::is_same_v<decltype(v.tex_coord), std::array<float, 2>>);
  static_assert(std::is_same_v<decltype(v.bone_weights), std::array<float, 4>>);
  static_assert(std::is_same_v<decltype(v.bone_indices), std::array<std::uint8_t, 4>>);
  static_assert(std::is_same_v<decltype(v.color_role), std::uint8_t>);
  SUCCEED();
}

TEST(RiggedMeshCache, RuntimeBakeGuardAllowsHitsButRejectsMisses) {
  RuntimeBakeGuardReset guard_reset;
  RiggedMeshCache cache;
  auto const& spec = Render::Humanoid::humanoid_creature_spec();
  auto const bind = Render::Humanoid::humanoid_bind_palette();

  const auto* full = cache.get_or_bake(spec, CreatureLOD::Full, bind);
  ASSERT_NE(full, nullptr);
  EXPECT_EQ(cache.size(), 1U);

  Render::Creature::set_runtime_bake_forbidden(true);

  EXPECT_EQ(cache.get_or_bake(spec, CreatureLOD::Full, bind), full)
      << "warmed rigged mesh hits must remain usable during gameplay";
  EXPECT_EQ(cache.get_or_bake(spec, CreatureLOD::Minimal, bind, 0U, {}, 999U), nullptr)
      << "warmed gameplay must not bake a missing rigged mesh";
  EXPECT_EQ(cache.size(), 1U);
}

TEST(RiggedMeshCache, FrameStatsCountBakesAndHits) {
  RuntimeBakeGuardReset guard_reset;
  RiggedMeshCache cache;
  auto const& spec = Render::Humanoid::humanoid_creature_spec();
  auto const bind = Render::Humanoid::humanoid_bind_palette();

  ASSERT_NE(cache.get_or_bake(spec, CreatureLOD::Full, bind), nullptr);
  EXPECT_EQ(cache.frame_stats().bakes, 1U);
  EXPECT_EQ(cache.frame_stats().misses, 0U);
  EXPECT_EQ(cache.frame_stats().hits, 0U);

  ASSERT_NE(cache.get_or_bake(spec, CreatureLOD::Full, bind), nullptr);
  EXPECT_EQ(cache.frame_stats().hits, 1U);
  EXPECT_EQ(cache.frame_stats().bakes, 1U);
  EXPECT_EQ(cache.frame_stats().misses, 0U);
}

TEST(RiggedMeshCache, FrameStatsResetClearsCounters) {
  RuntimeBakeGuardReset guard_reset;
  RiggedMeshCache cache;
  auto const& spec = Render::Humanoid::humanoid_creature_spec();
  auto const bind = Render::Humanoid::humanoid_bind_palette();

  cache.get_or_bake(spec, CreatureLOD::Full, bind);
  EXPECT_GT(cache.frame_stats().bakes, 0U);

  cache.reset_frame_stats();
  EXPECT_EQ(cache.frame_stats().hits, 0U);
  EXPECT_EQ(cache.frame_stats().misses, 0U);
  EXPECT_EQ(cache.frame_stats().bakes, 0U);
  EXPECT_EQ(cache.frame_stats().skin_atlas_builds, 0U);
  EXPECT_EQ(cache.frame_stats().skin_ubo_uploads, 0U);
  EXPECT_EQ(cache.frame_stats().skin_ubo_bytes_uploaded, 0U);
}

TEST(RiggedMeshCache, FrameStatsTrackSkinUploadCounters) {
  RiggedMeshCache cache;

  cache.record_skin_atlas_build();
  cache.record_skin_ubo_upload(4096U);
  cache.record_skin_ubo_upload(128U);

  EXPECT_EQ(cache.frame_stats().skin_atlas_builds, 1U);
  EXPECT_EQ(cache.frame_stats().skin_ubo_uploads, 2U);
  EXPECT_EQ(cache.frame_stats().skin_ubo_bytes_uploaded, 4224U);

  cache.reset_frame_stats();
  EXPECT_EQ(cache.frame_stats().skin_atlas_builds, 0U);
  EXPECT_EQ(cache.frame_stats().skin_ubo_uploads, 0U);
  EXPECT_EQ(cache.frame_stats().skin_ubo_bytes_uploaded, 0U);
}

TEST(RiggedMeshCache, DeferredSkinUploadRemainsQueuedWithoutRenderContext) {
  RuntimeBakeGuardReset guard_reset;
  RiggedMeshCache cache;

  cache.mark_skin_ubo_upload_pending();
  ASSERT_TRUE(cache.has_pending_skin_ubo_uploads());

  Render::Creature::set_runtime_bake_forbidden(true);
  cache.upload_pending_skin_ubos();

  EXPECT_TRUE(cache.has_pending_skin_ubo_uploads());
  EXPECT_TRUE(Render::Creature::runtime_bake_forbidden())
      << "a failed initialization attempt must preserve the gameplay guard";
}
TEST(RiggedMeshCache, FrameStatsMissOnRuntimeBakeRejection) {
  RuntimeBakeGuardReset guard_reset;
  RiggedMeshCache cache;
  auto const& spec = Render::Humanoid::humanoid_creature_spec();
  auto const bind = Render::Humanoid::humanoid_bind_palette();

  Render::Creature::set_runtime_bake_forbidden(true);
  EXPECT_EQ(cache.get_or_bake(spec, CreatureLOD::Full, bind, 0U, {}, 999U), nullptr);
  EXPECT_EQ(cache.frame_stats().misses, 1U);
  EXPECT_EQ(cache.frame_stats().bakes, 0U);
  EXPECT_EQ(cache.frame_stats().hits, 0U);
}

TEST(RiggedMeshCache, PrehashedLookupHonorsAttachmentSetId) {
  RiggedMeshCache cache;
  auto const& spec = Render::Humanoid::humanoid_creature_spec();
  auto const bind = Render::Humanoid::humanoid_bind_palette();

  const auto* first =
      cache.get_or_bake_prehashed(spec, CreatureLOD::Full, bind, 0U, {}, 0x12U, 1U, 0U);
  ASSERT_NE(first, nullptr);

  const auto* hit =
      cache.get_or_bake_prehashed(spec, CreatureLOD::Full, bind, 0U, {}, 0x12U, 1U, 0U);
  EXPECT_EQ(hit, first);

  const auto* different_set =
      cache.get_or_bake_prehashed(spec, CreatureLOD::Full, bind, 0U, {}, 0x12U, 2U, 0U);
  ASSERT_NE(different_set, nullptr);
  EXPECT_NE(different_set, first);
  EXPECT_EQ(cache.size(), 2U);
}

TEST(RiggedMeshCache, PrehashedLookupFallsBackToHashWhenSetIdIsInvalid) {
  RiggedMeshCache cache;
  auto const& spec = Render::Humanoid::humanoid_creature_spec();
  auto const bind = Render::Humanoid::humanoid_bind_palette();

  const auto* first =
      cache.get_or_bake_prehashed(spec, CreatureLOD::Full, bind, 0U, {}, 0xAAU, 0U, 0U);
  const auto* second =
      cache.get_or_bake_prehashed(spec, CreatureLOD::Full, bind, 0U, {}, 0xBBU, 0U, 0U);

  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  EXPECT_NE(first, second);
  EXPECT_EQ(cache.size(), 2U);
}

} // namespace
