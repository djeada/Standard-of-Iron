#include <QString>
#include <QVector3D>

#include <chrono>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "app/audio/audio_resource_loader.h"
#include "app/audio/audio_system_proxy.h"
#include "game/audio/audio_cues.h"
#include "game/audio/audio_event_handler.h"
#include "game/audio/cue_trace.h"
#include "game/audio/spatial.h"
#include "game/core/component_gameplay.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/render_bridge/selection_controller.h"
#include "game/systems/builder_product_types.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/combat_system/damage_application.h"
#include "game/systems/commander_system.h"
#include "game/systems/default_content.h"
#include "game/systems/gate_service.h"
#include "game/systems/gate_system.h"
#include "game/systems/healing_system.h"
#include "game/systems/nation_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/production_service.h"
#include "game/systems/production_system.h"
#include "game/systems/projectile_system.h"
#include "game/systems/selection_system.h"
#include "game/systems/wall_network_service.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"
#include "game/wildlife/bird_flock.h"
#include "game/wildlife/wildlife_config.h"
#include "game/wildlife/wildlife_system.h"

namespace {

using Engine::Core::Entity;
using Engine::Core::World;
using Game::Audio::CueTrace;

constexpr int k_local_owner = 1;
constexpr int k_enemy_owner = 2;

class AudioGameplayScenarioTest : public ::testing::Test {
protected:
  static void SetUpTestSuite() {
    auto& audio = AudioSystem::get_instance();
    audio.shutdown();
    s_audio_ready = audio.initialize();
    if (!s_audio_ready) {
      return;
    }

    s_saved_master = audio.get_master_volume();
    s_saved_sound = audio.get_sound_volume();
    audio.set_master_volume(1.0F);
    audio.set_sound_volume(1.0F);

    AudioResourceLoader::load_audio_resources(AudioLoadPolicy::Startup);
    AudioResourceLoader::load_audio_resources(AudioLoadPolicy::Mission);
    AudioResourceLoader::load_audio_cues();
  }

  static void TearDownTestSuite() {
    Game::Audio::CueRegistry::instance().clear();
    auto& audio = AudioSystem::get_instance();
    if (s_audio_ready) {
      audio.set_master_volume(s_saved_master);
      audio.set_sound_volume(s_saved_sound);
    }
    audio.shutdown();
    s_audio_ready = false;
  }

  void SetUp() override {
    if (!s_audio_ready) {
      GTEST_SKIP() << "Audio backend is unavailable in this environment";
    }

    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();

    Game::Map::MapDefinition map_definition;
    map_definition.grid.width = 64;
    map_definition.grid.height = 64;
    map_definition.grid.tile_size = 1.0F;
    Game::Map::TerrainService::instance().initialize(map_definition);
    Game::Systems::NavGrid::initialize(map_definition.grid.width,
                                       map_definition.grid.height);

    auto& resources = Game::Systems::PlayerResourceRegistry::instance();
    resources.clear();
    resources.set(k_local_owner, Game::Systems::ResourceType::Wood, 1000);
    resources.set(k_local_owner, Game::Systems::ResourceType::Stone, 1000);
    resources.set(k_local_owner, Game::Systems::ResourceType::Iron, 1000);

    auto& nations = Game::Systems::NationRegistry::instance();
    nations.clear();
    Game::Systems::initialize_default_content(nations);
    nations.set_player_nation(k_local_owner, Game::Systems::NationID::RomanRepublic);

    auto& owners = Game::Systems::OwnerRegistry::instance();
    owners.clear();
    owners.register_owner_with_id(
        k_local_owner, Game::Systems::OwnerType::Player, "Defender");
    owners.register_owner_with_id(
        k_enemy_owner, Game::Systems::OwnerType::AI, "Raider");
    owners.set_owner_team(k_local_owner, 1);
    owners.set_owner_team(k_enemy_owner, 2);
    owners.set_local_player_id(k_local_owner);

    AudioResourceLoader::load_audio_cues();
    Game::Audio::CueRegistry::instance().reset_cooldowns();
    AudioSystem::get_instance().reset_playback_throttles();
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    CueTrace::instance().reset();

    m_handler = std::make_unique<Game::Audio::AudioEventHandler>(&m_world);
    ASSERT_TRUE(m_handler->initialize());
    m_handler->set_local_owner_id(k_local_owner);
  }

  void TearDown() override {
    m_handler.reset();
    Game::Wildlife::BirdFlockManager::instance().reset();
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::PlayerResourceRegistry::instance().clear();
    Game::Systems::GateService::clear_blockers();
    Game::Systems::OwnerRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    CueTrace::instance().reset();
  }

  static auto heard(const char* cue_id) -> bool {
    for (int attempt = 0; attempt < 200; ++attempt) {
      if (CueTrace::instance().record_for(cue_id).accepted > 0U) {
        return true;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return false;
  }

  static auto requests(const char* cue_id) -> std::uint64_t {
    return CueTrace::instance().record_for(cue_id).requests;
  }

  auto add_soldier(int owner_id, Game::Units::SpawnType type, float x) -> Entity* {
    auto* entity = m_world.create_entity();
    auto* transform =
        entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, 0.0F);
    transform->scale = {0.55F, 0.55F, 0.55F};
    auto* unit =
        entity->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
    unit->owner_id = owner_id;
    unit->spawn_type = type;
    auto* attack = entity->add_component<Engine::Core::AttackComponent>();
    attack->can_melee = true;
    attack->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;
    return entity;
  }

  auto add_barracks(int owner_id, float x) -> Entity* {
    auto* entity = m_world.create_entity();
    entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, 0.0F);
    auto* unit =
        entity->add_component<Engine::Core::UnitComponent>(900, 900, 0.0F, 0.0F);
    unit->owner_id = owner_id;
    unit->spawn_type = Game::Units::SpawnType::Barracks;
    entity->add_component<Engine::Core::BuildingComponent>();
    return entity;
  }

  auto add_gate(int owner_id, float x, float z) -> Entity* {
    auto* entity = m_world.create_entity();
    entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, z);
    entity->add_component<Engine::Core::RenderableComponent>();
    auto* unit =
        entity->add_component<Engine::Core::UnitComponent>(700, 700, 0.0F, 0.0F);
    unit->owner_id = owner_id;
    unit->spawn_type = Game::Units::SpawnType::WallGate;
    entity->add_component<Engine::Core::BuildingComponent>();
    auto* wall = entity->add_component<Engine::Core::WallSegmentComponent>();
    const auto snapped = Game::Systems::WallNetworkService::snap_world_position(x, z);
    wall->grid_x = snapped.x;
    wall->grid_z = snapped.z;
    entity->add_component<Engine::Core::GateComponent>();
    const auto extent = Game::Systems::GateService::structure_extent(0.0F);
    Game::Systems::BuildingCollisionRegistry::instance().register_building(
        entity->get_id(),
        "wall_gate",
        x,
        z,
        owner_id,
        {.width = extent.half_x * 2.0F, .depth = extent.half_z * 2.0F});
    return entity;
  }

  void tick_gates(float seconds) {
    Game::Systems::GateSystem system;
    for (float elapsed = 0.0F; elapsed < seconds; elapsed += 0.05F) {
      system.update(&m_world, 0.05F);
    }
  }

  World m_world;
  std::unique_ptr<Game::Audio::AudioEventHandler> m_handler;

  static bool s_audio_ready;
  static float s_saved_master;
  static float s_saved_sound;
};

bool AudioGameplayScenarioTest::s_audio_ready = false;
float AudioGameplayScenarioTest::s_saved_master = 1.0F;
float AudioGameplayScenarioTest::s_saved_sound = 1.0F;

TEST_F(AudioGameplayScenarioTest, ASwordLandingOnAPlayerSoldierIsHeard) {
  auto* attacker = add_soldier(k_enemy_owner, Game::Units::SpawnType::Knight, 0.0F);
  auto* target = add_soldier(k_local_owner, Game::Units::SpawnType::Spearman, 2.0F);

  Game::Systems::Combat::apply_unit_damage(&m_world, target, 10, attacker->get_id());

  EXPECT_TRUE(heard(Game::Audio::Cue::k_combat_hit_sword))
      << "a knight struck a player soldier and the blow was silent";
}

TEST_F(AudioGameplayScenarioTest, TheAttackersWeaponPicksTheImpactSound) {
  auto* attacker = add_soldier(k_enemy_owner, Game::Units::SpawnType::Spearman, 0.0F);
  auto* target = add_soldier(k_local_owner, Game::Units::SpawnType::Knight, 2.0F);

  Game::Systems::Combat::apply_unit_damage(&m_world, target, 10, attacker->get_id());

  EXPECT_TRUE(heard(Game::Audio::Cue::k_combat_hit_spear));
  EXPECT_EQ(requests(Game::Audio::Cue::k_combat_hit_sword), 0U)
      << "a spear thrust played the sword impact";
}

TEST_F(AudioGameplayScenarioTest, ADyingSoldierIsHeardAndReportedLost) {
  auto* attacker = add_soldier(k_enemy_owner, Game::Units::SpawnType::Knight, 0.0F);
  auto* target = add_soldier(k_local_owner, Game::Units::SpawnType::Spearman, 2.0F);

  Game::Systems::Combat::apply_unit_damage(&m_world, target, 10000, attacker->get_id());

  EXPECT_TRUE(heard(Game::Audio::Cue::k_combat_death));
  EXPECT_TRUE(heard(Game::Audio::Cue::k_alert_unit_lost))
      << "the player lost a soldier and was never told";
}

TEST_F(AudioGameplayScenarioTest, KillingAnEnemyDoesNotRaiseTheLostUnitAlert) {
  auto* attacker = add_soldier(k_local_owner, Game::Units::SpawnType::Knight, 0.0F);
  auto* target = add_soldier(k_enemy_owner, Game::Units::SpawnType::Spearman, 2.0F);

  Game::Systems::Combat::apply_unit_damage(&m_world, target, 10000, attacker->get_id());

  EXPECT_TRUE(heard(Game::Audio::Cue::k_combat_death));
  EXPECT_EQ(requests(Game::Audio::Cue::k_alert_unit_lost), 0U)
      << "killing an enemy told the player they had lost someone";
}

TEST_F(AudioGameplayScenarioTest, TwoRivalArmiesFightingIsNotThePlayersBusiness) {
  auto* attacker = add_soldier(k_enemy_owner, Game::Units::SpawnType::Knight, 0.0F);
  auto* target = add_soldier(3, Game::Units::SpawnType::Spearman, 2.0F);

  Game::Systems::Combat::apply_unit_damage(&m_world, target, 10, attacker->get_id());

  const auto record =
      CueTrace::instance().record_for(Game::Audio::Cue::k_combat_hit_sword);
  EXPECT_EQ(record.accepted, 0U) << "the player heard a fight they cannot see";
  EXPECT_EQ(record.outcomes.at(
                static_cast<std::size_t>(Game::Audio::CueOutcome::AudienceFiltered)),
            0U)
      << "the handler filtered by audience before the cue was ever requested, so this "
         "drop should not be attributed to the cue path";
}

TEST_F(AudioGameplayScenarioTest, AnAttackOnThePlayersBuildingRaisesTheAlarm) {
  auto* attacker = add_soldier(k_enemy_owner, Game::Units::SpawnType::Knight, 0.0F);
  auto* barracks = add_barracks(k_local_owner, 3.0F);

  Game::Systems::Combat::apply_unit_damage(&m_world, barracks, 25, attacker->get_id());

  EXPECT_TRUE(heard(Game::Audio::Cue::k_alert_base_under_attack))
      << "the player's barracks was struck and no alarm was raised";
}

TEST_F(AudioGameplayScenarioTest, AGateOpeningAndClosingIsHeard) {
  auto* gate = add_gate(k_local_owner, 0.0F, 0.0F);
  auto* troop = add_soldier(k_local_owner, Game::Units::SpawnType::Spearman, 0.0F);
  troop->get_component<Engine::Core::TransformComponent>()->position.z = -3.0F;

  tick_gates(2.0F);
  ASSERT_TRUE(gate->get_component<Engine::Core::GateComponent>()->is_passable());
  EXPECT_TRUE(heard(Game::Audio::Cue::k_build_gate_open))
      << "the gate swung open in silence";

  troop->get_component<Engine::Core::TransformComponent>()->position.z = -20.0F;
  tick_gates(4.0F);

  EXPECT_TRUE(heard(Game::Audio::Cue::k_build_gate_close))
      << "the gate shut in silence";
}

TEST_F(AudioGameplayScenarioTest, SelectingAndDeselectingTroopsIsHeard) {
  m_world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
  auto* selection_system = m_world.get_system<Game::Systems::SelectionSystem>();
  ASSERT_NE(selection_system, nullptr);
  Game::Systems::SelectionController controller(&m_world, selection_system, nullptr);

  auto* first = add_soldier(k_local_owner, Game::Units::SpawnType::Spearman, 0.0F);
  add_soldier(k_local_owner, Game::Units::SpawnType::Spearman, 2.0F);

  controller.select_single_unit(first->get_id(), k_local_owner);
  EXPECT_TRUE(heard(Game::Audio::Cue::k_ui_select_unit))
      << "clicking a soldier gave no acknowledgement";

  controller.select_all_player_troops(k_local_owner);
  EXPECT_TRUE(heard(Game::Audio::Cue::k_ui_select_group))
      << "selecting the whole army sounded like selecting one man";

  controller.on_right_click_clear_selection();
  EXPECT_TRUE(heard(Game::Audio::Cue::k_ui_deselect));
}

TEST_F(AudioGameplayScenarioTest, ABuilderReachingItsSiteStartsTheWorkSound) {
  auto* builder = m_world.create_entity();
  builder->add_component<Engine::Core::TransformComponent>(4.0F, 0.0F, 4.0F);
  builder->add_component<Engine::Core::MovementComponent>();
  auto* unit = builder->add_component<Engine::Core::UnitComponent>();
  unit->owner_id = k_local_owner;
  unit->spawn_type = Game::Units::SpawnType::Builder;

  auto* production = builder->add_component<Engine::Core::BuilderProductionComponent>();
  production->product_type = "barracks";
  production->has_construction_site = true;
  production->at_construction_site = false;
  production->construction_site_x = 4.0F;
  production->construction_site_z = 4.0F;

  Game::Systems::ProductionSystem system;
  system.update(&m_world, 0.1F);

  EXPECT_TRUE(heard(Game::Audio::Cue::k_build_construction_started))
      << "the builder started work on site and nothing was heard";
}

TEST_F(AudioGameplayScenarioTest, AWolfPackStartingAHuntIsHeard) {
  Game::Wildlife::WildlifeSettings settings = Game::Wildlife::default_settings();
  settings.enabled = true;
  settings.seed = 4242U;
  settings.sheep.enabled = true;
  settings.sheep.group_count = 1;
  settings.sheep.group_size_min = 4;
  settings.sheep.group_size_max = 4;
  settings.sheep.roam_radius = 8.0F;
  settings.sheep.alert_radius = 9.0F;
  settings.sheep.spawn_areas = {{0.0F, 0.0F, 2.0F}};
  settings.wolves.enabled = true;
  settings.wolves.group_count = 1;
  settings.wolves.group_size_min = 1;
  settings.wolves.group_size_max = 1;
  settings.wolves.aggression = 1.0F;
  settings.wolves.roam_radius = 12.0F;
  settings.wolves.spawn_areas = {{4.0F, 0.0F, 1.0F}};
  settings.birds.enabled = false;
  settings.birds.group_count = 0;

  Game::Wildlife::WildlifeSystem system;
  system.configure(settings, 1U);
  system.update(&m_world, 0.1F);

  for (float elapsed = 0.0F; elapsed < 2.0F; elapsed += 0.1F) {
    system.update(&m_world, 0.1F);
  }

  EXPECT_GT(system.stats().hunt_events, 0U) << "no hunt started, so nothing to hear";
  EXPECT_TRUE(heard(Game::Audio::Cue::k_wildlife_wolf_hunt))
      << "the pack began a hunt in silence";
}

TEST_F(AudioGameplayScenarioTest, MountedAndElephantAttacksHaveTheirOwnImpacts) {
  auto* rider = add_soldier(k_enemy_owner, Game::Units::SpawnType::MountedKnight, 0.0F);
  auto* target = add_soldier(k_local_owner, Game::Units::SpawnType::Spearman, 2.0F);
  Game::Systems::Combat::apply_unit_damage(&m_world, target, 5, rider->get_id());
  EXPECT_TRUE(heard(Game::Audio::Cue::k_combat_hit_cavalry));

  auto* elephant = add_soldier(k_enemy_owner, Game::Units::SpawnType::Elephant, 4.0F);
  auto* second = add_soldier(k_local_owner, Game::Units::SpawnType::Spearman, 6.0F);
  Game::Systems::Combat::apply_unit_damage(&m_world, second, 5, elephant->get_id());
  EXPECT_TRUE(heard(Game::Audio::Cue::k_combat_hit_elephant));
}

TEST_F(AudioGameplayScenarioTest, ABuildingComingDownIsHeardAsAStructureNotASoldier) {
  auto* attacker = add_soldier(k_enemy_owner, Game::Units::SpawnType::Knight, 0.0F);
  auto* barracks = add_barracks(k_local_owner, 3.0F);

  Game::Systems::Combat::apply_unit_damage(
      &m_world, barracks, 10000, attacker->get_id());

  EXPECT_TRUE(heard(Game::Audio::Cue::k_build_building_destroyed))
      << "the player's barracks was levelled without a sound";
  EXPECT_EQ(requests(Game::Audio::Cue::k_combat_death), 0U)
      << "a falling building cried out like a man";
}

TEST_F(AudioGameplayScenarioTest, AnArrowLeavingTheBowIsHeard) {
  auto* archer = add_soldier(k_local_owner, Game::Units::SpawnType::Archer, 0.0F);
  auto* target = add_soldier(k_enemy_owner, Game::Units::SpawnType::Spearman, 8.0F);

  Game::Systems::ProjectileSystem projectiles;
  projectiles.spawn_arrow(QVector3D(0.0F, 1.0F, 0.0F),
                          QVector3D(8.0F, 1.0F, 0.0F),
                          QVector3D(1.0F, 1.0F, 1.0F),
                          12.0F,
                          false,
                          Game::Systems::ProjectileKind::Arrow,
                          false,
                          0,
                          archer->get_id(),
                          target->get_id());
  projectiles.update(&m_world, 0.05F);

  EXPECT_TRUE(heard(Game::Audio::Cue::k_combat_arrow_launch))
      << "an arrow left the bow in silence";
}

TEST_F(AudioGameplayScenarioTest, TheInterfaceAnswersEveryButtonItIsGiven) {
  App::Models::AudioSystemProxy proxy;

  proxy.play_ui_hover();
  EXPECT_TRUE(heard(Game::Audio::Cue::k_ui_hover));

  proxy.play_ui_click();
  EXPECT_TRUE(heard(Game::Audio::Cue::k_ui_click));

  proxy.play_ui_back();
  EXPECT_TRUE(heard(Game::Audio::Cue::k_ui_back));

  proxy.play_ui_error();
  EXPECT_TRUE(heard(Game::Audio::Cue::k_ui_error));

  proxy.play_cue(QStringLiteral("ui.notification"));
  EXPECT_TRUE(heard(Game::Audio::Cue::k_ui_notification));
}

TEST_F(AudioGameplayScenarioTest, AVolleyOfArrowsIsHeardAsAVolley) {
  auto* archer = add_soldier(k_local_owner, Game::Units::SpawnType::Archer, 0.0F);

  Game::Systems::ProjectileSystem projectiles;
  for (int arrow = 0; arrow < 12; ++arrow) {
    projectiles.spawn_arrow(QVector3D(0.0F, 1.0F, static_cast<float>(arrow)),
                            QVector3D(8.0F, 1.0F, static_cast<float>(arrow)),
                            QVector3D(1.0F, 1.0F, 1.0F),
                            12.0F,
                            false,
                            Game::Systems::ProjectileKind::Arrow,
                            false,
                            0,
                            archer->get_id());
  }
  projectiles.update(&m_world, 0.05F);

  EXPECT_TRUE(heard(Game::Audio::Cue::k_combat_arrow_volley))
      << "a dozen arrows went up and the sky stayed quiet";
}

TEST_F(AudioGameplayScenarioTest, ASiegeShotIsHeardLeavingTheEngine) {
  auto* engine = add_soldier(k_local_owner, Game::Units::SpawnType::Ballista, 0.0F);

  Game::Systems::ProjectileSystem projectiles;
  projectiles.spawn_arrow(QVector3D(0.0F, 1.0F, 0.0F),
                          QVector3D(12.0F, 1.0F, 0.0F),
                          QVector3D(1.0F, 1.0F, 1.0F),
                          14.0F,
                          true,
                          Game::Systems::ProjectileKind::Arrow,
                          false,
                          0,
                          engine->get_id());
  projectiles.update(&m_world, 0.05F);

  EXPECT_TRUE(heard(Game::Audio::Cue::k_combat_siege_launch))
      << "the ballista loosed a bolt in silence";
}

TEST_F(AudioGameplayScenarioTest, AHealerMendingASoldierIsHeard) {
  auto* healer = m_world.create_entity();
  healer->add_component<Engine::Core::TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* healer_unit =
      healer->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  healer_unit->owner_id = k_local_owner;
  auto* healing = healer->add_component<Engine::Core::HealerComponent>();
  healing->healing_range = 8.0F;
  healing->healing_amount = 20;
  healing->healing_cooldown = 1.0F;
  healing->time_since_last_heal = 1.0F;

  auto* wounded = m_world.create_entity();
  wounded->add_component<Engine::Core::TransformComponent>(2.0F, 0.0F, 0.0F);
  auto* wounded_unit =
      wounded->add_component<Engine::Core::UnitComponent>(40, 100, 1.0F, 12.0F);
  wounded_unit->owner_id = k_local_owner;

  Game::Systems::HealingSystem system;
  system.update(&m_world, 0.1F);

  ASSERT_GT(wounded_unit->health, 40) << "nothing was healed, so nothing to hear";
  EXPECT_TRUE(heard(Game::Audio::Cue::k_combat_heal));
}

TEST_F(AudioGameplayScenarioTest, PlantingTheCommandersStandardSoundsTheRally) {
  auto* commander_entity = m_world.create_entity();
  commander_entity->add_component<Engine::Core::TransformComponent>(4.0F, 0.0F, 4.0F);
  auto* commander_unit = commander_entity->add_component<Engine::Core::UnitComponent>(
      200, 200, 1.0F, 12.0F);
  commander_unit->owner_id = k_local_owner;
  auto* commander = commander_entity->add_component<Engine::Core::CommanderComponent>();
  commander->begin_flag_rally(4.0F, 4.0F, true);

  Game::Systems::CommanderSystem system;
  system.update(&m_world, commander->flag_rally_cost + 0.1F);

  ASSERT_TRUE(commander->flag_rally_flag_active)
      << "the standard was never planted, so there is no rally to hear";
  EXPECT_TRUE(heard(Game::Audio::Cue::k_order_commander_rally));
}

TEST_F(AudioGameplayScenarioTest, QueueingATroopAcknowledgesTheOrder) {
  auto* barracks = m_world.create_entity();
  barracks->add_component<Engine::Core::TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* unit = barracks->add_component<Engine::Core::UnitComponent>();
  unit->spawn_type = Game::Units::SpawnType::Barracks;
  unit->owner_id = k_local_owner;
  auto* production = barracks->add_component<Engine::Core::ProductionComponent>();
  production->max_units = 10000;
  production->manpower_available = 1000;

  const auto result = Game::Systems::ProductionService::start_production(
      m_world, barracks->get_id(), Game::Units::TroopType::Swordsman);
  ASSERT_EQ(result, Game::Systems::ProductionResult::Success)
      << "the barracks refused the order, so there is no cue to judge";

  EXPECT_TRUE(heard(Game::Audio::Cue::k_build_unit_queued))
      << "a troop went into the queue with no acknowledgement";
}

TEST_F(AudioGameplayScenarioTest, ABlowOnAWallSoundsLikeStoneNotLikeAMan) {
  auto* attacker = add_soldier(k_enemy_owner, Game::Units::SpawnType::Knight, 0.0F);
  auto* barracks = add_barracks(k_local_owner, 3.0F);

  Game::Systems::Combat::apply_unit_damage(&m_world, barracks, 20, attacker->get_id());

  EXPECT_TRUE(heard(Game::Audio::Cue::k_combat_hit_structure))
      << "a sword on masonry played nothing of the kind";
  EXPECT_EQ(requests(Game::Audio::Cue::k_combat_hit_sword), 0U)
      << "hitting a building sounded like hitting a shield";
}

TEST_F(AudioGameplayScenarioTest, ASiegeEngineKeepsItsOwnImpactAgainstAWall) {
  auto* engine = add_soldier(k_enemy_owner, Game::Units::SpawnType::Catapult, 0.0F);
  auto* barracks = add_barracks(k_local_owner, 3.0F);

  Game::Systems::Combat::apply_unit_damage(&m_world, barracks, 20, engine->get_id());

  EXPECT_TRUE(heard(Game::Audio::Cue::k_combat_hit_siege))
      << "a catapult hit lost its own weight to the generic masonry cue";
}

TEST_F(AudioGameplayScenarioTest, FinishingWorkOnAStructureIsHeard) {
  auto* barracks = add_barracks(k_local_owner, 6.0F);

  auto* builder = m_world.create_entity();
  builder->add_component<Engine::Core::TransformComponent>(6.0F, 0.0F, 0.0F);
  builder->add_component<Engine::Core::MovementComponent>();
  auto* unit = builder->add_component<Engine::Core::UnitComponent>();
  unit->owner_id = k_local_owner;
  unit->spawn_type = Game::Units::SpawnType::Builder;

  auto* production = builder->add_component<Engine::Core::BuilderProductionComponent>();
  production->product_type = std::string(Game::Systems::k_builder_product_repair);
  production->in_progress = true;
  production->time_remaining = 0.0F;
  production->structure_task_entity_id = barracks->get_id();

  Game::Systems::ProductionSystem system;
  system.update(&m_world, 0.1F);

  EXPECT_TRUE(production->construction_complete)
      << "the work never finished, so there is no completion to hear";
  EXPECT_TRUE(heard(Game::Audio::Cue::k_build_construction_complete))
      << "the builder downed tools in silence";
}

TEST_F(AudioGameplayScenarioTest, AFightAcrossTheMapIsNotHeardAtTheCamera) {
  AudioSystem::get_instance().set_listener({.position = {0.0F, 40.0F, 0.0F},
                                            .right_x = 1.0F,
                                            .right_z = 0.0F,
                                            .valid = true});

  auto* attacker = add_soldier(k_enemy_owner, Game::Units::SpawnType::Knight, 400.0F);
  auto* target = add_soldier(k_local_owner, Game::Units::SpawnType::Spearman, 402.0F);

  Game::Systems::Combat::apply_unit_damage(&m_world, target, 10, attacker->get_id());

  for (int settle = 0; settle < 60; ++settle) {
    if (requests(Game::Audio::Cue::k_combat_hit_sword) > 0U) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  const auto record =
      CueTrace::instance().record_for(Game::Audio::Cue::k_combat_hit_sword);
  EXPECT_EQ(record.requests, 1U) << "the blow never reached the audio system";
  EXPECT_EQ(record.accepted, 0U)
      << "a fight 400 metres away took a voice from the mixer";
  EXPECT_EQ(
      record.outcomes.at(static_cast<std::size_t>(Game::Audio::CueOutcome::Muted)), 1U)
      << "distance should silence it, and say so";

  AudioSystem::get_instance().set_listener({});
}

TEST_F(AudioGameplayScenarioTest, AFightAtTheCameraIsHeardInFull) {
  AudioSystem::get_instance().set_listener({.position = {2.0F, 8.0F, 0.0F},
                                            .right_x = 1.0F,
                                            .right_z = 0.0F,
                                            .valid = true});

  auto* attacker = add_soldier(k_enemy_owner, Game::Units::SpawnType::Knight, 0.0F);
  auto* target = add_soldier(k_local_owner, Game::Units::SpawnType::Spearman, 2.0F);

  Game::Systems::Combat::apply_unit_damage(&m_world, target, 10, attacker->get_id());

  EXPECT_TRUE(heard(Game::Audio::Cue::k_combat_hit_sword))
      << "a blow under the camera was silenced by the spatial mix";

  AudioSystem::get_instance().set_listener({});
}

TEST_F(AudioGameplayScenarioTest, ADistantBattleIsCarriedAsOneMassNotAsSilence) {
  AudioSystem::get_instance().set_listener({.position = {0.0F, 40.0F, 0.0F},
                                            .right_x = 1.0F,
                                            .right_z = 0.0F,
                                            .valid = true});

  auto* attacker = add_soldier(k_enemy_owner, Game::Units::SpawnType::Knight, 400.0F);
  auto* target = add_soldier(k_local_owner, Game::Units::SpawnType::Spearman, 402.0F);

  for (int blow = 0; blow < 8; ++blow) {
    Game::Systems::Combat::apply_unit_damage(&m_world, target, 1, attacker->get_id());
  }

  EXPECT_TRUE(heard(Game::Audio::Cue::k_combat_distant_battle))
      << "a battle raged out of earshot and the player heard nothing at all";
  EXPECT_EQ(
      CueTrace::instance().record_for(Game::Audio::Cue::k_combat_hit_sword).accepted,
      0U)
      << "the individual blows should still be silent at that distance";

  AudioSystem::get_instance().set_listener({});
}

TEST_F(AudioGameplayScenarioTest, ATradedBlowOrTwoFarAwayIsNotABattle) {
  AudioSystem::get_instance().set_listener({.position = {0.0F, 40.0F, 0.0F},
                                            .right_x = 1.0F,
                                            .right_z = 0.0F,
                                            .valid = true});

  auto* attacker = add_soldier(k_enemy_owner, Game::Units::SpawnType::Knight, 400.0F);
  auto* target = add_soldier(k_local_owner, Game::Units::SpawnType::Spearman, 402.0F);

  Game::Systems::Combat::apply_unit_damage(&m_world, target, 1, attacker->get_id());
  Game::Systems::Combat::apply_unit_damage(&m_world, target, 1, attacker->get_id());

  std::this_thread::sleep_for(std::chrono::milliseconds(120));
  EXPECT_EQ(requests(Game::Audio::Cue::k_combat_distant_battle), 0U)
      << "two blows in the distance summoned a whole battle";

  AudioSystem::get_instance().set_listener({});
}

TEST_F(AudioGameplayScenarioTest, FightingUnderTheCameraNeverBecomesADistantBed) {
  AudioSystem::get_instance().set_listener({.position = {2.0F, 8.0F, 0.0F},
                                            .right_x = 1.0F,
                                            .right_z = 0.0F,
                                            .valid = true});

  auto* attacker = add_soldier(k_enemy_owner, Game::Units::SpawnType::Knight, 0.0F);
  auto* target = add_soldier(k_local_owner, Game::Units::SpawnType::Spearman, 2.0F);

  for (int blow = 0; blow < 10; ++blow) {
    Game::Systems::Combat::apply_unit_damage(&m_world, target, 1, attacker->get_id());
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(120));
  EXPECT_EQ(requests(Game::Audio::Cue::k_combat_distant_battle), 0U)
      << "a melee at the camera was replaced by a distant rumble";

  AudioSystem::get_instance().set_listener({});
}

TEST_F(AudioGameplayScenarioTest, EveryScenarioCueReachedTheMixerFromRealGameplay) {
  auto* attacker = add_soldier(k_enemy_owner, Game::Units::SpawnType::Knight, 0.0F);
  auto* target = add_soldier(k_local_owner, Game::Units::SpawnType::Spearman, 2.0F);

  Game::Systems::Combat::apply_unit_damage(&m_world, target, 10, attacker->get_id());
  ASSERT_TRUE(heard(Game::Audio::Cue::k_combat_hit_sword));

  const auto silent = CueTrace::instance().never_accepted_cues();
  std::string joined;
  for (const auto& cue_id : silent) {
    joined += cue_id + " ";
  }
  EXPECT_TRUE(silent.empty()) << "gameplay asked for cues the player never heard: "
                              << joined;
}

} // namespace
