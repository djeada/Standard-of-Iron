#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/default_content.h"
#include "game/systems/nation_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/pathfinding.h"
#include "game/systems/runtime_system_registry.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "game/wildlife/wildlife_config.h"
#include "game/wildlife/wildlife_system.h"

namespace {

using Engine::Core::AttackComponent;
using Engine::Core::AttackTargetComponent;
using Engine::Core::EntityID;
using Engine::Core::MovementComponent;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;
using Engine::Core::WildlifeComponent;
using Game::Session::SessionContext;
using Game::Systems::NavGrid;

constexpr int k_player = 1;
constexpr int k_map = 64;

struct Track {
  float x{0.0F};
  float z{0.0F};
  float yaw{0.0F};
  float travelled{0.0F};
  float bite_slide{0.0F};
  bool primed{false};
};

class WildlifeFightMotionTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::initialize_default_content(
        Game::Systems::NationRegistry::instance());
    NavGrid::initialize(k_map, k_map);
    m_factory = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*m_factory);
  }

  void TearDown() override {
    m_handles.clear();
    m_scope.reset();
    m_session.reset();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
  }

  void field() {
    Game::Map::MapDefinition map;
    map.grid.width = k_map;
    map.grid.height = k_map;
    map.grid.tile_size = 1.0F;
    map.biome.procedural_boulders_enabled = false;
    map.biome.procedural_iron_ore_enabled = false;
    map.biome.procedural_trees_enabled = false;

    m_session = std::make_unique<SessionContext>();
    m_session->world().set_presentation_enabled(false);
    m_scope = std::make_unique<Game::Session::ScopedSession>(*m_session);
    m_session->owners().register_owner_with_id(
        k_player, Game::Systems::OwnerType::Player, "player");
    m_session->owners().set_owner_team(k_player, 1);
    m_session->owners().set_local_player_id(k_player);
    Game::Systems::initialize_default_content(m_session->nations());
    m_session->nations().set_player_nation(k_player,
                                           Game::Systems::NationID::RomanRepublic);
    Game::Systems::register_runtime_systems(m_session->world());
    m_session->terrain().initialize(map);
    NavGrid::initialize(map.grid.width, map.grid.height);
    if (auto* pathfinder = NavGrid::get_pathfinder()) {
      pathfinder->mark_navigation_grid_dirty();
      pathfinder->update_navigation_grid();
    }
  }

  void spawn(Game::Units::SpawnType type, const QVector3D& position) {
    Game::Units::SpawnParams params;
    params.position = position;
    params.player_id = k_player;
    params.spawn_type = type;
    params.nation_id = Game::Systems::NationID::RomanRepublic;
    if (auto unit = m_factory->create(type, m_session->world(), params)) {
      m_handles.push_back(std::move(unit));
    }
  }

  void release_wildlife() {
    auto* system = m_session->world().get_system<Game::Wildlife::WildlifeSystem>();
    ASSERT_NE(system, nullptr);
    Game::Wildlife::WildlifeSettings settings = Game::Wildlife::default_settings();
    settings.enabled = true;
    settings.seed = 99U;
    settings.sheep.enabled = true;
    settings.sheep.group_count = 1;
    settings.sheep.group_size_min = 5;
    settings.sheep.group_size_max = 5;
    settings.sheep.roam_radius = 7.0F;
    settings.sheep.spawn_areas = {{32.0F, 0.0F, 2.5F}};
    settings.wolves.enabled = true;
    settings.wolves.group_count = 1;
    settings.wolves.group_size_min = 3;
    settings.wolves.group_size_max = 3;
    settings.wolves.aggression = 1.0F;
    settings.wolves.roam_radius = 16.0F;
    settings.wolves.alert_radius = 16.0F;
    settings.wolves.spawn_areas = {{40.0F, 0.0F, 2.0F}};
    settings.birds.enabled = false;
    settings.birds.group_count = 0;
    Game::Wildlife::sanitize(settings);
    system->configure(settings, 99U);
  }

  void stage_a_fight() {
    field();
    release_wildlife();
    for (int index = 0; index < 4; ++index) {
      spawn(Game::Units::SpawnType::Civilian,
            QVector3D(30.0F + (static_cast<float>(index) * 1.2F), 0.0F, 34.0F));
    }
    for (int index = 0; index < 3; ++index) {
      spawn(Game::Units::SpawnType::Spearman,
            QVector3D(38.0F + (static_cast<float>(index) * 1.2F), 0.0F, 30.0F));
    }
  }

  [[nodiscard]] auto tick_seconds() const -> float {
    return static_cast<float>(m_session->clock().tick_seconds());
  }

  void step_once() {
    const double step = m_session->clock().tick_seconds();
    m_session->clock().advance(step);
    while (m_session->clock().consume_tick()) {
      m_session->world().update(static_cast<float>(step));
    }
  }

  [[nodiscard]] auto animals() -> std::vector<Engine::Core::Entity*> {
    return m_session->world().collect_entities_with<WildlifeComponent>();
  }

  [[nodiscard]] auto bites() const -> unsigned {
    const auto* system =
        m_session->world().get_system<Game::Wildlife::WildlifeSystem>();
    return system != nullptr ? system->stats().bites : 0U;
  }

  std::unique_ptr<SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
  std::vector<std::unique_ptr<Game::Units::Unit>> m_handles;
};

} // namespace

TEST_F(WildlifeFightMotionTest, AnimalsNeverPivotFasterThanTheirOwnBodies) {
  stage_a_fight();

  const float step = tick_seconds();
  std::map<EntityID, Track> tracks;
  float worst_share = 0.0F;
  float worst_rate = 0.0F;
  float worst_allowance = 1.0F;
  std::string worst_species;

  for (float elapsed = 0.0F; elapsed < 120.0F; elapsed += step) {
    step_once();
    for (auto* animal : animals()) {
      const auto* unit = animal->get_component<UnitComponent>();
      const auto* transform = animal->get_component<TransformComponent>();
      if (unit == nullptr || transform == nullptr) {
        continue;
      }
      Track& track = tracks[animal->get_id()];
      const float previous = track.primed ? track.yaw : transform->rotation.y;
      track.yaw = transform->rotation.y;
      track.primed = true;

      const float turned = std::fabs(
          std::fmod((transform->rotation.y - previous + 540.0F), 360.0F) - 180.0F);
      const float rate = turned / step;

      const float allowance =
          Game::Units::body_turn_speed_degrees(unit->spawn_type) * 1.10F;
      if (rate / allowance > worst_share) {
        worst_share = rate / allowance;
        worst_rate = rate;
        worst_allowance = allowance;
        worst_species =
            unit->spawn_type == Game::Units::SpawnType::Wolf ? "wolf" : "sheep";
      }
    }
  }

  ASSERT_GT(bites(), 0U) << "the pack never reached the herd, so nothing was proved";
  EXPECT_LT(worst_share, 1.0F)
      << "a " << worst_species << " span at " << worst_rate << " deg/s, past the "
      << worst_allowance
      << " its body allows. Every system that turns a body clamps to the same "
         "table, so a rate above it means two of them stepped the same animal "
         "in one tick";
}

TEST_F(WildlifeFightMotionTest, AnimalsCoverNoGroundTheirWalkCycleDoesNotKnowAbout) {
  stage_a_fight();

  const float step = tick_seconds();
  std::map<EntityID, Track> tracks;
  float worst_skate = 0.0F;
  float total_skate = 0.0F;

  for (float elapsed = 0.0F; elapsed < 120.0F; elapsed += step) {
    step_once();
    for (auto* animal : animals()) {
      const auto* transform = animal->get_component<TransformComponent>();
      const auto* walk = animal->get_component<MovementComponent>();
      const auto* unit = animal->get_component<UnitComponent>();
      if (transform == nullptr || walk == nullptr || unit == nullptr ||
          unit->health <= 0) {
        continue;
      }
      Track& track = tracks[animal->get_id()];
      if (!track.primed) {
        track = {transform->position.x,
                 transform->position.z,
                 transform->rotation.y,
                 walk->get_travelled(),
                 0.0F,
                 true};
        continue;
      }

      const float dx = transform->position.x - track.x;
      const float dz = transform->position.z - track.z;
      const float moved = std::sqrt((dx * dx) + (dz * dz));
      const float stepped = std::max(0.0F, walk->get_travelled() - track.travelled);
      track.x = transform->position.x;
      track.z = transform->position.z;
      track.travelled = walk->get_travelled();

      const float skate = std::max(0.0F, moved - stepped - 0.002F);
      worst_skate = std::max(worst_skate, skate);
      total_skate += skate;
    }
  }

  ASSERT_GT(bites(), 0U) << "the pack never reached the herd, so nothing was proved";
  EXPECT_LT(total_skate, 0.5F)
      << "animals slid " << total_skate
      << " m without their walk cycle advancing (worst single tick " << worst_skate
      << " m); on screen that is a body gliding with its legs frozen";
}

TEST_F(WildlifeFightMotionTest, AWolfBitesFromAStandstill) {
  stage_a_fight();

  const float step = tick_seconds();
  std::map<EntityID, Track> tracks;
  float furthest_bite_slide = 0.0F;

  for (float elapsed = 0.0F; elapsed < 120.0F; elapsed += step) {
    step_once();
    for (auto* animal : animals()) {
      const auto* transform = animal->get_component<TransformComponent>();
      const auto* wildlife = animal->get_component<WildlifeComponent>();
      if (transform == nullptr || wildlife == nullptr) {
        continue;
      }
      Track& track = tracks[animal->get_id()];
      if (!track.primed) {
        track = {transform->position.x, transform->position.z, 0.0F, 0.0F, 0.0F, true};
        continue;
      }
      const float dx = transform->position.x - track.x;
      const float dz = transform->position.z - track.z;
      track.x = transform->position.x;
      track.z = transform->position.z;
      track.bite_slide = wildlife->bite_timer > 0.0F
                             ? track.bite_slide + std::sqrt((dx * dx) + (dz * dz))
                             : 0.0F;
      furthest_bite_slide = std::max(furthest_bite_slide, track.bite_slide);
    }
  }

  ASSERT_GT(bites(), 0U) << "no wolf ever bit, so nothing was proved";

  EXPECT_LT(furthest_bite_slide, 0.55F)
      << "a wolf covered " << furthest_bite_slide
      << " m during a single bite; it should skid to a stop as it lunges, not "
         "keep running with the snap pose on";
}

TEST_F(WildlifeFightMotionTest, WildlifeNeverFightsThroughTheRtsMeleeLock) {
  stage_a_fight();

  const float step = tick_seconds();
  int locked_ticks = 0;
  int ordered_ticks = 0;

  for (float elapsed = 0.0F; elapsed < 60.0F; elapsed += step) {
    step_once();
    for (auto* animal : animals()) {
      const auto* attack = animal->get_component<AttackComponent>();
      if (attack != nullptr && attack->in_melee_lock) {
        ++locked_ticks;
      }
      if (animal->get_component<AttackTargetComponent>() != nullptr) {
        ++ordered_ticks;
      }
    }
  }

  ASSERT_GT(bites(), 0U) << "the pack never reached the herd, so nothing was proved";

  EXPECT_EQ(locked_ticks, 0);
  EXPECT_EQ(ordered_ticks, 0);
}
