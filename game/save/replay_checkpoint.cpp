#include "replay_checkpoint.h"

#include <QJsonArray>
#include <QtEndian>

#include <algorithm>
#include <cmath>

#include "../command/command_queue.h"
#include "../command/json_u64.h"
#include "../core/world.h"
#include "../map/terrain.h"
#include "../map/terrain_service.h"
#include "../session/deterministic_rng.h"
#include "../session/session_context.h"
#include "../session/session_snapshot.h"
#include "../session/simulation_clock.h"
#include "../systems/navigation/navigation_service.h"
#include "../systems/player_resource_registry.h"
#include "../systems/world_restore.h"
#include "serialization.h"

namespace Game::Save {
namespace {

constexpr std::uint32_t k_max_checkpoint_bytes = 64 * 1024 * 1024;

using Game::Command::u64_from_json;
using Game::Command::u64_to_json;
using Game::Systems::k_all_resource_types;
using Game::Systems::OwnerResourceState;
using Game::Systems::resource_type_key;

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
  auto& world = session.world();
  const auto& clock = session.clock();
  const auto rng = session.rng().state();
  QJsonArray words;
  for (const auto word : rng.words) {
    words.append(u64_to_json(word));
  }
  const QJsonObject state{
      {"version", 1},
      {"tick", u64_to_json(clock.tick())},
      {"world_tick", u64_to_json(world.tick_id())},
      {"speed", clock.time_scale()},
      {"rng_seed", u64_to_json(rng.seed)},
      {"rng_draws", u64_to_json(rng.draws)},
      {"rng_words", words},
      {"world", Engine::Core::Serialization::serialize_world(&world).object()},
      {"systems", Game::Session::SessionSnapshot::capture({.world = &world})},
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
  if (state.value(QLatin1String("version")).toInt() != 1 || !tick || !world_tick ||
      !rng || !std::isfinite(speed) || speed <= 0.0 ||
      !world.value(QLatin1String("entities")).isArray()) {
    return false;
  }

  auto& target = session.world();
  target.clear();
  Engine::Core::Serialization::deserialize_world(&target, QJsonDocument(world));
  if (const auto* map = session.terrain().get_height_map()) {
    session.navigation().initialize(map->get_width(), map->get_height());
    session.terrain().seal();
  }
  (void)Game::Persistence::rebuild_registries_after_load(&target, 0);
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
  return true;
}

} // namespace Game::Save
