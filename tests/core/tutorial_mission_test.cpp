#include <QVector3D>

#include <algorithm>
#include <cstdio>
#include <gtest/gtest.h>
#include <limits>
#include <map>
#include <memory>
#include <vector>

#include "app/mission/tutorial_observation.h"
#include "app/orders/movement_utils.h"
#include "app/session/skirmish_loader.h"
#include "app/session/skirmish_runtime_coordinator.h"
#include "core/component_gameplay.h"
#include "core/world.h"
#include "game/core/event_manager.h"
#include "game/map/map_context.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/mission/campaign_manager.h"
#include "game/mission/mission_definition_view.h"
#include "game/mission/mission_setup_coordinator.h"
#include "game/mission/mission_waves.h"
#include "game/mission/tutorial_director.h"
#include "game/session/session_context.h"
#include "game/systems/default_content.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/match_snapshot.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/runtime_system_registry.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_config.h"
#include "render/scene/scene_renderer.h"
#include "scene/camera.h"
#include "tests/support/ai_quiesce.h"

namespace {

using Engine::Core::EntityID;
using Engine::Core::UnitComponent;

constexpr char k_mission_path[] = "assets/missions/tutorial.json";
constexpr int k_local_owner = 1;
constexpr float k_tick = 1.0F / 60.0F;

class TutorialMissionTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto& nations = Game::Systems::NationRegistry::instance();
    nations.clear();
    Game::Systems::initialize_default_content(nations);
    Game::Systems::OwnerRegistry::instance().clear();
    Game::Systems::GlobalStatsRegistry::instance().clear();

    m_world.set_presentation_enabled(false);
    Game::Systems::register_runtime_systems(m_world);

    int selected_player_id = k_local_owner;
    QString error;
    ASSERT_TRUE(m_campaign.start_mission_file(
        QString::fromLatin1(k_mission_path), selected_player_id, &error))
        << error.toStdString();
    ASSERT_TRUE(m_campaign.current_mission_definition().has_value());
    const auto& mission = *m_campaign.current_mission_definition();
    ASSERT_TRUE(mission.tutorial);

    const QString map_path = mission.map_path.startsWith(QStringLiteral(":/"))
                                 ? mission.map_path.mid(2)
                                 : mission.map_path;
    App::Core::SkirmishLoader loader(m_world, m_renderer, m_camera);
    const auto load_result = loader.start(map_path,
                                          build_campaign_player_configs(mission),
                                          k_local_owner,
                                          false,
                                          selected_player_id);
    ASSERT_TRUE(load_result.ok) << load_result.error_message.toStdString();

    m_level.map_path = map_path;
    m_level.grid_width = load_result.grid_width;
    m_level.grid_height = load_result.grid_height;
    m_level.tile_size = load_result.tile_size;

    (void)m_coordinator.apply_mission_setup({.world = m_world,
                                             .campaign = m_campaign,
                                             .level = m_level,
                                             .selected_player_id = selected_player_id,
                                             .local_owner_id = k_local_owner,
                                             .pending_waves = m_pending_waves});
  }

  void TearDown() override {
    TestSupport::quiesce_ai(m_world);
    Game::Map::TerrainService::instance().clear();
    Game::Map::VisibilityService::instance().reset();
    Game::Systems::GlobalStatsRegistry::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::OwnerRegistry::instance().clear();
  }

  auto living_units_of(int owner) -> std::vector<Engine::Core::Entity*> {
    std::vector<Engine::Core::Entity*> units;
    for (auto* entity : m_world.collect_entities_with<UnitComponent>()) {
      const auto* unit = entity->get_component<UnitComponent>();
      if (unit != nullptr && unit->health > 0 && unit->owner_id == owner) {
        units.push_back(entity);
      }
    }
    return units;
  }

  // Men standing, counted the way the battle report counts them.
  auto men_of(int owner) -> int {
    int men = 0;
    for (auto* entity : living_units_of(owner)) {
      const auto* unit = entity->get_component<UnitComponent>();
      if (Game::Units::is_building_spawn(unit->spawn_type)) {
        continue;
      }
      men += Engine::Core::resolve_surviving_individual_count(
          unit->health,
          unit->max_health,
          Game::Units::TroopConfig::instance().get_individuals_per_unit(
              unit->spawn_type));
    }
    return men;
  }

  // What GameEngine does after the mission setup: stock both sides as the map
  // and mission say, and let the AI make its first decisions. Without it the
  // outpost has nothing to build or recruit with and nothing can go wrong.
  void start_the_match() {
    const auto map = Game::Map::MapContextStore::acquire(m_level.map_path);
    ASSERT_TRUE(map.valid());
    m_level.starting_resources = map.definition()->starting_resources;
    App::Core::SkirmishRuntimeCoordinator runtime;
    runtime.initialize_player_resources({*m_session,
                                         m_level,
                                         k_local_owner,
                                         &*m_campaign.current_mission_definition(),
                                         nullptr});
    App::Core::SkirmishRuntimeCoordinator::prepare_ai_state(&m_world, m_session.get());
  }

  void run_seconds(float seconds) {
    const int ticks = static_cast<int>(seconds / k_tick);
    for (int tick = 0; tick < ticks; ++tick) {
      // Through the session clock, as the other tests here: orders given to
      // the world wait in the command log for a tick to seal them.
      m_session->advance(
          k_tick, 1, Game::Session::SessionContext::OverloadPolicy::KeepBacklog);
    }
  }

  static auto position(Engine::Core::Entity* entity) -> QVector3D {
    const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    return transform == nullptr
               ? QVector3D()
               : QVector3D(transform->position.x, 0.0F, transform->position.z);
  }

  std::unique_ptr<Game::Session::SessionContext> m_session{
      std::make_unique<Game::Session::SessionContext>()};
  Game::Session::ScopedSession m_scope{*m_session};
  Engine::Core::World& m_world{m_session->world()};
  Render::GL::Renderer m_renderer{Render::ShaderQuality::None};
  Render::GL::Camera m_camera;
  CampaignManager m_campaign;
  Game::Mission::MissionSetupCoordinator m_coordinator;
  Game::Mission::MissionWaves m_waves;
  Game::Systems::LevelSnapshot m_level;
  std::vector<Game::Mission::PendingMissionWave> m_pending_waves;
};

} // namespace

TEST_F(TutorialMissionTest, EveryForceFieldsExactlyOneCommander) {
  std::map<int, int> commanders;
  for (auto* entity :
       m_world.collect_entities_with<Engine::Core::CommanderComponent>()) {
    const auto* unit = entity->get_component<UnitComponent>();
    if (unit != nullptr && unit->health > 0) {
      ++commanders[unit->owner_id];
    }
  }
  EXPECT_EQ(commanders[1], 1) << "the player needs a commander to lose";
  EXPECT_EQ(commanders[2], 1) << "the assault step needs a Roman commander to kill";
}

TEST_F(TutorialMissionTest, ThePlayerStartsWithWhatTheFirstStepsNeed) {
  int builders = 0;
  int soldiers = 0;
  int barracks = 0;
  int homes = 0;
  for (auto* entity : living_units_of(k_local_owner)) {
    const auto type = entity->get_component<UnitComponent>()->spawn_type;
    if (type == Game::Units::SpawnType::Builder) {
      ++builders;
    } else if (type == Game::Units::SpawnType::Barracks) {
      ++barracks;
    } else if (type == Game::Units::SpawnType::Home) {
      ++homes;
    } else if (Game::Units::is_troop_spawn(type) &&
               entity->get_component<Engine::Core::CommanderComponent>() == nullptr) {
      ++soldiers;
    }
  }
  EXPECT_GE(builders, 2) << "the gather steps put one builder on each material";
  EXPECT_GE(soldiers, 3) << "the select, move and attack steps need soldiers";
  EXPECT_LT(soldiers, Game::Mission::k_tutorial_army_size)
      << "the army step must ask for recruits, not be met on arrival";
  EXPECT_EQ(barracks, 1) << "gathered loads are dropped at the barracks yard";
  EXPECT_EQ(homes, 0) << "the build step raises the first Home";
}

TEST_F(TutorialMissionTest, TheScoutingPartyWaitsToBeAttacked) {
  std::vector<Engine::Core::Entity*> scouts;
  for (auto* entity : living_units_of(2)) {
    const auto* unit = entity->get_component<UnitComponent>();
    if (Game::Units::is_building_spawn(unit->spawn_type) ||
        entity->get_component<Engine::Core::CommanderComponent>() != nullptr) {
      continue;
    }

    if (position(entity).z() > 0.0F) {
      scouts.push_back(entity);
    }
  }
  ASSERT_GE(static_cast<int>(scouts.size()), Game::Mission::k_tutorial_scout_count);

  std::vector<QVector3D> start;
  for (auto* scout : scouts) {
    EXPECT_EQ(scout->get_component<Engine::Core::AIControlledComponent>(), nullptr)
        << "a held scout must not be handed to the AI, or it marches away";
    EXPECT_NE(scout->get_component<Engine::Core::HoldModeComponent>(), nullptr);
    start.push_back(position(scout));
  }

  for (int tick = 0; tick < 60 * 20; ++tick) {
    m_session->advance(
        k_tick, 1, Game::Session::SessionContext::OverloadPolicy::KeepBacklog);
  }

  for (std::size_t i = 0; i < scouts.size(); ++i) {
    EXPECT_LT((position(scouts[i]) - start[i]).length(), 3.0F)
        << "scout " << i << " wandered off before the player came for it";
  }
}

TEST_F(TutorialMissionTest, TheRaidClosesOnThePlayerCampWhileTheCommanderKeepsHisPost) {
  ASSERT_EQ(m_pending_waves.size(), 1U) << "the defend step expects one raid";
  const auto& wave = m_pending_waves.front();
  const QVector3D camp = wave.defense_reference_world_position;
  const float spawn_distance = (wave.entry_world_position - camp).length();
  ASSERT_GT(spawn_distance, 20.0F);

  const auto effects = m_waves.spawn({.world = m_world,
                                      .level = m_level,
                                      .campaign_mission_elapsed = wave.trigger_time},
                                     wave);
  ASSERT_GE(effects.spawned_entity_ids.size(), 2U);

  auto closest = [&]() {
    float best = std::numeric_limits<float>::max();
    for (const auto id : effects.spawned_entity_ids) {
      auto* entity = m_world.get_entity(id);
      if (entity == nullptr) {
        continue;
      }
      best = std::min(best, (position(entity) - camp).length());
    }
    return best;
  };

  Engine::Core::Entity* roman_commander = nullptr;
  for (auto* entity :
       m_world.collect_entities_with<Engine::Core::CommanderComponent>()) {
    const auto* unit = entity->get_component<UnitComponent>();
    if (unit != nullptr && unit->owner_id != k_local_owner) {
      roman_commander = entity;
    }
  }
  ASSERT_NE(roman_commander, nullptr);
  const QVector3D post = position(roman_commander);

  const float at_spawn = closest();
  for (int tick = 0; tick < 60 * 40; ++tick) {
    m_session->advance(
        k_tick, 1, Game::Session::SessionContext::OverloadPolicy::KeepBacklog);
  }
  EXPECT_LT(closest(), at_spawn - 10.0F)
      << "the raid never marched on the camp the defend step guards";
  EXPECT_LT((position(roman_commander) - post).length(), 13.0F)
      << "the Roman commander followed the raid; the assault step expects him at "
         "his outpost";
}

TEST_F(TutorialMissionTest, TheRomanOutpostIsNamedAfterItsNationNotAPlaceholder) {
  const std::string name = m_session->owners().get_owner_name(2);
  EXPECT_EQ(name.find("AI Player"), std::string::npos) << name;
  EXPECT_EQ(name, "Roman Republic");
}

TEST_F(TutorialMissionTest, TheFallenRomanCommanderIsAnnouncedByNationName) {
  std::vector<QString> announcements;
  Engine::Core::ScopedEventSubscription<Engine::Core::MissionAnnouncementEvent> const
      subscription([&announcements](const Engine::Core::MissionAnnouncementEvent& e) {
        announcements.push_back(e.text);
      });

  Engine::Core::Entity* roman_commander = nullptr;
  for (auto* entity :
       m_world.collect_entities_with<Engine::Core::CommanderComponent>()) {
    const auto* unit = entity->get_component<UnitComponent>();
    if (unit != nullptr && unit->owner_id == 2) {
      roman_commander = entity;
    }
  }
  ASSERT_NE(roman_commander, nullptr);
  roman_commander->get_component<UnitComponent>()->health = 0;
  for (int tick = 0; tick < 10; ++tick) {
    m_session->advance(
        k_tick, 1, Game::Session::SessionContext::OverloadPolicy::KeepBacklog);
  }

  const auto lost =
      std::find_if(announcements.begin(), announcements.end(), [](const QString& text) {
        return text.contains(QStringLiteral("lost its commander"));
      });
  ASSERT_NE(lost, announcements.end()) << "no announcement for the fallen commander";
  EXPECT_TRUE(lost->contains(QStringLiteral("Roman Republic"))) << lost->toStdString();
  EXPECT_FALSE(lost->contains(QStringLiteral("AI Player"))) << lost->toStdString();
}

TEST_F(TutorialMissionTest, TheOutpostHoldsItsCampThroughTheEconomySteps) {
  // The economy steps (wood, stone and iron, a Home, recruits) take a new player
  // many minutes. The outpost must still be the camp the map drew: no builders,
  // no town, no recruits, nobody marching out to meet the player on the road.
  std::map<EntityID, QVector3D> start;
  for (auto* entity : living_units_of(2)) {
    start.emplace(entity->get_id(), position(entity));
  }
  ASSERT_FALSE(start.empty());

  start_the_match();
  run_seconds(8.0F * 60.0F);

  int structures = 0;
  int builders = 0;
  int newcomers = 0;
  float furthest = 0.0F;
  for (auto* entity : living_units_of(2)) {
    const auto* unit = entity->get_component<UnitComponent>();
    if (Game::Units::is_building_spawn(unit->spawn_type)) {
      ++structures;
    }
    if (unit->spawn_type == Game::Units::SpawnType::Builder) {
      ++builders;
    }
    const auto known = start.find(entity->get_id());
    if (known == start.end()) {
      ++newcomers;
      continue;
    }
    furthest = std::max(furthest, (position(entity) - known->second).length());
  }
  std::printf("[outpost] after 8 min: structures=%d builders=%d newcomers=%d "
              "furthest_move=%.1f\n",
              structures,
              builders,
              newcomers,
              static_cast<double>(furthest));
  EXPECT_EQ(structures, 1) << "the outpost built a town: it is one barracks";
  EXPECT_EQ(builders, 0) << "an outpost has no economy to put builders to";
  EXPECT_EQ(newcomers, 0) << "the outpost recruited; its garrison is authored";
  EXPECT_LT(furthest, 13.0F) << "an outpost soldier left his post";
}

TEST_F(TutorialMissionTest, TheScoutFightIsTheTwoScoutsAlone) {
  std::vector<EntityID> army;
  for (auto* entity : living_units_of(k_local_owner)) {
    const auto type = entity->get_component<UnitComponent>()->spawn_type;
    if (Game::Units::is_troop_spawn(type) && type != Game::Units::SpawnType::Builder &&
        entity->get_component<Engine::Core::CommanderComponent>() == nullptr) {
      army.push_back(entity->get_id());
    }
  }
  std::vector<EntityID> scouts;
  std::map<EntityID, QVector3D> garrison;
  for (auto* entity : living_units_of(2)) {
    const auto* unit = entity->get_component<UnitComponent>();
    if (Game::Units::is_building_spawn(unit->spawn_type)) {
      continue;
    }
    if (entity->get_component<Engine::Core::HoldModeComponent>() != nullptr &&
        position(entity).z() > 0.0F) {
      scouts.push_back(entity->get_id());
    } else {
      garrison.emplace(entity->get_id(), position(entity));
    }
  }
  ASSERT_EQ(static_cast<int>(scouts.size()), Game::Mission::k_tutorial_scout_count);
  ASSERT_FALSE(army.empty());

  // The player spends a minute on the select and move steps, then attacks.
  start_the_match();
  run_seconds(60.0F);
  const int men_before = men_of(k_local_owner);
  float elapsed = 0.0F;
  for (const auto scout : scouts) {
    auto* target = m_world.get_entity(scout);
    if (target == nullptr || target->get_component<UnitComponent>()->health <= 0) {
      continue;
    }
    ASSERT_TRUE(App::Utils::issue_attack_command(&m_world, army, scout, k_local_owner)
                    .accepted());
    while (elapsed < 150.0F && target->get_component<UnitComponent>()->health > 0) {
      run_seconds(1.0F);
      elapsed += 1.0F;
    }
  }
  const int men_after = men_of(k_local_owner);
  float garrison_moved = 0.0F;
  for (const auto& [id, at] : garrison) {
    if (auto* entity = m_world.get_entity(id)) {
      garrison_moved = std::max(garrison_moved, (position(entity) - at).length());
    }
  }
  std::printf("[scouts] fight took %.0f s; player men %d -> %d; garrison moved %.1f\n",
              static_cast<double>(elapsed),
              men_before,
              men_after,
              static_cast<double>(garrison_moved));
  for (const auto scout : scouts) {
    auto* entity = m_world.get_entity(scout);
    EXPECT_TRUE(entity == nullptr ||
                entity->get_component<UnitComponent>()->health <= 0)
        << "the scouts outlasted the player's whole starting army";
  }
  EXPECT_LT(garrison_moved, 13.0F) << "the outpost garrison joined the scout fight";
}

TEST_F(TutorialMissionTest, ShowMeOnStoneAndIronPointsAtTheBouldersAndOre) {
  auto& terrain = m_session->terrain();
  const auto points = App::Mission::resolve_tutorial_focus_points(
      {.world = &m_world,
       .local_owner_id = k_local_owner,
       .target = Game::Mission::TutorialFocusTarget::StoneAndIron,
       .owners = &m_session->owners(),
       .terrain = &terrain});
  ASSERT_FALSE(points.isEmpty());
  const float half = static_cast<float>(m_level.grid_width) * 0.5F;
  for (const auto& value : points) {
    const auto point = value.toMap();
    const QVector3D at(
        point.value("world_x").toFloat(), 0.0F, point.value("world_z").toFloat());
    EXPECT_LT(std::abs(at.x()), half) << "Show me left the map";
    EXPECT_LT(std::abs(at.z()), half) << "Show me left the map";
    float nearest = std::numeric_limits<float>::max();
    for (const auto& prop : terrain.world_props()) {
      if (!Game::Map::is_boulder_world_prop_type(prop.type) &&
          !Game::Map::is_iron_ore_world_prop_type(prop.type)) {
        continue;
      }
      const QVector3D world = terrain.world_prop_world_position(prop);
      nearest =
          std::min(nearest, (QVector3D(world.x(), 0.0F, world.z()) - at).length());
    }
    EXPECT_LT(nearest, 0.5F) << "a Show me point is not on a boulder or ore vein: "
                             << at.x() << "," << at.z();
  }
}

TEST_F(TutorialMissionTest, ShowMeOnTheAssaultPointsAtTheRomanCommandersCamp) {
  Engine::Core::Entity* roman_commander = nullptr;
  Engine::Core::Entity* own_barracks = nullptr;
  for (auto* entity : m_world.collect_entities_with<UnitComponent>()) {
    const auto* unit = entity->get_component<UnitComponent>();
    if (unit->owner_id == 2 &&
        entity->get_component<Engine::Core::CommanderComponent>() != nullptr) {
      roman_commander = entity;
    }
    if (unit->owner_id == k_local_owner &&
        unit->spawn_type == Game::Units::SpawnType::Barracks) {
      own_barracks = entity;
    }
  }
  ASSERT_NE(roman_commander, nullptr);
  ASSERT_NE(own_barracks, nullptr);

  const auto points = App::Mission::resolve_tutorial_focus_points(
      {.world = &m_world,
       .local_owner_id = k_local_owner,
       .target = Game::Mission::TutorialFocusTarget::EnemyCamp,
       .owners = &m_session->owners(),
       .terrain = &m_session->terrain()});
  ASSERT_FALSE(points.isEmpty());
  const auto lead = points.front().toMap();
  const QVector3D first(
      lead.value("world_x").toFloat(), 0.0F, lead.value("world_z").toFloat());
  EXPECT_LT((first - position(roman_commander)).length(), 0.5F)
      << "the assault is won on the Roman commander; Show me must lead with him";
  for (const auto& value : points) {
    const auto point = value.toMap();
    const QVector3D at(
        point.value("world_x").toFloat(), 0.0F, point.value("world_z").toFloat());
    EXPECT_LT((at - position(roman_commander)).length(), 21.0F);
    EXPECT_GT((at - position(own_barracks)).length(), 30.0F)
        << "Show me framed the player's own camp";
  }
}
