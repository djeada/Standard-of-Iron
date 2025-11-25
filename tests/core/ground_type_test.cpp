#include "map/map_loader.h"
#include "map/terrain.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryFile>
#include <gtest/gtest.h>

using namespace Game::Map;

class GroundTypeTest : public ::testing::Test {
protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(GroundTypeTest, GroundTypeEnumToString) {
  EXPECT_EQ(groundTypeToQString(GroundType::ForestMud),
            QStringLiteral("forest_mud"));
  EXPECT_EQ(groundTypeToQString(GroundType::GrassDry),
            QStringLiteral("grass_dry"));
  EXPECT_EQ(groundTypeToQString(GroundType::SoilRocky),
            QStringLiteral("soil_rocky"));
  EXPECT_EQ(groundTypeToQString(GroundType::AlpineMix),
            QStringLiteral("alpine_mix"));
  EXPECT_EQ(groundTypeToQString(GroundType::SoilFertile),
            QStringLiteral("soil_fertile"));
}

TEST_F(GroundTypeTest, GroundTypeStringToEnum) {
  GroundType result;

  EXPECT_TRUE(tryParseGroundType("forest_mud", result));
  EXPECT_EQ(result, GroundType::ForestMud);

  EXPECT_TRUE(tryParseGroundType("grass_dry", result));
  EXPECT_EQ(result, GroundType::GrassDry);

  EXPECT_TRUE(tryParseGroundType("soil_rocky", result));
  EXPECT_EQ(result, GroundType::SoilRocky);

  EXPECT_TRUE(tryParseGroundType("alpine_mix", result));
  EXPECT_EQ(result, GroundType::AlpineMix);

  EXPECT_TRUE(tryParseGroundType("soil_fertile", result));
  EXPECT_EQ(result, GroundType::SoilFertile);
}

TEST_F(GroundTypeTest, GroundTypeParsingCaseInsensitive) {
  GroundType result;

  EXPECT_TRUE(tryParseGroundType("FOREST_MUD", result));
  EXPECT_EQ(result, GroundType::ForestMud);

  EXPECT_TRUE(tryParseGroundType("Forest_Mud", result));
  EXPECT_EQ(result, GroundType::ForestMud);

  EXPECT_TRUE(tryParseGroundType("  grass_dry  ", result));
  EXPECT_EQ(result, GroundType::GrassDry);
}

TEST_F(GroundTypeTest, GroundTypeParsingInvalidReturnsDefault) {
  GroundType result = GroundType::ForestMud;

  EXPECT_FALSE(tryParseGroundType("invalid_type", result));
  EXPECT_FALSE(tryParseGroundType("", result));
  EXPECT_FALSE(tryParseGroundType("unknown", result));
}

TEST_F(GroundTypeTest, ApplyGroundTypeDefaultsForestMud) {
  BiomeSettings settings;
  applyGroundTypeDefaults(settings, GroundType::ForestMud);

  EXPECT_EQ(settings.groundType, GroundType::ForestMud);
  EXPECT_FLOAT_EQ(settings.grassPrimary.x(), 0.30F);
  EXPECT_FLOAT_EQ(settings.grassPrimary.y(), 0.60F);
  EXPECT_FLOAT_EQ(settings.grassPrimary.z(), 0.28F);
  EXPECT_FLOAT_EQ(settings.soilColor.x(), 0.28F);
  EXPECT_FLOAT_EQ(settings.soilColor.y(), 0.24F);
  EXPECT_FLOAT_EQ(settings.soilColor.z(), 0.18F);
}

TEST_F(GroundTypeTest, ApplyGroundTypeDefaultsGrassDry) {
  BiomeSettings settings;
  applyGroundTypeDefaults(settings, GroundType::GrassDry);

  EXPECT_EQ(settings.groundType, GroundType::GrassDry);
  EXPECT_FLOAT_EQ(settings.grassPrimary.x(), 0.58F);
  EXPECT_FLOAT_EQ(settings.grassPrimary.y(), 0.54F);
  EXPECT_FLOAT_EQ(settings.grassPrimary.z(), 0.32F);
  EXPECT_FLOAT_EQ(settings.terrainAmbientBoost, 1.18F);
  // Check ground-type-specific parameters
  EXPECT_FLOAT_EQ(settings.crackIntensity, 0.65F);
  EXPECT_FLOAT_EQ(settings.moistureLevel, 0.15F);
  EXPECT_FLOAT_EQ(settings.grassSaturation, 0.75F);
}

TEST_F(GroundTypeTest, ApplyGroundTypeDefaultsSoilRocky) {
  BiomeSettings settings;
  applyGroundTypeDefaults(settings, GroundType::SoilRocky);

  EXPECT_EQ(settings.groundType, GroundType::SoilRocky);
  EXPECT_FLOAT_EQ(settings.soilColor.x(), 0.55F);
  EXPECT_FLOAT_EQ(settings.soilColor.y(), 0.48F);
  EXPECT_FLOAT_EQ(settings.soilColor.z(), 0.38F);
  EXPECT_FLOAT_EQ(settings.terrainRockDetailStrength, 0.65F);
  // Check ground-type-specific parameters
  EXPECT_FLOAT_EQ(settings.rockExposure, 0.75F);
  EXPECT_FLOAT_EQ(settings.soilRoughness, 0.85F);
}

TEST_F(GroundTypeTest, ApplyGroundTypeDefaultsAlpineMix) {
  BiomeSettings settings;
  applyGroundTypeDefaults(settings, GroundType::AlpineMix);

  EXPECT_EQ(settings.groundType, GroundType::AlpineMix);
  EXPECT_FLOAT_EQ(settings.rockHigh.x(), 0.88F);
  EXPECT_FLOAT_EQ(settings.rockHigh.y(), 0.90F);
  EXPECT_FLOAT_EQ(settings.rockHigh.z(), 0.94F);
  EXPECT_FLOAT_EQ(settings.terrainAmbientBoost, 1.25F);
  // Check ground-type-specific parameters
  EXPECT_FLOAT_EQ(settings.snowCoverage, 0.55F);
  EXPECT_FLOAT_EQ(settings.snowColor.x(), 0.94F);
  EXPECT_FLOAT_EQ(settings.snowColor.y(), 0.96F);
  EXPECT_FLOAT_EQ(settings.snowColor.z(), 1.0F);
}

TEST_F(GroundTypeTest, ApplyGroundTypeDefaultsSoilFertile) {
  BiomeSettings settings;
  applyGroundTypeDefaults(settings, GroundType::SoilFertile);

  EXPECT_EQ(settings.groundType, GroundType::SoilFertile);
  EXPECT_FLOAT_EQ(settings.soilColor.x(), 0.20F);
  EXPECT_FLOAT_EQ(settings.soilColor.y(), 0.16F);
  EXPECT_FLOAT_EQ(settings.soilColor.z(), 0.12F);
  EXPECT_FLOAT_EQ(settings.terrainRockDetailStrength, 0.22F);
  // Check ground-type-specific parameters
  EXPECT_FLOAT_EQ(settings.moistureLevel, 0.80F);
  EXPECT_FLOAT_EQ(settings.grassSaturation, 1.15F);
  EXPECT_FLOAT_EQ(settings.rockExposure, 0.12F);
}

TEST_F(GroundTypeTest, MapLoaderWithGroundType) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  QJsonObject biome;
  biome["groundType"] = "grass_dry";
  biome["seed"] = 12345;

  QJsonObject grid;
  grid["width"] = 50;
  grid["height"] = 50;
  grid["tileSize"] = 1.0;

  QJsonObject root;
  root["name"] = "Test Map";
  root["grid"] = grid;
  root["biome"] = biome;

  QJsonDocument doc(root);
  temp_file.write(doc.toJson());
  temp_file.close();

  MapDefinition map_def;
  QString error;
  bool success = MapLoader::loadFromJsonFile(temp_file.fileName(), map_def, &error);

  ASSERT_TRUE(success) << "Failed to load map: " << error.toStdString();
  EXPECT_EQ(map_def.biome.groundType, GroundType::GrassDry);
  EXPECT_EQ(map_def.biome.seed, 12345U);
}

TEST_F(GroundTypeTest, MapLoaderWithoutGroundTypeUsesDefault) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  QJsonObject biome;
  biome["seed"] = 54321;

  QJsonObject grid;
  grid["width"] = 50;
  grid["height"] = 50;
  grid["tileSize"] = 1.0;

  QJsonObject root;
  root["name"] = "Test Map Without Ground Type";
  root["grid"] = grid;
  root["biome"] = biome;

  QJsonDocument doc(root);
  temp_file.write(doc.toJson());
  temp_file.close();

  MapDefinition map_def;
  QString error;
  bool success = MapLoader::loadFromJsonFile(temp_file.fileName(), map_def, &error);

  ASSERT_TRUE(success) << "Failed to load map: " << error.toStdString();
  EXPECT_EQ(map_def.biome.groundType, GroundType::ForestMud);
  EXPECT_EQ(map_def.biome.seed, 54321U);
}

TEST_F(GroundTypeTest, MapLoaderGroundTypeOverriddenByExplicitValues) {
  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());

  QJsonObject biome;
  biome["groundType"] = "alpine_mix";
  biome["seed"] = 99999;
  // Override the grass primary color that would be set by alpine_mix defaults
  QJsonArray grass_primary;
  grass_primary.append(0.10);
  grass_primary.append(0.20);
  grass_primary.append(0.30);
  biome["grassPrimary"] = grass_primary;

  QJsonObject grid;
  grid["width"] = 50;
  grid["height"] = 50;
  grid["tileSize"] = 1.0;

  QJsonObject root;
  root["name"] = "Test Map With Override";
  root["grid"] = grid;
  root["biome"] = biome;

  QJsonDocument doc(root);
  temp_file.write(doc.toJson());
  temp_file.close();

  MapDefinition map_def;
  QString error;
  bool success = MapLoader::loadFromJsonFile(temp_file.fileName(), map_def, &error);

  ASSERT_TRUE(success) << "Failed to load map: " << error.toStdString();
  EXPECT_EQ(map_def.biome.groundType, GroundType::AlpineMix);
  EXPECT_EQ(map_def.biome.seed, 99999U);
  // Grass primary should be the overridden values, not the alpine_mix defaults
  EXPECT_NEAR(map_def.biome.grassPrimary.x(), 0.10F, 0.001F);
  EXPECT_NEAR(map_def.biome.grassPrimary.y(), 0.20F, 0.001F);
  EXPECT_NEAR(map_def.biome.grassPrimary.z(), 0.30F, 0.001F);
}

TEST_F(GroundTypeTest, AllGroundTypesFromString) {
  auto forest_mud = groundTypeFromString("forest_mud");
  ASSERT_TRUE(forest_mud.has_value());
  EXPECT_EQ(forest_mud.value(), GroundType::ForestMud);

  auto grass_dry = groundTypeFromString("grass_dry");
  ASSERT_TRUE(grass_dry.has_value());
  EXPECT_EQ(grass_dry.value(), GroundType::GrassDry);

  auto soil_rocky = groundTypeFromString("soil_rocky");
  ASSERT_TRUE(soil_rocky.has_value());
  EXPECT_EQ(soil_rocky.value(), GroundType::SoilRocky);

  auto alpine_mix = groundTypeFromString("alpine_mix");
  ASSERT_TRUE(alpine_mix.has_value());
  EXPECT_EQ(alpine_mix.value(), GroundType::AlpineMix);

  auto soil_fertile = groundTypeFromString("soil_fertile");
  ASSERT_TRUE(soil_fertile.has_value());
  EXPECT_EQ(soil_fertile.value(), GroundType::SoilFertile);

  auto invalid = groundTypeFromString("invalid");
  EXPECT_FALSE(invalid.has_value());
}
