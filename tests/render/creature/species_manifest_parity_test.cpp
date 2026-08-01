#include <gtest/gtest.h>

#include <QMatrix4x4>

#include <set>
#include <string>
#include <vector>

#include "render/creature/species_manifest.h"
#include "render/elephant/elephant_manifest.h"
#include "render/horse/horse_manifest.h"
#include "render/humanoid/humanoid_manifest.h"

namespace {

using Render::Creature::SpeciesManifest;

// Every species is baked through one code path, so every species has to supply
// the same manifest. These tests exist so that a new creature cannot quietly
// grow a bespoke bake path again.
auto all_manifests() -> std::vector<const SpeciesManifest*> {
  std::vector<const SpeciesManifest*> out;
  for (auto const profile : Render::Humanoid::humanoid_bake_profiles()) {
    out.push_back(&Render::Humanoid::humanoid_manifest(profile));
  }
  out.push_back(&Render::Horse::horse_manifest());
  out.push_back(&Render::Elephant::elephant_manifest());
  return out;
}

TEST(SpeciesManifestParityTest, EveryManifestIsBakeable) {
  for (const auto* manifest : all_manifests()) {
    SCOPED_TRACE(std::string(manifest->species_name));
    EXPECT_FALSE(manifest->species_name.empty());
    EXPECT_FALSE(manifest->bpat_file_name.empty());
    ASSERT_NE(manifest->bind_palette, nullptr);
    ASSERT_NE(manifest->creature_spec, nullptr);
    ASSERT_NE(manifest->bake_clip_frame, nullptr);
    EXPECT_NE(manifest->topology, nullptr);
    EXPECT_FALSE(manifest->clips.empty());
    EXPECT_FALSE(manifest->bind_palette().empty());
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
    SCOPED_TRACE(std::string(manifest->species_name));
    EXPECT_TRUE(ids.insert(manifest->species_id).second)
        << "duplicate species id " << manifest->species_id;
    EXPECT_TRUE(files.insert(std::string(manifest->bpat_file_name)).second)
        << "duplicate bpat file " << manifest->bpat_file_name;
  }
}

TEST(SpeciesManifestParityTest, ClipFrameHookFillsOnePalettePerBone) {
  for (const auto* manifest : all_manifests()) {
    SCOPED_TRACE(std::string(manifest->species_name));
    auto const bones = manifest->bind_palette().size();
    ASSERT_GT(bones, 0U);
    std::vector<QMatrix4x4> palettes;
    manifest->bake_clip_frame(0U, 0U, palettes, nullptr);
    EXPECT_EQ(palettes.size(), bones);
  }
}

// A species with sockets must emit exactly one transform per socket per frame,
// and one without must not be handed a socket sink at all.
TEST(SpeciesManifestParityTest, SocketsAreBakedAlongsidePalettes) {
  for (const auto* manifest : all_manifests()) {
    SCOPED_TRACE(std::string(manifest->species_name));
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
  // The prebaked snapshot pays for itself on imported meshes of a few thousand
  // vertices. The humanoid's minimal graph is a handful of primitives, so it
  // deliberately names no snapshot rather than dumping vertices per frame.
  EXPECT_FALSE(Render::Horse::horse_manifest().minimal_snapshot_file_name.empty());
  EXPECT_FALSE(Render::Elephant::elephant_manifest().minimal_snapshot_file_name.empty());
  for (auto const profile : Render::Humanoid::humanoid_bake_profiles()) {
    EXPECT_TRUE(
        Render::Humanoid::humanoid_manifest(profile).minimal_snapshot_file_name.empty());
  }
}

} // namespace
