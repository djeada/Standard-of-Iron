#include "replay_checkpoint.h"

#include <QJsonArray>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <limits>
#include <ranges>

#include "../command/command_queue.h"
#include "../command/command_system.h"
#include "../command/json_u64.h"
#include "../command/replay.h"
#include "../core/world.h"
#include "../map/terrain.h"
#include "../map/terrain_service.h"
#include "../map/visibility_service.h"
#include "../session/deterministic_rng.h"
#include "../session/session_context.h"
#include "../session/session_snapshot.h"
#include "../session/simulation_clock.h"
#include "../session/world_digest.h"
#include "../systems/alliance_board.h"
#include "../systems/combat_system.h"
#include "../systems/economy/gather_loop_system.h"
#include "../systems/economy/marketplace_system.h"
#include "../systems/global_stats_registry.h"
#include "../systems/movement/movement_system.h"
#include "../systems/movement/route_follow_system.h"
#include "../systems/navigation/navigation_service.h"
#include "../systems/navigation/pathfinding.h"
#include "../systems/player_resource_registry.h"
#include "../systems/projectile_system.h"
#include "../systems/settlement_life_system.h"
#include "../systems/undead_awakening_system.h"
#include "../systems/world_restore.h"
#include "../wildlife/wildlife_system.h"
#include "entity_codec.h"
#include "serialization.h"

namespace Game::Save {
namespace {

constexpr std::uint32_t k_max_checkpoint_bytes = 64 * 1024 * 1024;

using Game::Command::u64_from_json;
using Game::Command::u64_to_json;
using Game::Systems::k_all_resource_types;
using Game::Systems::OwnerResourceState;
using Game::Systems::resource_type_key;

auto encode_registry(const Engine::Core::RegistryCheckpoint& state) -> QJsonObject {
  QJsonArray generations;
  for (const auto generation : state.generations) {
    generations.append(static_cast<qint64>(generation));
  }
  QJsonArray free;
  for (const auto index : state.free_slots) {
    free.append(static_cast<qint64>(index));
  }
  QJsonObject orders;
  for (const auto& order : state.component_orders) {
    QJsonArray ids;
    for (const auto id : order.entities) {
      ids.append(u64_to_json(id));
    }
    orders[QString::fromStdString(order.type)] = ids;
  }
  return {
      {"generations", generations}, {"free_slots", free}, {"component_orders", orders}};
}

auto decode_registry(const QJsonObject& snapshot)
    -> std::optional<Engine::Core::RegistryCheckpoint> {
  const auto generations = snapshot.value("generations");
  const auto free = snapshot.value("free_slots");
  const auto orders = snapshot.value("component_orders");
  if (!generations.isArray() || !free.isArray() || !orders.isObject()) {
    return std::nullopt;
  }
  Engine::Core::RegistryCheckpoint state;
  const auto read_indices = [](const QJsonArray& rows,
                               std::vector<std::uint32_t>& out) {
    for (const auto row : rows) {
      const auto value = row.toInteger(-1);
      if (value < 0 || value > std::numeric_limits<std::uint32_t>::max()) {
        return false;
      }
      out.push_back(static_cast<std::uint32_t>(value));
    }
    return true;
  };
  if (!read_indices(generations.toArray(), state.generations) ||
      !read_indices(free.toArray(), state.free_slots) || state.generations.empty()) {
    return std::nullopt;
  }
  const auto order_object = orders.toObject();
  for (auto it = order_object.begin(); it != order_object.end(); ++it) {
    if (!it.value().isArray()) {
      return std::nullopt;
    }
    auto& order = state.component_orders.emplace_back();
    order.type = it.key().toStdString();
    for (const auto value : it.value().toArray()) {
      const auto id = u64_from_json(value);
      if (!id) {
        return std::nullopt;
      }
      order.entities.push_back(*id);
    }
  }
  return state;
}

auto encode_resources(const std::vector<OwnerResourceState>& rows) -> QJsonArray {
  QJsonArray resources;
  for (const auto& row : rows) {
    QJsonObject amounts;
    for (const auto type : k_all_resource_types) {
      amounts[QLatin1String(resource_type_key(type))] = row.amounts.get(type);
    }
    resources.append(QJsonObject{{"owner_id", row.owner_id}, {"resources", amounts}});
  }
  return resources;
}

auto decode_resources(const QJsonValue& resources) -> std::vector<OwnerResourceState> {
  std::vector<OwnerResourceState> rows;
  for (const auto value : resources.toArray()) {
    const auto row = value.toObject();
    const auto amounts = row.value(QLatin1String("resources")).toObject();
    OwnerResourceState decoded{.owner_id =
                                   row.value(QLatin1String("owner_id")).toInt()};
    for (const auto type : k_all_resource_types) {
      decoded.amounts.set(
          type, amounts.value(QLatin1String(resource_type_key(type))).toInt());
    }
    rows.push_back(decoded);
  }
  return rows;
}

auto decode_rng(const QJsonObject& state)
    -> std::optional<Game::Session::DeterministicRng::State> {
  const auto seed = u64_from_json(state.value(QLatin1String("rng_seed")));
  const auto draws = u64_from_json(state.value(QLatin1String("rng_draws")));
  const auto words = state.value(QLatin1String("rng_words")).toArray();
  if (!seed || !draws || words.size() != 4) {
    return std::nullopt;
  }
  Game::Session::DeterministicRng::State rng{.seed = *seed, .draws = *draws};
  for (std::size_t i = 0; i < rng.words.size(); ++i) {
    const auto word = u64_from_json(words.at(static_cast<qsizetype>(i)));
    if (!word) {
      return std::nullopt;
    }
    rng.words[i] = *word;
  }
  if (std::ranges::all_of(rng.words, [](auto word) { return word == 0; })) {
    return std::nullopt;
  }
  return rng;
}

} // namespace

auto capture_replay_checkpoint(Game::Session::SessionContext& session) -> QByteArray {
  using Engine::Core::Serialization;
  auto& world = session.world();
  const auto& clock = session.clock();
  const auto rng = session.rng().state();
  const auto* routes = world.get_system<Game::Systems::RouteFollowSystem>();
  const auto* commands = world.get_system<Game::Command::CommandSystem>();
  const auto* projectiles = world.get_system<Game::Systems::ProjectileSystem>();
  const auto* combat = world.get_system<Game::Systems::CombatSystem>();
  const auto* movement = world.get_system<Game::Systems::MovementSystem>();
  const auto* pathfinder = session.navigation().pathfinder();
  const auto* gather = world.get_system<Game::Systems::GatherLoopSystem>();
  const auto* settlement = world.get_system<Game::Systems::SettlementLifeSystem>();
  const auto* undead = world.get_system<Game::Systems::UndeadAwakeningSystem>();
  const auto* wildlife = world.get_system<Game::Wildlife::WildlifeSystem>();
  QJsonArray reserved_props;
  for (const auto id : session.terrain().reserved_world_props()) {
    reserved_props.append(u64_to_json(id));
  }
  QJsonArray words;
  for (const auto word : rng.words) {
    words.append(u64_to_json(word));
  }
  const QJsonObject state{
      {"version", 3},
      {"tick", u64_to_json(clock.tick())},
      {"world_tick", u64_to_json(world.tick_id())},
      {"speed", clock.time_scale()},
      {"rng_seed", u64_to_json(rng.seed)},
      {"rng_draws", u64_to_json(rng.draws)},
      {"rng_words", words},
      {"world",
       Serialization::serialize_world(&world,
                                      Serialization::WorldCapture::ReplayCheckpoint)
           .object()},
      {"registry", encode_registry(world.registry().capture_checkpoint())},
      {"commander_control",
       commands != nullptr ? commands->capture_checkpoint() : QJsonArray{}},
      {"projectiles",
       projectiles != nullptr ? QJsonValue(projectiles->capture_checkpoint())
                              : QJsonValue()},
      {"routes",
       routes != nullptr ? QJsonValue(routes->capture_checkpoint()) : QJsonValue()},
      {"movement",
       movement != nullptr ? QJsonValue(movement->capture_checkpoint()) : QJsonValue()},
      {"navigation",
       pathfinder != nullptr ? QJsonValue(pathfinder->capture_checkpoint())
                             : QJsonValue()},
      {"combat",
       combat != nullptr ? QJsonValue(combat->capture_checkpoint()) : QJsonValue()},
      {"systems", Game::Session::SessionSnapshot::capture({.world = &world})},
      {"visibility", session.simulation_visibility().capture_state()},
      {"alliance", session.alliance().capture_checkpoint()},
      {"tribute", session.marketplace().capture_checkpoint()},
      {"stats", session.stats().serialize_counters()},
      {"reserved_props", reserved_props},
      {"gather_cooldown",
       gather != nullptr ? QJsonValue(gather->think_cooldown()) : QJsonValue()},
      {"settlement",
       settlement != nullptr ? QJsonValue(settlement->capture_checkpoint())
                             : QJsonValue()},
      {"undead",
       undead != nullptr ? QJsonValue(undead->capture_checkpoint()) : QJsonValue()},
      {"wildlife",
       wildlife != nullptr ? QJsonValue(wildlife->capture_checkpoint()) : QJsonValue()},
      {"resources", encode_resources(session.economy().snapshot())},
      {"harvested", encode_resources(session.economy().harvested_snapshot())}};
  const auto bytes = QJsonDocument(state).toJson(QJsonDocument::Compact);
  return bytes.size() <= k_max_checkpoint_bytes ? qCompress(bytes, 3) : QByteArray{};
}

auto restore_replay_checkpoint(Game::Session::SessionContext& session,
                               const QByteArray& snapshot) -> bool {
  if (session.clock().tick() != 0 || snapshot.size() < 4 ||
      qFromBigEndian<std::uint32_t>(snapshot.constData()) > k_max_checkpoint_bytes) {
    return false;
  }
  const auto state = QJsonDocument::fromJson(qUncompress(snapshot)).object();
  const auto tick = u64_from_json(state.value(QLatin1String("tick")));
  const auto world_tick = u64_from_json(state.value(QLatin1String("world_tick")));
  const auto rng = decode_rng(state);
  const auto speed = state.value(QLatin1String("speed")).toDouble();
  const auto world = state.value(QLatin1String("world")).toObject();
  const auto registry = decode_registry(state.value("registry").toObject());
  if (state.value(QLatin1String("version")).toInt() != 3 || !tick || !world_tick ||
      !rng || !std::isfinite(speed) || speed <= 0.0 || !registry ||
      !world.value(QLatin1String("entities")).isArray()) {
    return false;
  }
  const auto* terrain = session.terrain().get_height_map();
  const auto terrain_state = world.value("terrain");
  if (terrain != nullptr) {
    const auto map = terrain_state.toObject();
    if (map.value("width").toInt() != terrain->get_width() ||
        map.value("height").toInt() != terrain->get_height() ||
        map.value("tile_size").toDouble() != terrain->get_tile_size()) {
      return false;
    }
  } else if (!terrain_state.isUndefined()) {
    return false;
  }
  const auto slot_count = registry->generations.size();
  for (const auto entity : world.value("entities").toArray()) {
    const auto id = entity.toObject().value("id").toInteger();
    if (id <= 0 || Engine::Core::Handle::index_of(id) >= slot_count) {
      return false;
    }
  }
  const auto next = world.value("nextEntityId").toInteger();
  if (next <= 0 || Engine::Core::Handle::index_of(next) != slot_count) {
    return false;
  }

  const Game::Session::ScopedThreadSession scope(session);
  auto& target = session.world();
  target.clear();
  Engine::Core::Serialization::deserialize_world(
      &target,
      QJsonDocument(world),
      Engine::Core::Serialization::WorldCapture::ReplayCheckpoint);
  if (!state.value("reserved_props").isArray()) {
    return false;
  }
  for (const auto row : state.value("reserved_props").toArray()) {
    const auto id = u64_from_json(row);
    if (!id || !session.terrain().reserve_world_prop(*id)) {
      return false;
    }
  }
  for (const auto value : world.value("entities").toArray()) {
    const auto row = value.toObject();
    auto* entity = target.get_entity(
        static_cast<Engine::Core::EntityID>(row.value("id").toInteger()));
    if (entity == nullptr ||
        !Engine::Core::EntityCodec::read_checkpoint_components(*entity, row)) {
      return false;
    }
  }
  if (!target.registry().restore_checkpoint(*registry)) {
    return false;
  }
  if (auto* commands = target.get_system<Game::Command::CommandSystem>();
      commands != nullptr && !commands->restore_checkpoint(
                                 target, state.value("commander_control").toArray())) {
    return false;
  }
  if (auto* projectiles = target.get_system<Game::Systems::ProjectileSystem>();
      projectiles != nullptr &&
      !projectiles->restore_checkpoint(state.value("projectiles").toObject())) {
    return false;
  }
  if (auto* routes = target.get_system<Game::Systems::RouteFollowSystem>();
      routes != nullptr &&
      !routes->restore_checkpoint(state.value("routes").toObject())) {
    return false;
  }
  if (const auto* map = session.terrain().get_height_map()) {
    session.navigation().initialize(map->get_width(), map->get_height());
    session.terrain().seal();
  }
  if (!session.simulation_visibility().restore_state(
          state.value("visibility").toObject(),
          session.terrain().get_height_map(),
          *tick)) {
    return false;
  }
  (void)Game::Persistence::rebuild_registries_after_load(&target, 0);
  if (auto* pathfinder = session.navigation().pathfinder();
      pathfinder != nullptr &&
      !pathfinder->restore_checkpoint(state.value("navigation").toObject())) {
    return false;
  }
  if (auto* movement = target.get_system<Game::Systems::MovementSystem>();
      movement != nullptr &&
      !movement->restore_checkpoint(state.value("movement").toObject())) {
    return false;
  }
  if (auto* combat = target.get_system<Game::Systems::CombatSystem>();
      combat != nullptr &&
      !combat->restore_checkpoint(state.value("combat").toObject())) {
    return false;
  }
  if (!session.alliance().restore_checkpoint(state.value("alliance").toObject()) ||
      !state.value("tribute").isArray() ||
      !session.marketplace().restore_checkpoint(state.value("tribute").toArray())) {
    return false;
  }
  if (auto* gather = target.get_system<Game::Systems::GatherLoopSystem>()) {
    const auto cooldown = state.value("gather_cooldown").toDouble(-1.0);
    if (cooldown < 0.0 ||
        cooldown > Game::Systems::GatherLoopSystem::k_think_interval) {
      return false;
    }
    gather->restore_think_cooldown(static_cast<float>(cooldown));
  }
  if (auto* settlement = target.get_system<Game::Systems::SettlementLifeSystem>();
      settlement != nullptr &&
      !settlement->restore_checkpoint(state.value("settlement").toArray())) {
    return false;
  }
  session.stats().restore_counters(state.value("stats").toArray());
  session.clock().restore(*tick);
  session.clock().set_time_scale(speed);
  target.restore_tick_id(*world_tick);
  session.commands().clear(*tick);
  session.rng().restore(*rng);
  session.economy().restore(decode_resources(state.value(QLatin1String("resources"))));
  session.economy().restore_harvested(
      decode_resources(state.value(QLatin1String("harvested"))));
  (void)Game::Session::SessionSnapshot::restore(
      {.world = &target}, state.value(QLatin1String("systems")).toObject());
  if (auto* undead = target.get_system<Game::Systems::UndeadAwakeningSystem>();
      undead != nullptr &&
      !undead->restore_checkpoint(state.value("undead").toObject())) {
    return false;
  }
  if (auto* wildlife = target.get_system<Game::Wildlife::WildlifeSystem>();
      wildlife != nullptr &&
      !wildlife->restore_checkpoint(state.value("wildlife").toObject())) {
    return false;
  }
  return true;
}

auto seek_replay(Game::Session::SessionContext& session,
                 std::uint64_t target_tick) -> std::optional<std::uint64_t> {
  auto* player = session.replay_player();
  if (player == nullptr || session.clock().tick() != 0 ||
      target_tick > player->file().last_tick()) {
    return std::nullopt;
  }
  std::uint64_t checkpoint_tick = 0;
  if (const auto* checkpoint = player->file().checkpoint_at_or_before(target_tick)) {
    if (!restore_replay_checkpoint(
            session,
            std::get<Game::Command::Checkpoint>(checkpoint->payload).snapshot) ||
        session.clock().tick() != checkpoint->tick) {
      return std::nullopt;
    }
    checkpoint_tick = checkpoint->tick;
    player->resume_after(checkpoint_tick);
    if (player->digest_due(checkpoint_tick) &&
        !player->check(checkpoint_tick, Game::Session::subsystem_digests(session))) {
      return std::nullopt;
    }
  }
  while (session.clock().tick() < target_tick) {
    const auto steps =
        std::min<std::uint64_t>(target_tick - session.clock().tick(), 120);
    if (session.advance_from_log(static_cast<int>(steps)) == 0 ||
        player->divergence()) {
      return std::nullopt;
    }
  }
  return checkpoint_tick;
}

} // namespace Game::Save
