#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "game/core/component_economy.h"
#include "game/core/component_presentation.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/command_service.h"
#include "game/systems/default_content.h"
#include "game/systems/formation_combat_geometry.h"
#include "game/systems/nation_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/pathfinding.h"
#include "game/systems/runtime_system_registry.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "game/wildlife/wildlife_config.h"
#include "game/wildlife/wildlife_species.h"
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

struct BodyPoint {
  float x{0.0F};
  float z{0.0F};
  float radius{0.0F};
};

auto nearest_body_to(const Engine::Core::Entity& entity,
                     float from_x,
                     float from_z) -> BodyPoint {
  auto const* transform = entity.get_component<TransformComponent>();
  BodyPoint point{transform->position.x,
                  transform->position.z,
                  std::max(transform->scale.x, transform->scale.z) * 0.5F};
  if (!Game::Systems::FormationCombat::has_formation_slots(entity)) {
    return point;
  }
  auto const layout = Game::Systems::FormationCombat::resolve_layout(entity);
  float nearest = 1.0e9F;
  for (auto const& anchor :
       Game::Systems::FormationCombat::soldier_spatial_anchors(entity, layout)) {
    float const distance = std::hypot(anchor.world_x - from_x, anchor.world_z - from_z);
    if (distance < nearest) {
      nearest = distance;
      point = {anchor.world_x, anchor.world_z, layout.body_radius};
    }
  }
  return point;
}

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

TEST_F(WildlifeFightMotionTest, ASingleWolfClosesOnFleeingSheepAndFinishesTheHunt) {
  field();
  auto* system = m_session->world().get_system<Game::Wildlife::WildlifeSystem>();
  ASSERT_NE(system, nullptr);
  auto settings = Game::Wildlife::default_settings();
  settings.enabled = true;
  settings.seed = 1416U;
  settings.birds.enabled = false;
  settings.sheep.group_count = 1;
  settings.sheep.group_size_min = settings.sheep.group_size_max = 1;
  settings.sheep.roam_radius = 4.0F;
  settings.sheep.spawn_areas = {{0.0F, 0.0F, 1.0F}};
  settings.sheep.respawn = false;
  settings.wolves.group_count = 1;
  settings.wolves.group_size_min = settings.wolves.group_size_max = 1;
  settings.wolves.aggression = 1.0F;
  settings.wolves.roam_radius = 16.0F;
  settings.wolves.spawn_areas = {{3.0F, 0.0F, 1.0F}};
  settings.wolves.respawn = false;
  system->configure(settings, 1416U);
  step_once();
  EntityID prey_id = 0;
  for (auto* animal : animals()) {
    if (animal->get_component<WildlifeComponent>()->species ==
        Game::Wildlife::Species::Sheep) {
      prey_id = animal->get_id();
    }
  }
  ASSERT_NE(prey_id, 0U);
  bool killed = false;
  std::string history;
  int last_second = -1;
  for (float elapsed = 0.0F; elapsed < 45.0F; elapsed += tick_seconds()) {
    step_once();
    if (static_cast<int>(elapsed) / 5 != last_second) {
      last_second = static_cast<int>(elapsed) / 5;
      history += "\nt=" + std::to_string(elapsed);
      for (auto* animal : animals()) {
        auto const* tr = animal->get_component<TransformComponent>();
        auto const* wc = animal->get_component<WildlifeComponent>();
        history +=
            " species=" + std::to_string(static_cast<int>(wc->species)) +
            " hp=" + std::to_string(animal->get_component<UnitComponent>()->health) +
            " pos=" + std::to_string(tr->position.x) + "," +
            std::to_string(tr->position.z) + " yaw=" + std::to_string(tr->rotation.y) +
            " bites=" + std::to_string(bites());
      }
    }
    auto const* prey = m_session->world().get_entity(prey_id);
    if (prey == nullptr || prey->get_component<UnitComponent>()->health <= 0) {
      killed = true;
      break;
    }
  }
  EXPECT_TRUE(killed) << "a faster predator must not orbit stale positions indefinitely"
                      << history;
  EXPECT_GE(bites(), 4U);
}

TEST_F(WildlifeFightMotionTest, EveryBiteStartsFacingItsCommittedTargetAtContactRange) {
  stage_a_fight();
  std::map<EntityID, float> timers;
  unsigned checked = 0;
  for (float elapsed = 0.0F; elapsed < 60.0F; elapsed += tick_seconds()) {
    step_once();
    for (auto* animal : animals()) {
      auto const* wildlife = animal->get_component<WildlifeComponent>();
      if (wildlife->species != Game::Wildlife::Species::Wolf) {
        continue;
      }
      float& previous = timers[animal->get_id()];
      bool const started = wildlife->bite_timer > previous;
      previous = wildlife->bite_timer;
      if (!started) {
        continue;
      }
      auto const* target = m_session->world().get_entity(wildlife->bite_target_id);
      ASSERT_NE(target, nullptr);
      auto const* hunter = animal->get_component<TransformComponent>();
      BodyPoint const prey =
          nearest_body_to(*target, hunter->position.x, hunter->position.z);
      float const dx = prey.x - hunter->position.x;
      float const dz = prey.z - hunter->position.z;
      float const yaw = std::atan2(dx, dz) * 180.0F / 3.14159265F;

      EXPECT_LE(std::abs(std::remainder(yaw - hunter->rotation.y, 360.0F)),
                12.0F +
                    Game::Units::body_turn_speed_degrees(Game::Units::SpawnType::Wolf) *
                        tick_seconds());
      EXPECT_LE(std::hypot(dx, dz),
                Game::Wildlife::k_wolf_bite_range + prey.radius + 0.15F);
      ++checked;
    }
  }
  EXPECT_GT(checked, 3U) << "the fixture must exercise repeated commitments";
}

TEST_F(WildlifeFightMotionTest, AUnitThatWalksIntoAWolfIsInAFightOnContact) {
  field();
  auto* system = m_session->world().get_system<Game::Wildlife::WildlifeSystem>();
  ASSERT_NE(system, nullptr);
  Game::Wildlife::WildlifeSettings settings = Game::Wildlife::default_settings();
  settings.enabled = true;
  settings.seed = 7U;
  settings.sheep.enabled = false;
  settings.sheep.group_count = 0;
  settings.wolves.enabled = true;
  settings.wolves.group_count = 1;
  settings.wolves.group_size_min = 1;
  settings.wolves.group_size_max = 1;
  settings.wolves.aggression = 1.0F;
  settings.wolves.roam_radius = 16.0F;
  settings.wolves.alert_radius = 16.0F;
  settings.wolves.spawn_areas = {{20.0F, 20.0F, 0.5F}};
  settings.birds.enabled = false;
  settings.birds.group_count = 0;
  Game::Wildlife::sanitize(settings);
  system->configure(settings, 7U);

  spawn(Game::Units::SpawnType::Spearman, QVector3D(12.0F, 0.0F, 20.0F));
  ASSERT_FALSE(m_handles.empty());
  const EntityID troop = m_handles.back()->id();
  step_once();
  const auto wolves = animals();
  ASSERT_EQ(wolves.size(), 1U);
  const EntityID wolf = wolves.front()->get_id();

  auto& world = m_session->world();
  Game::Systems::CommandService::move_unit(world, troop, QVector3D(30.0F, 0.0F, 20.0F));

  auto position = [&](EntityID id) {
    auto const* transform = world.get_entity(id)->get_component<TransformComponent>();
    return QVector3D(transform->position.x, 0.0F, transform->position.z);
  };
  auto troop_engaged = [&]() {
    auto* entity = world.get_entity(troop);
    auto const* target = entity->get_component<AttackTargetComponent>();
    auto const* attack = entity->get_component<AttackComponent>();
    return (target != nullptr && target->target_id == wolf) ||
           (attack != nullptr && attack->in_melee_lock &&
            attack->melee_lock_target_id == wolf);
  };
  auto wolf_committed = [&]() {
    auto const* wildlife = world.get_entity(wolf)->get_component<WildlifeComponent>();
    return wildlife != nullptr &&
           wildlife->behavior == Game::Wildlife::Behavior::Stalk &&
           wildlife->focus_id == troop;
  };
  auto wolf_alive = [&]() {
    auto const* unit = world.get_entity(wolf)->get_component<UnitComponent>();
    return unit != nullptr && unit->health > 0;
  };

  const float step = tick_seconds();
  float first_touch = -1.0F;
  float troop_answered = -1.0F;
  float wolf_answered = -1.0F;
  float closest = 1.0e9F;
  for (float elapsed = 0.0F; elapsed < 10.0F; elapsed += step) {
    step_once();
    QVector3D const wolf_at = position(wolf);
    BodyPoint const edge =
        nearest_body_to(*world.get_entity(troop), wolf_at.x(), wolf_at.z());
    float const gap = std::hypot(edge.x - wolf_at.x(), edge.z - wolf_at.z());
    closest = std::min(closest, gap);
    if (first_touch < 0.0F && gap <= 1.6F) {
      first_touch = elapsed;
    }
    if (troop_answered < 0.0F && troop_engaged()) {
      troop_answered = elapsed;
    }
    if (wolf_answered < 0.0F && wolf_committed()) {
      wolf_answered = elapsed;
    }
    if (!wolf_alive()) {
      break;
    }
  }

  ASSERT_GE(first_touch, 0.0F)
      << "the troop never reached the wolf; closest " << closest << " m, troop at "
      << position(troop).x() << ", wolf at " << position(wolf).x();
  EXPECT_GE(troop_answered, 0.0F) << "the troop walked into a wolf and never fought it";
  EXPECT_GE(wolf_answered, 0.0F)
      << "the wolf let a troop walk into it and never turned";
  if (troop_answered >= 0.0F) {

    EXPECT_LE(troop_answered - first_touch, 1.0F)
        << "the troop answered " << (troop_answered - first_touch)
        << " s after contact";
  }
  if (wolf_answered >= 0.0F) {
    EXPECT_LE(wolf_answered - first_touch, 0.5F)
        << "the wolf answered " << (wolf_answered - first_touch) << " s after contact";
  }
  if (wolf_alive()) {
    EXPECT_LE((position(troop) - position(wolf)).length(), 3.0F)
        << "the troop walked " << (position(troop) - position(wolf)).length()
        << " m away from a wolf it had run into";
  }
}

namespace {

void release_hunting_pack(Game::Session::SessionContext& session,
                          int wolves,
                          const QVector3D& den) {
  auto* system = session.world().get_system<Game::Wildlife::WildlifeSystem>();
  ASSERT_NE(system, nullptr);
  Game::Wildlife::WildlifeSettings settings = Game::Wildlife::default_settings();
  settings.enabled = true;
  settings.seed = 1416U;
  settings.sheep.enabled = false;
  settings.sheep.group_count = 0;
  settings.wolves.enabled = true;
  settings.wolves.group_count = 1;
  settings.wolves.group_size_min = wolves;
  settings.wolves.group_size_max = wolves;
  settings.wolves.aggression = 1.0F;
  settings.wolves.roam_radius = 16.0F;
  settings.wolves.alert_radius = 8.0F;
  settings.wolves.respawn = false;
  settings.wolves.spawn_areas = {{den.x(), den.z(), 1.0F}};
  settings.birds.enabled = false;
  settings.birds.group_count = 0;
  Game::Wildlife::sanitize(settings);
  system->configure(settings, 1416U);
}

} // namespace

TEST_F(WildlifeFightMotionTest, ABittenBuilderGangFightsTheWolfOffBareHanded) {
  field();
  m_session->world().set_presentation_enabled(true);
  release_hunting_pack(*m_session, 1, QVector3D(39.0F, 0.0F, 32.0F));
  spawn(Game::Units::SpawnType::Builder, QVector3D(32.0F, 0.0F, 32.0F));
  ASSERT_FALSE(m_handles.empty());
  const EntityID gang = m_handles.back()->id();
  step_once();
  const auto wolves = animals();
  ASSERT_EQ(wolves.size(), 1U);
  const EntityID wolf = wolves.front()->get_id();

  auto& world = m_session->world();
  auto position = [&](EntityID id) {
    auto const* transform = world.get_entity(id)->get_component<TransformComponent>();
    return QVector3D(transform->position.x, 0.0F, transform->position.z);
  };
  auto locked_on_wolf = [&]() {
    auto const* attack = world.get_entity(gang)->get_component<AttackComponent>();
    return attack != nullptr && attack->in_melee_lock &&
           attack->melee_lock_target_id == wolf;
  };
  auto wolf_health = [&]() {
    auto const* unit = world.get_entity(wolf)->get_component<UnitComponent>();
    return unit != nullptr ? unit->health : 0;
  };
  int const wolf_health_before = wolf_health();

  const float step = tick_seconds();
  bool answered = false;
  bool whole_gang_struck = false;
  float order_time = -1.0F;
  QVector3D ordered_from;
  bool held_after_order = false;
  for (float elapsed = 0.0F; elapsed < 30.0F && wolf_health() > 0; elapsed += step) {
    step_once();
    auto* entity = world.get_entity(gang);
    auto const* target = entity->get_component<AttackTargetComponent>();
    answered = answered || locked_on_wolf() ||
               (target != nullptr && target->target_id == wolf);

    auto const* presentation =
        entity->get_component<Engine::Core::FormationPresentationComponent>();
    if (presentation != nullptr && presentation->melee_ordered) {
      int alive = 0;
      int striking = 0;
      for (auto const& soldier : presentation->soldiers) {
        if (!soldier.alive) {
          continue;
        }
        ++alive;
        using Role = Engine::Core::FormationSoldierCombatRole;
        if (soldier.combat_role == Role::LeadStrike ||
            soldier.combat_role == Role::SupportStrike) {
          ++striking;
        }
      }
      whole_gang_struck = whole_gang_struck || (alive > 1 && striking == alive);
    }

    if (order_time < 0.0F && locked_on_wolf()) {
      order_time = elapsed;
      ordered_from = position(gang);
      Game::Systems::CommandService::move_unit(
          world, gang, QVector3D(8.0F, 0.0F, 32.0F));
    }
    if (order_time >= 0.0F && !held_after_order && elapsed - order_time >= 1.5F &&
        wolf_health() > 0) {
      held_after_order = true;
      EXPECT_TRUE(locked_on_wolf()) << "a move order walked the gang out of the fight";
      EXPECT_LE((position(gang) - ordered_from).length(), 1.0F)
          << "the locked gang walked " << (position(gang) - ordered_from).length()
          << " m away from the wolf on a move order";
    }
  }

  ASSERT_GT(bites(), 0U) << "the wolf never bit the gang, so nothing was proved";
  EXPECT_TRUE(answered) << "the builders stood still while a wolf bit them";
  EXPECT_TRUE(whole_gang_struck)
      << "only part of the gang fought; every builder should throw punches";
  EXPECT_LT(wolf_health(), wolf_health_before)
      << "the builders never landed a blow on the wolf";
}

TEST_F(WildlifeFightMotionTest, AWolfFightingABareHandedCivilianKeepsBiting) {
  field();
  m_session->world().set_presentation_enabled(true);
  release_hunting_pack(*m_session, 1, QVector3D(39.0F, 0.0F, 32.0F));
  spawn(Game::Units::SpawnType::Civilian, QVector3D(32.0F, 0.0F, 32.0F));
  ASSERT_FALSE(m_handles.empty());
  const EntityID civilian = m_handles.back()->id();
  if (auto* person =
          m_session->world().get_entity(civilian)->get_component<UnitComponent>()) {
    person->max_health = 400;
    person->health = 400;
  }
  step_once();
  const auto wolves = animals();
  ASSERT_EQ(wolves.size(), 1U);
  const EntityID wolf = wolves.front()->get_id();

  auto& world = m_session->world();
  auto alive = [&](EntityID id) {
    auto* entity = world.get_entity(id);
    auto const* unit =
        entity != nullptr ? entity->get_component<UnitComponent>() : nullptr;
    return unit != nullptr && unit->health > 0;
  };

  const float step = tick_seconds();
  float lock_time = -1.0F;
  unsigned bites_at_lock = 0U;
  float next_sample = 0.0F;
  std::string timeline;
  for (float elapsed = 0.0F; elapsed < 20.0F && alive(civilian) && alive(wolf);
       elapsed += step) {
    step_once();
    auto* person = world.get_entity(civilian);
    auto const* person_attack = person->get_component<AttackComponent>();
    if (lock_time < 0.0F && person_attack != nullptr && person_attack->in_melee_lock) {
      lock_time = elapsed;
      bites_at_lock = bites();
    }
    if (elapsed >= next_sample) {
      next_sample += 1.0F;
      auto* animal = world.get_entity(wolf);
      auto const* wildlife = animal->get_component<WildlifeComponent>();
      auto const* attack = animal->get_component<AttackComponent>();
      auto const* movement = animal->get_component<MovementComponent>();
      auto const* transform = animal->get_component<TransformComponent>();
      auto const* person_transform = person->get_component<TransformComponent>();
      float const gap =
          std::hypot(transform->position.x - person_transform->position.x,
                     transform->position.z - person_transform->position.z);
      timeline +=
          "\n t=" + std::to_string(elapsed) + " gap=" + std::to_string(gap) +
          " wolf_lock=" + std::to_string(attack != nullptr && attack->in_melee_lock) +
          " stagger=" +
          std::to_string(animal->has_component<Engine::Core::StaggerComponent>()) +
          " has_target=" + std::to_string(movement->get_has_target()) + " goal=(" +
          std::to_string(movement->get_goal_x()) + "," +
          std::to_string(movement->get_goal_y()) + ") v=(" +
          std::to_string(movement->get_vx()) + "," +
          std::to_string(movement->get_vz()) +
          ") state_timer=" + std::to_string(wildlife->state_timer) +
          " bite_timer=" + std::to_string(wildlife->bite_timer) +
          " flinch=" + std::to_string(wildlife->flinch_timer) +
          " stall=" + std::to_string(wildlife->stall_timer) +
          " focus=" + std::to_string(wildlife->focus_id) +
          " behavior=" + std::to_string(static_cast<int>(wildlife->behavior));
    }
  }

  RecordProperty("timeline", timeline);
  RecordProperty("lock_time", std::to_string(lock_time));
  RecordProperty("civilian_alive", alive(civilian) ? "yes" : "no");
  RecordProperty("wolf_alive", alive(wolf) ? "yes" : "no");
  RecordProperty("bites_total", std::to_string(bites()));
  ASSERT_GE(lock_time, 0.0F) << "the civilian never locked onto the wolf" << timeline;
  unsigned const bites_after_lock = bites() - bites_at_lock;
  EXPECT_GE(bites_after_lock, 3U)
      << "the wolf stopped biting once the civilian fought back (" << bites_after_lock
      << " bites after the lock at " << lock_time << " s)" << timeline;
}

TEST_F(WildlifeFightMotionTest,
       ASquadLockedOnWolvesKeepsLandingBlowsUntilThePackIsDead) {
  field();
  release_hunting_pack(*m_session, 2, QVector3D(39.0F, 0.0F, 32.0F));
  spawn(Game::Units::SpawnType::Knight, QVector3D(32.0F, 0.0F, 32.0F));
  ASSERT_FALSE(m_handles.empty());
  step_once();
  ASSERT_EQ(animals().size(), 2U);

  auto living_wolves = [&]() {
    int count = 0;
    for (auto* animal : animals()) {
      auto const* unit = animal->get_component<UnitComponent>();
      if (unit != nullptr && unit->health > 0) {
        ++count;
      }
    }
    return count;
  };

  const float step = tick_seconds();
  for (float elapsed = 0.0F; elapsed < 30.0F && living_wolves() > 0; elapsed += step) {
    step_once();
  }

  ASSERT_GT(bites(), 0U) << "the pack never engaged the squad, so nothing was proved";
  EXPECT_EQ(living_wolves(), 0)
      << "a squad locked onto a wolf stopped landing blows before the pack was dead";
}
