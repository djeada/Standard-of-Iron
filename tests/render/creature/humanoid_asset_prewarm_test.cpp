

#include <QMatrix4x4>

#include <gtest/gtest.h>

#include "animation/bpat/bpat_reader.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/assets/creature_asset_prewarmer.h"
#include "render/creature/assets/creature_lod_geometry.h"
#include "render/creature/pipeline/creature_asset.h"
#include "render/creature/pipeline/creature_asset_init.h"
#include "render/creature/runtime_bake_guard.h"
#include "render/elephant/elephant_spec.h"
#include "render/horse/horse_spec.h"
#include "render/humanoid/asset/humanoid_asset_prewarmer.h"
#include "render/rigged_mesh_cache.h"

namespace {

auto humanoid_request(Render::Creature::CreatureLOD lod)
    -> Render::Humanoid::HumanoidAssetRequest {
  return {.archetype = Render::Creature::ArchetypeRegistry::k_humanoid_base,
          .lod = lod,
          .variant = Render::Creature::k_canonical_variant,
          .creature_asset = Render::Creature::Pipeline::k_humanoid_asset};
}

auto resolve_handle(const Render::Humanoid::HumanoidAssetRequest& request)
    -> const Render::Creature::Pipeline::CreatureRenderAssetHandle* {
  auto& registry =
      Render::Creature::Pipeline::CreatureRenderAssetHandleRegistry::instance();
  const auto id = registry.get_or_create(request.creature_asset, request.archetype);
  return registry.get(id);
}

} // namespace

TEST(HumanoidAssetPrewarm, PrewarmingNeedsNoEntityAndNoDrawContext) {
  Render::GL::RiggedMeshCache cache;
  EXPECT_EQ(cache.size(), 0U);

  Render::Humanoid::HumanoidAssetPrewarmer prewarmer(cache, false);
  EXPECT_TRUE(prewarmer.prewarm(humanoid_request(Render::Creature::CreatureLOD::Full)));
  EXPECT_EQ(cache.size(), 1U);
}

TEST(HumanoidAssetPrewarm, PrewarmingIsIdempotent) {
  Render::GL::RiggedMeshCache cache;
  Render::Humanoid::HumanoidAssetPrewarmer prewarmer(cache, false);
  const auto request = humanoid_request(Render::Creature::CreatureLOD::Full);

  EXPECT_TRUE(prewarmer.prewarm(request));
  EXPECT_TRUE(prewarmer.prewarm(request));
  EXPECT_EQ(cache.size(), 1U) << "a second prewarm must not build a second asset";
}

TEST(HumanoidAssetPrewarm, EachLodIsItsOwnAsset) {
  Render::GL::RiggedMeshCache cache;
  Render::Humanoid::HumanoidAssetPrewarmer prewarmer(cache, false);

  EXPECT_TRUE(prewarmer.prewarm(humanoid_request(Render::Creature::CreatureLOD::Full)));
  EXPECT_TRUE(
      prewarmer.prewarm(humanoid_request(Render::Creature::CreatureLOD::Minimal)));
  EXPECT_EQ(cache.size(), 2U);
}

TEST(HumanoidAssetPrewarm, PrewarmedAssetSatisfiesTheStrictRuntimeLookup) {
  Render::GL::RiggedMeshCache cache;
  Render::Humanoid::HumanoidAssetPrewarmer prewarmer(cache, false);
  const auto request = humanoid_request(Render::Creature::CreatureLOD::Full);

  const auto* handle = resolve_handle(request);
  ASSERT_NE(handle, nullptr);
  ASSERT_TRUE(handle->valid());

  ASSERT_TRUE(prewarmer.prewarm(request));

  bool found_any = false;
  for (const auto& playback : handle->playback) {
    if (playback.blob == nullptr || playback.frame_count == 0U) {
      continue;
    }
    const auto key = Render::Creature::Pipeline::rigged_asset_key(
        *handle, request.lod, playback.blob->species_id());
    EXPECT_NE(cache.find_rigged_asset(key), nullptr)
        << "the prewarmed asset is not the one submission would look up";
    found_any = true;
    break;
  }
  EXPECT_TRUE(found_any) << "the humanoid archetype has no baked clips to prewarm";
}

TEST(HumanoidAssetPrewarm, StrictLookupReportsAMissInsteadOfBaking) {
  Render::GL::RiggedMeshCache cache;
  const auto request = humanoid_request(Render::Creature::CreatureLOD::Full);
  const auto* handle = resolve_handle(request);
  ASSERT_NE(handle, nullptr);
  ASSERT_TRUE(handle->valid());

  const Render::GL::RiggedMeshCache::Key key =
      Render::Creature::Pipeline::rigged_asset_key(*handle, request.lod, 0U);

  EXPECT_EQ(cache.require_rigged_asset(key, "tests/never_prewarmed"), nullptr);
  EXPECT_EQ(cache.size(), 0U) << "a strict lookup must never construct an asset";
}

TEST(CreatureAssetPrewarm, QuadrupedAssetsPrewarmFromAKeyAlone) {
  Render::GL::RiggedMeshCache cache;
  Render::Creature::CreatureAssetPrewarmer prewarmer(cache, false);

  EXPECT_TRUE(
      prewarmer.prewarm({.archetype = Render::Creature::ArchetypeRegistry::k_horse_base,
                         .creature_asset = Render::Creature::Pipeline::k_horse_asset,
                         .lod = Render::Creature::CreatureLOD::Full}));
  EXPECT_TRUE(prewarmer.prewarm(
      {.archetype = Render::Creature::ArchetypeRegistry::k_elephant_base,
       .creature_asset = Render::Creature::Pipeline::k_elephant_asset,
       .lod = Render::Creature::CreatureLOD::Full}));
  EXPECT_EQ(cache.size(), 2U);
}

TEST(CreatureAssetPrewarm, QuadrupedPrewarmingIsIdempotent) {
  Render::GL::RiggedMeshCache cache;
  Render::Creature::CreatureAssetPrewarmer prewarmer(cache, false);
  const Render::Creature::CreatureAssetKey key{
      .archetype = Render::Creature::ArchetypeRegistry::k_horse_base,
      .creature_asset = Render::Creature::Pipeline::k_horse_asset,
      .lod = Render::Creature::CreatureLOD::Full};

  EXPECT_TRUE(prewarmer.prewarm(key));
  EXPECT_TRUE(prewarmer.prewarm(key));
  EXPECT_EQ(cache.size(), 1U) << "a second prewarm must not build a second asset";
}

TEST(CreatureAssetPrewarm, SpeciesSpecCompilesNoGeometryOncePrewarmed) {
  Render::GL::RiggedMeshCache cache;
  Render::Creature::CreatureAssetPrewarmer prewarmer(cache, false);
  ASSERT_TRUE(
      prewarmer.prewarm({.archetype = Render::Creature::ArchetypeRegistry::k_horse_base,
                         .creature_asset = Render::Creature::Pipeline::k_horse_asset,
                         .lod = Render::Creature::CreatureLOD::Full}));
  ASSERT_TRUE(prewarmer.prewarm(
      {.archetype = Render::Creature::ArchetypeRegistry::k_elephant_base,
       .creature_asset = Render::Creature::Pipeline::k_elephant_asset,
       .lod = Render::Creature::CreatureLOD::Full}));

  const auto before = Render::Creature::creature_lod_geometry_compile_count();

  (void)Render::Horse::horse_creature_spec();
  (void)Render::Elephant::elephant_creature_spec();

  EXPECT_EQ(Render::Creature::creature_lod_geometry_compile_count(), before)
      << "reading a species spec compiled geometry";
}

TEST(CreatureAssetPrewarm, LodGeometryIsCompiledOncePerSpecies) {
  Render::Horse::initialize_horse_asset();
  const auto before = Render::Creature::creature_lod_geometry_compile_count();
  Render::Horse::initialize_horse_asset();
  Render::Horse::initialize_horse_asset();
  EXPECT_EQ(Render::Creature::creature_lod_geometry_compile_count(), before)
      << "re-initializing an asset recompiled its geometry";
}
