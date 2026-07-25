#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>

#include <algorithm>
#include <gtest/gtest.h>

#include "tools/map_editor/mission_data.h"

namespace {

TEST(MapEditorMissionDataTest, DefaultMissionUsesSupportedRuntimeOptions) {
  MapEditor::MissionData mission;

  EXPECT_TRUE(mission.validate().isEmpty());
  EXPECT_EQ(mission.value("player_setup").toObject().value("nation").toString(),
            QStringLiteral("roman_republic"));
  ASSERT_EQ(mission.array("victory_conditions").size(), 1);
  EXPECT_EQ(
      mission.array("victory_conditions").first().toObject().value("type").toString(),
      QStringLiteral("destroy_all_enemies"));
}

TEST(MapEditorMissionDataTest, RoundTripsEditorAuthoredMission) {
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("mission.json"));

  MapEditor::MissionData mission;
  mission.set_value("id", "editor_round_trip");
  mission.set_value("title", "Editor Round Trip");
  mission.set_array(
      "events",
      QJsonArray{QJsonObject{
          {"trigger", QJsonObject{{"type", "timer"}, {"time", 30.0}}},
          {"actions",
           QJsonArray{QJsonObject{{"type", "show_message"},
                                  {"text", "The second phase begins."}}}}}});
  QString error;
  ASSERT_TRUE(mission.save_to_json(path, &error)) << error.toStdString();

  MapEditor::MissionData loaded;
  ASSERT_TRUE(loaded.load_from_json(path, &error)) << error.toStdString();
  EXPECT_EQ(loaded.id(), QStringLiteral("editor_round_trip"));
  EXPECT_EQ(loaded.array("events").size(), 1);
  EXPECT_TRUE(loaded.validate().isEmpty());
}

TEST(MapEditorMissionDataTest, RejectsUnsupportedSelectionsAndIncompleteZones) {
  MapEditor::MissionData mission;
  QJsonObject root = mission.root();
  QJsonObject player = root.value("player_setup").toObject();
  player["nation"] = "invented_empire";
  root["player_setup"] = player;
  root["victory_conditions"] = QJsonArray{QJsonObject{
      {"type", "clear_undead_zone"}, {"description", "Cleanse the ruins."}}};
  mission.set_root(root);

  const QStringList errors = mission.validate();
  EXPECT_TRUE(std::any_of(errors.begin(), errors.end(), [](const QString& error) {
    return error.contains(QStringLiteral("unsupported nation"));
  }));
  EXPECT_TRUE(std::any_of(errors.begin(), errors.end(), [](const QString& error) {
    return error.contains(QStringLiteral("zone_id"));
  }));
}

} // namespace
