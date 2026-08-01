#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <vector>

#include "render/creature/part_graph.h"
#include "render/creature/rigged_mesh_asset.h"
#include "render/creature/rigged_mesh_registry.h"
#include "render/creature/runtime_bake_guard.h"
#include "render/creature/spec.h"
#include "render/elephant/elephant_spec.h"
#include "render/horse/horse_spec.h"
#include "render/humanoid/humanoid_spec.h"
#include "render/rigged_mesh_bake.h"
#include "render/rigged_mesh_cache.h"

namespace {

using Render::Creature::CreatureLOD;
namespace rigged = Render::Creature::Rigged;

auto round_trip(std::span<const Render::GL::RiggedVertex> vertices,
                std::span<const std::uint32_t> indices,
                CreatureLOD lod) -> rigged::RiggedMeshBlob {
  rigged::RiggedMeshWriter const writer(lod, vertices, indices);
  std::ostringstream out(std::ios::binary);
  if (!writer.write(out)) {
    return rigged::RiggedMeshBlob{};
  }
  auto const text = out.str();
  return rigged::RiggedMeshBlob::from_bytes(
      std::vector<std::uint8_t>(text.begin(), text.end()));
}

TEST(RiggedMeshAssetTest, RoundTripsVerticesAndIndices) {
  std::vector<Render::GL::RiggedVertex> vertices(3);
  for (std::size_t i = 0; i < vertices.size(); ++i) {
    vertices[i].position_bone_local = {float(i), float(i) + 0.5F, float(i) + 1.5F};
    vertices[i].normal_bone_local = {0.0F, 1.0F, 0.0F};
    vertices[i].bone_indices = {static_cast<std::uint8_t>(i), 0U, 0U, 0U};
    vertices[i].bone_weights = {1.0F, 0.0F, 0.0F, 0.0F};
    vertices[i].color_role = static_cast<std::uint8_t>(i + 1U);
  }
  std::vector<std::uint32_t> const indices{0U, 1U, 2U};

  auto const blob = round_trip(vertices, indices, CreatureLOD::Minimal);
  ASSERT_TRUE(blob.loaded()) << blob.last_error();
  EXPECT_EQ(blob.lod(), CreatureLOD::Minimal);
  ASSERT_EQ(blob.vertices_view().size(), vertices.size());
  ASSERT_EQ(blob.indices_view().size(), indices.size());
  for (std::size_t i = 0; i < vertices.size(); ++i) {
    EXPECT_EQ(blob.vertices_view()[i].position_bone_local,
              vertices[i].position_bone_local);
    EXPECT_EQ(blob.vertices_view()[i].bone_weights, vertices[i].bone_weights);
    EXPECT_EQ(blob.vertices_view()[i].color_role, vertices[i].color_role);
  }
}

TEST(RiggedMeshAssetTest, RejectsCorruptData) {
  EXPECT_FALSE(rigged::RiggedMeshBlob::from_bytes({1, 2, 3}).loaded());

  std::vector<Render::GL::RiggedVertex> const vertices(3);
  std::vector<std::uint32_t> const bad_indices{0U, 1U, 9U};
  rigged::RiggedMeshWriter const writer(CreatureLOD::Full, vertices, bad_indices);
  std::ostringstream out(std::ios::binary);
  EXPECT_FALSE(writer.write(out)) << "an out-of-range index must not be written";
}

TEST(RiggedMeshAssetTest, FileNamesFollowThePartGraph) {
  EXPECT_EQ(rigged::asset_file_name("humanoid", CreatureLOD::Full),
            "humanoid_full.bprm");
  EXPECT_EQ(rigged::asset_file_name("skeleton_humanoid", CreatureLOD::Minimal),
            "skeleton_humanoid_minimal.bprm");

  EXPECT_EQ(rigged::asset_file_name("humanoid.sword_ready", CreatureLOD::Full),
            "humanoid_sword_ready_full.bprm");
}

TEST(RiggedMeshAssetTest, BakedBodiesMatchTheirPartGraphs) {
  struct Case {
    const char* file_base;
    const Render::Creature::CreatureSpec& (*spec)() noexcept;
    std::span<const QMatrix4x4> (*bind)() noexcept;
  };
  std::array<Case, 4> const cases{{
      {"humanoid",
       &Render::Humanoid::humanoid_creature_spec,
       &Render::Humanoid::humanoid_bind_palette},
      {"skeleton_humanoid",
       &Render::Humanoid::skeleton_humanoid_creature_spec,
       &Render::Humanoid::humanoid_bind_palette},
      {"horse",
       &Render::Horse::horse_creature_spec,
       &Render::Horse::horse_bind_palette},
      {"elephant",
       &Render::Elephant::elephant_creature_spec,
       &Render::Elephant::elephant_bind_palette},
  }};

  namespace fs = std::filesystem;
  std::array<fs::path, 4> const roots{fs::path{"assets/creatures"},
                                      fs::path{"../assets/creatures"},
                                      fs::path{"../../assets/creatures"},
                                      fs::path{"../../../assets/creatures"}};

  std::size_t checked = 0U;
  for (auto const& item : cases) {
    for (auto const lod : {CreatureLOD::Full, CreatureLOD::Minimal}) {
      auto const name = rigged::asset_file_name(item.file_base, lod);
      fs::path found;
      for (auto const& root : roots) {
        if (fs::exists(root / name)) {
          found = root / name;
          break;
        }
      }
      if (found.empty()) {
        continue;
      }

      auto const blob = rigged::RiggedMeshBlob::from_file(found.string());
      ASSERT_TRUE(blob.loaded()) << name << ": " << blob.last_error();

      Render::Creature::BakeInput input{};
      input.graph = &Render::Creature::part_graph_for(item.spec(), lod);
      input.bind_pose = item.bind();
      auto const expected = Render::Creature::bake_rigged_mesh_cpu(input);

      EXPECT_EQ(blob.vertices_view().size(), expected.vertices.size()) << name;
      EXPECT_EQ(blob.indices_view().size(), expected.indices.size()) << name;
      ++checked;
    }
  }
  EXPECT_GT(checked, 0U) << "no baked bodies found to check";
}

TEST(RiggedMeshAssetTest, LoadedBodiesSatisfyTheRuntimeBakeGuard) {
  struct Restore {
    ~Restore() {

      rigged::RiggedMeshRegistry::instance().clear();
      Render::Creature::set_runtime_bake_forbidden(false);
    }
  } const restore;

  auto const loaded =
      rigged::RiggedMeshRegistry::instance().load_all("assets/creatures");
  if (loaded == 0U) {
    GTEST_SKIP() << "no baked bodies in this working tree";
  }

  auto const& spec = Render::Humanoid::humanoid_creature_spec();
  auto const bind = Render::Humanoid::humanoid_bind_palette();

  Render::GL::RiggedMeshCache cache;
  Render::Creature::set_runtime_bake_forbidden(true);

  const auto* body = cache.get_or_bake(spec,
                                       CreatureLOD::Full,
                                       bind,
                                       0U,
                                       {},
                                       Render::Creature::Bpat::k_species_humanoid);
  ASSERT_NE(body, nullptr)
      << "a prebaked body must resolve even when runtime baking is forbidden";
  EXPECT_EQ(cache.frame_stats().misses, 0U);

  rigged::RiggedMeshRegistry::instance().clear();
  Render::GL::RiggedMeshCache empty_cache;
  EXPECT_EQ(empty_cache.get_or_bake(spec,
                                    CreatureLOD::Full,
                                    bind,
                                    0U,
                                    {},
                                    Render::Creature::Bpat::k_species_humanoid),
            nullptr)
      << "without the baked asset the guard must still reject a runtime bake";
}

} // namespace
