#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <gtest/gtest.h>

#include "game/map/campaign_loader.h"
#include "game/map/mission_loader.h"

// docs/CAMPAIGN_MISSIONS.md is the one-page overview of the shipped campaign:
// opponents, wave pressure, whether the player has a base, what wins the
// mission. A hand-maintained table like that rots the first time someone tunes
// a mission, so this reads the table back and holds it to the assets.
namespace {

auto repo_root() -> QDir {
  QDir dir = QDir::current();
  for (int depth = 0; depth < 8; ++depth) {
    if (dir.exists(QStringLiteral("docs/CAMPAIGN_MISSIONS.md"))) {
      return dir;
    }
    if (!dir.cdUp()) {
      break;
    }
  }
  return QDir::current();
}

auto read_text(const QString& relative) -> QString {
  QFile file(repo_root().filePath(relative));
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return {};
  }
  return QString::fromUtf8(file.readAll());
}

// Every row of the overview table, split into its cells. Rows are recognised by
// their leading mission number so the surrounding prose cannot be mistaken for
// data.
auto split_row(const QString& line) -> QStringList {
  QStringList cells = line.split(QLatin1Char('|'), Qt::SkipEmptyParts);
  for (QString& cell : cells) {
    cell = cell.trimmed();
  }
  return cells;
}

auto overview_rows() -> QList<QStringList> {
  QList<QStringList> rows;
  const QStringList lines =
      read_text(QStringLiteral("docs/CAMPAIGN_MISSIONS.md")).split(QLatin1Char('\n'));
  bool in_first_table = false;
  for (const QString& line : lines) {
    if (!line.startsWith(QLatin1Char('|'))) {
      if (in_first_table) {
        break;
      }
      continue;
    }

    const QStringList cells = split_row(line);
    // The header is found by its cells rather than by the raw line: the
    // markdown formatter pads every column, so the spacing is not ours to
    // depend on.
    if (!in_first_table) {
      in_first_table = cells.size() >= 3 && cells.at(0) == QStringLiteral("#") &&
                       cells.at(1) == QStringLiteral("Mission") &&
                       cells.at(2) == QStringLiteral("Region");
      continue;
    }

    bool numbered = false;
    if (cells.isEmpty() || cells.first().toInt(&numbered) <= 0 || !numbered) {
      continue;
    }
    rows.append(cells);
  }
  return rows;
}

auto load_campaign() -> Game::Campaign::CampaignDefinition {
  Game::Campaign::CampaignDefinition campaign;
  QString error;
  EXPECT_TRUE(Game::Campaign::CampaignLoader::load_from_json_file(
      repo_root().filePath(QStringLiteral("assets/campaigns/second_punic_war.json")),
      campaign,
      &error))
      << error.toStdString();
  return campaign;
}

auto load_mission(const QString& mission_id) -> Game::Mission::MissionDefinition {
  Game::Mission::MissionDefinition mission;
  QString error;
  EXPECT_TRUE(Game::Mission::MissionLoader::load_from_json_file(
      repo_root().filePath(QStringLiteral("assets/missions/%1.json").arg(mission_id)),
      mission,
      &error))
      << error.toStdString();
  return mission;
}

// The maps are read as raw JSON: what the roster needs from them -- how many
// units and barracks each owner starts with -- is map authoring data, not
// something the mission types model.
auto map_json_for(const Game::Mission::MissionDefinition& mission) -> QJsonObject {
  QString name = mission.map_path;
  const int slash = name.lastIndexOf(QLatin1Char('/'));
  if (slash >= 0) {
    name = name.mid(slash + 1);
  }
  QFile file(repo_root().filePath(QStringLiteral("assets/maps/%1").arg(name)));
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  return QJsonDocument::fromJson(file.readAll()).object();
}

auto player_troop_count(const QJsonObject& map) -> int {
  int count = 0;
  for (const auto& value : map.value(QStringLiteral("spawns")).toArray()) {
    if (value.toObject().value(QStringLiteral("player_id")).toInt() == 1) {
      ++count;
    }
  }
  return count;
}

auto player_has_barracks(const QJsonObject& map) -> bool {
  for (const auto& value : map.value(QStringLiteral("structures")).toArray()) {
    const QJsonObject structure = value.toObject();
    if (structure.value(QStringLiteral("player_id")).toInt() == 1 &&
        structure.value(QStringLiteral("type")).toString() ==
            QStringLiteral("barracks")) {
      return true;
    }
  }
  return false;
}

auto wave_count(const Game::Mission::MissionDefinition& mission) -> int {
  int waves = 0;
  for (const auto& ai : mission.ai_setups) {
    waves += static_cast<int>(ai.waves.size());
  }
  return waves;
}

TEST(CampaignRosterDocTest, TheOverviewListsEveryMissionInOrder) {
  const auto campaign = load_campaign();
  const auto rows = overview_rows();

  ASSERT_EQ(static_cast<std::size_t>(rows.size()), campaign.missions.size())
      << "docs/CAMPAIGN_MISSIONS.md lists " << rows.size() << " missions; the "
      << "campaign ships " << campaign.missions.size();

  for (int i = 0; i < rows.size(); ++i) {
    EXPECT_EQ(rows[i].first().toInt(), i + 1)
        << "the overview is out of order at row " << i;
    const auto mission =
        load_mission(campaign.missions[static_cast<std::size_t>(i)].mission_id);
    EXPECT_EQ(rows[i].value(1), mission.title)
        << "row " << i + 1 << " names a mission the campaign does not run there";
  }
}

TEST(CampaignRosterDocTest, TheOverviewMatchesWhatTheMissionsActuallyDo) {
  const auto campaign = load_campaign();
  const auto rows = overview_rows();
  ASSERT_EQ(static_cast<std::size_t>(rows.size()), campaign.missions.size());

  for (int i = 0; i < rows.size(); ++i) {
    const auto& entry = campaign.missions[static_cast<std::size_t>(i)];
    const auto mission = load_mission(entry.mission_id);
    const QJsonObject map = map_json_for(mission);
    ASSERT_FALSE(map.isEmpty())
        << mission.map_path.toStdString() << " could not be read";

    const QStringList& row = rows[i];
    const std::string where = mission.title.toStdString();

    EXPECT_EQ(row.value(2), entry.world_region_id.value_or(QString()))
        << where << ": region";
    EXPECT_EQ(row.value(3).toInt(), static_cast<int>(mission.ai_setups.size()))
        << where << ": opponent count";
    EXPECT_EQ(row.value(4).toInt(), wave_count(mission)) << where << ": wave count";
    EXPECT_EQ(row.value(5) != QStringLiteral("none"), player_has_barracks(map))
        << where << ": whether the player starts with a barracks";
    EXPECT_EQ(row.value(6).toInt(), player_troop_count(map))
        << where << ": starting troops";
  }
}

TEST(CampaignRosterDocTest, TheGoalColumnAgreesWithTheVictoryConditions) {
  const auto campaign = load_campaign();
  const auto rows = overview_rows();
  ASSERT_EQ(static_cast<std::size_t>(rows.size()), campaign.missions.size());

  for (int i = 0; i < rows.size(); ++i) {
    const auto mission =
        load_mission(campaign.missions[static_cast<std::size_t>(i)].mission_id);

    bool takes_ground = false;
    bool holds_ground = false;
    bool builds = false;
    for (const auto& condition : mission.victory_conditions) {
      const QString type = condition.type;
      takes_ground = takes_ground || type == QStringLiteral("capture_structures") ||
                     type == QStringLiteral("control_structures");
      holds_ground = holds_ground || type == QStringLiteral("survive_waves") ||
                     type == QStringLiteral("survive_duration") ||
                     type == QStringLiteral("survive_undead_wave");
      builds = builds || type == QStringLiteral("accumulate_resources");
    }

    QString expected = QStringLiteral("Offensive");
    if (builds) {
      expected = QStringLiteral("Economic");
    } else if (takes_ground && holds_ground) {
      expected = QStringLiteral("Both");
    } else if (holds_ground) {
      expected = QStringLiteral("Defensive");
    }

    EXPECT_EQ(rows[i].value(7), expected)
        << mission.title.toStdString()
        << ": the goal column disagrees with the victory conditions";
  }
}

} // namespace
