

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/default_content.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/runtime_system_registry.h"
#include "game/units/spawn_type.h"

namespace {

using Engine::Core::AttackComponent;
using Engine::Core::EntityID;
using Engine::Core::MovementComponent;
using Engine::Core::StaminaComponent;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;
using Game::Session::ScopedSession;
using Game::Session::SessionContext;

constexpr int k_map_size = 96;

struct Readout {
  int tick{0};
  int health{0};
  int max_health{0};
  double health_ratio{0.0};
  double stamina_ratio{1.0};
  bool resolved{false};
};

auto read_unit(Engine::Core::World& world, EntityID id) -> Readout {
  Readout out;
  auto* entity = world.get_entity(id);
  if (entity == nullptr) {
    return out;
  }
  const auto* unit = entity->get_component<UnitComponent>();
  if (unit == nullptr) {
    return out;
  }
  out.resolved = true;
  out.health = unit->health;
  out.max_health = unit->max_health;
  out.health_ratio =
      unit->max_health > 0
          ? static_cast<double>(std::clamp(unit->health, 0, unit->max_health)) /
                static_cast<double>(unit->max_health)
          : 0.0;
  if (const auto* stamina = entity->get_component<StaminaComponent>()) {
    out.stamina_ratio = static_cast<double>(stamina->get_stamina_ratio());
  }
  return out;
}

auto spawn_line(SessionContext& session,
                int owner_id,
                Game::Units::SpawnType type,
                float origin_x,
                float origin_z,
                int count) -> std::vector<EntityID> {
  std::vector<EntityID> ids;
  ids.reserve(static_cast<std::size_t>(count));
  for (int index = 0; index < count; ++index) {
    auto* entity = session.world().create_entity();
    auto* transform = entity->add_component<TransformComponent>();
    transform->position.x = origin_x + static_cast<float>(index) * 3.0F;
    transform->position.z = origin_z;

    auto* unit = entity->add_component<UnitComponent>(120, 120, 2.4F, 14.0F);
    unit->owner_id = owner_id;
    unit->spawn_type = type;

    entity->add_component<MovementComponent>();
    entity->add_component<AttackComponent>(12.0F, 8.0F, 1.0F);
    ids.push_back(entity->get_id());
  }
  return ids;
}

auto make_battle() -> std::unique_ptr<SessionContext> {
  Game::Systems::NavGrid::initialize(k_map_size, k_map_size);

  auto session = std::make_unique<SessionContext>();
  auto& owners = session->owners();
  owners.register_owner_with_id(1, Game::Systems::OwnerType::Player, "blue");
  owners.register_owner_with_id(2, Game::Systems::OwnerType::AI, "red");
  owners.set_owner_team(1, 1);
  owners.set_owner_team(2, 2);
  Game::Systems::initialize_default_content(session->nations());
  Game::Systems::register_runtime_systems(session->world());

  Game::Map::MapDefinition map_definition;
  map_definition.grid.width = k_map_size;
  map_definition.grid.height = k_map_size;
  map_definition.grid.tile_size = 1.0F;
  session->terrain().initialize(map_definition);
  session->world().set_presentation_enabled(false);
  return session;
}

struct RoundTrip {
  bool found{false};
  int at_tick{0};
  double before{0.0};
  double bottom{0.0};
  double after{0.0};
};

auto find_round_trip(const std::vector<double>& series, double drop) -> RoundTrip {
  RoundTrip trip;
  for (std::size_t i = 1; i + 1 < series.size(); ++i) {
    const double before = series[i - 1];
    const double bottom = series[i];
    const double after = series[i + 1];
    if (before - bottom >= drop && after - bottom >= drop) {
      trip.found = true;
      trip.at_tick = static_cast<int>(i);
      trip.before = before;
      trip.bottom = bottom;
      trip.after = after;
      return trip;
    }
  }
  return trip;
}

} // namespace

TEST(SelectedUnitReadout, HealthAndStaminaNeverDipAndRecoverDuringAFight) {
  auto session = make_battle();
  const ScopedSession scope(*session);

  const auto blue =
      spawn_line(*session, 1, Game::Units::SpawnType::Spearman, 30.0F, 40.0F, 6);
  spawn_line(*session, 2, Game::Units::SpawnType::Spearman, 30.0F, 52.0F, 6);
  ASSERT_FALSE(blue.empty());
  const EntityID watched = blue.front();

  std::vector<Readout> samples;
  const double step = session->clock().tick_seconds();
  for (int tick = 0; tick < 900; ++tick) {
    session->clock().advance(step);
    while (session->clock().consume_tick()) {
      session->world().update(static_cast<float>(step));
    }
    Readout sample = read_unit(session->world(), watched);
    sample.tick = tick;
    if (!sample.resolved) {
      break;
    }
    samples.push_back(sample);
  }

  ASSERT_GT(samples.size(), 200U) << "the watched unit vanished too early to judge";

  std::vector<double> health;
  std::vector<double> stamina;
  std::vector<double> max_health;
  health.reserve(samples.size());
  stamina.reserve(samples.size());
  max_health.reserve(samples.size());
  for (const auto& sample : samples) {
    health.push_back(sample.health_ratio);
    stamina.push_back(sample.stamina_ratio);
    max_health.push_back(static_cast<double>(sample.max_health));
  }

  const auto health_trip = find_round_trip(health, 0.25);
  EXPECT_FALSE(health_trip.found)
      << "health ratio dipped and recovered at tick " << health_trip.at_tick << ": "
      << health_trip.before << " -> " << health_trip.bottom << " -> "
      << health_trip.after;

  const auto stamina_trip = find_round_trip(stamina, 0.25);
  EXPECT_FALSE(stamina_trip.found)
      << "stamina ratio dipped and recovered at tick " << stamina_trip.at_tick << ": "
      << stamina_trip.before << " -> " << stamina_trip.bottom << " -> "
      << stamina_trip.after;

  const double first_max = max_health.front();
  const auto changed =
      std::find_if(max_health.begin(), max_health.end(), [first_max](double value) {
        return std::abs(value - first_max) > 0.5;
      });
  EXPECT_EQ(changed, max_health.end())
      << "max_health moved from " << first_max << " to " << *changed << " at sample "
      << std::distance(max_health.begin(), changed);
}
