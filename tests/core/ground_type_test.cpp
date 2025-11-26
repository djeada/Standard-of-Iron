#include "map/map_loader.h"
#include "map/terrain.h"
#include <QJsonArray>
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
  EXPECT_EQ(ground_type_to_qstring(GroundType::ForestMud),
            QStringLiteral("forest_mud"));
  EXPECT_EQ(ground_type_to_qstring(GroundType::GrassDry),
            QStringLiteral("grass_dry"));
  EXPECT_EQ(ground_type_to_qstring(GroundType::SoilRocky),
            QStringLiteral("soil_rocky"));
  EXPECT_EQ(ground_type_to_qstring(GroundType::AlpineMix),
            QStringLiteral("alpine_mix"));
  EXPECT_EQ(ground_type_to_qstring(GroundType::SoilFertile),
            QStringLiteral("soil_fertile"));
}

TEST_F(GroundTypeTest, GroundTypeStringToEnum) {
  GroundType result;

  EXPECT_TRUE(try_parse_ground_type("forest_mud", result));
  EXPECT_EQ(result, GroundType::ForestMud);

  EXPECT_TRUE(try_parse_ground_type("grass_dry", result));
  EXPECT_EQ(result, GroundType::GrassDry);

  EXPECT_TRUE(try_parse_ground_type("soil_rocky", result));
  EXPECT_EQ(result, GroundType::SoilRocky);

  EXPECT_TRUE(try_parse_ground_type("alpine_mix", result));
  EXPECT_EQ(result, GroundType::AlpineMix);

  EXPECT_TRUE(try_parse_ground_type("soil_fertile", result));
  EXPECT_EQ(result, GroundType::SoilFertile);
}

TEST_F(GroundTypeTest, GroundTypeParsingCaseInsensitive) {
  GroundType result;

  EXPECT_TRUE(try_parse_ground_type("FOREST_MUD", result));
  EXPECT_EQ(result, GroundType::ForestMud);

  EXPECT_TRUE(try_parse_ground_type("Forest_Mud", result));
  EXPECT_EQ(result, GroundType::ForestMud);

  EXPECT_TRUE(try_parse_ground_type("  grass_dry  ", result));
  EXPECT_EQ(result, GroundType::GrassDry);
}

TEST_F(GroundTypeTest, GroundTypeParsingInvalidReturnsDefault) {
  GroundType result = GroundType::ForestMud;

  EXPECT_FALSE(try_parse_ground_type("invalid_type", result));
  EXPECT_FALSE(try_parse_ground_type("", result));
  EXPECT_FALSE(try_parse_ground_type("unknown", result));
}

TEST_F(GroundTypeTest, ApplyGroundTypeDefaultsForestMud) {
  BiomeSettings settings;
  apply_ground_type_defaults(settings, GroundType::ForestMud);

  EXPECT_EQ(settings.ground_type, GroundType::ForestMud);
  EXPECT_FLOAT_EQ(settings.grass_primary.x(), 0.30F);
  EXPECT_FLOAT_EQ(settings.grass_primary.y(), 0.60F);
  EXPECT_FLOAT_EQ(settings.grass_primary.z(), 0.28F);
  EXPECT_FLOAT_EQ(settings.soil_color.x(), 0.28F);
  EXPECT_FLOAT_EQ(settings.soil_color.y(), 0.24F);
  EXPECT_FLOAT_EQ(settings.soil_color.z(), 0.18F);
}

TEST_F(GroundTypeTest, ApplyGroundTypeDefaultsGrassDry) {
  BiomeSettings settings;
  apply_ground_type_defaults(settings, GroundType::GrassDry);

  EXPECT_EQ(settings.ground_type, GroundType::GrassDry);
  EXPECT_FLOAT_EQ(settings.grass_primary.x(), 0.58F);
  EXPECT_FLOAT_EQ(settings.grass_primary.y(), 0.54F);
  EXPECT_FLOAT_EQ(settings.grass_primary.z(), 0.32F);
  EXPECT_FLOAT_EQ(settings.terrain_ambient_boost, 1.18F);
  // Check ground-type-specific parameters
  EXPECT_FLOAT_EQ(settings.crack_intensity, 0.65F);
  EXPECT_FLOAT_EQ(settings.moisture_level, 0.15F);
  EXPECT_FLOAT_EQ(settings.grass_saturation, 0.75F);
}

TEST_F(GroundTypeTest, ApplyGroundTypeDefaultsSoilRocky) {
  BiomeSettings settings;
  apply_ground_type_defaults(settings, GroundType::SoilRocky);

  EXPECT_EQ(settings.ground_type, GroundType::SoilRocky);
  EXPECT_FLOAT_EQ(settings.soil_color.x(), 0.55F);
  EXPECT_FLOAT_EQ(settings.soil_color.y(), 0.48F);
  EXPECT_FLOAT_EQ(settings.soil_color.z(), 0.38F);
  EXPECT_FLOAT_EQ(settings.terrain_rock_detail_strength, 0.65F);
  // Check ground-type-specific parameters
  EXPECT_FLOAT_EQ(settings.rock_exposure, 0.75F);
  EXPECT_FLOAT_EQ(settings.soil_roughness, 0.85F);
}

TEST_F(GroundTypeTest, ApplyGroundTypeDefaultsAlpineMix) {
  BiomeSettings settings;
  apply_ground_type_defaults(settings, GroundType::AlpineMix);

  EXPECT_EQ(settings.ground_type, GroundType::AlpineMix);
  EXPECT_FLOAT_EQ(settings.rock_high.x(), 0.88F);
  EXPECT_FLOAT_EQ(settings.rock_high.y(), 0.90F);
  EXPECT_FLOAT_EQ(settings.rock_high.z(), 0.94F);
  EXPECT_FLOAT_EQ(settings.terrain_ambient_boost, 1.25F);
  // Check ground-type-specific parameters
  EXPECT_FLOAT_EQ(settings.snow_coverage, 0.55F);
  EXPECT_FLOAT_EQ(settings.snow_color.x(), 0.94F);
  EXPECT_FLOAT_EQ(settings.snow_color.y(), 0.96F);
  EXPECT_FLOAT_EQ(settings.snow_color.z(), 1.0F);
}

TEST_F(GroundTypeTest, ApplyGroundTypeDefaultsSoilFertile) {
  BiomeSettings settings;
  apply_ground_type_defaults(settings, GroundType::SoilFertile);

  EXPECT_EQ(settings.ground_type, GroundType::SoilFertile);
  EXPECT_FLOAT_EQ(settings.soil_color.x(), 0.20F);
  EXPECT_FLOAT_EQ(settings.soil_color.y(), 0.16F);
  EXPECT_FLOAT_EQ(settings.soil_color.z(), 0.12F);
  EXPECT_FLOAT_EQ(settings.terrain_rock_detail_strength, 0.22F);
  // Check ground-type-specific parameters
  EXPECT_FLOAT_EQ(settings.moisture_level, 0.80F);
  EXPECT_FLOAT_EQ(settings.grass_saturation, 1.15F);
  EXPECT_FLOAT_EQ(settings.rock_exposure, 0.12F);
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
  bool success =
      MapLoader::loadFromJsonFile(temp_file.fileName(), map_def, &error);

  ASSERT_TRUE(success) << "Failed to load map: " << error.toStdString();
  EXPECT_EQ(map_def.biome.ground_type, GroundType::GrassDry);
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
  bool success =
      MapLoader::loadFromJsonFile(temp_file.fileName(), map_def, &error);

  ASSERT_TRUE(success) << "Failed to load map: " << error.toStdString();
  EXPECT_EQ(map_def.biome.ground_type, GroundType::ForestMud);
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
  bool success =
      MapLoader::loadFromJsonFile(temp_file.fileName(), map_def, &error);

  ASSERT_TRUE(success) << "Failed to load map: " << error.toStdString();
  EXPECT_EQ(map_def.biome.ground_type, GroundType::AlpineMix);
  EXPECT_EQ(map_def.biome.seed, 99999U);
  // Grass primary should be the overridden values, not the alpine_mix defaults
  EXPECT_NEAR(map_def.biome.grass_primary.x(), 0.10F, 0.001F);
  EXPECT_NEAR(map_def.biome.grass_primary.y(), 0.20F, 0.001F);
  EXPECT_NEAR(map_def.biome.grass_primary.z(), 0.30F, 0.001F);
}

TEST_F(GroundTypeTest, AllGroundTypesFromString) {
  auto forest_mud = ground_type_from_string("forest_mud");
  ASSERT_TRUE(forest_mud.has_value());
  EXPECT_EQ(forest_mud.value(), GroundType::ForestMud);

  auto grass_dry = ground_type_from_string("grass_dry");
  ASSERT_TRUE(grass_dry.has_value());
  EXPECT_EQ(grass_dry.value(), GroundType::GrassDry);

  auto soil_rocky = ground_type_from_string("soil_rocky");
  ASSERT_TRUE(soil_rocky.has_value());
  EXPECT_EQ(soil_rocky.value(), GroundType::SoilRocky);

  auto alpine_mix = ground_type_from_string("alpine_mix");
  ASSERT_TRUE(alpine_mix.has_value());
  EXPECT_EQ(alpine_mix.value(), GroundType::AlpineMix);

  auto soil_fertile = ground_type_from_string("soil_fertile");
  ASSERT_TRUE(soil_fertile.has_value());
  EXPECT_EQ(soil_fertile.value(), GroundType::SoilFertile);

  auto invalid = ground_type_from_string("invalid");
  EXPECT_FALSE(invalid.has_value());
}
