#include <QMatrix4x4>

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
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
#include "render/rigged_mesh_bake.h"
#include "render/snapshot_mesh_bake.h"
#include "render/wildlife/sheep_manifest.h"
#include "render/wildlife/wolf_manifest.h"

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

TEST(SpeciesManifestParityTest, AnimalDeathEndsExactlyAtTheCorpsePose) {
  for (auto const* recipe : {&Render::Horse::horse_bake_recipe(),
                             &Render::Elephant::elephant_bake_recipe(),
                             &Render::Wildlife::wolf_bake_recipe(),
                             &Render::Wildlife::sheep_bake_recipe()}) {
    SCOPED_TRACE(std::string(recipe->runtime->species_name));
    std::vector<QMatrix4x4> dying;
    std::vector<QMatrix4x4> dead;
    for (std::size_t clip = 0; clip < recipe->clips.size(); ++clip) {
      auto const& desc = recipe->clips[clip];
      if (desc.name == "die") {
        recipe->bake_clip_frame(clip, desc.frame_count - 1U, dying, nullptr);
      } else if (desc.name == "dead") {
        recipe->bake_clip_frame(clip, 0U, dead, nullptr);
      }
    }
    ASSERT_FALSE(dying.empty());
    ASSERT_EQ(dying.size(), dead.size());
    for (std::size_t bone = 0; bone < dying.size(); ++bone) {
      for (int entry = 0; entry < 16; ++entry) {
        EXPECT_NEAR(
            dying[bone].constData()[entry], dead[bone].constData()[entry], 1.0e-5F)
            << "bone " << bone << " matrix entry " << entry;
      }
    }
  }
}

TEST(SpeciesManifestParityTest, ElephantFallsOntoGroundInsteadOfRemainingSeated) {
  auto const& recipe = Render::Elephant::elephant_bake_recipe();
  auto const bind = recipe.runtime->bind_palette();
  auto const& spec = recipe.runtime->creature_spec();
  auto const mesh = Render::Creature::bake_rigged_mesh_cpu({&spec.lod_full, bind});
  ASSERT_FALSE(mesh.vertices.empty());
  auto const clip = std::find_if(recipe.clips.begin(),
                                 recipe.clips.end(),
                                 [](auto const& desc) { return desc.name == "die"; });
  ASSERT_NE(clip, recipe.clips.end());
  float standing_height = 0.0F;
  float corpse_height = 0.0F;
  for (std::uint32_t frame = 0; frame < clip->frame_count; ++frame) {
    std::vector<QMatrix4x4> palette;
    recipe.bake_clip_frame(
        static_cast<std::size_t>(clip - recipe.clips.begin()), frame, palette, nullptr);
    for (std::size_t bone = 0; bone < palette.size(); ++bone) {
      palette[bone] *= bind[bone].inverted();
    }
    auto const vertices = Render::GL::bake_snapshot_vertices(mesh.vertices, palette);
    float lowest = std::numeric_limits<float>::max();
    float highest = std::numeric_limits<float>::lowest();
    for (auto const& vertex : vertices) {
      float const y = vertex.position_bone_local[1];
      ASSERT_TRUE(std::isfinite(y));
      lowest = std::min(lowest, y);
      highest = std::max(highest, y);
    }
    EXPECT_NEAR(lowest, 0.0F, 1.0e-4F) << "frame " << frame;
    if (frame == 0U) {
      standing_height = highest;
    }
    corpse_height = highest;
  }

  EXPECT_LT(corpse_height, standing_height * 0.80F);
}
