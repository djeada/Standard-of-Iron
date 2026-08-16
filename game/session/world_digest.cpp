#include "world_digest.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "../core/component.h"
#include "../core/world.h"
#include "../systems/owner_registry.h"
#include "../systems/player_resource_registry.h"
#include "../systems/resource_types.h"
#include "deterministic_rng.h"
#include "session_context.h"
#include "simulation_clock.h"

namespace Game::Session {

namespace {

constexpr std::uint64_t k_offset = 1469598103934665603ULL;
constexpr std::uint64_t k_prime = 1099511628211ULL;

void mix(std::uint64_t& digest, std::uint64_t value) {
  for (int shift = 0; shift < 64; shift += 8) {
    digest ^= (value >> shift) & 0xFFU;
    digest *= k_prime;
  }
}

auto quantise(float value) -> std::int64_t {
  if (!std::isfinite(value)) {
    return INT64_MIN;
  }
  return static_cast<std::int64_t>(std::llround(static_cast<double>(value) * 1000.0));
}

struct EntityLine {
  Engine::Core::EntityID id = 0;
  int owner = 0;
  int kind = 0;
  std::int64_t x = 0;
  std::int64_t y = 0;
  std::int64_t z = 0;
  std::int64_t yaw = 0;
  int health = 0;
  int max_health = 0;
};

auto collect(const Engine::Core::World& world) -> std::vector<EntityLine> {
  std::vector<EntityLine> lines;
  lines.reserve(world.entity_count());
  world.for_each_entity([&lines](const Engine::Core::Entity& entity) {
    EntityLine line;
    line.id = entity.get_id();
    if (const auto* transform =
            entity.get_component<Engine::Core::TransformComponent>()) {
      line.x = quantise(transform->position.x);
      line.y = quantise(transform->position.y);
      line.z = quantise(transform->position.z);
      line.yaw = quantise(transform->rotation.y);
    }
    if (const auto* unit = entity.get_component<Engine::Core::UnitComponent>()) {
      line.owner = unit->owner_id;
      line.kind = static_cast<int>(unit->spawn_type);
      line.health = unit->health;
      line.max_health = unit->max_health;
    }
    lines.push_back(line);
  });
  std::sort(lines.begin(), lines.end(), [](const EntityLine& a, const EntityLine& b) {
    return a.id < b.id;
  });
  return lines;
}

} // namespace

auto world_digest(const Engine::Core::World& world) -> std::uint64_t {
  std::uint64_t digest = k_offset;
  for (const auto& line : collect(world)) {
    mix(digest, line.id);
    mix(digest, static_cast<std::uint64_t>(line.owner));
    mix(digest, static_cast<std::uint64_t>(line.kind));
    mix(digest, static_cast<std::uint64_t>(line.x));
    mix(digest, static_cast<std::uint64_t>(line.y));
    mix(digest, static_cast<std::uint64_t>(line.z));
    mix(digest, static_cast<std::uint64_t>(line.yaw));
    mix(digest, static_cast<std::uint64_t>(line.health));
  }
  return digest;
}

auto session_digest(SessionContext& session) -> std::uint64_t {
  std::uint64_t digest = world_digest(session.world());
  mix(digest, session.clock().tick());
  mix(digest, session.rng().draw_count());
  for (const auto& owner : session.owners().get_all_owners()) {
    mix(digest, static_cast<std::uint64_t>(owner.owner_id));
    const auto stock = session.economy().get_all(owner.owner_id);
    for (const auto type : Game::Systems::k_all_resource_types) {
      mix(digest, static_cast<std::uint64_t>(stock.get(type)));
    }
  }
  return digest;
}

auto describe_world(const Engine::Core::World& world) -> std::string {
  std::string out;
  char buffer[160];
  for (const auto& line : collect(world)) {
    std::snprintf(buffer,
                  sizeof(buffer),
                  "%llu owner=%d kind=%d pos=(%lld,%lld,%lld) yaw=%lld hp=%d/%d\n",
                  static_cast<unsigned long long>(line.id),
                  line.owner,
                  line.kind,
                  static_cast<long long>(line.x),
                  static_cast<long long>(line.y),
                  static_cast<long long>(line.z),
                  static_cast<long long>(line.yaw),
                  line.health,
                  line.max_health);
    out += buffer;
  }
  return out;
}

} // namespace Game::Session
