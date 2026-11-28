#include "game/mission/mission_convoy.h"

#include <QJsonArray>
#include <QVariantList>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <numbers>

#include "game/core/component_commander.h"
#include "game/core/component_gameplay.h"
#include "game/core/event_manager.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/systems/nation_registry.h"
#include "game/units/combat_role.h"
#include "game/util/asset_text.h"

namespace Game::Mission {
namespace {

constexpr float k_turn_lookahead = 1.4F;
constexpr float k_escort_radius = 15.0F;
constexpr float k_recent_hit_seconds = 2.5F;

auto as_world_position(const Engine::Core::TransformComponent& transform) -> QVector3D {
  return {transform.position.x, transform.position.y, transform.position.z};
}

} // namespace

void MissionConvoy::bind(const ConvoyDefinition& definition,
                         Engine::Core::World& world,
                         const Game::Systems::LevelSnapshot& level,
                         int owner_id) {
  reset();
  if (definition.route.size() < 2 || definition.wagons.isEmpty()) {
    return;
  }
  m_world = &world;
  m_owner_id = owner_id;
  m_speed = definition.speed;
  m_spacing = definition.wagon_spacing;
  m_max_health = definition.wagon_health;
  const auto to_world = make_mission_position_to_world(level);
  for (const auto& point : definition.route) {
    const QVector3D position = to_world(point.position);
    const float distance =
        m_route.empty()
            ? 0.0F
            : m_route.back().distance + position.distanceToPoint(m_route.back().world);
    m_route.push_back(
        {.world = position,
         .id = point.id,
         .name = Game::Util::tr_asset(Game::Util::k_missions_context, point.name),
         .distance = distance});
  }
  for (const auto& name : definition.wagons) {
    m_wagons.push_back(
        {.name = Game::Util::tr_asset(Game::Util::k_missions_context, name)});
  }
  m_departure_distance = m_spacing * static_cast<float>(m_wagons.size() - 1);
  m_distance = m_departure_distance;
}

void MissionConvoy::reset() {
  *this = MissionConvoy{};
}

void MissionConvoy::spawn_wagons() {
  if (!active()) {
    return;
  }
  using namespace Engine::Core;
  const auto& nations = Game::Session::session_for(*m_world).nations();
  const auto* player_nation = nations.get_nation_for_player(m_owner_id);
  const auto nation =
      player_nation != nullptr ? player_nation->id : nations.default_nation_id();
  for (std::size_t i = 0; i < m_wagons.size(); ++i) {
    auto& wagon = m_wagons[i];
    auto* entity = m_world->create_entity();
    wagon.id = entity->get_id();
    wagon.last_health = m_max_health;
    entity->add_component<TransformComponent>();
    entity->add_component<RenderableComponent>()->renderer_id = "supply_wagon";
    auto* unit =
        entity->add_component<UnitComponent>(m_max_health, m_max_health, 0.0F, 12.0F);
    unit->owner_id = m_owner_id;
    unit->nation_id = nation;
    unit->spawn_type = Game::Units::SpawnType::SupplyWagon;
    unit->death_sequence_override =
        static_cast<std::uint8_t>(DeathSequenceProfile::SiegeEngine);
    place_wagon(wagon.id, m_distance - m_spacing * static_cast<float>(i));
    EventManager::instance().publish(
        UnitSpawnedEvent(wagon.id, m_owner_id, unit->spawn_type, true));
  }

  for (auto [id, unit] : m_world->view<const UnitComponent>()) {
    if (unit.owner_id == m_owner_id && unit.health > 0 &&
        !m_world->has<CommanderComponent>(id) &&
        (Game::Units::is_combatant_spawn(unit.spawn_type) ||
         unit.spawn_type == Game::Units::SpawnType::Healer)) {
      assign_escort(id);
    }
  }
}

auto MissionConvoy::position_at(float distance) const -> QVector3D {
  const auto to = std::upper_bound(
      m_route.begin(), m_route.end(), distance, [](float at, const Waypoint& point) {
        return at < point.distance;
      });
  if (to == m_route.end()) {
    return m_route.back().world;
  }
  if (to == m_route.begin()) {
    return m_route.front().world;
  }
  const auto& from = *std::prev(to);
  const float fraction = (distance - from.distance) / (to->distance - from.distance);
  return from.world + (to->world - from.world) * fraction;
}

void MissionConvoy::place_wagon(Engine::Core::EntityID id, float distance) {
  auto* transform = m_world->try_get<Engine::Core::TransformComponent>(id);
  if (transform == nullptr) {
    return;
  }
  const auto front = position_at(distance + k_turn_lookahead);
  const auto rear = position_at(distance - k_turn_lookahead);
  const auto point = position_at(distance);
  const auto& terrain = Game::Session::session_for(*m_world).terrain();
  transform->position = {
      point.x(), terrain.resolve_surface_world_y(point.x(), point.z()), point.z()};
  if (front != rear) {
    const auto direction = front - rear;
    transform->rotation.y =
        std::atan2(direction.x(), direction.z()) * (180.0F / std::numbers::pi_v<float>);
  }
}

auto MissionConvoy::last_travel_distance() const -> float {
  return m_route.back().distance + m_departure_distance;
}

auto MissionConvoy::wagon_health(const Wagon& wagon) const -> int {
  const auto* unit = m_world->try_get<Engine::Core::UnitComponent>(wagon.id);
  if (unit == nullptr || unit->spawn_type != Game::Units::SpawnType::SupplyWagon ||
      m_world->has<Engine::Core::PendingRemovalComponent>(wagon.id)) {
    return 0;
  }
  return std::max(0, unit->health);
}

auto MissionConvoy::delivered_wagons() const -> int {
  return static_cast<int>(
      std::count_if(m_wagons.begin(), m_wagons.end(), [](const Wagon& wagon) {
        return wagon.delivered;
      }));
}

auto MissionConvoy::available_wagons() const -> int {
  return static_cast<int>(
      std::count_if(m_wagons.begin(), m_wagons.end(), [this](const Wagon& wagon) {
        return wagon.delivered || wagon_health(wagon) > 0;
      }));
}

auto MissionConvoy::nearest_wagon(const QVector3D& from) const
    -> Engine::Core::EntityID {
  Engine::Core::EntityID best = 0;
  float nearest = std::numeric_limits<float>::infinity();
  for (const auto& wagon : m_wagons) {
    if (wagon.delivered || wagon_health(wagon) <= 0) {
      continue;
    }
    const auto* transform =
        m_world->try_get<Engine::Core::TransformComponent>(wagon.id);
    if (transform == nullptr) {
      continue;
    }
    const float distance = (as_world_position(*transform) - from).lengthSquared();
    if (distance < nearest) {
      best = wagon.id;
      nearest = distance;
    }
  }
  return best;
}

void MissionConvoy::assign_escort(Engine::Core::EntityID escort_id) const {
  if (!active()) {
    return;
  }
  using namespace Engine::Core;
  auto* escort = m_world->get_entity(escort_id);
  const auto* transform = m_world->try_get<TransformComponent>(escort_id);
  if (escort == nullptr || transform == nullptr) {
    return;
  }
  const auto wagon_id = nearest_wagon(as_world_position(*transform));
  const auto* anchor = m_world->try_get<TransformComponent>(wagon_id);
  if (anchor == nullptr) {
    return;
  }
  auto* guard = get_or_add_component<GuardModeComponent>(*escort);
  guard->active = true;
  guard->has_guard_target = true;
  guard->guarded_entity_id = wagon_id;
  guard->guard_position_x = anchor->position.x;
  guard->guard_position_z = anchor->position.z;
  guard->guard_radius = k_escort_radius;
  guard->returning_to_guard_position = false;
  guard->has_reach_center = false;
}

void MissionConvoy::reassign_stranded_escorts() const {
  for (const auto& wagon : m_wagons) {
    if (!wagon.delivered && wagon_health(wagon) > 0) {
      continue;
    }
    for (auto [id, unit, guard] :
         m_world->view<const Engine::Core::UnitComponent,
                       const Engine::Core::GuardModeComponent>()) {
      if (unit.owner_id == m_owner_id && unit.health > 0 && guard.active &&
          wagon.id != 0 && guard.guarded_entity_id == wagon.id) {
        assign_escort(id);
      }
    }
  }
}

void MissionConvoy::advance(float dt,
                            float elapsed,
                            std::vector<PendingMissionWave>& waves) {
  if (!active()) {
    return;
  }
  // The road is authored for the convoy. Infantry occupying a tile should never
  // stop the column from leaving a checkpoint after the ambush has been broken.
  float next_distance = std::min(last_travel_distance(), m_distance + m_speed * dt);
  while (m_next_point < m_route.size() &&
         m_route[m_next_point].distance <= next_distance) {
    auto& point = m_route[m_next_point];
    if (point.reached_at < 0.0F) {
      point.reached_at = elapsed;
    }
    const bool blocked =
        !point.id.isEmpty() &&
        std::any_of(waves.begin(), waves.end(), [&point](const auto& w) {
          return !w.reinforcement && !w.cleared && w.convoy_checkpoint == point.id;
        });
    if (blocked) {
      next_distance = std::max(m_distance, point.distance);
      break;
    }
    ++m_next_point;
  }
  const bool moved = next_distance > m_distance;
  m_distance = next_distance;
  bool wagon_retired = false;
  for (std::size_t i = 0; i < m_wagons.size(); ++i) {
    auto& wagon = m_wagons[i];
    if (wagon.delivered) {
      continue;
    }
    const int health = wagon_health(wagon);
    if (health < wagon.last_health) {
      wagon.last_hit_at = elapsed;
      wagon_retired |= health == 0;
    }
    wagon.last_health = health;
    if (health <= 0) {
      continue;
    }
    const float offset = m_spacing * static_cast<float>(i);
    if (m_distance >= m_route.back().distance + offset &&
        m_next_point == m_route.size()) {
      wagon.delivered = true;
      wagon_retired = true;
      m_world->destroy_entity(wagon.id);
    } else if (moved) {
      place_wagon(wagon.id, m_distance - offset);
    }
  }
  if (wagon_retired) {
    reassign_stranded_escorts();
  }
  update_wave_checkpoints(waves);
}

void MissionConvoy::update_wave_checkpoints(
    std::vector<PendingMissionWave>& waves) const {
  for (auto& wave : waves) {
    if (wave.convoy_checkpoint.isEmpty() || wave.spawned) {
      continue;
    }
    const auto point = std::find_if(
        m_route.begin(), m_route.end(), [&wave](const Waypoint& candidate) {
          return candidate.id == wave.convoy_checkpoint;
        });
    if (point != m_route.end()) {
      wave.checkpoint_reached_at = point->reached_at;
      wave.defense_reference_world_position = point->world;
    }
  }
}

auto MissionConvoy::status(
    float elapsed, const std::vector<PendingMissionWave>& waves) const -> QVariantMap {
  if (!active()) {
    return {};
  }
  QVariantList wagons;
  for (const auto& wagon : m_wagons) {
    const int health = wagon.delivered ? wagon.last_health : wagon_health(wagon);
    QVariantMap row{{"name", wagon.name},
                    {"health", health / static_cast<double>(m_max_health)},
                    {"state",
                     wagon.delivered ? QStringLiteral("safe")
                     : health > 0    ? QStringLiteral("moving")
                                     : QStringLiteral("lost")},
                    {"under_fire",
                     health > 0 && !wagon.delivered &&
                         elapsed - wagon.last_hit_at < k_recent_hit_seconds}};
    if (health > 0 && !wagon.delivered) {
      if (const auto* transform =
              m_world->try_get<Engine::Core::TransformComponent>(wagon.id)) {
        row.insert("world_x", transform->position.x);
        row.insert("world_z", transform->position.z);
      }
    }
    wagons.push_back(row);
  }

  const auto named =
      std::find_if(m_route.begin() + m_next_point,
                   m_route.end(),
                   [](const Waypoint& point) { return !point.name.isEmpty(); });
  const bool holding =
      m_next_point < m_route.size() && m_route[m_next_point].reached_at >= 0.0F;
  QVariantMap status{
      {"wagons", wagons},
      {"delivered", delivered_wagons()},
      {"fraction",
       std::clamp(
           (m_distance - m_departure_distance) / m_route.back().distance, 0.0F, 1.0F)},
      {"holding", holding}};
  const auto& target = named == m_route.end() ? m_route.back() : *named;
  status.insert("target_x", target.world.x());
  status.insert("target_z", target.world.z());
  status.insert("destination", target.name);

  for (const auto& wave : waves) {
    if (!wave.reinforcement || wave.spawned || wave.ready_at < 0.0F) {
      continue;
    }
    const auto seconds =
        static_cast<int>(std::ceil(std::max(0.0F, wave.ready_at - elapsed)));
    if (!status.contains("relief_eta") || seconds < status["relief_eta"].toInt()) {
      status.insert("relief_eta", seconds);
      status.insert("relief_name",
                    Game::Util::tr_asset(Game::Util::k_missions_context, wave.label));
    }
  }
  return status;
}

auto MissionConvoy::serialize() const -> QJsonObject {
  if (!active()) {
    return {};
  }
  QJsonArray wagons;
  for (const auto& wagon : m_wagons) {
    wagons.append(QJsonObject{{"id", static_cast<qint64>(wagon.id)},
                              {"health", wagon.last_health},
                              {"delivered", wagon.delivered}});
  }
  QJsonArray reached;
  for (const auto& point : m_route) {
    reached.append(point.reached_at);
  }
  return {{"distance", m_distance},
          {"next_point", static_cast<int>(m_next_point)},
          {"reached", reached},
          {"wagons", wagons}};
}

void MissionConvoy::restore(const QJsonObject& state) {
  if (!active()) {
    return;
  }
  m_distance = std::clamp(static_cast<float>(state["distance"].toDouble()),
                          m_departure_distance,
                          last_travel_distance());
  m_next_point = static_cast<std::size_t>(
      std::clamp(state["next_point"].toInt(), 0, static_cast<int>(m_route.size())));
  const auto reached = state["reached"].toArray();
  for (qsizetype i = 0;
       i < reached.size() && i < static_cast<qsizetype>(m_route.size());
       ++i) {
    m_route[static_cast<std::size_t>(i)].reached_at =
        static_cast<float>(reached[i].toDouble(-1.0));
  }
  const auto wagons = state["wagons"].toArray();
  for (qsizetype i = 0;
       i < wagons.size() && i < static_cast<qsizetype>(m_wagons.size());
       ++i) {
    auto& wagon = m_wagons[static_cast<std::size_t>(i)];
    const auto saved = wagons[i].toObject();
    wagon.id =
        static_cast<Engine::Core::EntityID>(saved["id"].toVariant().toULongLong());
    wagon.delivered = saved["delivered"].toBool();
    wagon.last_health =
        wagon.delivered ? saved["health"].toInt(m_max_health) : wagon_health(wagon);
  }
  reassign_stranded_escorts();
}

} // namespace Game::Mission
