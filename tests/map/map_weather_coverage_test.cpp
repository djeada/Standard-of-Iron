#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QString>

#include <gtest/gtest.h>

#include "game/map/map_loader.h"

namespace {

auto maps_directory() -> QDir {
  QDir dir(QStringLiteral("assets/maps"));
  EXPECT_TRUE(dir.exists()) << dir.absolutePath().toStdString();
  return dir;
}

auto shipped_maps() -> QStringList {
  return maps_directory().entryList(
      {QStringLiteral("map_*.json")}, QDir::Files, QDir::Name);
}

auto raw_rain_object(const QString& file_name) -> QJsonObject {
  QFile file(maps_directory().filePath(file_name));
  EXPECT_TRUE(file.open(QIODevice::ReadOnly)) << file_name.toStdString();
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
  return document.object().value(QStringLiteral("rain")).toObject();
}

auto load_map(const QString& file_name) -> Game::Map::MapDefinition {
  Game::Map::MapDefinition map;
  QString error;
  EXPECT_TRUE(Game::Map::MapLoader::load_from_json_file(
      maps_directory().filePath(file_name), map, &error))
      << file_name.toStdString() << ": " << error.toStdString();
  return map;
}

} // namespace

TEST(MapWeatherCoverageTest, EveryShippedMapDeclaresAWeatherChoice) {
  const QStringList maps = shipped_maps();
  ASSERT_FALSE(maps.isEmpty());

  for (const QString& file_name : maps) {
    const QJsonObject rain = raw_rain_object(file_name);
    EXPECT_FALSE(rain.isEmpty())
        << file_name.toStdString()
        << " has no \"rain\" block; clear weather must be stated explicitly";
    EXPECT_TRUE(rain.contains(QStringLiteral("enabled")))
        << file_name.toStdString() << " does not say whether weather is enabled";
  }
}

TEST(MapWeatherCoverageTest, TheShippedSetSpansClearRainAndSnow) {
  bool has_clear = false;
  bool has_rain = false;
  bool has_snow = false;

  for (const QString& file_name : shipped_maps()) {
    const auto map = load_map(file_name);
    if (!map.rain.enabled) {
      has_clear = true;
      continue;
    }
    if (map.rain.type == Game::Map::WeatherType::Snow) {
      has_snow = true;
    } else {
      has_rain = true;
    }
  }

  EXPECT_TRUE(has_clear) << "no map ships with clear skies";
  EXPECT_TRUE(has_rain) << "no map ships with rain";
  EXPECT_TRUE(has_snow) << "no map ships with snow";
}

TEST(MapWeatherCoverageTest, TheAlpineCrossingSnows) {
  const auto map = load_map(QStringLiteral("map_crossing_alps.json"));

  ASSERT_TRUE(map.rain.enabled);
  EXPECT_EQ(map.rain.type, Game::Map::WeatherType::Snow);
  EXPECT_GE(map.rain.intensity, Game::Map::k_weather_intensity_heavy);
  EXPECT_GT(map.rain.wind_strength, 0.0F);
}

TEST(MapWeatherCoverageTest, EveryPrecipitatingMapAuthorsItsWind) {
  for (const QString& file_name : shipped_maps()) {
    const auto map = load_map(file_name);
    if (!map.rain.enabled) {
      continue;
    }
    EXPECT_GT(map.rain.intensity, 0.0F) << file_name.toStdString();
    EXPECT_GT(map.rain.wind_strength, 0.0F)
        << file_name.toStdString() << " enables weather but leaves the wind still";
    EXPECT_GE(map.rain.wind_direction_deg, 0.0F) << file_name.toStdString();
    EXPECT_LT(map.rain.wind_direction_deg, 360.0F) << file_name.toStdString();
  }
}
