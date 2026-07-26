#include "battle_simulation.h"

#include <QCoreApplication>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <memory>
#include <system_error>
#include <unordered_map>
#include <vector>

#include "render/creature/bpat/bpat_registry.h"
#include "render/creature/snapshot_mesh_registry.h"

#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/core/event_manager.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/systems/arrow_system.h"
#include "game/systems/cleanup_system.h"
#include "game/systems/combat_status_effect_system.h"
#include "game/systems/combat_system.h"
#include "game/systems/combat_system/mounted_charge_processor.h"
#include "game/systems/command_service.h"
#include "game/systems/commander_system.h"
#include "game/systems/healing_beam_system.h"
#include "game/systems/healing_system.h"
#include "game/systems/movement_system.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/projectile_system.h"
#include "game/systems/stamina_system.h"
#include "game/systems/terrain_alignment_system.h"
#include "game/systems/troop_profile_service.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "game/units/unit.h"

namespace Balance {

namespace {

constexpr int k_side_a_owner = 1;
constexpr int k_side_b_owner = 2;
// Sampling cadence for cohesion / idle diagnostics. Fine enough to catch a
// collapsing formation, coarse enough not to dominate the run cost.
constexpr float k_sample_interval = 0.5F;

auto factory_registry() -> Game::Units::UnitFactoryRegistry& {
  static Game::Units::UnitFactoryRegistry registry;
  return registry;
}

void load_creature_pose_assets() {
  namespace fs = std::filesystem;
  const fs::path app_dir =
      fs::path(QCoreApplication::applicationDirPath().toStdString());
  const std::array<fs::path, 6> roots{
      fs::current_path() / "assets" / "creatures",
      fs::current_path() / ".." / "assets" / "creatures",
      app_dir / "assets" / "creatures",
      app_dir / ".." / "assets" / "creatures",
      app_dir / ".." / ".." / "assets" / "creatures",
      app_dir / ".." / ".." / ".." / "assets" / "creatures",
  };
  for (const auto& root : roots) {
    std::error_code error;
    if (!fs::exists(root / "humanoid.bpat", error)) {
      continue;
    }
    Render::Creature::Bpat::BpatRegistry::instance().load_all(root.string());
    Render::Creature::Snapshot::SnapshotMeshRegistry::instance().load_all(root.string());
    return;
  }
  qWarning("balance_sim: creature pose assets not found; melee traces will miss");
}

// Same integer hash the combat systems use, so jitter shares their statistical
// profile and stays reproducible across platforms.
auto hash_unit(std::uint32_t value) -> float {
  value ^= value >> 16U;
  value *= 0x7feb352dU;
  value ^= value >> 15U;
  value *= 0x846ca68bU;
  value ^= value >> 16U;
  return static_cast<float>(value & 0x00FFFFFFU) / static_cast<float>(0x00FFFFFFU);
}

auto jitter(std::uint32_t seed, std::uint32_t salt, float magnitude) -> float {
  return (hash_unit(seed ^ (salt * 0x9E3779B9U)) * 2.0F - 1.0F) * magnitude;
}

auto flat_map_definition(int width, int height) -> Game::Map::MapDefinition {
  Game::Map::MapDefinition def;
  def.name = QStringLiteral("balance_sim_flat");
  def.grid.width = width;
  def.grid.height = height;
  def.grid.tile_size = 1.0F;
  return def;
}

struct SpawnedUnit {
  std::unique_ptr<Game::Units::Unit> unit;
  Engine::Core::EntityID id{0};
};

// Plans where a side's squads stand without touching the world yet, so both
// sides can be spawned interleaved (see `spawn_planned`).
auto plan_side(const FixtureSide& side,
               int owner_id,
               const QVector3D& centre,
               float enemy_direction,
               std::uint32_t seed,
               std::uint32_t salt_base,
               float jitter_magnitude,
               int& total_cost,
               double& total_health) -> std::vector<Game::Units::SpawnParams> {
  auto& profiles = Game::Systems::TroopProfileService::instance();
  std::vector<Game::Units::SpawnParams> plan;

  int row_offset = 0;
  std::uint32_t salt = salt_base;
  for (const auto& group : side.groups) {
    const auto profile = profiles.get_profile(side.nation, group.troop);
    const int per_row = std::max(1, profile.max_units_per_row);
    // Squads are wide formations; keep enough lateral room that neighbours do
    // not start the fight already overlapping.
    const float lateral_spacing =
        std::max(2.5F, profile.visuals.selection_ring_size * 2.4F);
    const float rank_spacing = 3.0F;

    for (int index = 0; index < group.count; ++index) {
      const int row = index / per_row;
      const int column = index % per_row;
      const int row_width = std::min(per_row, group.count - row * per_row);

      const float lateral =
          (static_cast<float>(column) - (static_cast<float>(row_width) - 1.0F) * 0.5F) *
          lateral_spacing;
      // Later ranks stack away from the enemy so the front rank makes contact first.
      const float depth =
          -static_cast<float>(row + row_offset) * rank_spacing * enemy_direction;

      Game::Units::SpawnParams params;
      params.position =
          QVector3D(centre.x() + depth + jitter(seed, salt++, jitter_magnitude),
                    0.0F,
                    centre.z() + lateral + jitter(seed, salt++, jitter_magnitude));
      // Yaw convention matches the combat code: atan2(dx, dz) in degrees.
      params.rotation_y = enemy_direction > 0.0F ? 90.0F : 270.0F;
      params.player_id = owner_id;
      params.spawn_type = Game::Units::spawn_typeFromTroopType(group.troop);
      params.ai_controlled = false;
      params.nation_id = side.nation;
      params.is_initial_spawn = true;
      params.enables_production = false;
      plan.push_back(params);

      total_cost += profile.production.cost;
      total_health += profile.combat.max_health;
    }
    row_offset += (group.count + per_row - 1) / per_row;
  }
  return plan;
}

auto spawn_planned(Engine::Core::World& world,
                   const Game::Units::SpawnParams& params) -> SpawnedUnit {
  auto unit = factory_registry().create(
      Game::Units::spawn_typeToTroopType(params.spawn_type)
          .value_or(Game::Units::TroopType::Swordsman),
      world,
      params);
  if (!unit) {
    return {};
  }
  const auto id = unit->id();
  // Troop factories ignore SpawnParams::rotation_y, but formation geometry is
  // measured from the transform yaw, so a line that has not turned to face the
  // enemy would start the fight sideways.
  if (auto* entity = world.get_entity(id)) {
    if (auto* transform = entity->get_component<Engine::Core::TransformComponent>()) {
      transform->rotation.y = params.rotation_y;
    }
  }
  return {std::move(unit), id};
}

struct SideRuntime {
  int owner_id{0};
  Stance stance{Stance::Attack};
  std::vector<Engine::Core::EntityID> ids;
  std::vector<std::unique_ptr<Game::Units::Unit>> units;
};

auto live_entities(Engine::Core::World& world,
                   const std::vector<Engine::Core::EntityID>& ids)
    -> std::vector<Engine::Core::Entity*> {
  std::vector<Engine::Core::Entity*> alive;
  alive.reserve(ids.size());
  for (auto id : ids) {
    auto* entity = world.get_entity(id);
    if (entity == nullptr) {
      continue;
    }
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || unit->health <= 0) {
      continue;
    }
    alive.push_back(entity);
  }
  return alive;
}

auto centroid(const std::vector<Engine::Core::Entity*>& entities) -> QVector3D {
  if (entities.empty()) {
    return {};
  }
  QVector3D sum;
  for (auto* entity : entities) {
    const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (transform != nullptr) {
      sum += QVector3D(transform->position.x, 0.0F, transform->position.z);
    }
  }
  return sum / static_cast<float>(entities.size());
}

auto cohesion_radius(const std::vector<Engine::Core::Entity*>& entities) -> double {
  if (entities.size() < 2) {
    return 0.0;
  }
  const QVector3D centre = centroid(entities);
  double sum_sq = 0.0;
  for (auto* entity : entities) {
    const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (transform == nullptr) {
      continue;
    }
    const double dx = transform->position.x - centre.x();
    const double dz = transform->position.z - centre.z();
    sum_sq += dx * dx + dz * dz;
  }
  return std::sqrt(sum_sq / static_cast<double>(entities.size()));
}

auto is_idle_in_contact(Engine::Core::World& world,
                        Engine::Core::Entity* entity,
                        const std::vector<Engine::Core::Entity*>& enemies) -> bool {
  const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
  const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
  if (unit == nullptr || transform == nullptr) {
    return false;
  }

  // Hold mode is a standing order to not move, so a braced line is doing exactly
  // what it was told rather than idling.
  const auto* hold = entity->get_component<Engine::Core::HoldModeComponent>();
  if (hold != nullptr && hold->active) {
    return false;
  }

  const auto* attack = entity->get_component<Engine::Core::AttackComponent>();
  if (attack != nullptr && attack->in_melee_lock) {
    return false;
  }
  const auto* target = entity->get_component<Engine::Core::AttackTargetComponent>();
  if (target != nullptr && target->target_id != 0 &&
      world.get_entity(target->target_id) != nullptr) {
    return false;
  }
  const auto* movement = entity->get_component<Engine::Core::MovementComponent>();
  if (movement != nullptr && movement->get_has_target()) {
    return false;
  }

  const float vision_sq = unit->vision_range * unit->vision_range;
  for (auto* enemy : enemies) {
    const auto* enemy_transform =
        enemy->get_component<Engine::Core::TransformComponent>();
    if (enemy_transform == nullptr) {
      continue;
    }
    const float dx = enemy_transform->position.x - transform->position.x;
    const float dz = enemy_transform->position.z - transform->position.z;
    if (dx * dx + dz * dz <= vision_sq) {
      return true;
    }
  }
  return false;
}

void reissue_attack_orders(Engine::Core::World& world,
                           SideRuntime& side,
                           const std::vector<Engine::Core::EntityID>& enemy_ids);

void apply_stance(Engine::Core::World& world,
                  SideRuntime& side,
                  Stance stance,
                  const std::vector<Engine::Core::EntityID>& enemy_ids) {
  switch (stance) {
  case Stance::Stand:
    return;
  case Stance::Hold:
    for (auto& unit : side.units) {
      unit->set_hold_mode(true);
    }
    return;
  case Stance::Charge:
    for (auto id : side.ids) {
      if (auto* entity = world.get_entity(id)) {
        (void)Game::Systems::Combat::request_mounted_charge(
            *entity, Engine::Core::MountedChargeIntentSource::Player);
      }
    }
    for (auto& unit : side.units) {
      unit->set_run_mode(true);
    }
    break;
  case Stance::Attack:
    break;
  }

  // Attack and Charge both fan out across the enemy line so the whole side
  // commits, mirroring a player box-selecting and right-clicking.
  reissue_attack_orders(world, side, enemy_ids);
}

// A single right-click only lasts until its target dies. Re-issuing keeps the
// side committed the way a player or the AI would, which is what an attack-move
// order models; without it a fight can end with survivors idling in sight of
// each other.
void reissue_attack_orders(Engine::Core::World& world,
                           SideRuntime& side,
                           const std::vector<Engine::Core::EntityID>& enemy_ids) {
  std::vector<Engine::Core::EntityID> live_enemies;
  for (auto* enemy : live_entities(world, enemy_ids)) {
    live_enemies.push_back(enemy->get_id());
  }
  if (live_enemies.empty()) {
    return;
  }

  std::size_t target_index = 0;
  for (auto id : side.ids) {
    auto* entity = world.get_entity(id);
    if (entity == nullptr) {
      continue;
    }
    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || unit->health <= 0) {
      continue;
    }
    const auto target_id = live_enemies[target_index % live_enemies.size()];
    ++target_index;

    const auto* current = entity->get_component<Engine::Core::AttackTargetComponent>();
    if (current != nullptr && current->target_id != 0) {
      auto* held = world.get_entity(current->target_id);
      const auto* held_unit =
          held != nullptr ? held->get_component<Engine::Core::UnitComponent>() : nullptr;
      if (held_unit != nullptr && held_unit->health > 0) {
        continue;
      }
    }
    Game::Systems::CommandService::attack_target(world, {id}, target_id, true);
  }
}

} // namespace

auto outcome_name(Outcome outcome) -> QString {
  switch (outcome) {
  case Outcome::SideAWins:
    return QStringLiteral("side_a");
  case Outcome::SideBWins:
    return QStringLiteral("side_b");
  case Outcome::Timeout:
    return QStringLiteral("timeout");
  case Outcome::MutualElimination:
    return QStringLiteral("mutual");
  }
  return QStringLiteral("timeout");
}

void initialize_simulation_environment() {
  static bool initialized = false;
  if (initialized) {
    return;
  }
  initialized = true;

  // Melee resolution runs a weapon trace against baked creature poses. Without
  // the BPAT blobs the trace finds no contact and melee silently deals zero
  // damage, so load them before any battle runs.
  load_creature_pose_assets();

  Game::Systems::NationRegistry::instance().initialize_defaults();
  Game::Units::register_built_in_units(factory_registry());
}

auto run_battle(const Fixture& fixture,
                std::uint32_t seed,
                bool swap_sides,
                std::vector<TraceSample>* trace) -> BattleResult {
  initialize_simulation_environment();

  const FixtureSide& side_a_def = swap_sides ? fixture.side_b : fixture.side_a;
  const FixtureSide& side_b_def = swap_sides ? fixture.side_a : fixture.side_b;

  BattleResult result;
  result.seed = seed;
  result.sides_swapped = swap_sides;

  auto& owners = Game::Systems::OwnerRegistry::instance();
  owners.clear();
  owners.register_owner_with_id(k_side_a_owner, Game::Systems::OwnerType::Player, "A");
  owners.register_owner_with_id(k_side_b_owner, Game::Systems::OwnerType::Player, "B");
  owners.set_local_player_id(k_side_a_owner);

  auto& nations = Game::Systems::NationRegistry::instance();
  nations.set_player_nation(k_side_a_owner, side_a_def.nation);
  nations.set_player_nation(k_side_b_owner, side_b_def.nation);

  Game::Map::TerrainService::instance().initialize(
      flat_map_definition(fixture.grid_width, fixture.grid_height));
  Game::Systems::CommandService::initialize(fixture.grid_width, fixture.grid_height);

  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::ArrowSystem>());
  world.add_system(std::make_unique<Game::Systems::CombatStatusEffectSystem>());
  world.add_system(std::make_unique<Game::Systems::ProjectileSystem>());
  world.add_system(std::make_unique<Game::Systems::StaminaSystem>());
  world.add_system(std::make_unique<Game::Systems::MovementSystem>());
  world.add_system(std::make_unique<Game::Systems::CombatSystem>());
  world.add_system(std::make_unique<Game::Systems::CommanderSystem>());
  world.add_system(std::make_unique<Game::Systems::HealingBeamSystem>());
  world.add_system(std::make_unique<Game::Systems::HealingSystem>());
  world.add_system(std::make_unique<Game::Systems::TerrainAlignmentSystem>());
  world.add_system(std::make_unique<Game::Systems::CleanupSystem>());

  SideRuntime side_a{k_side_a_owner, side_a_def.stance, {}, {}};
  SideRuntime side_b{k_side_b_owner, side_b_def.stance, {}, {}};

  // Both the navigation grid and the terrain height map are centred on the
  // world origin, so the battle line is laid out around (0, 0).
  const QVector3D centre_a(-fixture.separation * 0.5F, 0.0F, 0.0F);
  const QVector3D centre_b(fixture.separation * 0.5F, 0.0F, 0.0F);

  const auto plan_a =
      plan_side(side_a_def, k_side_a_owner, centre_a, 1.0F, seed, 0x1000U,
                fixture.spawn_jitter, result.side_a.starting_cost,
                result.side_a.starting_health);
  const auto plan_b =
      plan_side(side_b_def, k_side_b_owner, centre_b, -1.0F, seed, 0x2000U,
                fixture.spawn_jitter, result.side_b.starting_cost,
                result.side_b.starting_health);

  // Systems iterate entities in id order, so spawning one side first hands it a
  // half-tick head start in every exchange. Interleave the two sides so the
  // spawn-side bias metric measures position, not creation order.
  for (std::size_t index = 0; index < std::max(plan_a.size(), plan_b.size()); ++index) {
    if (index < plan_a.size()) {
      auto spawned = spawn_planned(world, plan_a[index]);
      if (spawned.unit) {
        side_a.ids.push_back(spawned.id);
        side_a.units.push_back(std::move(spawned.unit));
      }
    }
    if (index < plan_b.size()) {
      auto spawned = spawn_planned(world, plan_b[index]);
      if (spawned.unit) {
        side_b.ids.push_back(spawned.id);
        side_b.units.push_back(std::move(spawned.unit));
      }
    }
  }
  result.side_a.starting_units = static_cast<int>(side_a.ids.size());
  result.side_b.starting_units = static_cast<int>(side_b.ids.size());

  std::unordered_map<Engine::Core::EntityID, int> owner_of;
  for (auto id : side_a.ids) {
    owner_of[id] = k_side_a_owner;
  }
  for (auto id : side_b.ids) {
    owner_of[id] = k_side_b_owner;
  }

  bool contact_seen = false;
  DamageBreakdown damage_a;
  DamageBreakdown damage_b;
  InvalidBehaviourCounts invalid;

  const Engine::Core::ScopedEventSubscription<Engine::Core::CombatHitEvent>
      hit_subscription([&](const Engine::Core::CombatHitEvent& event) {
        auto attacker_it = owner_of.find(event.attacker_id);
        auto target_it = owner_of.find(event.target_id);
        if (attacker_it == owner_of.end() || target_it == owner_of.end()) {
          return;
        }
        if (attacker_it->second == target_it->second) {
          ++invalid.friendly_fire_hits;
          return;
        }

        bool ranged = false;
        if (auto* attacker = world.get_entity(event.attacker_id)) {
          if (const auto* attack =
                  attacker->get_component<Engine::Core::AttackComponent>()) {
            ranged = attack->current_mode ==
                     Engine::Core::AttackComponent::CombatMode::Ranged;
            // Only a unit that could have swung instead is misbehaving; a siege
            // engine has no melee attack to switch to.
            if (ranged && attack->in_melee_lock && attack->can_melee) {
              ++invalid.ranged_shots_while_melee_locked;
            }
          }
        }

        DamageBreakdown& bucket =
            attacker_it->second == k_side_a_owner ? damage_a : damage_b;
        (ranged ? bucket.ranged : bucket.melee) += event.damage;
        contact_seen = true;
      });

  apply_stance(world, side_a, side_a.stance, side_b.ids);
  apply_stance(world, side_b, side_b.stance, side_a.ids);

  const float dt = fixture.timestep;
  float elapsed = 0.0F;
  float next_sample = 0.0F;
  double cohesion_sum_a = 0.0;
  double cohesion_sum_b = 0.0;
  int cohesion_samples = 0;

  Outcome outcome = Outcome::Timeout;

  constexpr float k_order_refresh_interval = 2.0F;
  float next_order_refresh = k_order_refresh_interval;

  while (elapsed < fixture.duration_seconds) {
    const bool was_contact = contact_seen;
    world.update(dt);
    elapsed += dt;

    auto alive_a = live_entities(world, side_a.ids);
    auto alive_b = live_entities(world, side_b.ids);

    if (!was_contact && contact_seen) {
      result.first_contact_time = elapsed;
      result.first_contact_distance =
          (centroid(alive_a) - centroid(alive_b)).length();
    }

    if (elapsed >= next_order_refresh) {
      next_order_refresh += k_order_refresh_interval;
      for (auto* pair : {&side_a, &side_b}) {
        if (pair->stance == Stance::Attack || pair->stance == Stance::Charge) {
          reissue_attack_orders(
              world, *pair, pair == &side_a ? side_b.ids : side_a.ids);
        }
      }
    }

    if (trace != nullptr && (trace->empty() ||
                             elapsed - trace->back().time_seconds >= 1.0F)) {
      TraceSample sample;
      sample.time_seconds = elapsed;
      sample.alive_a = static_cast<int>(alive_a.size());
      sample.alive_b = static_cast<int>(alive_b.size());
      sample.centroid_gap = (centroid(alive_a) - centroid(alive_b)).length();
      for (const auto* group : {&alive_a, &alive_b}) {
        for (auto* entity : *group) {
          if (const auto* unit = entity->get_component<Engine::Core::UnitComponent>()) {
            (group == &alive_a ? sample.health_a : sample.health_b) += unit->health;
          }
          const auto* target =
              entity->get_component<Engine::Core::AttackTargetComponent>();
          if (target != nullptr && target->target_id != 0) {
            ++sample.units_with_target;
          }
          const auto* attack = entity->get_component<Engine::Core::AttackComponent>();
          if (attack != nullptr && attack->in_melee_lock) {
            ++sample.units_in_melee_lock;
          }
          const auto* movement =
              entity->get_component<Engine::Core::MovementComponent>();
          if (movement != nullptr && movement->get_has_target()) {
            ++sample.units_moving;
          }
        }
      }
      trace->push_back(sample);
    }

    if (elapsed >= next_sample) {
      next_sample += k_sample_interval;
      if (!alive_a.empty() && !alive_b.empty()) {
        cohesion_sum_a += cohesion_radius(alive_a);
        cohesion_sum_b += cohesion_radius(alive_b);
        ++cohesion_samples;

        for (auto* entity : alive_a) {
          if (is_idle_in_contact(world, entity, alive_b)) {
            invalid.idle_unit_seconds_in_contact += k_sample_interval;
          }
        }
        for (auto* entity : alive_b) {
          if (is_idle_in_contact(world, entity, alive_a)) {
            invalid.idle_unit_seconds_in_contact += k_sample_interval;
          }
        }
      }
    }

    if (alive_a.empty() || alive_b.empty()) {
      if (alive_a.empty() && alive_b.empty()) {
        outcome = Outcome::MutualElimination;
      } else if (alive_b.empty()) {
        outcome = Outcome::SideAWins;
      } else {
        outcome = Outcome::SideBWins;
      }
      break;
    }
  }

  auto final_a = live_entities(world, side_a.ids);
  auto final_b = live_entities(world, side_b.ids);
  auto accumulate_health = [](const std::vector<Engine::Core::Entity*>& entities) {
    double total = 0.0;
    for (auto* entity : entities) {
      if (const auto* unit = entity->get_component<Engine::Core::UnitComponent>()) {
        total += std::max(0, unit->health);
      }
    }
    return total;
  };

  result.outcome = outcome;
  result.elapsed_seconds = elapsed;
  result.side_a.surviving_units = static_cast<int>(final_a.size());
  result.side_b.surviving_units = static_cast<int>(final_b.size());
  result.side_a.surviving_health = accumulate_health(final_a);
  result.side_b.surviving_health = accumulate_health(final_b);
  result.side_a.damage_dealt = damage_a;
  result.side_b.damage_dealt = damage_b;
  result.invalid = invalid;
  if (cohesion_samples > 0) {
    result.side_a.mean_cohesion_radius =
        cohesion_sum_a / static_cast<double>(cohesion_samples);
    result.side_b.mean_cohesion_radius =
        cohesion_sum_b / static_cast<double>(cohesion_samples);
  }

  // Everything above is keyed to spawn position (left/right). Report keys to the
  // fixture's own side A/B so a swapped run aggregates with an unswapped one.
  if (swap_sides) {
    std::swap(result.side_a, result.side_b);
    if (result.outcome == Outcome::SideAWins) {
      result.outcome = Outcome::SideBWins;
    } else if (result.outcome == Outcome::SideBWins) {
      result.outcome = Outcome::SideAWins;
    }
  }

  world.clear();
  return result;
}

} // namespace Balance
