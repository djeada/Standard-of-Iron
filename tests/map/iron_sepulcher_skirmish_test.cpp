#include <QVariantList>
#include <QVariantMap>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

#include "app/audio/audio_resource_loader.h"
#include "app/session/skirmish_loader.h"
#include "core/component_gameplay.h"
#include "core/event_manager.h"
#include "core/world.h"
#include "game/audio/audio_cues.h"
#include "game/audio/audio_event_handler.h"
#include "game/camera_framing.h"
#include "game/game_config.h"
#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/session/session_context.h"
#include "game/systems/capture_system.h"
#include "game/systems/default_content.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/runtime_system_registry.h"
#include "game/systems/undead_awakening_system.h"
#include "game/systems/victory_service.h"
#include "render/scene_renderer.h"
#include "scene/camera.h"

namespace {

auto victory_services() -> Game::Systems::VictoryService::Services {
  auto& session = Game::Session::SessionContext::active();
  return {.stats = session.stats(),
          .owners = session.owners(),
          .nations = session.nations(),
          .economy = session.economy()};
}

constexpr char k_map_path[] = "assets/maps/map_iron_sepulcher_watch.json";
constexpr int k_local_player_id = 1;

auto solo_player_configs() -> QVariantList {
  QVariantMap player;
  player["player_id"] = k_local_player_id;
  player["team_id"] = 1;
  player["colorHex"] = QStringLiteral("#C8322D");
  player["isHuman"] = true;
  player["nationId"] = QStringLiteral("roman_republic");
  return QVariantList{player};
}

auto zone_world_position(const Game::Map::MapDefinition& map_definition,
                         const Game::Map::UndeadZone& zone) -> QVector3D {
  const float half_width = static_cast<float>(map_definition.grid.width) * 0.5F - 0.5F;
  const float half_height =
      static_cast<float>(map_definition.grid.height) * 0.5F - 0.5F;
  return {(zone.x - half_width) * map_definition.grid.tile_size,
          0.0F,
          (zone.z - half_height) * map_definition.grid.tile_size};
}

auto find_zone(const Game::Map::MapDefinition& map_definition,
               const QString& zone_id) -> const Game::Map::UndeadZone* {
  for (const auto& zone : map_definition.undead_zones) {
    if (zone.id == zone_id) {
      return &zone;
    }
  }
  return nullptr;
}

auto living_guardians_of_owner(Engine::Core::World& world, int owner_id)
    -> std::vector<Engine::Core::UnitComponent*> {
  std::vector<Engine::Core::UnitComponent*> units;
  for (auto* entity : world.collect_entities_with<Engine::Core::UnitComponent>()) {
    auto* unit = entity != nullptr
                     ? entity->get_component<Engine::Core::UnitComponent>()
                     : nullptr;
    if (unit != nullptr && unit->owner_id == owner_id && unit->health > 0 &&
        Game::Units::is_troop_spawn(unit->spawn_type)) {
      units.push_back(unit);
    }
  }
  return units;
}

auto find_local_commander(Engine::Core::World& world) -> Engine::Core::Entity* {
  for (auto* entity : world.collect_entities_with<Engine::Core::UnitComponent>()) {
    if (entity == nullptr ||
        entity->get_component<Engine::Core::CommanderComponent>() == nullptr) {
      continue;
    }
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit != nullptr && unit->owner_id == k_local_player_id && unit->health > 0) {
      return entity;
    }
  }
  return nullptr;
}

class IronSepulcherSkirmishTest : public ::testing::Test {
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
};

} // namespace

TEST_F(IronSepulcherSkirmishTest, SoloSkirmishAwakensAndIsWonByPurifyingTheShrine) {
  Engine::Core::World world;
  Game::Systems::register_runtime_systems(world);

  Render::GL::Renderer renderer(Render::ShaderQuality::None);
  Render::GL::Camera camera;
  App::Core::SkirmishLoader loader(world, renderer, camera);

  int selected_player_id = k_local_player_id;
  const auto load_result = loader.start(QString::fromLatin1(k_map_path),
                                        solo_player_configs(),
                                        k_local_player_id,
                                        true,
                                        selected_player_id);
  ASSERT_TRUE(load_result.ok) << load_result.error_message.toStdString();
  EXPECT_FALSE(load_result.is_spectator_mode);

  Game::Map::MapDefinition map_definition;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      QString::fromLatin1(k_map_path), map_definition, &error))
      << error.toStdString();

  auto* undead = world.get_system<Game::Systems::UndeadAwakeningSystem>();
  ASSERT_NE(undead, nullptr);
  undead->configure(map_definition);

  Game::Systems::VictoryService victory(victory_services());
  victory.configure(load_result.victory_config, selected_player_id);
  victory.set_undead_zone_query(undead);

  const auto* shrine = find_zone(map_definition, QStringLiteral("shrine_sentinels"));
  ASSERT_NE(shrine, nullptr);

  EXPECT_TRUE(Game::Systems::OwnerRegistry::instance().are_enemies(k_local_player_id,
                                                                   shrine->owner_id));

  undead->update(&world, 0.1F);
  victory.update(world, 0.4F);
  EXPECT_TRUE(living_guardians_of_owner(world, shrine->owner_id).empty())
      << "the sepulcher must stay dormant until a living unit walks in";
  EXPECT_FALSE(victory.is_game_over());

  auto* commander = find_local_commander(world);
  ASSERT_NE(commander, nullptr);
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);
  const QVector3D shrine_world = zone_world_position(map_definition, *shrine);
  transform->position.x = shrine_world.x();
  transform->position.z = shrine_world.z();

  undead->update(&world, 0.1F);
  auto guardians = living_guardians_of_owner(world, shrine->owner_id);
  ASSERT_FALSE(guardians.empty()) << "entering the shrine radius must wake the zone";
  for (auto* guardian : guardians) {
    EXPECT_EQ(guardian->nation_id, Game::Systems::NationID::IronSepulcher);
  }

  victory.update(world, 0.1F);
  EXPECT_FALSE(victory.is_game_over())
      << "the shrine is not purified while its guardians live";

  for (auto* guardian : guardians) {
    guardian->health = 0;
  }
  undead->update(&world, 0.1F);
  EXPECT_TRUE(undead->is_shrine_purified(QStringLiteral("shrine_sentinels")));

  victory.update(world, 0.1F);
  EXPECT_EQ(victory.get_victory_state(), QStringLiteral("victory"));
}

TEST_F(IronSepulcherSkirmishTest, WakingTheSepulcherAnnouncesItselfWithSoundAndMusic) {
  Engine::Core::World world;
  Game::Systems::register_runtime_systems(world);

  Render::GL::Renderer renderer(Render::ShaderQuality::None);
  Render::GL::Camera camera;
  App::Core::SkirmishLoader loader(world, renderer, camera);

  int selected_player_id = k_local_player_id;
  const auto load_result = loader.start(QString::fromLatin1(k_map_path),
                                        solo_player_configs(),
                                        k_local_player_id,
                                        true,
                                        selected_player_id);
  ASSERT_TRUE(load_result.ok) << load_result.error_message.toStdString();

  Game::Map::MapDefinition map_definition;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      QString::fromLatin1(k_map_path), map_definition, &error))
      << error.toStdString();

  auto* undead = world.get_system<Game::Systems::UndeadAwakeningSystem>();
  ASSERT_NE(undead, nullptr);
  undead->configure(map_definition);

  QStringList cues;
  QStringList music;
  const Engine::Core::ScopedEventSubscription<Engine::Core::AudioCueEvent> cue_sub(
      [&cues](const Engine::Core::AudioCueEvent& event) {
        cues.append(QString::fromStdString(event.cue_id));
      });
  const Engine::Core::ScopedEventSubscription<Engine::Core::MusicTriggerEvent>
      music_sub([&music](const Engine::Core::MusicTriggerEvent& event) {
        music.append(QString::fromStdString(event.music_id));
      });

  const auto* shrine = find_zone(map_definition, QStringLiteral("shrine_sentinels"));
  ASSERT_NE(shrine, nullptr);

  undead->update(&world, 0.1F);
  EXPECT_FALSE(cues.contains(QStringLiteral("alert.undead_awakening")))
      << "a dormant sepulcher must stay quiet";

  auto* commander = find_local_commander(world);
  ASSERT_NE(commander, nullptr);
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);
  const QVector3D shrine_world = zone_world_position(map_definition, *shrine);
  transform->position.x = shrine_world.x();
  transform->position.z = shrine_world.z();

  undead->update(&world, 0.1F);
  ASSERT_FALSE(living_guardians_of_owner(world, shrine->owner_id).empty())
      << "the commander walking in must wake the zone";

  EXPECT_TRUE(cues.contains(QStringLiteral("alert.undead_awakening")))
      << "the dead rose in silence; cues seen: " << cues.join(", ").toStdString();

  for (int tick = 0; tick < 10; ++tick) {
    undead->update(&world, 0.1F);
  }
  EXPECT_TRUE(music.contains(QStringLiteral("music.event.skeletons_awaken")))
      << "standing in a woken sepulcher played no music; music seen: "
      << music.join(", ").toStdString();

  music.clear();
  int stops = 0;
  const Engine::Core::ScopedEventSubscription<Engine::Core::MusicStopEvent> stop_sub(
      [&stops](const Engine::Core::MusicStopEvent&) { ++stops; });
  transform->position.x = shrine_world.x() + 1000.0F;
  transform->position.z = shrine_world.z() + 1000.0F;
  for (int tick = 0; tick < 10; ++tick) {
    undead->update(&world, 0.1F);
  }
  EXPECT_GT(stops, 0) << "the awakening music followed the player out of the zone";

  auto& audio = AudioSystem::get_instance();
  audio.shutdown();
  if (!audio.initialize()) {
    GTEST_SKIP() << "Audio backend is unavailable in this environment";
  }
  audio.set_master_volume(1.0F);
  audio.set_sound_volume(1.0F);
  AudioResourceLoader::load_audio_resources(AudioLoadPolicy::Startup);
  AudioResourceLoader::load_audio_resources(AudioLoadPolicy::Mission);
  AudioResourceLoader::load_audio_cues();

  Game::Audio::AudioEventHandler handler(&world);
  ASSERT_TRUE(handler.initialize());

  Engine::Core::EventManager::instance().publish(
      Engine::Core::AudioCueEvent("alert.undead_awakening"));

  bool opened = false;
  for (int settle = 0; settle < 100 && !opened; ++settle) {
    opened = audio.get_active_channel_count() > 0;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  EXPECT_TRUE(opened)
      << "the awakening cue was published and bound, but nothing reached the mixer";

  handler.shutdown();
  Game::Audio::CueRegistry::instance().clear();
  audio.shutdown();
}

TEST_F(IronSepulcherSkirmishTest, EveryZoneOnTheMapOwnsAShrineBarracks) {
  Engine::Core::World world;
  Game::Systems::register_runtime_systems(world);

  Render::GL::Renderer renderer(Render::ShaderQuality::None);
  Render::GL::Camera camera;
  App::Core::SkirmishLoader loader(world, renderer, camera);

  int selected_player_id = k_local_player_id;
  const auto load_result = loader.start(QString::fromLatin1(k_map_path),
                                        solo_player_configs(),
                                        k_local_player_id,
                                        true,
                                        selected_player_id);
  ASSERT_TRUE(load_result.ok) << load_result.error_message.toStdString();

  Game::Map::MapDefinition map_definition;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      QString::fromLatin1(k_map_path), map_definition, &error))
      << error.toStdString();
  ASSERT_GE(map_definition.undead_zones.size(), 2U)
      << "this map is the fixture for a multi-zone sepulcher";

  auto* undead = world.get_system<Game::Systems::UndeadAwakeningSystem>();
  ASSERT_NE(undead, nullptr);
  undead->configure(map_definition);
  undead->update(&world, 0.1F);

  EXPECT_TRUE(undead->zones_without_shrine().empty());

  std::vector<std::uint64_t> shrine_prop_ids;
  for (const auto& zone : map_definition.undead_zones) {
    EXPECT_TRUE(undead->has_shrine(zone.id)) << zone.id.toStdString();

    const Engine::Core::EntityID anchor_id = undead->anchor_entity(zone.id);
    ASSERT_NE(anchor_id, 0U) << zone.id.toStdString();
    auto* anchor = world.get_entity(anchor_id);
    ASSERT_NE(anchor, nullptr);
    auto* unit = anchor->get_component<Engine::Core::UnitComponent>();
    ASSERT_NE(unit, nullptr);
    EXPECT_EQ(unit->spawn_type, Game::Units::SpawnType::Barracks);
    EXPECT_EQ(unit->owner_id, zone.owner_id);
    EXPECT_EQ(unit->nation_id, Game::Systems::NationID::IronSepulcher);
    EXPECT_EQ(anchor->get_component<Engine::Core::ProductionComponent>(), nullptr)
        << zone.id.toStdString() << " must be captured, not recruited from";

    const QVector3D zone_world = zone_world_position(map_definition, zone);
    const QVector3D shrine = undead->shrine_world_position(zone.id);
    const float dx = shrine.x() - zone_world.x();
    const float dz = shrine.z() - zone_world.z();
    EXPECT_LE(std::sqrt(dx * dx + dz * dz), zone.radius)
        << zone.id.toStdString() << " put its shrine outside its own zone";

    shrine_prop_ids.push_back(undead->shrine_prop_id(zone.id));
  }

  std::sort(shrine_prop_ids.begin(), shrine_prop_ids.end());
  EXPECT_EQ(std::unique(shrine_prop_ids.begin(), shrine_prop_ids.end()),
            shrine_prop_ids.end())
      << "two zones cannot share one shrine";

  int shrine_props = 0;
  for (const auto& prop : Game::Map::TerrainService::instance().world_props()) {
    if (prop.type == Game::Map::WorldProp::Type::MagicShrine) {
      ++shrine_props;
    }
  }
  EXPECT_EQ(shrine_props, static_cast<int>(map_definition.undead_zones.size()));
}

TEST_F(IronSepulcherSkirmishTest, DormantSepulcherDoesNotHandTheSkirmishAnEarlyWin) {
  Engine::Core::World world;
  Game::Systems::register_runtime_systems(world);

  Render::GL::Renderer renderer(Render::ShaderQuality::None);
  Render::GL::Camera camera;
  App::Core::SkirmishLoader loader(world, renderer, camera);

  int selected_player_id = k_local_player_id;
  const auto load_result = loader.start(QString::fromLatin1(k_map_path),
                                        solo_player_configs(),
                                        k_local_player_id,
                                        true,
                                        selected_player_id);
  ASSERT_TRUE(load_result.ok) << load_result.error_message.toStdString();

  Game::Map::MapDefinition map_definition;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      QString::fromLatin1(k_map_path), map_definition, &error))
      << error.toStdString();

  auto* undead = world.get_system<Game::Systems::UndeadAwakeningSystem>();
  ASSERT_NE(undead, nullptr);
  undead->configure(map_definition);

  Game::Systems::VictoryService victory(victory_services());
  victory.configure(load_result.victory_config, selected_player_id);
  victory.set_undead_zone_query(undead);

  for (int tick = 0; tick < 20; ++tick) {
    undead->update(&world, 0.1F);
    victory.update(world, 0.1F);
  }

  EXPECT_FALSE(victory.is_game_over())
      << "an untouched sepulcher must neither win nor lose the match outright";
}

TEST_F(IronSepulcherSkirmishTest, RazingTheShrineBarracksDestroysItsGarrison) {
  Engine::Core::World world;
  Game::Systems::register_runtime_systems(world);

  Render::GL::Renderer renderer(Render::ShaderQuality::None);
  Render::GL::Camera camera;
  App::Core::SkirmishLoader loader(world, renderer, camera);

  int selected_player_id = k_local_player_id;
  const auto load_result = loader.start(QString::fromLatin1(k_map_path),
                                        solo_player_configs(),
                                        k_local_player_id,
                                        true,
                                        selected_player_id);
  ASSERT_TRUE(load_result.ok) << load_result.error_message.toStdString();

  Game::Map::MapDefinition map_definition;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      QString::fromLatin1(k_map_path), map_definition, &error))
      << error.toStdString();

  auto* undead = world.get_system<Game::Systems::UndeadAwakeningSystem>();
  ASSERT_NE(undead, nullptr);
  undead->configure(map_definition);

  Game::Systems::VictoryService victory(victory_services());
  victory.configure(load_result.victory_config, selected_player_id);
  victory.set_undead_zone_query(undead);

  const auto* shrine = find_zone(map_definition, QStringLiteral("shrine_sentinels"));
  ASSERT_NE(shrine, nullptr);

  auto* commander = find_local_commander(world);
  ASSERT_NE(commander, nullptr);
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);
  const QVector3D shrine_world = zone_world_position(map_definition, *shrine);
  transform->position.x = shrine_world.x();
  transform->position.z = shrine_world.z();

  undead->update(&world, 0.1F);
  ASSERT_FALSE(living_guardians_of_owner(world, shrine->owner_id).empty());

  const Engine::Core::EntityID anchor_id =
      undead->anchor_entity(QStringLiteral("shrine_sentinels"));
  ASSERT_NE(anchor_id, 0U);
  auto* anchor = world.get_entity(anchor_id);
  ASSERT_NE(anchor, nullptr);
  auto* anchor_unit = anchor->get_component<Engine::Core::UnitComponent>();
  ASSERT_NE(anchor_unit, nullptr);
  EXPECT_EQ(anchor_unit->spawn_type, Game::Units::SpawnType::Barracks);
  EXPECT_EQ(anchor_unit->owner_id, shrine->owner_id);
  EXPECT_EQ(anchor_unit->nation_id, Game::Systems::NationID::IronSepulcher);
  EXPECT_EQ(anchor->get_component<Engine::Core::ProductionComponent>(), nullptr)
      << "the shrine is a barracks only so it can be taken or razed";

  anchor_unit->health = 0;
  undead->update(&world, 0.1F);

  EXPECT_TRUE(living_guardians_of_owner(world, shrine->owner_id).empty())
      << "razing the shrine must put its risen garrison down";
  EXPECT_TRUE(undead->is_shrine_purified(QStringLiteral("shrine_sentinels")));

  victory.update(world, 0.4F);
  EXPECT_EQ(victory.get_victory_state(), QStringLiteral("victory"));
}

TEST_F(IronSepulcherSkirmishTest, ShrineFlagOnlyFallsBetweenWaves) {
  Engine::Core::World world;
  Game::Systems::register_runtime_systems(world);

  Render::GL::Renderer renderer(Render::ShaderQuality::None);
  Render::GL::Camera camera;
  App::Core::SkirmishLoader loader(world, renderer, camera);

  int selected_player_id = k_local_player_id;
  const auto load_result = loader.start(QString::fromLatin1(k_map_path),
                                        solo_player_configs(),
                                        k_local_player_id,
                                        true,
                                        selected_player_id);
  ASSERT_TRUE(load_result.ok) << load_result.error_message.toStdString();

  Game::Map::MapDefinition map_definition;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      QString::fromLatin1(k_map_path), map_definition, &error))
      << error.toStdString();

  auto* undead = world.get_system<Game::Systems::UndeadAwakeningSystem>();
  ASSERT_NE(undead, nullptr);
  undead->configure(map_definition);

  const auto* shrine = find_zone(map_definition, QStringLiteral("shrine_sentinels"));
  ASSERT_NE(shrine, nullptr);

  auto* commander = find_local_commander(world);
  ASSERT_NE(commander, nullptr);
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);
  const QVector3D shrine_world = zone_world_position(map_definition, *shrine);
  transform->position.x = shrine_world.x();
  transform->position.z = shrine_world.z();

  undead->update(&world, 0.1F);
  ASSERT_FALSE(living_guardians_of_owner(world, shrine->owner_id).empty());

  auto* anchor =
      world.get_entity(undead->anchor_entity(QStringLiteral("shrine_sentinels")));
  ASSERT_NE(anchor, nullptr);
  auto* anchor_unit = anchor->get_component<Engine::Core::UnitComponent>();
  ASSERT_NE(anchor_unit, nullptr);

  Game::Systems::CaptureSystem capture_system;
  for (int tick = 0; tick < 40; ++tick) {
    undead->update(&world, 0.25F);
    capture_system.update(&world, 0.25F);
  }

  auto* capture = anchor->get_component<Engine::Core::CaptureComponent>();
  ASSERT_NE(capture, nullptr);
  EXPECT_FALSE(capture->is_being_captured)
      << "the flag must hold while the sepulcher's guardians still stand";
  EXPECT_FLOAT_EQ(capture->capture_progress, 0.0F);
  EXPECT_EQ(anchor_unit->owner_id, shrine->owner_id);

  for (auto* guardian : living_guardians_of_owner(world, shrine->owner_id)) {
    guardian->health = 0;
  }
  undead->update(&world, 0.25F);
  EXPECT_FALSE(capture->capture_blocked);

  capture_system.update(&world, 0.25F);
  EXPECT_TRUE(capture->is_being_captured)
      << "with the guardians down the flag starts coming off the pole";
  EXPECT_GT(capture->capture_progress, 0.0F);
}

TEST_F(IronSepulcherSkirmishTest, CapturingTheShrineBarracksDestroysItsGarrison) {
  Engine::Core::World world;
  Game::Systems::register_runtime_systems(world);

  Render::GL::Renderer renderer(Render::ShaderQuality::None);
  Render::GL::Camera camera;
  App::Core::SkirmishLoader loader(world, renderer, camera);

  int selected_player_id = k_local_player_id;
  const auto load_result = loader.start(QString::fromLatin1(k_map_path),
                                        solo_player_configs(),
                                        k_local_player_id,
                                        true,
                                        selected_player_id);
  ASSERT_TRUE(load_result.ok) << load_result.error_message.toStdString();

  Game::Map::MapDefinition map_definition;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      QString::fromLatin1(k_map_path), map_definition, &error))
      << error.toStdString();

  auto* undead = world.get_system<Game::Systems::UndeadAwakeningSystem>();
  ASSERT_NE(undead, nullptr);
  undead->configure(map_definition);

  const auto* shrine = find_zone(map_definition, QStringLiteral("shrine_sentinels"));
  ASSERT_NE(shrine, nullptr);

  auto* commander = find_local_commander(world);
  ASSERT_NE(commander, nullptr);
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);
  const QVector3D shrine_world = zone_world_position(map_definition, *shrine);
  transform->position.x = shrine_world.x();
  transform->position.z = shrine_world.z();

  undead->update(&world, 0.1F);
  ASSERT_FALSE(living_guardians_of_owner(world, shrine->owner_id).empty());

  auto* anchor =
      world.get_entity(undead->anchor_entity(QStringLiteral("shrine_sentinels")));
  ASSERT_NE(anchor, nullptr);
  anchor->get_component<Engine::Core::UnitComponent>()->owner_id = k_local_player_id;

  undead->update(&world, 0.1F);

  EXPECT_TRUE(living_guardians_of_owner(world, shrine->owner_id).empty())
      << "capturing the shrine must put its risen garrison down";
  EXPECT_TRUE(undead->is_shrine_purified(QStringLiteral("shrine_sentinels")));
}

TEST_F(IronSepulcherSkirmishTest, TheOpeningShotIsFramedByTheMapNotByAFlatDefault) {
  Engine::Core::World world;
  Game::Systems::register_runtime_systems(world);

  Render::GL::Renderer renderer(Render::ShaderQuality::None);
  Render::GL::Camera camera;
  App::Core::SkirmishLoader loader(world, renderer, camera);

  int selected_player_id = k_local_player_id;
  const auto load_result = loader.start(QString::fromLatin1(k_map_path),
                                        solo_player_configs(),
                                        k_local_player_id,
                                        true,
                                        selected_player_id);
  ASSERT_TRUE(load_result.ok) << load_result.error_message.toStdString();
  ASSERT_TRUE(load_result.has_focus_position)
      << "the load must find the camp it frames";

  Game::Map::MapDefinition map_definition;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      QString::fromLatin1(k_map_path), map_definition, &error))
      << error.toStdString();

  const auto expected = Game::reset_framing({.distance = map_definition.camera.distance,
                                             .pitch = map_definition.camera.tilt_deg,
                                             .yaw = map_definition.camera.yaw_deg});

  EXPECT_NEAR(camera.get_distance(), expected.distance, 0.01F);
  EXPECT_NEAR(camera.get_distance(),
              Game::GameConfig::instance().camera_reset_framing().distance,
              0.01F)
      << "a reset must land on the framing the battle opened with";
  EXPECT_GT(camera.get_distance(),
            Game::GameConfig::instance().get_camera_default_distance())
      << "the old flat 12-unit default put the camera inside the camp";
}
