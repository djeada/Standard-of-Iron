#include <QFile>
#include <QTemporaryDir>

#include <filesystem>
#include <gtest/gtest.h>

#include "game/systems/construction_cost_catalog.h"
#include "game/systems/wall_network_service.h"

namespace {

namespace fs = std::filesystem;

auto repo_root() -> fs::path {
  auto dir = fs::current_path();
  for (int depth = 0; depth < 6; ++depth) {
    if (fs::exists(dir / "assets" / "data" / "construction" / "catalog.json")) {
      return dir;
    }
    dir = dir.parent_path();
  }
  return {};
}

struct CatalogGuard {
  ~CatalogGuard() { Game::Systems::reset_construction_catalog(); }
};

TEST(ConstructionCostCatalogTest, TheShippedCatalogDrivesTheDismantleRules) {
  CatalogGuard guard;
  const auto root = repo_root();
  ASSERT_FALSE(root.empty()) << "run from the repo or the build tree";
  const auto path = QString::fromStdString(
      (root / "assets" / "data" / "construction" / "catalog.json").string());
  ASSERT_TRUE(Game::Systems::load_construction_catalog(path));

  EXPECT_FALSE(Game::Systems::dismantle_info("barracks").allowed)
      << "the seat of a force must stay protected";
  EXPECT_TRUE(Game::Systems::dismantle_info("defense_tower").allowed);

  const auto cost = Game::Systems::construction_cost_info("home").resource_costs;
  const auto refund = Game::Systems::dismantle_refund("home");
  EXPECT_GT(refund.get(Game::Systems::ResourceType::Wood), 0);
  for (const auto resource_type : Game::Systems::k_all_resource_types) {
    EXPECT_LE(refund.get(resource_type), cost.get(resource_type));
  }

  EXPECT_TRUE(Game::Systems::dismantle_refund("barracks").empty())
      << "a building that cannot be taken down pays nothing";

  EXPECT_LT(Game::Systems::dismantle_duration("defense_tower"),
            Game::Systems::construction_build_time("defense_tower"))
      << "taking a building down should be quicker than raising it";
}

TEST(ConstructionCostCatalogTest, ShippedCatalogLoadsAndAgreesWithTheWallConstants) {
  CatalogGuard guard;
  const auto root = repo_root();
  ASSERT_FALSE(root.empty()) << "run from the repo or the build tree";
  const auto path = QString::fromStdString(
      (root / "assets" / "data" / "construction" / "catalog.json").string());
  ASSERT_TRUE(Game::Systems::load_construction_catalog(path));

  EXPECT_EQ(Game::Systems::construction_cost_info("wall_segment")
                .resource_costs.get(Game::Systems::ResourceType::Wood),
            Game::Systems::WallNetworkService::k_wall_segment_wood_cost);
  EXPECT_EQ(Game::Systems::construction_cost_info("wall_gate")
                .resource_costs.get(Game::Systems::ResourceType::Wood),
            Game::Systems::WallNetworkService::k_wall_gate_wood_cost);
  EXPECT_GT(Game::Systems::construction_build_time("defense_tower"), 0.0F);
  EXPECT_GT(Game::Systems::construction_build_time("cut_tree"), 0.0F);
}

TEST(ConstructionCostCatalogTest, ALoadedFileOverridesTheBuiltInTableItemByItem) {
  CatalogGuard guard;
  const float builtin_tower_time =
      Game::Systems::construction_build_time("defense_tower");
  const auto builtin_temple =
      Game::Systems::construction_cost_info("temple").resource_costs;

  QTemporaryDir dir;
  const QString path = dir.filePath("catalog.json");
  {
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(
        R"({"items":{"defense_tower":{"costs":{"wood":1,"stone":2},"build_time":99}}})");
  }
  ASSERT_TRUE(Game::Systems::load_construction_catalog(path));

  EXPECT_FLOAT_EQ(Game::Systems::construction_build_time("defense_tower"), 99.0F);
  EXPECT_EQ(Game::Systems::construction_cost_info("defense_tower")
                .resource_costs.get(Game::Systems::ResourceType::Stone),
            2);

  EXPECT_EQ(Game::Systems::construction_cost_info("temple").resource_costs.get(
                Game::Systems::ResourceType::Gold),
            builtin_temple.get(Game::Systems::ResourceType::Gold));

  Game::Systems::reset_construction_catalog();
  EXPECT_FLOAT_EQ(Game::Systems::construction_build_time("defense_tower"),
                  builtin_tower_time);
}

TEST(ConstructionCostCatalogTest, RefusesAMalformedFileWhole) {
  CatalogGuard guard;
  QTemporaryDir dir;
  const QString path = dir.filePath("bad.json");
  {
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(R"({"items":{"home":{"costs":{"plutonium":5}}}})");
  }
  EXPECT_FALSE(Game::Systems::load_construction_catalog(path));
  EXPECT_EQ(Game::Systems::construction_cost_info("home").resource_costs.get(
                Game::Systems::ResourceType::Wood),
            50);
}

} // namespace
