#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <gtest/gtest.h>

namespace {

auto load_json_object(const QString& file_path) -> QJsonObject {
  QFile file(file_path);
  EXPECT_TRUE(file.open(QIODevice::ReadOnly)) << file_path.toStdString();
  QJsonParseError error;
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
  EXPECT_EQ(error.error, QJsonParseError::NoError)
      << file_path.toStdString() << ": " << error.errorString().toStdString();
  EXPECT_TRUE(document.isObject()) << file_path.toStdString();
  return document.object();
}

auto asset_dir_path(const QString& relative_path) -> QString {
  return QDir(QCoreApplication::applicationDirPath())
      .absoluteFilePath(QStringLiteral("../../assets/%1").arg(relative_path));
}

} // namespace

TEST(MissionAssetRulesTest, CurrentMissionsDeclareCommanderDefeatRules) {
  QDir const missions_dir(asset_dir_path(QStringLiteral("missions")));
  ASSERT_TRUE(missions_dir.exists()) << missions_dir.path().toStdString();

  const QStringList files = missions_dir.entryList({"*.json"}, QDir::Files, QDir::Name);
  ASSERT_FALSE(files.isEmpty());

  for (const QString& file_name : files) {
    const QJsonObject root = load_json_object(missions_dir.absoluteFilePath(file_name));
    const QJsonArray defeat_conditions = root.value("defeat_conditions").toArray();

    bool has_lose_commander = false;
    bool has_only_commander_remaining = false;
    for (const auto& value : defeat_conditions) {
      const QString type = value.toObject().value("type").toString();
      has_lose_commander =
          has_lose_commander || type == QStringLiteral("lose_commander");
      has_only_commander_remaining = has_only_commander_remaining ||
                                     type == QStringLiteral("only_commander_remaining");
    }

    EXPECT_TRUE(has_lose_commander) << file_name.toStdString();
    EXPECT_TRUE(has_only_commander_remaining) << file_name.toStdString();
  }
}

TEST(MissionAssetRulesTest, CurrentMapsDeclareCommanderDefeatRules) {
  QDir const maps_dir(asset_dir_path(QStringLiteral("maps")));
  ASSERT_TRUE(maps_dir.exists()) << maps_dir.path().toStdString();

  const QStringList files = maps_dir.entryList({"*.json"}, QDir::Files, QDir::Name);
  ASSERT_FALSE(files.isEmpty());

  for (const QString& file_name : files) {
    const QJsonObject root = load_json_object(maps_dir.absoluteFilePath(file_name));
    const QJsonObject victory = root.value("victory").toObject();
    ASSERT_FALSE(victory.isEmpty()) << file_name.toStdString();

    const QJsonArray defeat_conditions = victory.value("defeat_conditions").toArray();
    bool has_no_commander = false;
    bool has_only_commander_remaining = false;
    for (const auto& value : defeat_conditions) {
      const QString type = value.toString();
      has_no_commander = has_no_commander || type == QStringLiteral("no_commander");
      has_only_commander_remaining = has_only_commander_remaining ||
                                     type == QStringLiteral("only_commander_remaining");
    }

    EXPECT_TRUE(has_no_commander) << file_name.toStdString();
    EXPECT_TRUE(has_only_commander_remaining) << file_name.toStdString();
  }
}

TEST(MissionAssetRulesTest, CrossingRhoneUsesAuthoredLocalAiRoles) {
  const QJsonObject root =
      load_json_object(asset_dir_path(QStringLiteral("maps/map_crossing_rhone.json")));
  const QJsonArray spawns = root.value("spawns").toArray();
  ASSERT_FALSE(spawns.isEmpty());

  auto find_spawn = [&](const QString& id) -> QJsonObject {
    for (const auto& value : spawns) {
      const QJsonObject spawn = value.toObject();
      if (spawn.value("id").toString() == id) {
        return spawn;
      }
    }
    return {};
  };

  const QJsonObject hill_archer = find_spawn(QStringLiteral("rome_north_archer_01"));
  ASSERT_FALSE(hill_archer.isEmpty());
  EXPECT_EQ(hill_archer.value("behavior").toString(), QStringLiteral("hold"));

  const QJsonObject north_spearman =
      find_spawn(QStringLiteral("rome_north_spearman_01"));
  ASSERT_FALSE(north_spearman.isEmpty());
  EXPECT_EQ(north_spearman.value("behavior").toString(), QStringLiteral("guard"));
  EXPECT_DOUBLE_EQ(north_spearman.value("guard_radius").toDouble(), 14.0);

  const QJsonObject signal_guard =
      find_spawn(QStringLiteral("rome_north_signal_guard_01"));
  ASSERT_FALSE(signal_guard.isEmpty());
  EXPECT_EQ(signal_guard.value("behavior").toString(), QStringLiteral("patrol"));
  EXPECT_GE(signal_guard.value("patrol_waypoints").toArray().size(), 2);

  int local_role_count = 0;
  for (const auto& value : spawns) {
    const QJsonObject spawn = value.toObject();
    if (spawn.contains("behavior")) {
      ++local_role_count;
    }
  }
  EXPECT_GE(local_role_count, 20);
}
