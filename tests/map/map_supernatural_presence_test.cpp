#include <QDir>
#include <QString>

#include <gtest/gtest.h>

#include "game/map/map_loader.h"
#include "game/map/terrain_service.h"

// Iron ore is drawn with a ghost-light: cyan and violet veins, twinkling
// crystals, an emissive pulse. That is the right look for a barrow and the
// wrong one for a valley the map itself describes as "the plain match, with no
// Sepulcher to distract either side" - where the ore was still the brightest
// thing on the hillside. A battlefield says what walks on it; the ore is lit by
// that rather than by a constant.
namespace {

auto load(const QString& file_name) -> Game::Map::MapDefinition {
  Game::Map::MapDefinition map;
  QString error;
  EXPECT_TRUE(Game::Map::MapLoader::load_from_json_file(
      QDir(QStringLiteral("assets/maps")).filePath(file_name), map, &error))
      << file_name.toStdString() << ": " << error.toStdString();
  return map;
}

class MapSupernaturalPresenceTest : public ::testing::Test {
protected:
  void TearDown() override { Game::Map::TerrainService::instance().clear(); }
};

} // namespace

TEST_F(MapSupernaturalPresenceTest, APlainValleyIsNotHaunted) {
  for (const char* file_name : {"map_rivers.json", "map_tutorial.json"}) {
    const auto map = load(QString::fromLatin1(file_name));
    ASSERT_TRUE(map.undead_zones.empty())
        << file_name
        << " has grown an undead zone; this test picked it as a "
           "battlefield where nothing walks";

    auto& terrain = Game::Map::TerrainService::instance();
    terrain.initialize(map);
    EXPECT_FLOAT_EQ(terrain.supernatural_presence(), 0.0F)
        << file_name << " would light its iron ore like a barrow";
    terrain.clear();
  }
}

TEST_F(MapSupernaturalPresenceTest, ABattlefieldWithUndeadZonesIsHaunted) {
  for (const char* file_name :
       {"map_iron_sepulcher_watch.json", "map_spanish_grove.json"}) {
    const auto map = load(QString::fromLatin1(file_name));
    ASSERT_FALSE(map.undead_zones.empty()) << file_name;

    auto& terrain = Game::Map::TerrainService::instance();
    terrain.initialize(map);
    EXPECT_GT(terrain.supernatural_presence(), 0.0F)
        << file_name << " lost the ghost-light it is authored to have";
    terrain.clear();
  }
}

TEST_F(MapSupernaturalPresenceTest, ThePresenceIsClampedToItsRange) {
  auto& terrain = Game::Map::TerrainService::instance();
  terrain.set_supernatural_presence(-4.0F);
  EXPECT_FLOAT_EQ(terrain.supernatural_presence(), 0.0F);
  terrain.set_supernatural_presence(9.0F);
  EXPECT_FLOAT_EQ(terrain.supernatural_presence(), 1.0F);
}
