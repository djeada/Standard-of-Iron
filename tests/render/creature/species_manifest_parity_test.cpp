#include <QMatrix4x4>

#include <gtest/gtest.h>
#include <set>
#include <string>
#include <vector>

#include "render/creature/bake/creature_bake_recipe.h"
#include "render/creature/schema/creature_runtime_manifest.h"
#include "render/elephant/elephant_bake_recipe.h"
#include "render/elephant/elephant_manifest.h"
#include "render/horse/horse_bake_recipe.h"
#include "render/horse/horse_manifest.h"
#include "render/humanoid/asset/humanoid_manifest.h"

namespace {

using Render::Creature::CreatureBakeRecipe;

auto all_manifests() -> std::vector<const CreatureBakeRecipe*> {
  std::vector<const CreatureBakeRecipe*> out;
  for (auto const profile : Render::Humanoid::humanoid_bake_profiles()) {
    out.push_back(&Render::Humanoid::humanoid_bake_recipe(profile));
  }
  out.push_back(&Render::Horse::horse_bake_recipe());
  out.push_back(&Render::Elephant::elephant_bake_recipe());
  return out;
}

TEST(SpeciesManifestParityTest, EveryManifestIsBakeable) {
  for (const auto* manifest : all_manifests()) {
    SCOPED_TRACE(std::string(manifest->runtime->species_name));
    EXPECT_FALSE(manifest->runtime->species_name.empty());
    EXPECT_FALSE(manifest->runtime->bpat_file_name.empty());
    ASSERT_NE(manifest->runtime->bind_palette, nullptr);
    ASSERT_NE(manifest->runtime->creature_spec, nullptr);
    ASSERT_NE(manifest->bake_clip_frame, nullptr);
    EXPECT_NE(manifest->runtime->topology, nullptr);
    EXPECT_FALSE(manifest->clips.empty());
    EXPECT_FALSE(manifest->runtime->bind_palette().empty());
    for (auto const& clip : manifest->clips) {
      SCOPED_TRACE(std::string(clip.name));
      EXPECT_FALSE(clip.name.empty());
      EXPECT_GT(clip.frame_count, 0U);
      EXPECT_GT(clip.fps, 0.0F);
    }
  }
}

TEST(SpeciesManifestParityTest, SpeciesIdsAndFileNamesAreUnique) {
  std::set<std::uint32_t> ids;
  std::set<std::string> files;
  for (const auto* manifest : all_manifests()) {
    SCOPED_TRACE(std::string(manifest->runtime->species_name));
    EXPECT_TRUE(ids.insert(manifest->runtime->species_id).second)
        << "duplicate species id " << manifest->runtime->species_id;
    EXPECT_TRUE(files.insert(std::string(manifest->runtime->bpat_file_name)).second)
        << "duplicate bpat file " << manifest->runtime->bpat_file_name;
  }
}

TEST(SpeciesManifestParityTest, ClipFrameHookFillsOnePalettePerBone) {
  for (const auto* manifest : all_manifests()) {
    SCOPED_TRACE(std::string(manifest->runtime->species_name));
    auto const bones = manifest->runtime->bind_palette().size();
    ASSERT_GT(bones, 0U);
    std::vector<QMatrix4x4> palettes;
    manifest->bake_clip_frame(0U, 0U, palettes, nullptr);
    EXPECT_EQ(palettes.size(), bones);
  }
}

TEST(SpeciesManifestParityTest, SocketsAreBakedAlongsidePalettes) {
  for (const auto* manifest : all_manifests()) {
    SCOPED_TRACE(std::string(manifest->runtime->species_name));
    if (manifest->sockets.empty()) {
      continue;
    }
    for (auto const& socket : manifest->sockets) {
      EXPECT_FALSE(socket.name.empty());
    }
    std::vector<QMatrix4x4> palettes;
    std::vector<QMatrix4x4> sockets;
    manifest->bake_clip_frame(0U, 0U, palettes, &sockets);
    EXPECT_EQ(sockets.size(), manifest->sockets.size());
  }
}

TEST(SpeciesManifestParityTest, OnlySpeciesNamingASnapshotShipOne) {

  EXPECT_FALSE(
      Render::Horse::horse_runtime_manifest().minimal_snapshot_file_name.empty());
  EXPECT_FALSE(
      Render::Elephant::elephant_runtime_manifest().minimal_snapshot_file_name.empty());
  for (auto const profile : Render::Humanoid::humanoid_bake_profiles()) {
    EXPECT_TRUE(Render::Humanoid::humanoid_runtime_manifest(profile)
                    .minimal_snapshot_file_name.empty());
  }
}

} // namespace
