

#include "render/creature/spec.h"
#include "render/elephant/elephant_spec.h"
#include "render/horse/horse_spec.h"
#include "render/humanoid/humanoid_spec.h"
#include "render/rigged_mesh.h"
#include "render/rigged_mesh_cache.h"

#include <gtest/gtest.h>
#include <type_traits>

namespace {

using Render::Creature::CreatureLOD;
using Render::GL::RiggedMeshCache;
using Render::GL::RiggedVertex;

TEST(RiggedMeshCache, RepeatedCallsForSameKeyReturnSameEntry) {
  RiggedMeshCache cache;
  auto const &spec = Render::Humanoid::humanoid_creature_spec();
  auto const bind = Render::Humanoid::humanoid_bind_palette();

  EXPECT_EQ(cache.size(), 0U);

  const auto *first = cache.get_or_bake(spec, CreatureLOD::Full, bind);
  ASSERT_NE(first, nullptr);
  EXPECT_EQ(cache.size(), 1U);

  const auto *second = cache.get_or_bake(spec, CreatureLOD::Full, bind);
  EXPECT_EQ(first, second) << "second get_or_bake must hit the cache";
  EXPECT_EQ(first->mesh.get(), second->mesh.get())
      << "the underlying mesh must not be re-baked";
  EXPECT_EQ(cache.size(), 1U)
      << "cache must not grow when re-asked for the same key";

  for (int i = 0; i < 64; ++i) {
    EXPECT_EQ(cache.get_or_bake(spec, CreatureLOD::Full, bind), first);
  }
  EXPECT_EQ(cache.size(), 1U);
}

TEST(RiggedMeshCache, PerUnitVariantBucketDefaultsToZeroAndDeduplicates) {

  RiggedMeshCache cache;
  auto const &spec = Render::Humanoid::humanoid_creature_spec();
  auto const bind = Render::Humanoid::humanoid_bind_palette();

  const auto *via_default = cache.get_or_bake(spec, CreatureLOD::Full, bind);
  const auto *via_explicit_zero =
      cache.get_or_bake(spec, CreatureLOD::Full, bind, 0);
  EXPECT_EQ(via_default, via_explicit_zero);
  EXPECT_EQ(cache.size(), 1U);
}

TEST(RiggedMeshCache, FullAndMinimalBakeIndependentlyButOnlyOnceEach) {
  RiggedMeshCache cache;
  auto const &spec = Render::Humanoid::humanoid_creature_spec();
  auto const bind = Render::Humanoid::humanoid_bind_palette();

  const auto *full = cache.get_or_bake(spec, CreatureLOD::Full, bind);
  const auto *minimal = cache.get_or_bake(spec, CreatureLOD::Minimal, bind);

  ASSERT_NE(full, nullptr);
  ASSERT_NE(minimal, nullptr);
  EXPECT_NE(full, minimal);
  EXPECT_EQ(cache.size(), 2U);

  EXPECT_EQ(cache.get_or_bake(spec, CreatureLOD::Full, bind), full);
  EXPECT_EQ(cache.get_or_bake(spec, CreatureLOD::Minimal, bind), minimal);
  EXPECT_EQ(cache.size(), 2U);
}

TEST(RiggedMeshCache, DifferentSpeciesProduceDistinctEntries) {
  RiggedMeshCache cache;

  auto const &humanoid = Render::Humanoid::humanoid_creature_spec();
  auto const &horse = Render::Horse::horse_creature_spec();
  auto const &elephant = Render::Elephant::elephant_creature_spec();

  const auto *h = cache.get_or_bake(humanoid, CreatureLOD::Full,
                                    Render::Humanoid::humanoid_bind_palette());
  const auto *r = cache.get_or_bake(horse, CreatureLOD::Full,
                                    Render::Horse::horse_bind_palette());
  const auto *e = cache.get_or_bake(elephant, CreatureLOD::Full,
                                    Render::Elephant::elephant_bind_palette());

  ASSERT_NE(h, nullptr);
  ASSERT_NE(r, nullptr);
  ASSERT_NE(e, nullptr);
  EXPECT_NE(h, r);
  EXPECT_NE(r, e);
  EXPECT_NE(h, e);
  EXPECT_EQ(cache.size(), 3U);

  for (int i = 0; i < 32; ++i) {
    cache.get_or_bake(humanoid, CreatureLOD::Full,
                      Render::Humanoid::humanoid_bind_palette());
    cache.get_or_bake(horse, CreatureLOD::Full,
                      Render::Horse::horse_bind_palette());
    cache.get_or_bake(elephant, CreatureLOD::Full,
                      Render::Elephant::elephant_bind_palette());
  }
  EXPECT_EQ(cache.size(), 3U)
      << "spawning many units of each species must not re-bake meshes";
}

TEST(RiggedMeshCache, BakedVertexFormatCarriesRoleIndexButNoPerUnitColour) {

  static_assert(sizeof(RiggedVertex) ==
                    sizeof(float) * (3 + 3 + 2 + 4) + sizeof(std::uint8_t) * 8,
                "RiggedVertex may contain stable authoring metadata like a "
                "role index, but per-unit colour still belongs in draw-time "
                "uniforms, never in baked vertices.");

  RiggedVertex v{};
  static_assert(
      std::is_same_v<decltype(v.position_bone_local), std::array<float, 3>>);
  static_assert(
      std::is_same_v<decltype(v.normal_bone_local), std::array<float, 3>>);
  static_assert(std::is_same_v<decltype(v.tex_coord), std::array<float, 2>>);
  static_assert(std::is_same_v<decltype(v.bone_weights), std::array<float, 4>>);
  static_assert(
      std::is_same_v<decltype(v.bone_indices), std::array<std::uint8_t, 4>>);
  static_assert(std::is_same_v<decltype(v.color_role), std::uint8_t>);
  SUCCEED();
}

} // namespace
