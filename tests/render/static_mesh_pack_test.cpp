#include <QCoreApplication>
#include <QVector3D>

#include <cstdint>
#include <filesystem>
#include <gtest/gtest.h>
#include <set>
#include <string>
#include <vector>

#include "animation/bpat/asset_compression.h"
#include "render/entity/building_archetype_catalog.h"
#include "render/entity/building_archetype_library.h"
#include "render/static_mesh_pack.h"

namespace {

using namespace Render::GL;

auto sample_mesh() -> MergedBuildingMesh {
  MergedBuildingMesh mesh;
  for (int i = 0; i < 4; ++i) {
    mesh.vertices.push_back(MergedBuildingVertex{
        .position = {static_cast<float>(i), 1.0F, 2.0F},
        .normal = {0.0F, 1.0F, 0.0F},
        .tex_coord = {0.25F, 0.75F},
        .color_alpha = {0.1F, 0.2F, 0.3F, 1.0F},
        .material = {0.0F, 1.0F, 2.0F, 3.0F},
    });
  }
  mesh.indices = {0U, 1U, 2U, 2U, 1U, 3U};
  mesh.ranges.push_back(MergedBuildingRange{.first_index = 0U, .index_count = 6U});
  mesh.bounds_center = QVector3D(1.5F, 1.0F, 2.0F);
  mesh.bounds_radius = 1.5F;
  return mesh;
}

auto load_shipped_pack(std::vector<StaticMeshPackEntry>& entries) -> bool {
  std::vector<std::uint8_t> bytes;
  std::string error;
  const std::string path = "assets/meshes/" + std::string(k_building_mesh_pack);
  return Render::Creature::Bpat::read_asset_file(path, bytes, error) &&
         parse_static_mesh_pack(bytes.data(), bytes.size(), entries, error);
}

} // namespace

TEST(StaticMeshPack, RoundTripsEveryField) {
  const std::vector<StaticMeshPackEntry> written{{"a/normal", sample_mesh()}};
  const std::string bytes = serialize_static_mesh_pack(written);
  std::vector<StaticMeshPackEntry> read;
  std::string error;
  ASSERT_TRUE(parse_static_mesh_pack(
      reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size(), read, error))
      << error;
  ASSERT_EQ(read.size(), 1U);
  const MergedBuildingMesh& mesh = read[0].mesh;
  EXPECT_EQ(read[0].name, "a/normal");
  ASSERT_EQ(mesh.vertices.size(), 4U);
  EXPECT_FLOAT_EQ(mesh.vertices[3].position[0], 3.0F);
  EXPECT_FLOAT_EQ(mesh.vertices[1].material[3], 3.0F);
  EXPECT_EQ(mesh.indices, sample_mesh().indices);
  ASSERT_EQ(mesh.ranges.size(), 1U);
  EXPECT_EQ(mesh.ranges[0].index_count, 6U);
  EXPECT_EQ(mesh.ranges[0].texture, nullptr);
  EXPECT_FLOAT_EQ(mesh.bounds_radius, 1.5F);
  EXPECT_FLOAT_EQ(mesh.bounds_center.x(), 1.5F);
}

TEST(StaticMeshPack, RejectsTruncatedAndOutOfRangeData) {
  const std::string bytes = serialize_static_mesh_pack({{"a/normal", sample_mesh()}});
  std::vector<StaticMeshPackEntry> read;
  std::string error;
  EXPECT_FALSE(
      parse_static_mesh_pack(reinterpret_cast<const std::uint8_t*>(bytes.data()),
                             bytes.size() - 1,
                             read,
                             error));

  MergedBuildingMesh broken = sample_mesh();
  broken.indices.back() = 99U;
  const std::string bad = serialize_static_mesh_pack({{"broken", broken}});
  EXPECT_FALSE(parse_static_mesh_pack(
      reinterpret_cast<const std::uint8_t*>(bad.data()), bad.size(), read, error));
}

TEST(StaticMeshPack, CatalogNamesAreTheBuildingNames) {
  for (const auto& entry : building_archetype_catalog()) {
    EXPECT_EQ(entry.build(BuildingState::Normal).name(), entry.name)
        << "the renderer looks the building up by its catalog name";
  }
}

TEST(StaticMeshPack, PrefersBakedAssetsBesideTheExecutable) {
  const std::filesystem::path executable_assets =
      std::filesystem::path(QCoreApplication::applicationDirPath().toStdString()) /
      "assets/meshes";
  ASSERT_TRUE(std::filesystem::is_directory(executable_assets));
  EXPECT_EQ(find_baked_mesh_directory("assets/meshes").lexically_normal(),
            executable_assets.lexically_normal());
}

TEST(StaticMeshPack, ShippedBuildingPackHasEveryCatalogBuilding) {
  auto& library = StaticMeshLibrary::instance();
  for (const auto& entry : building_archetype_catalog()) {
    for (const BuildingState state :
         {BuildingState::Normal, BuildingState::Damaged, BuildingState::Destroyed}) {
      EXPECT_NE(library.find(building_mesh_key(entry.name, state)), nullptr)
          << building_mesh_key(entry.name, state) << " is not baked";
    }
  }
}

TEST(StaticMeshPack, ShippedBuildingPackMatchesTheCatalog) {
  std::vector<StaticMeshPackEntry> shipped;
  ASSERT_TRUE(load_shipped_pack(shipped))
      << "assets/meshes/buildings.smpk is missing; build bake_creature_assets";

  const std::vector<StaticMeshPackEntry> fresh = bake_building_meshes();
  ASSERT_EQ(shipped.size(), fresh.size());
  std::set<std::string> names;
  for (std::size_t i = 0; i < fresh.size(); ++i) {
    EXPECT_TRUE(names.insert(fresh[i].name).second) << "duplicate " << fresh[i].name;
    EXPECT_EQ(shipped[i].name, fresh[i].name);
    EXPECT_EQ(shipped[i].mesh.vertices.size(), fresh[i].mesh.vertices.size())
        << fresh[i].name;
    EXPECT_EQ(shipped[i].mesh.indices.size(), fresh[i].mesh.indices.size())
        << fresh[i].name;
    EXPECT_TRUE(fresh[i].mesh.dynamic_draws.empty()) << fresh[i].name;
    EXPECT_FALSE(fresh[i].mesh.indices.empty()) << fresh[i].name;
  }
}

TEST(StaticMeshPack, LibraryServesTheShippedBakeAndAddedMeshes) {
  auto& library = StaticMeshLibrary::instance();
  const auto home =
      library.find(building_mesh_key("roman_home", BuildingState::Normal));
  ASSERT_NE(home, nullptr) << "the library loads assets/meshes on first use";
  EXPECT_FALSE(home->indices.empty());

  library.add("static_mesh_pack_test/sample", sample_mesh());
  const auto sample = library.find("static_mesh_pack_test/sample");
  ASSERT_NE(sample, nullptr);
  EXPECT_NE(sample->id, home->id);
  EXPECT_EQ(library.find("static_mesh_pack_test/missing"), nullptr);
}
