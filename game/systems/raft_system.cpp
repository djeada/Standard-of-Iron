#include "raft_system.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

#include "core/component_core.h"
#include "core/component_gameplay.h"
#include "core/entity.h"
#include "core/event_manager.h"
#include "core/world.h"
#include "core/world_spatial_index.h"
#include "game/map/bridge_geometry.h"
#include "game/map/terrain.h"
#include "game/map/terrain_service.h"
#include "game/systems/formation_combat_geometry.h"
#include "game/systems/movement/command_service.h"
#include "units/spawn_type.h"

namespace Game::Systems {

namespace {

constexpr float k_dock_overlap = 0.35F;
constexpr float k_landing_probe_start = 1.2F;
constexpr float k_landing_probe_end = 14.0F;
constexpr float k_landing_probe_step = 0.5F;
constexpr float k_landing_footing = 1.2F;
constexpr float k_requested_goal_slack = 3.0F;
constexpr float k_reorder_interval = 2.0F;
constexpr float k_clear_landing_distance = 4.0F;
constexpr float k_landing_fan_spacing = 6.0F;
constexpr int k_landing_fan_slots = 5;
constexpr float k_min_crossing_seconds = 1.0F;
constexpr float k_water_surface_lift = 0.02F;
constexpr float k_rad_to_deg = 180.0F / std::numbers::pi_v<float>;

[[nodiscard]] auto flat(const QVector3D& v) -> QVector3D {
  return {v.x(), 0.0F, v.z()};
}

[[nodiscard]] auto flat_distance(const QVector3D& a, const QVector3D& b) -> float {
  return flat(a - b).length();
}

[[nodiscard]] auto smoothstep(float t) -> float {
  float const c = std::clamp(t, 0.0F, 1.0F);
  return c * c * (3.0F - 2.0F * c);
}

[[nodiscard]] auto
authored_to_world(float coord, int grid_size, float tile_size) -> float {
  float const safe_tile = std::max(tile_size, 1.0e-4F);
  return (coord - (static_cast<float>(grid_size) * 0.5F - 0.5F)) * safe_tile;
}

[[nodiscard]] auto phase_name(RaftSystem::Phase phase) -> const char* {
  switch (phase) {
  case RaftSystem::Phase::Docked:
    return "docked";
  case RaftSystem::Phase::Boarding:
    return "boarding";
  case RaftSystem::Phase::Crossing:
    return "crossing";
  case RaftSystem::Phase::Landing:
    return "landing";
  }
  return "docked";
}

[[nodiscard]] auto phase_from_name(const QString& name) -> RaftSystem::Phase {
  if (name == QLatin1String("boarding")) {
    return RaftSystem::Phase::Boarding;
  }
  if (name == QLatin1String("crossing")) {
    return RaftSystem::Phase::Crossing;
  }
  if (name == QLatin1String("landing")) {
    return RaftSystem::Phase::Landing;
  }
  return RaftSystem::Phase::Docked;
}

void announce_context_change() {
  Engine::Core::EventManager::instance().publish(
      Engine::Core::ContextActionsChangedEvent{});
}

[[nodiscard]] auto can_ride(const Engine::Core::UnitComponent& unit) -> bool {
  return unit.health > 0 && Game::Units::is_troop_spawn(unit.spawn_type) &&
         !Game::Units::is_siege_engine_spawn(unit.spawn_type) &&
         unit.spawn_type != Game::Units::SpawnType::Elephant;
}

} // namespace

RaftSystem::RaftSystem(Services services)
    : m_services(services) {
}

RaftSystem::~RaftSystem() = default;

void RaftSystem::configure(const Game::Map::MapDefinition& map_definition) {
  m_rafts.clear();
  m_in_reach.clear();
  m_rafts.reserve(map_definition.rafts.size());
  for (const auto& definition : map_definition.rafts) {
    QVector3D const at =
        map_definition.coordSystem == Game::Map::CoordSystem::World
            ? QVector3D(definition.x, 0.0F, definition.z)
            : QVector3D(authored_to_world(definition.x,
                                          map_definition.grid.width,
                                          map_definition.grid.tile_size),
                        0.0F,
                        authored_to_world(definition.z,
                                          map_definition.grid.height,
                                          map_definition.grid.tile_size));
    if (std::any_of(m_rafts.begin(), m_rafts.end(), [&](const Raft& raft) {
          return raft.id == definition.id;
        })) {
      qWarning() << "Raft" << definition.id << "is declared twice - skipping";
      continue;
    }
    (void)place(definition, at);
  }
}

auto RaftSystem::place(const Game::Map::RaftCrossing& definition,
                       const QVector3D& at) -> bool {
  auto const* height_map = m_services.terrain.get_height_map();
  if (height_map == nullptr) {
    return false;
  }

  struct Candidate {
    const Game::Map::RiverSegment* river = nullptr;
    Game::Map::RibbonCrossSection section;
    float depth_ratio = std::numeric_limits<float>::max();
  };
  Candidate best;
  for (auto const& river : height_map->get_river_segments()) {
    QVector3D const span = flat(river.end - river.start);
    float const length_sq = QVector3D::dotProduct(span, span);
    if (length_sq < 1.0e-4F) {
      continue;
    }
    float const t = std::clamp(
        QVector3D::dotProduct(flat(at - river.start), span) / length_sq, 0.0F, 1.0F);
    auto const section = Game::Map::river_drawn_cross_section(river, t);
    float const ratio = flat_distance(at, section.center) / section.half_width;
    if (ratio <= 1.0F && ratio < best.depth_ratio) {
      best = {.river = &river, .section = section, .depth_ratio = ratio};
    }
  }

  if (best.river == nullptr) {
    qWarning() << "Raft" << definition.id << "is not on a river - skipping";
    return false;
  }
  if (!river_admits_raft(best.river->width)) {
    qWarning() << "Raft" << definition.id << "sits on a river" << best.river->width
               << "wide; a raft needs at least" << k_raft_min_river_width
               << "- skipping";
    return false;
  }

  QVector3D const downstream = flat(best.river->end - best.river->start).normalized();
  QVector3D const across(-downstream.z(), 0.0F, downstream.x());
  QVector3D centre = best.section.center;
  centre.setY(centre.y() + k_water_surface_lift);
  float const half_width = best.section.half_width;

  auto const near_landing = find_landing(centre - across * half_width, -across);
  auto const far_landing = find_landing(centre + across * half_width, across);
  if (!near_landing.has_value() || !far_landing.has_value()) {
    qWarning() << "Raft" << definition.id
               << "has no dry ground to land on at one of its banks - skipping";
    return false;
  }

  float const dock_offset =
      std::max(0.0F, half_width - k_raft_deck_half_length + k_dock_overlap);
  Raft raft;
  raft.id = definition.id;
  raft.across = across;
  raft.docks = {centre - across * dock_offset, centre + across * dock_offset};
  raft.landings = {*near_landing, *far_landing};
  raft.speed = definition.speed;
  m_rafts.push_back(std::move(raft));
  return true;
}

auto RaftSystem::find_landing(const QVector3D& water_edge, const QVector3D& outward)
    const -> std::optional<QVector3D> {
  auto const& terrain = m_services.terrain;
  QVector3D const lateral(-outward.z(), 0.0F, outward.x());
  auto const dry = [&](const QVector3D& p) {
    return !terrain.is_forbidden_world(p.x(), p.z());
  };
  for (float reach = Game::Map::k_water_bank_clearance + k_landing_probe_start;
       reach <= k_landing_probe_end;
       reach += k_landing_probe_step) {
    QVector3D point = water_edge + outward * reach;
    if (dry(point) && dry(point + lateral * k_landing_footing) &&
        dry(point - lateral * k_landing_footing) &&
        dry(point + outward * k_landing_footing)) {
      point.setY(terrain.get_terrain_height(point.x(), point.z()));
      return point;
    }
  }
  return std::nullopt;
}

auto RaftSystem::crossing_seconds(const Raft& raft) const -> float {
  return std::max(k_min_crossing_seconds,
                  flat_distance(raft.docks[0], raft.docks[1]) /
                      std::max(raft.speed, 0.1F));
}

auto RaftSystem::deck_position(const Raft& raft) const -> QVector3D {
  if (raft.phase != Phase::Crossing) {
    return raft.docks[raft.side];
  }
  float const t = smoothstep(raft.elapsed / crossing_seconds(raft));
  return raft.docks[raft.side] +
         (raft.docks[1 - raft.side] - raft.docks[raft.side]) * t;
}

auto RaftSystem::passenger_position(const Raft& raft) const -> QVector3D {
  float const step = smoothstep(raft.elapsed / k_raft_board_seconds);
  QVector3D const& landing = raft.landings[raft.side];
  QVector3D const& dock = raft.docks[raft.side];
  switch (raft.phase) {
  case Phase::Boarding:
    return landing + (dock - landing) * step;
  case Phase::Landing:
    return dock + (landing - dock) * step;
  case Phase::Docked:
  case Phase::Crossing:
    break;
  }
  return deck_position(raft);
}

auto RaftSystem::raft(std::size_t index) const -> RaftView {
  if (index >= m_rafts.size()) {
    return {};
  }
  auto const& raft = m_rafts[index];
  RaftView view{.id = raft.id,
                .position = deck_position(raft),
                .across = raft.across,
                .landings = raft.landings,
                .docks = raft.docks,
                .side = raft.side,
                .phase = raft.phase,
                .passenger = raft.passenger,
                .queue = {}};
  for (auto const& request : raft.queue) {
    view.queue.push_back(request.unit);
  }
  return view;
}

auto RaftSystem::is_waiting_or_aboard(Engine::Core::EntityID unit) const -> bool {
  return std::any_of(m_rafts.begin(), m_rafts.end(), [unit](const Raft& raft) {
    return raft.passenger == unit ||
           std::any_of(raft.queue.begin(),
                       raft.queue.end(),
                       [unit](const Request& request) { return request.unit == unit; });
  });
}

auto RaftSystem::raft_in_reach(Engine::Core::World& world,
                               Engine::Core::EntityID unit_id) const
    -> std::optional<Reach> {
  auto const* unit = world.try_get<Engine::Core::UnitComponent>(unit_id);
  auto const* transform = world.try_get<Engine::Core::TransformComponent>(unit_id);
  if (unit == nullptr || transform == nullptr ||
      world.try_get<Engine::Core::MovementComponent>(unit_id) == nullptr ||
      world.has<Engine::Core::WallWalkerComponent>(unit_id) || !can_ride(*unit) ||
      is_waiting_or_aboard(unit_id)) {
    return std::nullopt;
  }
  QVector3D const at(transform->position.x, 0.0F, transform->position.z);
  std::optional<Reach> best;
  float best_distance = k_raft_call_radius;
  for (std::size_t index = 0; index < m_rafts.size(); ++index) {
    for (int side = 0; side < 2; ++side) {
      float const distance = flat_distance(m_rafts[index].landings[side], at);
      if (distance <= best_distance) {
        best_distance = distance;
        best = Reach{.raft = index, .side = side};
      }
    }
  }
  return best;
}

auto RaftSystem::order_crossing(Engine::Core::World& world,
                                Engine::Core::EntityID unit) -> bool {
  auto const reach = raft_in_reach(world, unit);
  if (!reach.has_value()) {
    return false;
  }
  auto& raft = m_rafts[reach->raft];
  raft.queue.push_back({.unit = unit, .side = reach->side});
  Game::Systems::CommandService::move_unit(world, unit, raft.landings[reach->side]);
  announce_context_change();
  return true;
}

void RaftSystem::prune_queue(Engine::Core::World& world, Raft& raft, float dt) {
  for (std::size_t index = 0; index < raft.queue.size();) {
    auto& request = raft.queue[index];
    auto const* unit = world.try_get<Engine::Core::UnitComponent>(request.unit);
    auto const* transform =
        world.try_get<Engine::Core::TransformComponent>(request.unit);
    auto* movement = world.try_get<Engine::Core::MovementComponent>(request.unit);
    bool drop = unit == nullptr || transform == nullptr || movement == nullptr ||
                unit->health <= 0 ||
                world.has<Engine::Core::PendingRemovalComponent>(request.unit);

    QVector3D const& landing = raft.landings[request.side];
    float distance = 0.0F;
    if (!drop) {
      distance = flat_distance(
          QVector3D(transform->position.x, 0.0F, transform->position.z), landing);
      bool const sent_elsewhere =
          movement->get_has_requested_goal() &&
          flat_distance(QVector3D(movement->get_requested_goal_x(),
                                  0.0F,
                                  movement->get_requested_goal_z()),
                        landing) > k_requested_goal_slack;
      bool const wandered_off =
          !movement->get_has_target() && distance > k_raft_call_radius * 1.5F;
      drop = sent_elsewhere || wandered_off;
    }

    bool const is_head = index == 0U;
    bool const raft_waiting_here =
        raft.phase == Phase::Docked && raft.side == request.side;
    if (!drop && is_head && raft_waiting_here) {
      request.waited += dt;
      drop = request.waited > k_raft_approach_timeout;
    }
    if (drop) {
      raft.queue.erase(raft.queue.begin() + static_cast<std::ptrdiff_t>(index));
      continue;
    }

    request.reorder_cooldown = std::max(0.0F, request.reorder_cooldown - dt);
    if (!movement->get_has_target() && distance > k_raft_board_reach &&
        request.reorder_cooldown <= 0.0F) {
      request.reorder_cooldown = k_reorder_interval;
      Game::Systems::CommandService::move_unit(world, request.unit, landing);
    }
    ++index;
  }
}

void RaftSystem::board(Engine::Core::World& world, Raft& raft, const Request& request) {
  raft.passenger = request.unit;
  raft.phase = Phase::Boarding;
  raft.elapsed = 0.0F;
  if (auto* unit = world.try_get<Engine::Core::UnitComponent>(request.unit)) {
    raft.saved_files_override = unit->formation_files_override;
    float spacing = 0.8F;
    if (auto const* entity = world.get_entity(request.unit)) {
      spacing = std::max(0.3F, FormationCombat::resolve_layout_spacing(*entity));
    }
    unit->formation_files_override = std::max(
        1,
        static_cast<int>((k_raft_deck_half_width * 2.0F - spacing * 0.75F) / spacing) +
            1);
  }
  hold_passenger(world, raft);
}

void RaftSystem::hold_passenger(Engine::Core::World& world, Raft& raft) {
  auto const id = raft.passenger;
  auto* transform = world.try_get<Engine::Core::TransformComponent>(id);
  if (transform == nullptr) {
    return;
  }
  QVector3D const at = passenger_position(raft);
  float const deck_y = raft.docks[raft.side].y() + k_raft_deck_height;
  transform->position.x = at.x();
  transform->position.z = at.z();
  transform->position.y =
      std::max(deck_y, m_services.terrain.get_terrain_height(at.x(), at.z()));

  int const from_side = raft.phase == Phase::Landing ? 1 - raft.side : raft.side;
  QVector3D const heading = from_side == 0 ? raft.across : -raft.across;
  float const yaw = std::atan2(heading.x(), heading.z()) * k_rad_to_deg;
  transform->rotation.y = yaw;
  transform->desired_yaw = yaw;
  transform->has_desired_yaw = true;

  if (auto* movement = world.try_get<Engine::Core::MovementComponent>(id)) {
    movement->stop();
  }
  auto* rider = world.try_get<Engine::Core::RaftRiderComponent>(id);
  if (rider == nullptr) {
    rider = world.emplace<Engine::Core::RaftRiderComponent>(id);
  }
  if (rider != nullptr) {
    rider->deck_y = deck_y;
    rider->afloat = raft.phase == Phase::Crossing;
  }
}

void RaftSystem::release_passenger(Engine::Core::World& world,
                                   Raft& raft,
                                   bool landed) {
  auto const id = raft.passenger;
  raft.passenger = 0;
  world.remove<Engine::Core::RaftRiderComponent>(id);
  announce_context_change();
  auto* unit = world.try_get<Engine::Core::UnitComponent>(id);
  if (unit == nullptr) {
    return;
  }
  unit->formation_files_override = raft.saved_files_override;
  raft.saved_files_override = 0;
  if (!landed || unit->health <= 0) {
    return;
  }
  QVector3D const outward = raft.side == 0 ? -raft.across : raft.across;
  QVector3D const along(-raft.across.z(), 0.0F, raft.across.x());
  int const slot = raft.landed % k_landing_fan_slots;
  float const fan = static_cast<float>((slot + 1) / 2) * (slot % 2 == 0 ? -1.0F : 1.0F);
  ++raft.landed;
  QVector3D spot = raft.landings[raft.side] + outward * k_clear_landing_distance +
                   along * (fan * k_landing_fan_spacing);
  if (m_services.terrain.is_forbidden_world(spot.x(), spot.z())) {
    spot = raft.landings[raft.side] + outward * k_clear_landing_distance;
  }
  Game::Systems::CommandService::move_unit(world, id, spot);
}

void RaftSystem::advance(Engine::Core::World& world, Raft& raft, float dt) {
  prune_queue(world, raft, dt);

  if (raft.passenger != 0) {
    auto const* unit = world.try_get<Engine::Core::UnitComponent>(raft.passenger);
    if (unit == nullptr || unit->health <= 0 ||
        world.has<Engine::Core::PendingRemovalComponent>(raft.passenger)) {
      release_passenger(world, raft, false);
      if (raft.phase != Phase::Crossing) {
        raft.phase = Phase::Docked;
        raft.elapsed = 0.0F;
      }
    }
  }

  switch (raft.phase) {
  case Phase::Docked: {
    if (raft.queue.empty()) {
      return;
    }
    auto const head = raft.queue.front();
    if (head.side != raft.side) {
      raft.phase = Phase::Crossing;
      raft.elapsed = 0.0F;
      return;
    }
    auto const* transform = world.try_get<Engine::Core::TransformComponent>(head.unit);
    if (transform != nullptr &&
        flat_distance(QVector3D(transform->position.x, 0.0F, transform->position.z),
                      raft.landings[raft.side]) <= k_raft_board_reach) {
      raft.queue.erase(raft.queue.begin());
      board(world, raft, head);
    }
    return;
  }
  case Phase::Boarding:
    raft.elapsed += dt;
    if (raft.elapsed >= k_raft_board_seconds) {
      raft.phase = Phase::Crossing;
      raft.elapsed = 0.0F;
    }
    break;
  case Phase::Crossing:
    raft.elapsed += dt;
    if (raft.elapsed >= crossing_seconds(raft)) {
      raft.side = 1 - raft.side;
      raft.elapsed = 0.0F;
      raft.phase = raft.passenger != 0 ? Phase::Landing : Phase::Docked;
    }
    break;
  case Phase::Landing:
    raft.elapsed += dt;
    if (raft.elapsed >= k_raft_board_seconds) {
      raft.elapsed = k_raft_board_seconds;
      hold_passenger(world, raft);
      release_passenger(world, raft, true);
      raft.elapsed = 0.0F;
      raft.phase = Phase::Docked;
      return;
    }
    break;
  }
  if (raft.passenger != 0) {
    hold_passenger(world, raft);
  }
}

void RaftSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }
  float const dt = std::max(0.0F, delta_time);
  if (dt > 0.0F) {
    for (auto& raft : m_rafts) {
      advance(*world, raft, dt);
    }
    track_reach(*world);
  } else {
    for (auto& raft : m_rafts) {
      if (raft.passenger != 0) {
        hold_passenger(*world, raft);
      }
    }
  }
  publish_render_views(*world);
}

void RaftSystem::track_reach(Engine::Core::World& world) {
  if (m_rafts.empty()) {
    return;
  }
  thread_local std::vector<Engine::Core::EntityID> in_reach;
  in_reach.clear();
  auto& index = world.spatial_index();
  index.refresh(world);
  for (auto const& raft : m_rafts) {
    for (auto const& landing : raft.landings) {
      index.for_each_in_radius(
          landing.x(),
          landing.z(),
          k_raft_call_radius,
          [](const Engine::Core::WorldSpatialIndex::Entry& entry) {
            if (entry.health > 0 &&
                !entry.is(Engine::Core::WorldSpatialIndex::k_building) &&
                !entry.is(Engine::Core::WorldSpatialIndex::k_wildlife)) {
              in_reach.push_back(entry.id);
            }
          });
    }
  }
  std::sort(in_reach.begin(), in_reach.end());
  in_reach.erase(std::unique(in_reach.begin(), in_reach.end()), in_reach.end());
  if (in_reach != m_in_reach) {
    m_in_reach.assign(in_reach.begin(), in_reach.end());
    announce_context_change();
  }
}

void RaftSystem::publish_render_views(Engine::Core::World& world) const {
  auto& frame = world.render_effects_frame();
  frame.rafts.clear();
  frame.rafts.reserve(m_rafts.size());
  for (std::size_t index = 0; index < m_rafts.size(); ++index) {
    auto const& raft = m_rafts[index];
    frame.rafts.push_back(
        {.position = deck_position(raft),
         .across = raft.across,
         .docks = raft.docks,
         .half_length = k_raft_deck_half_length,
         .half_width = k_raft_deck_half_width,
         .moving = raft.phase == Phase::Crossing,
         .seed = static_cast<std::uint32_t>((index + 1U) * 2246822519U)});
  }
}

auto RaftSystem::serialize_state() const -> QJsonObject {
  QJsonArray rafts;
  for (const auto& raft : m_rafts) {
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), raft.id);
    obj.insert(QStringLiteral("side"), raft.side);
    obj.insert(QStringLiteral("phase"), phase_name(raft.phase));
    obj.insert(QStringLiteral("elapsed"), raft.elapsed);
    obj.insert(QStringLiteral("passenger"), static_cast<qint64>(raft.passenger));
    obj.insert(QStringLiteral("saved_files"), raft.saved_files_override);
    obj.insert(QStringLiteral("landed"), raft.landed);
    QJsonArray queue;
    for (auto const& request : raft.queue) {
      QJsonObject entry;
      entry.insert(QStringLiteral("unit"), static_cast<qint64>(request.unit));
      entry.insert(QStringLiteral("side"), request.side);
      entry.insert(QStringLiteral("waited"), request.waited);
      entry.insert(QStringLiteral("reorder"), request.reorder_cooldown);
      queue.append(entry);
    }
    obj.insert(QStringLiteral("queue"), queue);
    rafts.append(obj);
  }
  QJsonObject state;
  state.insert(QStringLiteral("rafts"), rafts);
  return state;
}

void RaftSystem::restore_state(const QJsonObject& state) {
  for (const auto value : state.value(QStringLiteral("rafts")).toArray()) {
    auto const obj = value.toObject();
    QString const id = obj.value(QStringLiteral("id")).toString();
    auto it = std::find_if(m_rafts.begin(), m_rafts.end(), [&](const Raft& raft) {
      return raft.id == id;
    });
    if (it == m_rafts.end()) {
      continue;
    }
    it->side = std::clamp(obj.value(QStringLiteral("side")).toInt(), 0, 1);
    it->phase = phase_from_name(obj.value(QStringLiteral("phase")).toString());
    it->elapsed = float(obj.value(QStringLiteral("elapsed")).toDouble());
    it->passenger = static_cast<Engine::Core::EntityID>(
        obj.value(QStringLiteral("passenger")).toInteger());
    it->saved_files_override = obj.value(QStringLiteral("saved_files")).toInt();
    it->landed = obj.value(QStringLiteral("landed")).toInt();
    it->queue.clear();
    for (const auto entry_value : obj.value(QStringLiteral("queue")).toArray()) {
      auto const entry = entry_value.toObject();
      it->queue.push_back(
          {.unit = static_cast<Engine::Core::EntityID>(
               entry.value(QStringLiteral("unit")).toInteger()),
           .side = std::clamp(entry.value(QStringLiteral("side")).toInt(), 0, 1),
           .waited = float(entry.value(QStringLiteral("waited")).toDouble()),
           .reorder_cooldown =
               float(entry.value(QStringLiteral("reorder")).toDouble())});
    }
  }
}

} // namespace Game::Systems
