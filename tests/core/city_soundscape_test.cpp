#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <numbers>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "app/audio/audio_coordinator.h"
#include "app/audio/audio_resource_loader.h"
#include "app/audio/city_soundscape.h"
#include "app/commander/commander_camera_rig.h"
#include "app/commander/commander_presentation.h"
#include "game/audio/audio_clock.h"
#include "game/audio/audio_cues.h"
#include "game/audio/audio_event_handler.h"
#include "game/audio/audio_system.h"
#include "game/audio/cue_ids.h"
#include "game/audio/cue_trace.h"
#include "game/core/component_commander.h"
#include "game/core/component_economy.h"
#include "game/core/component_gameplay.h"
#include "game/core/component_structures.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/combat_actions/combat_action_definition.h"
#include "game/systems/combat_system/combat_action_processor.h"
#include "game/systems/default_content.h"
#include "game/systems/nation_registry.h"
#include "game/systems/navigation/nav_grid.h"
#include "game/units/spawn_type.h"
#include "game/wildlife/wildlife_config.h"

namespace {

using Game::Audio::CitySoundscape;
using Game::Audio::CueTrace;
using Game::Audio::FootstepSurface;

struct RenderedClockScope {
  RenderedClockScope() { Game::Audio::Clock::use_rendered_time(true); }
  ~RenderedClockScope() { Game::Audio::Clock::use_rendered_time(false); }
  RenderedClockScope(const RenderedClockScope&) = delete;
  auto operator=(const RenderedClockScope&) -> RenderedClockScope& = delete;
};

auto load_walk_points(const QString& json_path)
    -> std::vector<std::pair<float, float>> {
  std::vector<std::pair<float, float>> out;
  QFile file(json_path);
  if (!file.open(QIODevice::ReadOnly)) {
    return out;
  }
  const auto doc = QJsonDocument::fromJson(file.readAll());
  const auto pts = doc.object().value(QStringLiteral("points")).toArray();
  out.reserve(static_cast<std::size_t>(pts.size()));
  for (const auto& entry : pts) {
    const auto arr = entry.toArray();
    if (arr.size() >= 2) {
      out.emplace_back(static_cast<float>(arr.at(0).toDouble()),
                       static_cast<float>(arr.at(1).toDouble()));
    }
  }
  return out;
}

auto contains_bed(const std::vector<std::string>& beds, const std::string& id) -> bool {
  return std::find(beds.begin(), beds.end(), id) != beds.end();
}

class CitySoundscapeTest : public ::testing::Test {
protected:
  static void SetUpTestSuite() {
    qputenv("SOI_AUDIO_OFFLINE", "1");
    auto& audio = AudioSystem::get_instance();
    audio.shutdown();
    s_audio_ready = audio.initialize();
    if (!s_audio_ready) {
      return;
    }
    audio.set_master_volume(1.0F);
    audio.set_sound_volume(1.0F);
    AudioResourceLoader::load_audio_resources(AudioLoadPolicy::Startup);
    AudioResourceLoader::load_audio_resources(AudioLoadPolicy::Mission);
    AudioResourceLoader::load_audio_cues();
  }

  static void TearDownTestSuite() {
    CitySoundscape::instance().stop();
    Game::Audio::CueRegistry::instance().clear();
    AudioSystem::get_instance().shutdown();
    s_audio_ready = false;
  }

  void SetUp() override {
    if (!s_audio_ready) {
      GTEST_SKIP() << "Audio backend unavailable";
    }
    CitySoundscape::instance().stop();
    CueTrace::instance().reset();
    AudioSystem::get_instance().set_listener({.position = {0.0F, 1.7F, 0.0F},
                                              .right_x = 1.0F,
                                              .right_z = 0.0F,
                                              .valid = true});
  }

  void TearDown() override {
    CitySoundscape::instance().stop();
    AudioSystem::get_instance().set_listener({});
  }

  static void set_listener_xz(float x, float z) {
    AudioSystem::get_instance().set_listener(
        {.position = {x, 1.7F, z}, .right_x = 1.0F, .right_z = 0.0F, .valid = true});
    CueTrace::instance().set_listener({.x = x, .y = 1.7F, .z = z, .mode = "commander"});
  }

  static bool s_audio_ready;
};

bool CitySoundscapeTest::s_audio_ready = false;

TEST_F(CitySoundscapeTest, MapLoaderReadsCircleAndPolygonAmbienceZones) {
  Game::Map::MapDefinition rome;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      QStringLiteral("assets/maps/map_rome.json"), rome, &error))
      << error.toStdString();
  EXPECT_GE(rome.ambience_zones.size(), 10U);

  bool saw_circle = false;
  bool saw_polygon = false;
  for (const auto& zone : rome.ambience_zones) {
    if (zone.bed == QStringLiteral("ambient.quay_work") && zone.polygon.empty()) {
      saw_circle = true;
      EXPECT_GT(zone.radius, 10.0F);
      EXPECT_GT(zone.fade, 0.0F);
    }
    if (zone.bed == QStringLiteral("ambient.arena_interior") &&
        zone.polygon.size() >= 8U) {
      saw_polygon = true;
      EXPECT_GT(zone.fade, 0.0F);
    }
  }
  EXPECT_TRUE(saw_circle) << "Rome should include circle ambience zones";
  EXPECT_TRUE(saw_polygon) << "Rome Colosseum bowl should be a polygon ambience zone";

  Game::Map::MapDefinition carthage;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      QStringLiteral("assets/maps/map_carthage.json"), carthage, &error))
      << error.toStdString();
  EXPECT_GE(carthage.ambience_zones.size(), 8U);
}

TEST_F(CitySoundscapeTest, RomeWalkCrossfadesDistinctDistrictZonesSmoothly) {
  Game::Map::MapDefinition rome;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      QStringLiteral("assets/maps/map_rome.json"), rome, &error))
      << error.toStdString();
  const auto walk =
      load_walk_points(QStringLiteral("tools/arena/promos/city_film/rome_walk.json"));
  ASSERT_GE(walk.size(), 10U);

  auto& scape = CitySoundscape::instance();
  scape.configure(&rome,
                  QStringLiteral("ambient.mediterranean_city_market"),
                  QStringLiteral("roman"));
  EXPECT_GT(scape.zone_count(), rome.ambience_zones.size());

  std::set<std::string> heard_along_walk;
  std::size_t max_simultaneous_beds = 0;

  set_listener_xz(walk.front().first, walk.front().second);
  for (int warm = 0; warm < 8; ++warm) {
    scape.update(0.25F, nullptr, 0.0F);
  }
  EXPECT_TRUE(contains_bed(scape.active_bed_ids(), "ambient.quay_work"));

  for (std::size_t seg = 0; seg + 1 < walk.size(); ++seg) {
    const auto [x0, z0] = walk[seg];
    const auto [x1, z1] = walk[seg + 1];
    const float dist = std::hypot(x1 - x0, z1 - z0);
    const int steps = std::max(1, static_cast<int>(std::ceil(dist / 1.8F)));
    for (int step = 1; step <= steps; ++step) {
      const float t = static_cast<float>(step) / static_cast<float>(steps);
      set_listener_xz(x0 + (x1 - x0) * t, z0 + (z1 - z0) * t);
      scape.update(0.35F, nullptr, 0.0F);
      const auto active = scape.active_bed_ids();
      EXPECT_GE(active.size(), 1U);
      max_simultaneous_beds = std::max(max_simultaneous_beds, active.size());
      for (const auto& bed : active) {
        heard_along_walk.insert(bed);
      }
    }
  }
  for (int settle = 0; settle < 10; ++settle) {
    scape.update(0.30F, nullptr, 0.0F);
    for (const auto& bed : scape.active_bed_ids()) {
      heard_along_walk.insert(bed);
    }
  }

  EXPECT_TRUE(heard_along_walk.contains("ambient.city_street_day"));
  EXPECT_TRUE(heard_along_walk.contains("ambient.quay_work"));
  EXPECT_TRUE(heard_along_walk.contains("ambient.stone_tunnel"));
  EXPECT_TRUE(heard_along_walk.contains("ambient.livestock_market"));
  EXPECT_TRUE(heard_along_walk.contains("ambient.city_square_crowd"));
  EXPECT_TRUE(heard_along_walk.contains("ambient.arena_interior"));
  // During a crossfade at most 1 outgoing bed + 3 target beds may overlap briefly,
  // settling to <= 3 steady-state beds.
  EXPECT_LE(scape.active_bed_ids().size(), 3U);
  EXPECT_LE(max_simultaneous_beds, 5U);
}

TEST_F(CitySoundscapeTest, CarthageWalkCrossfadesHarbourAgoraBazaarAndByrsa) {
  Game::Map::MapDefinition carthage;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      QStringLiteral("assets/maps/map_carthage.json"), carthage, &error))
      << error.toStdString();
  const auto walk = load_walk_points(
      QStringLiteral("tools/arena/promos/city_film/carthage_walk.json"));
  ASSERT_GE(walk.size(), 10U);

  auto& scape = CitySoundscape::instance();
  scape.configure(&carthage,
                  QStringLiteral("ambient.mediterranean_city_market"),
                  QStringLiteral("carthage"));

  std::set<std::string> heard_along_walk;
  for (std::size_t seg = 0; seg + 1 < walk.size(); ++seg) {
    const auto [x0, z0] = walk[seg];
    const auto [x1, z1] = walk[seg + 1];
    const float dist = std::hypot(x1 - x0, z1 - z0);
    const int steps = std::max(1, static_cast<int>(std::ceil(dist / 1.8F)));
    for (int step = 1; step <= steps; ++step) {
      const float t = static_cast<float>(step) / static_cast<float>(steps);
      set_listener_xz(x0 + (x1 - x0) * t, z0 + (z1 - z0) * t);
      scape.update(0.35F, nullptr, 0.0F);
      for (const auto& bed : scape.active_bed_ids()) {
        heard_along_walk.insert(bed);
      }
    }
  }
  for (int settle = 0; settle < 10; ++settle) {
    scape.update(0.30F, nullptr, 0.0F);
    for (const auto& bed : scape.active_bed_ids()) {
      heard_along_walk.insert(bed);
    }
  }

  EXPECT_TRUE(heard_along_walk.contains("ambient.quay_work"));
  EXPECT_TRUE(heard_along_walk.contains("ambient.city_square_crowd"));
  EXPECT_TRUE(heard_along_walk.contains("ambient.livestock_market"));
  EXPECT_TRUE(heard_along_walk.contains("ambient.temple_precinct"));
  EXPECT_LE(scape.active_bed_ids().size(), 3U);
}

TEST_F(CitySoundscapeTest, DarknessAmountSwitchesEveryZoneAndBaseBedToNightVariants) {
  Game::Map::MapDefinition rome;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      QStringLiteral("assets/maps/map_rome.json"), rome, &error))
      << error.toStdString();

  auto& scape = CitySoundscape::instance();
  scape.configure(&rome,
                  QStringLiteral("ambient.mediterranean_city_market"),
                  QStringLiteral("roman"));

  // Garden of Sallust (120, -296) in daylight -> city_garden_day + city_street_day
  set_listener_xz(120.0F, -296.0F);
  for (int i = 0; i < 12; ++i) {
    scape.update(0.30F, nullptr, 0.0F);
  }
  EXPECT_TRUE(contains_bed(scape.active_bed_ids(), "ambient.city_garden_day"));
  EXPECT_TRUE(contains_bed(scape.active_bed_ids(), "ambient.city_street_day"));

  // Switch darkness_amount to 0.85 -> crossfades to city_garden_dusk +
  // city_street_night
  for (int i = 0; i < 14; ++i) {
    scape.update(0.30F, nullptr, 0.85F);
  }
  EXPECT_TRUE(contains_bed(scape.active_bed_ids(), "ambient.city_garden_dusk"));
  EXPECT_TRUE(contains_bed(scape.active_bed_ids(), "ambient.city_street_night"));
  EXPECT_FALSE(contains_bed(scape.active_bed_ids(), "ambient.city_garden_day"));
  EXPECT_FALSE(contains_bed(scape.active_bed_ids(), "ambient.city_street_day"));

  // Hysteresis band (0.43 is between 0.38 and 0.48): holds night without flapping
  for (int i = 0; i < 6; ++i) {
    scape.update(0.30F, nullptr, 0.43F);
  }
  EXPECT_TRUE(contains_bed(scape.active_bed_ids(), "ambient.city_garden_dusk"));
}

TEST_F(CitySoundscapeTest, DerivedZonesCoverUnauthoredWaterMarketsGardensAndBarracks) {
  Game::Map::MapDefinition unauthored;
  unauthored.name = QStringLiteral("UnauthoredCity");
  unauthored.grid.width = 400;
  unauthored.grid.height = 400;
  for (int i = 0; i < 310; ++i) {
    Game::Map::StructureEntry s;
    s.type = Game::Units::SpawnType::Home;
    s.geometry = Game::Map::PointStructureGeometry{
        QVector3D(-150.0F + static_cast<float>(i % 20) * 5.0F,
                  0.0F,
                  -150.0F + static_cast<float>(i / 20) * 5.0F)};
    unauthored.structures.push_back(s);
  }
  {
    Game::Map::StructureEntry market;
    market.type = Game::Units::SpawnType::Marketplace;
    market.geometry = Game::Map::PointStructureGeometry{QVector3D(80.0F, 0.0F, 0.0F)};
    unauthored.structures.push_back(market);
  }
  {
    Game::Map::StructureEntry barracks;
    barracks.type = Game::Units::SpawnType::Barracks;
    barracks.geometry = Game::Map::PointStructureGeometry{QVector3D(0.0F, 0.0F, 90.0F)};
    unauthored.structures.push_back(barracks);
  }
  {
    Game::Map::Lake harbor;
    harbor.center = QVector3D(-90.0F, 0.0F, 80.0F);
    harbor.width = 40.0F;
    harbor.depth = 36.0F;
    unauthored.lakes.push_back(harbor);
  }
  for (int i = 0; i < 10; ++i) {
    Game::Map::WorldProp tree;
    tree.type = (i % 2 == 0) ? Game::Map::WorldProp::Type::OliveTree
                             : Game::Map::WorldProp::Type::Plant;
    tree.x = 92.0F + static_cast<float>(i % 4) * 2.0F;
    tree.z = 92.0F + static_cast<float>(i / 4) * 2.0F;
    unauthored.world_props.push_back(tree);
  }

  auto& scape = CitySoundscape::instance();
  scape.configure(&unauthored,
                  QStringLiteral("ambient.mediterranean_city_market"),
                  QStringLiteral("roman"));
  EXPECT_GE(scape.zone_count(), 4U);

  const auto check_spot = [&](float x, float z, const std::string& expected_bed) {
    set_listener_xz(x, z);
    for (int i = 0; i < 12; ++i) {
      scape.update(0.30F, nullptr, 0.0F);
    }
    EXPECT_TRUE(contains_bed(scape.active_bed_ids(), expected_bed))
        << "missing derived bed " << expected_bed << " at (" << x << ", " << z << ")";
  };

  check_spot(80.0F, 0.0F, "ambient.mediterranean_city_market");
  check_spot(-90.0F, 80.0F, "ambient.mediterranean_harbor");
  check_spot(94.0F, 94.0F, "ambient.city_garden_day");
  check_spot(0.0F, 90.0F, "ambient.drill_field");
}

TEST_F(CitySoundscapeTest,
       PointEmittersRespectGlobalBudgetAndFireSpatialCityAndWildlifeCues) {
  RenderedClockScope const clock_scope;
  Engine::Core::World world;
  Game::Audio::AudioEventHandler handler(&world);
  ASSERT_TRUE(handler.initialize());

  Game::Map::MapDefinition map;
  map.name = QStringLiteral("DenseEmitterCity");
  map.biome.seed = 42;
  for (int i = 0; i < 320; ++i) {
    Game::Map::StructureEntry home;
    home.type = Game::Units::SpawnType::Home;
    home.geometry = Game::Map::PointStructureGeometry{
        QVector3D(-40.0F + static_cast<float>(i % 20) * 4.5F,
                  0.0F,
                  -40.0F + static_cast<float>(i / 20) * 4.5F)};
    map.structures.push_back(home);
  }
  {
    Game::Map::StructureEntry market;
    market.type = Game::Units::SpawnType::Marketplace;
    market.geometry = Game::Map::PointStructureGeometry{QVector3D(6.0F, 0.0F, 4.0F)};
    map.structures.push_back(market);
  }
  {
    Game::Map::StructureEntry temple;
    temple.type = Game::Units::SpawnType::Temple;
    temple.geometry = Game::Map::PointStructureGeometry{QVector3D(-8.0F, 0.0F, 6.0F)};
    map.structures.push_back(temple);
  }
  {
    Game::Map::Lake basin;
    basin.center = QVector3D(10.0F, 0.0F, -8.0F);
    basin.width = 36.0F;
    basin.depth = 36.0F;
    map.lakes.push_back(basin);
  }
  {
    Game::Map::AmbienceZone livestock;
    livestock.id = QStringLiteral("cattle_pen");
    livestock.bed = QStringLiteral("ambient.livestock_market");
    livestock.x = -6.0F;
    livestock.z = -10.0F;
    livestock.radius = 22.0F;
    map.ambience_zones.push_back(livestock);

    Game::Map::AmbienceZone square;
    square.id = QStringLiteral("forum");
    square.bed = QStringLiteral("ambient.city_square_crowd");
    square.x = 4.0F;
    square.z = 8.0F;
    square.radius = 22.0F;
    map.ambience_zones.push_back(square);

    Game::Map::AmbienceZone garden;
    garden.id = QStringLiteral("garden");
    garden.bed = QStringLiteral("ambient.city_garden_day");
    garden.night_bed = QStringLiteral("ambient.city_garden_dusk");
    garden.x = 0.0F;
    garden.z = 12.0F;
    garden.radius = 20.0F;
    map.ambience_zones.push_back(garden);
  }
  {
    Game::Map::RoadSegment road;
    road.start = QVector3D(-20.0F, 0.0F, 0.0F);
    road.end = QVector3D(20.0F, 0.0F, 0.0F);
    map.roads.push_back(road);
  }

  // Calm grazing sheep within 20 m
  for (int i = 0; i < 3; ++i) {
    auto* sheep = world.create_entity();
    sheep->add_component<Engine::Core::TransformComponent>(
        3.0F + static_cast<float>(i), 0.0F, 2.0F);
    auto* wl = sheep->add_component<Engine::Core::WildlifeComponent>();
    wl->species = Game::Wildlife::Species::Sheep;
    wl->behavior = Game::Wildlife::Behavior::Graze;
    wl->alarm_timer = 0.0F;
  }

  // Idle war elephants within 32 m
  for (int i = 0; i < 2; ++i) {
    auto* el = world.create_entity();
    el->add_component<Engine::Core::TransformComponent>(
        -4.0F - static_cast<float>(i) * 2.0F, 0.0F, 3.0F);
    auto* el_comp = el->add_component<Engine::Core::ElephantComponent>();
    el_comp->charge_state = Engine::Core::ElephantComponent::ChargeState::Idle;
  }

  // Active producing barracks within 34 m
  {
    auto* barracks = world.create_entity();
    barracks->add_component<Engine::Core::TransformComponent>(5.0F, 0.0F, -3.0F);
    auto* unit = barracks->add_component<Engine::Core::UnitComponent>();
    unit->health = 500;
    unit->max_health = 500;
    unit->spawn_type = Game::Units::SpawnType::Barracks;
    auto* prod = barracks->add_component<Engine::Core::ProductionComponent>();
    prod->in_progress = true;
  }

  auto& scape = CitySoundscape::instance();
  scape.configure(
      &map, QStringLiteral("ambient.city_street_day"), QStringLiteral("carthage"));
  set_listener_xz(0.0F, 0.0F);
  CueTrace::instance().reset();

  // Simulate 90 seconds of daytime + 30 seconds of nighttime (120 s total)
  constexpr float k_dt = 0.25F;
  for (int step = 0; step < 360; ++step) {
    Game::Audio::Clock::advance_rendered(k_dt);
    scape.update(k_dt, &world, 0.0F);
  }
  for (int step = 0; step < 120; ++step) {
    Game::Audio::Clock::advance_rendered(k_dt);
    scape.update(k_dt, &world, 0.85F);
  }

  static constexpr const char* k_emitter_cues[] = {
      Game::Audio::Cue::k_city_hawker_call,
      Game::Audio::Cue::k_city_dog_bark_distant,
      Game::Audio::Cue::k_city_door_close,
      Game::Audio::Cue::k_city_cart_wheels,
      Game::Audio::Cue::k_city_forge_hammer,
      Game::Audio::Cue::k_city_temple_bell,
      Game::Audio::Cue::k_city_water_splash,
      Game::Audio::Cue::k_wildlife_sheep_bleat_calm,
      Game::Audio::Cue::k_wildlife_lamb_bleat,
      Game::Audio::Cue::k_wildlife_cattle_low,
      Game::Audio::Cue::k_wildlife_elephant_rumble,
      Game::Audio::Cue::k_wildlife_elephant_trunk,
      Game::Audio::Cue::k_wildlife_gull,
      Game::Audio::Cue::k_wildlife_pigeon_flutter,
      Game::Audio::Cue::k_wildlife_cricket,
  };

  std::uint64_t total_requests = 0;
  std::uint64_t total_accepted = 0;
  for (const char* cue_id : k_emitter_cues) {
    const auto rec = CueTrace::instance().record_for(cue_id);
    total_requests += rec.requests;
    total_accepted += rec.accepted;
  }

  // Global budget: <= ~1 one-shot per second across 120 simulated seconds!
  EXPECT_GT(total_accepted, 25U);
  EXPECT_LE(total_requests, 120U)
      << "point emitters exceeded the <= 1 one-shot/second city budget";

  // Verify entity-tied and night emitters specifically fired
  EXPECT_GT(CueTrace::instance()
                    .record_for(Game::Audio::Cue::k_wildlife_sheep_bleat_calm)
                    .accepted +
                CueTrace::instance()
                    .record_for(Game::Audio::Cue::k_wildlife_lamb_bleat)
                    .accepted,
            0U);
  EXPECT_GT(CueTrace::instance()
                    .record_for(Game::Audio::Cue::k_wildlife_elephant_rumble)
                    .accepted +
                CueTrace::instance()
                    .record_for(Game::Audio::Cue::k_wildlife_elephant_trunk)
                    .accepted,
            0U);
  EXPECT_GT(
      CueTrace::instance().record_for(Game::Audio::Cue::k_city_forge_hammer).accepted,
      0U);
  EXPECT_GT(
      CueTrace::instance().record_for(Game::Audio::Cue::k_wildlife_cricket).accepted,
      0U);

  // Also verify every single new city/wildlife cue resolves and is accepted when fired
  Game::Audio::Clock::advance_rendered(30.0);
  for (const char* cue_id : k_emitter_cues) {
    EXPECT_TRUE(Game::Audio::play_cue_at(
        cue_id, Game::Audio::WorldPoint{1.0F, 0.0F, 1.0F}, 0.5F))
        << "failed to play " << cue_id;
  }

  handler.shutdown();
}

TEST_F(CitySoundscapeTest, FootstepSurfacesAndFpvMeleeSwingPlayDistinctCues) {
  RenderedClockScope const clock_scope;
  Game::Map::MapDefinition map;
  map.grid.width = 128;
  map.grid.height = 128;
  map.grid.tile_size = 1.0F;
  {
    Game::Map::RoadSegment road;
    road.start = QVector3D(-20.0F, 0.0F, 0.0F);
    road.end = QVector3D(20.0F, 0.0F, 0.0F);
    road.width = 6.0F;
    map.roads.push_back(road);
  }
  {
    Game::Map::AmbienceZone tunnel;
    tunnel.id = QStringLiteral("vault");
    tunnel.bed = QStringLiteral("ambient.stone_tunnel");
    tunnel.x = 30.0F;
    tunnel.z = 30.0F;
    tunnel.radius = 8.0F;
    tunnel.fade = 3.0F;
    map.ambience_zones.push_back(tunnel);

    Game::Map::AmbienceZone garden;
    garden.id = QStringLiteral("garden_path");
    garden.bed = QStringLiteral("ambient.city_garden_day");
    garden.x = -30.0F;
    garden.z = 30.0F;
    garden.radius = 12.0F;
    garden.fade = 4.0F;
    map.ambience_zones.push_back(garden);
  }

  Game::Map::TerrainService::instance().clear();
  Game::Map::TerrainService::instance().initialize(map);
  Game::Systems::NavGrid::initialize(map.grid.width, map.grid.height);

  Engine::Core::World world;
  Game::Audio::AudioEventHandler handler(&world);
  ASSERT_TRUE(handler.initialize());

  auto& scape = CitySoundscape::instance();
  scape.configure(
      &map, QStringLiteral("ambient.city_street_day"), QStringLiteral("roman"));

  EXPECT_EQ(CitySoundscape::classify_footstep_surface(world, 30.0F, 30.0F),
            FootstepSurface::StoneInterior);
  EXPECT_EQ(CitySoundscape::classify_footstep_surface(world, -30.0F, 30.0F),
            FootstepSurface::Gravel);
  EXPECT_EQ(CitySoundscape::classify_footstep_surface(world, 0.0F, 0.0F),
            FootstepSurface::Stone);
  EXPECT_EQ(CitySoundscape::classify_footstep_surface(world, 0.0F, -35.0F),
            FootstepSurface::Grass);

  // Verify the new footstep cues play through the mixer
  Game::Audio::Clock::advance_rendered(1.0);
  EXPECT_TRUE(Game::Audio::play_cue(Game::Audio::Cue::k_move_footstep_stone_interior));
  Game::Audio::Clock::advance_rendered(1.0);
  EXPECT_TRUE(Game::Audio::play_cue(Game::Audio::Cue::k_move_footstep_gravel));

  // Verify FPV commander melee swing fires k_combat_melee_swing on WeaponTraceStart
  auto* commander = world.create_entity();
  commander->add_component<Engine::Core::TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* unit = commander->add_component<Engine::Core::UnitComponent>();
  unit->health = 200;
  unit->max_health = 200;
  unit->owner_id = 1;
  auto* cmd = commander->add_component<Engine::Core::CommanderComponent>();
  cmd->fpv_controlled = true;
  auto* action = commander->add_component<Engine::Core::RpgCommanderActionComponent>();
  action->combat_action_id = static_cast<std::uint16_t>(
      Game::Systems::CombatActions::CombatActionId::RpgSwordSlashLeft);
  action->action_running = true;

  Game::Audio::Clock::advance_rendered(1.0);
  Game::Systems::Combat::process_authored_combat_action(
      &world, *commander, nullptr, 0.45F);

  for (int attempt = 0; attempt < 60; ++attempt) {
    if (CueTrace::instance()
            .record_for(Game::Audio::Cue::k_combat_melee_swing)
            .accepted > 0U) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  EXPECT_GT(
      CueTrace::instance().record_for(Game::Audio::Cue::k_combat_melee_swing).accepted,
      0U);

  handler.shutdown();
}

} // namespace
