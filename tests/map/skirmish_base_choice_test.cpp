#include <QVariantList>
#include <QVariantMap>
#include <QVector3D>

#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <vector>

#include "app/session/skirmish_loader.h"
#include "core/component_core.h"
#include "core/world.h"
#include "game/map/base_options.h"
#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/systems/default_content.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/runtime_system_registry.h"
#include "render/scene_renderer.h"
#include "scene/camera.h"

namespace {

constexpr char k_map_path[] = "assets/maps/map_rivers.json";
constexpr int k_local_player_id = 1;
constexpr char k_chosen_base[] = "north_toll_barracks";
constexpr char k_authored_base[] = "p1_barracks";

auto player_configs(const QString& local_base_key) -> QVariantList {
  QVariantMap human;
  human["player_id"] = k_local_player_id;
  human["team_id"] = 1;
  human["colorHex"] = QStringLiteral("#C8322D");
  human["isHuman"] = true;
  human["nationId"] = QStringLiteral("roman_republic");
  human["baseKey"] = local_base_key;

  QVariantMap cpu;
  cpu["player_id"] = 2;
  cpu["team_id"] = 2;
  cpu["colorHex"] = QStringLiteral("#2D5FC8");
  cpu["isHuman"] = false;
  cpu["nationId"] = QStringLiteral("carthaginian_empire");
  cpu["baseKey"] = QStringLiteral("p2_barracks");

  return QVariantList{human, cpu};
}

auto base_position(const Game::Map::MapDefinition& def,
                   const QString& key) -> QVector3D {
  for (const auto& option : Game::Map::collect_base_options(def)) {
    if (option.key == key) {
      return option.position;
    }
  }
  ADD_FAILURE() << "the map no longer authors " << key.toStdString();
  return {};
}

auto flat_distance(const QVector3D& lhs, const QVector3D& rhs) -> float {
  return std::hypot(lhs.x() - rhs.x(), lhs.z() - rhs.z());
}

struct OwnedEntity {
  Game::Units::SpawnType spawn_type = Game::Units::SpawnType::Archer;
  QVector3D position;
};

auto entities_owned_by(Engine::Core::World& world,
                       int owner_id) -> std::vector<OwnedEntity> {
  std::vector<OwnedEntity> owned;
  for (auto* entity : world.collect_entities_with<Engine::Core::UnitComponent>()) {
    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (unit == nullptr || transform == nullptr || unit->owner_id != owner_id) {
      continue;
    }
    owned.push_back(
        {.spawn_type = unit->spawn_type,
         .position = QVector3D(transform->position.x, 0.0F, transform->position.z)});
  }
  return owned;
}

class SkirmishBaseChoiceTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto& nations = Game::Systems::NationRegistry::instance();
    nations.clear();
    Game::Systems::initialize_default_content(nations);
    Game::Systems::OwnerRegistry::instance().clear();
    Game::Systems::GlobalStatsRegistry::instance().clear();
  }

  void TearDown() override {
    Game::Map::TerrainService::instance().clear();
    Game::Map::VisibilityService::instance().reset();
    Game::Systems::GlobalStatsRegistry::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::OwnerRegistry::instance().clear();
  }

  auto load(const QString& local_base_key) -> App::Core::SkirmishLoadResult {
    Game::Systems::register_runtime_systems(m_world);
    int selected_player_id = k_local_player_id;
    return m_loader.start(QString::fromLatin1(k_map_path),
                          player_configs(local_base_key),
                          k_local_player_id,
                          true,
                          selected_player_id);
  }

  Engine::Core::World m_world;
  Render::GL::Renderer m_renderer{Render::ShaderQuality::None};
  Render::GL::Camera m_camera;
  App::Core::SkirmishLoader m_loader{m_world, m_renderer, m_camera};
};

} // namespace

TEST_F(SkirmishBaseChoiceTest, TheChosenBaseIsTheOnlyOneTheLocalPlayerStartsWith) {
  Game::Map::MapDefinition def;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      QString::fromLatin1(k_map_path), def, &error))
      << error.toStdString();
  const QVector3D chosen = base_position(def, QString::fromLatin1(k_chosen_base));

  const auto result = load(QString::fromLatin1(k_chosen_base));
  ASSERT_TRUE(result.ok) << result.error_message.toStdString();

  std::vector<QVector3D> barracks;
  for (const auto& owned : entities_owned_by(m_world, k_local_player_id)) {
    if (owned.spawn_type == Game::Units::SpawnType::Barracks) {
      barracks.push_back(owned.position);
    }
  }

  ASSERT_EQ(barracks.size(), 1U) << "a player must field exactly one starting barracks";
  EXPECT_LT(flat_distance(barracks.front(), chosen), 0.01F)
      << "the match did not start at the base the setup screen picked";
}

TEST_F(SkirmishBaseChoiceTest, TheAbandonedCampIsLeftAsCapturableAsAnyOtherOutpost) {
  Game::Map::MapDefinition def;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      QString::fromLatin1(k_map_path), def, &error))
      << error.toStdString();
  const QVector3D authored = base_position(def, QString::fromLatin1(k_authored_base));
  const QVector3D untouched_outpost =
      base_position(def, QStringLiteral("south_toll_barracks"));

  const auto result = load(QString::fromLatin1(k_chosen_base));
  ASSERT_TRUE(result.ok) << result.error_message.toStdString();

  int abandoned_owner = std::numeric_limits<int>::min();
  int outpost_owner = std::numeric_limits<int>::max();
  for (auto* entity : m_world.collect_entities_with<Engine::Core::UnitComponent>()) {
    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (unit == nullptr || transform == nullptr ||
        unit->spawn_type != Game::Units::SpawnType::Barracks) {
      continue;
    }
    const QVector3D position(transform->position.x, 0.0F, transform->position.z);
    if (flat_distance(position, authored) < 0.01F) {
      abandoned_owner = unit->owner_id;
    }
    if (flat_distance(position, untouched_outpost) < 0.01F) {
      outpost_owner = unit->owner_id;
    }
  }

  EXPECT_NE(abandoned_owner, std::numeric_limits<int>::min())
      << "the camp the player left behind vanished from the map";
  EXPECT_EQ(abandoned_owner, outpost_owner)
      << "the camp the player left behind should sit there as a prize, exactly "
         "like the outposts the map authored as neutral";
}

TEST_F(SkirmishBaseChoiceTest, TheStartingRetinueMovesToTheChosenBase) {
  Game::Map::MapDefinition def;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      QString::fromLatin1(k_map_path), def, &error))
      << error.toStdString();
  const QVector3D chosen = base_position(def, QString::fromLatin1(k_chosen_base));
  const QVector3D authored = base_position(def, QString::fromLatin1(k_authored_base));
  ASSERT_GT(flat_distance(chosen, authored), 50.0F)
      << "this check only means something if the two camps are far apart";

  const auto result = load(QString::fromLatin1(k_chosen_base));
  ASSERT_TRUE(result.ok) << result.error_message.toStdString();

  int retinue = 0;
  for (const auto& owned : entities_owned_by(m_world, k_local_player_id)) {
    if (owned.spawn_type == Game::Units::SpawnType::Barracks) {
      continue;
    }
    ++retinue;
    EXPECT_LT(flat_distance(owned.position, chosen), 20.0F)
        << "a starting unit was stranded at the camp its player left";
  }
  EXPECT_GT(retinue, 0) << "the map authors starting units that must be checked";
}

TEST_F(SkirmishBaseChoiceTest, TheOpeningCameraFramesTheChosenBase) {
  Game::Map::MapDefinition def;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      QString::fromLatin1(k_map_path), def, &error))
      << error.toStdString();
  const QVector3D chosen = base_position(def, QString::fromLatin1(k_chosen_base));

  const auto result = load(QString::fromLatin1(k_chosen_base));
  ASSERT_TRUE(result.ok) << result.error_message.toStdString();
  ASSERT_TRUE(result.has_focus_position);

  EXPECT_LT(flat_distance(result.focus_position, chosen), 0.01F)
      << "the match opened looking at a camp the player does not hold";
}

TEST_F(SkirmishBaseChoiceTest, LeavingTheBaseUnsetKeepsTheMapsOwnSeating) {
  Game::Map::MapDefinition def;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      QString::fromLatin1(k_map_path), def, &error))
      << error.toStdString();
  const QVector3D authored = base_position(def, QString::fromLatin1(k_authored_base));

  const auto result = load(QString());
  ASSERT_TRUE(result.ok) << result.error_message.toStdString();

  std::vector<QVector3D> barracks;
  for (const auto& owned : entities_owned_by(m_world, k_local_player_id)) {
    if (owned.spawn_type == Game::Units::SpawnType::Barracks) {
      barracks.push_back(owned.position);
    }
  }

  ASSERT_EQ(barracks.size(), 1U);
  EXPECT_LT(flat_distance(barracks.front(), authored), 0.01F)
      << "a match with no base choice must play exactly as it did before";
}
