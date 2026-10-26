#include <QDir>
#include <QString>

#include <gtest/gtest.h>

#include "game/map/map_loader.h"
#include "game/map/terrain_topology_audit.h"

namespace {

struct CampaignTopology {
  const char* file;
  int road_components;
  int hills_with_one_approach;
};

constexpr CampaignTopology k_reviewed_campaign_maps[] = {
    {"map_battle_cannae.json", 1, 1},
    {"map_battle_ticino.json", 2, 2},
    {"map_battle_trasimene.json", 1, 1},
    {"map_battle_trebia.json", 1, 1},
    {"map_battle_zama.json", 1, 5},
    {"map_campania_campaign.json", 1, 4},
    {"map_crossing_alps.json", 1, 0},
    {"map_crossing_rhone.json", 2, 0},
};

} // namespace

TEST(CampaignMapTopology, RoadNetworksAndHillApproachesMatchTheReview) {
  const QDir maps(QStringLiteral("assets/maps"));
  ASSERT_TRUE(maps.exists()) << maps.absolutePath().toStdString();

  for (const auto& reviewed : k_reviewed_campaign_maps) {
    Game::Map::MapDefinition map;
    QString error;
    ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
        maps.filePath(QString::fromLatin1(reviewed.file)), map, &error))
        << reviewed.file << ": " << error.toStdString();

    const auto audit = Game::Map::audit_terrain_topology(map);
    EXPECT_EQ(audit.road_components, reviewed.road_components)
        << reviewed.file
        << ": a road network split or joined; an accidental break must be "
           "repaired, an intentional one reviewed";
    EXPECT_EQ(audit.hills_without_two_approaches, reviewed.hills_with_one_approach)
        << reviewed.file
        << ": a contested hill lost its second approach, or a single-approach "
           "hill changed without review";
    EXPECT_EQ(audit.invalid_river_endpoints, 0) << reviewed.file;
  }
}
