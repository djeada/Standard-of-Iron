#include "world_digest.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "../core/component_commander.h"
#include "../core/component_economy.h"
#include "../core/component_gameplay.h"
#include "../core/component_structures.h"
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

  std::int64_t goal_x = 0;
  std::int64_t goal_z = 0;
  std::int64_t path_index = -1;
  int movement_state = -1;
  std::int64_t stamina = -1;
  int running = -1;
  int hold_active = -1;
  std::int64_t hold_exit_cooldown = -1;
  int guard_active = -1;
  Engine::Core::EntityID guarded_entity = 0;
  std::int64_t formation_id = -1;
  int formation_slot = -1;

  Engine::Core::EntityID attack_target = 0;
  std::int64_t attack_cooldown = -1;
  std::int64_t stagger_remaining = -1;
  int stagger_tier = -1;
  std::int64_t poise = -1;
  std::int64_t morale = -1;
  int routing = -1;

  std::int64_t production_remaining = -1;
  int production_queue = -1;
  int produced_count = -1;
  int capturing_player = -2;
  std::int64_t capture_progress = -1;
  int population_contribution = -1;
  Engine::Core::EntityID nearest_barracks = 0;
  std::int64_t family_cooldown = -1;

  int wildlife_species = -1;
  int wildlife_behavior = -1;
  int wildlife_group = -1;
  std::int64_t wildlife_target_x = 0;
  std::int64_t wildlife_target_z = 0;
  std::int64_t wildlife_think_cooldown = -1;
  std::int64_t wildlife_bite_timer = -1;
  Engine::Core::EntityID wildlife_focus = 0;
  Engine::Core::EntityID wildlife_aggressor = 0;
  std::uint32_t wildlife_rng = 0;
};

auto collect(const Engine::Core::World& world) -> std::vector<EntityLine> {
  std::vector<EntityLine> lines;
  lines.reserve(world.entity_count());
  world.for_each_entity([&lines, &world](const Engine::Core::Entity& entity) {
    EntityLine line;
    line.id = entity.get_id();
    if (const auto* transform =
            world.try_get<Engine::Core::TransformComponent>(line.id)) {
      line.x = quantise(transform->position.x);
      line.y = quantise(transform->position.y);
      line.z = quantise(transform->position.z);
      line.yaw = quantise(transform->rotation.y);
    }
    if (const auto* unit = world.try_get<Engine::Core::UnitComponent>(line.id)) {
      line.owner = unit->owner_id;
      line.kind = static_cast<int>(unit->spawn_type);
      line.health = unit->health;
      line.max_health = unit->max_health;
    }
    if (const auto* movement =
            world.try_get<Engine::Core::MovementComponent>(line.id)) {
      line.goal_x = quantise(movement->get_goal_x());
      line.goal_z = quantise(movement->get_goal_y());
      line.path_index = static_cast<std::int64_t>(movement->get_path_index());
      line.movement_state = static_cast<int>(movement->get_state());
    }
    if (const auto* stamina = world.try_get<Engine::Core::StaminaComponent>(line.id)) {
      line.stamina = quantise(stamina->stamina);
      line.running = static_cast<int>(stamina->is_running);
    }
    if (const auto* hold = world.try_get<Engine::Core::HoldModeComponent>(line.id)) {
      line.hold_active = static_cast<int>(hold->active);
      line.hold_exit_cooldown = quantise(hold->exit_cooldown);
    }
    if (const auto* guard = world.try_get<Engine::Core::GuardModeComponent>(line.id)) {
      line.guard_active = static_cast<int>(guard->active);
      line.guarded_entity = guard->guarded_entity_id;
    }
    if (const auto* formation =
            world.try_get<Engine::Core::FormationModeComponent>(line.id)) {
      line.formation_id = static_cast<std::int64_t>(formation->formation_id);
      line.formation_slot = formation->stable_slot_id;
    }
    if (const auto* target =
            world.try_get<Engine::Core::AttackTargetComponent>(line.id)) {
      line.attack_target = target->target_id;
    }
    if (const auto* attack = world.try_get<Engine::Core::AttackComponent>(line.id)) {
      line.attack_cooldown = quantise(attack->time_since_last);
    }
    if (const auto* stagger = world.try_get<Engine::Core::StaggerComponent>(line.id)) {
      line.stagger_remaining = quantise(stagger->remaining);
      line.stagger_tier = static_cast<int>(stagger->tier);
    }
    if (const auto* poise = world.try_get<Engine::Core::PoiseComponent>(line.id)) {
      line.poise = quantise(poise->current);
    }
    if (const auto* morale = world.try_get<Engine::Core::MoraleComponent>(line.id)) {
      line.morale = quantise(morale->morale);
      line.routing = static_cast<int>(morale->routing);
    }
    if (const auto* production =
            world.try_get<Engine::Core::ProductionComponent>(line.id)) {
      line.production_remaining = quantise(production->time_remaining);
      line.production_queue = static_cast<int>(production->production_queue.size());
      line.produced_count = production->produced_count;
    }
    if (const auto* capture = world.try_get<Engine::Core::CaptureComponent>(line.id)) {
      line.capturing_player = capture->capturing_player_id;
      line.capture_progress = quantise(capture->capture_progress);
    }
    if (const auto* home = world.try_get<Engine::Core::HomeComponent>(line.id)) {
      line.population_contribution = home->population_contribution;
      line.nearest_barracks = home->nearest_barracks_id;
      line.family_cooldown = quantise(home->family_generation_cooldown);
    }
    if (const auto* wildlife =
            world.try_get<Engine::Core::WildlifeComponent>(line.id)) {
      line.wildlife_species = static_cast<int>(wildlife->species);
      line.wildlife_behavior = static_cast<int>(wildlife->behavior);
      line.wildlife_group = static_cast<int>(wildlife->group_id);
      line.wildlife_target_x = quantise(wildlife->target_x);
      line.wildlife_target_z = quantise(wildlife->target_z);
      line.wildlife_think_cooldown = quantise(wildlife->think_cooldown);
      line.wildlife_bite_timer = quantise(wildlife->bite_timer);
      line.wildlife_focus = wildlife->focus_id;
      line.wildlife_aggressor = wildlife->aggressor_id;
      line.wildlife_rng = wildlife->rng_state;
    }
    lines.push_back(line);
  });
  std::sort(lines.begin(), lines.end(), [](const EntityLine& a, const EntityLine& b) {
    return a.id < b.id;
  });
  return lines;
}

void mix_identity(std::uint64_t& digest, const EntityLine& line) {
  mix(digest, line.id);
  mix(digest, static_cast<std::uint64_t>(line.owner));
  mix(digest, static_cast<std::uint64_t>(line.kind));
  mix(digest, static_cast<std::uint64_t>(line.x));
  mix(digest, static_cast<std::uint64_t>(line.y));
  mix(digest, static_cast<std::uint64_t>(line.z));
  mix(digest, static_cast<std::uint64_t>(line.yaw));
  mix(digest, static_cast<std::uint64_t>(line.health));
  mix(digest, static_cast<std::uint64_t>(line.max_health));
}

void mix_movement(std::uint64_t& digest, const EntityLine& line) {
  mix(digest, line.id);
  mix(digest, static_cast<std::uint64_t>(line.goal_x));
  mix(digest, static_cast<std::uint64_t>(line.goal_z));
  mix(digest, static_cast<std::uint64_t>(line.path_index));
  mix(digest, static_cast<std::uint64_t>(line.movement_state));
  mix(digest, static_cast<std::uint64_t>(line.stamina));
  mix(digest, static_cast<std::uint64_t>(line.running));
  mix(digest, static_cast<std::uint64_t>(line.hold_active));
  mix(digest, static_cast<std::uint64_t>(line.hold_exit_cooldown));
  mix(digest, static_cast<std::uint64_t>(line.guard_active));
  mix(digest, line.guarded_entity);
  mix(digest, static_cast<std::uint64_t>(line.formation_id));
  mix(digest, static_cast<std::uint64_t>(line.formation_slot));
}

void mix_combat(std::uint64_t& digest, const EntityLine& line) {
  mix(digest, line.id);
  mix(digest, line.attack_target);
  mix(digest, static_cast<std::uint64_t>(line.attack_cooldown));
  mix(digest, static_cast<std::uint64_t>(line.stagger_remaining));
  mix(digest, static_cast<std::uint64_t>(line.stagger_tier));
  mix(digest, static_cast<std::uint64_t>(line.poise));
}

void mix_status(std::uint64_t& digest, const EntityLine& line) {
  mix(digest, line.id);
  mix(digest, static_cast<std::uint64_t>(line.morale));
  mix(digest, static_cast<std::uint64_t>(line.routing));
  mix(digest, static_cast<std::uint64_t>(line.capturing_player));
  mix(digest, static_cast<std::uint64_t>(line.capture_progress));
}

void mix_economy_line(std::uint64_t& digest, const EntityLine& line) {
  mix(digest, line.id);
  mix(digest, static_cast<std::uint64_t>(line.production_remaining));
  mix(digest, static_cast<std::uint64_t>(line.production_queue));
  mix(digest, static_cast<std::uint64_t>(line.produced_count));
  mix(digest, static_cast<std::uint64_t>(line.population_contribution));
  mix(digest, line.nearest_barracks);
  mix(digest, static_cast<std::uint64_t>(line.family_cooldown));
}

void mix_wildlife(std::uint64_t& digest, const EntityLine& line) {
  if (line.wildlife_species < 0) {
    return;
  }
  mix(digest, line.id);
  mix(digest, static_cast<std::uint64_t>(line.wildlife_species));
  mix(digest, static_cast<std::uint64_t>(line.wildlife_behavior));
  mix(digest, static_cast<std::uint64_t>(line.wildlife_group));
  mix(digest, static_cast<std::uint64_t>(line.wildlife_target_x));
  mix(digest, static_cast<std::uint64_t>(line.wildlife_target_z));
  mix(digest, static_cast<std::uint64_t>(line.wildlife_think_cooldown));
  mix(digest, static_cast<std::uint64_t>(line.wildlife_bite_timer));
  mix(digest, line.wildlife_focus);
  mix(digest, line.wildlife_aggressor);
  mix(digest, line.wildlife_rng);
}

} // namespace

auto world_digest(const Engine::Core::World& world) -> std::uint64_t {
  std::uint64_t digest = k_offset;
  for (const auto& line : collect(world)) {
    mix_identity(digest, line);
    mix_movement(digest, line);
    mix_combat(digest, line);
    mix_status(digest, line);
    mix_economy_line(digest, line);
    mix_wildlife(digest, line);
  }
  return digest;
}

auto subsystem_digests(SessionContext& session) -> SubsystemDigests {
  SubsystemDigests digests;
  digests.identity = k_offset;
  digests.movement = k_offset;
  digests.combat = k_offset;
  digests.status = k_offset;
  digests.economy = k_offset;
  digests.wildlife = k_offset;
  digests.session = k_offset;

  for (const auto& line : collect(session.world())) {
    mix_identity(digests.identity, line);
    mix_movement(digests.movement, line);
    mix_combat(digests.combat, line);
    mix_status(digests.status, line);
    mix_economy_line(digests.economy, line);
    mix_wildlife(digests.wildlife, line);
  }

  mix(digests.session, session.clock().tick());
  mix(digests.session, session.rng().draw_count());
  for (const auto& owner : session.owners().get_all_owners()) {
    mix(digests.session, static_cast<std::uint64_t>(owner.owner_id));
    const auto stock = session.economy().get_all(owner.owner_id);
    for (const auto type : Game::Systems::k_all_resource_types) {
      mix(digests.session, static_cast<std::uint64_t>(stock.get(type)));
    }
  }

  digests.root = k_offset;
  mix(digests.root, digests.identity);
  mix(digests.root, digests.movement);
  mix(digests.root, digests.combat);
  mix(digests.root, digests.status);
  mix(digests.root, digests.economy);
  mix(digests.root, digests.wildlife);
  mix(digests.root, digests.session);
  return digests;
}

auto name_of_first_difference(const SubsystemDigests& recorded,
                              const SubsystemDigests& observed) -> const char* {
  if (recorded.identity != observed.identity) {
    return "identity";
  }
  if (recorded.movement != observed.movement) {
    return "movement";
  }
  if (recorded.combat != observed.combat) {
    return "combat";
  }
  if (recorded.status != observed.status) {
    return "status";
  }
  if (recorded.economy != observed.economy) {
    return "economy";
  }
  if (recorded.wildlife != observed.wildlife) {
    return "wildlife";
  }
  if (recorded.session != observed.session) {
    return "session";
  }
  if (recorded.root != observed.root) {
    return "root";
  }
  return nullptr;
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
