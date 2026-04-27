#include "render/creature/snapshot_mesh_asset.h"
#include "render/creature/snapshot_mesh_registry.h"

#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

using Render::Creature::CreatureLOD;
using Render::Creature::Snapshot::ClipDescriptor;
using Render::Creature::Snapshot::SnapshotMeshBlob;
using Render::Creature::Snapshot::SnapshotMeshRegistry;
using Render::Creature::Snapshot::SnapshotMeshWriter;
using Render::GL::RiggedVertex;

auto make_test_vertices() -> std::vector<RiggedVertex> {
  std::vector<RiggedVertex> vertices(3);
  vertices[0].position_bone_local = {-1.0F, 0.0F, 0.0F};
  vertices[1].position_bone_local = {1.0F, 0.0F, 0.0F};
  vertices[2].position_bone_local = {0.0F, 1.0F, 0.0F};
  for (auto &v : vertices) {
    v.normal_bone_local = {0.0F, 1.0F, 0.0F};
    v.bone_indices = {0, 0, 0, 0};
    v.bone_weights = {1.0F, 0.0F, 0.0F, 0.0F};
  }
  return vertices;
}

auto serialize(const SnapshotMeshWriter &writer) -> std::vector<std::uint8_t> {
  std::stringstream ss(std::ios::out | std::ios::binary | std::ios::in);
  EXPECT_TRUE(writer.write(ss));
  std::string s = ss.str();
  return {s.begin(), s.end()};
}

} // namespace

TEST(SnapshotMeshAsset, WriteAndReadbackRoundTripsMinimalClip) {
  const std::array<std::uint32_t, 3> indices{0U, 1U, 2U};
  SnapshotMeshWriter writer(Render::Creature::Bpat::kSpeciesHorse,
                            CreatureLOD::Minimal, 3U, indices);
  writer.add_clip(ClipDescriptor{"idle", 1U});
  auto vertices = make_test_vertices();
  writer.append_clip_vertices(vertices);

  auto blob = SnapshotMeshBlob::from_bytes(serialize(writer));
  ASSERT_TRUE(blob.loaded()) << blob.last_error();
  EXPECT_EQ(blob.species_id(), Render::Creature::Bpat::kSpeciesHorse);
  EXPECT_EQ(blob.lod(), CreatureLOD::Minimal);
  EXPECT_EQ(blob.clip_count(), 1U);
  EXPECT_EQ(blob.frame_total(), 1U);
  EXPECT_EQ(blob.index_count(), 3U);
  EXPECT_EQ(blob.vertex_count(), 3U);
  EXPECT_EQ(blob.clip(0).name, "idle");

  auto const frame_vertices = blob.frame_vertices_view(0U);
  ASSERT_EQ(frame_vertices.size(), 3U);
  EXPECT_FLOAT_EQ(frame_vertices[1].position_bone_local[0], 1.0F);
  auto const indices = blob.indices_view();
  ASSERT_EQ(indices.size(), 3U);
  EXPECT_EQ(indices[2], 2U);
}

TEST(SnapshotMeshRegistry, LoadsMinimalHorseAssetFromFile) {
  auto const temp_dir =
      std::filesystem::temp_directory_path() / "soi_snapshot_mesh_asset_test";
  std::filesystem::create_directories(temp_dir);
  auto const asset_path = temp_dir / "horse_minimal.bpsm";

  const std::array<std::uint32_t, 3> indices{0U, 1U, 2U};
  SnapshotMeshWriter writer(Render::Creature::Bpat::kSpeciesHorse,
                            CreatureLOD::Minimal, 3U, indices);
  writer.add_clip(ClipDescriptor{"idle", 1U});
  auto vertices = make_test_vertices();
  writer.append_clip_vertices(vertices);

  std::ofstream out(asset_path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(out.good());
  ASSERT_TRUE(writer.write(out));
  out.close();

  auto &registry = SnapshotMeshRegistry::instance();
  registry.clear();
  ASSERT_TRUE(registry.load_species(Render::Creature::Bpat::kSpeciesHorse,
                                    CreatureLOD::Minimal,
                                    asset_path.string()))
      << registry.last_error();

  auto const *blob =
      registry.blob(Render::Creature::Bpat::kSpeciesHorse, CreatureLOD::Minimal);
  ASSERT_NE(blob, nullptr);
  EXPECT_EQ(blob->clip(0).name, "idle");
}
