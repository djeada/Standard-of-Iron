#include "skirmish_screen_system.h"

#include <QVector3D>

#include <limits>

#include "../core/component_gameplay.h"
#include "../core/system_context.h"
#include "../core/world.h"
#include "../units/spawn_type.h"
#include "combat_system/combat_utils.h"
#include "movement/command_service.h"
#include "owner_registry.h"

namespace Game::Systems {

namespace {

using Index = Engine::Core::WorldSpatialIndex;

[[nodiscard]] auto living_unit(const Index::Entry& entry) -> bool {
  return entry.is(Index::k_alive) && entry.health > 0 && !entry.is(Index::k_building) &&
         !entry.is(Index::k_wildlife) && !entry.is(Index::k_pending_removal);
}

[[nodiscard]] auto closes_to_melee(Engine::Core::World& world,
                                   Engine::Core::EntityID id) -> bool {
  auto const* unit = world.try_get<Engine::Core::UnitComponent>(id);
  if (unit == nullptr || Game::Units::is_skirmisher_spawn(unit->spawn_type)) {
    return false;
  }
  if (Game::Units::is_cavalry(unit->spawn_type) ||
      unit->spawn_type == Game::Units::SpawnType::Elephant) {
    return true;
  }
  return Combat::is_melee_mode(world.try_get<Engine::Core::AttackComponent>(id));
}

[[nodiscard]] auto holds_the_line(Engine::Core::World& world,
                                  Engine::Core::EntityID id) -> bool {
  auto const* unit = world.try_get<Engine::Core::UnitComponent>(id);
  if (unit == nullptr || Game::Units::is_skirmisher_spawn(unit->spawn_type) ||
      !Combat::is_infantry_spawn(unit->spawn_type)) {
    return false;
  }
  return Combat::is_melee_mode(world.try_get<Engine::Core::AttackComponent>(id));
}

} // namespace

void SkirmishScreenSystem::run(Engine::Core::SystemContext& context) {
  m_clock += context.delta_time();
  auto& world = context.world();
  auto& index = context.spatial_index();
  index.refresh(world);

  for (auto [entity_ref, movement_ref, transform_ref, unit_ref] :
       context.entity_view<Engine::Core::MovementComponent,
                           const Engine::Core::TransformComponent,
                           const Engine::Core::UnitComponent>()) {
    auto* entity = &entity_ref;
    if (!Game::Units::is_skirmisher_spawn(unit_ref.spawn_type) ||
        unit_ref.health <= 0 || movement_ref.get_has_target() ||
        Combat::is_unit_in_hold_mode(entity) || Combat::is_unit_in_guard_mode(entity)) {
      continue;
    }
    auto const id = entity->get_id();
    if (auto it = m_last_withdraw.find(id);
        it != m_last_withdraw.end() && m_clock - it->second < k_withdraw_cooldown) {
      continue;
    }

    float const x = transform_ref.position.x;
    float const z = transform_ref.position.z;
    int const owner = unit_ref.owner_id;

    float threat_dist_sq = std::numeric_limits<float>::max();
    QVector3D threat;
    index.for_each_in_radius(x, z, k_threat_radius, [&](const Index::Entry& entry) {
      if (!living_unit(entry) || !m_owners.are_enemies(owner, entry.owner_id) ||
          !closes_to_melee(world, entry.id)) {
        return;
      }
      float const dx = entry.x - x;
      float const dz = entry.z - z;
      if (dx * dx + dz * dz < threat_dist_sq) {
        threat_dist_sq = dx * dx + dz * dz;
        threat = QVector3D(entry.x, 0.0F, entry.z);
      }
    });
    if (threat_dist_sq == std::numeric_limits<float>::max()) {
      continue;
    }

    float line_dist_sq = std::numeric_limits<float>::max();
    QVector3D line;
    index.for_each_in_radius(
        x, z, k_line_search_radius, [&](const Index::Entry& entry) {
          if (entry.id == id || entry.owner_id != owner || !living_unit(entry) ||
              !holds_the_line(world, entry.id)) {
            return;
          }
          float const dx = entry.x - x;
          float const dz = entry.z - z;
          if (dx * dx + dz * dz < line_dist_sq) {
            line_dist_sq = dx * dx + dz * dz;
            line = QVector3D(entry.x, 0.0F, entry.z);
          }
        });

    QVector3D const here(x, 0.0F, z);
    QVector3D goal;
    if (line_dist_sq != std::numeric_limits<float>::max()) {
      QVector3D away = line - threat;
      away.setY(0.0F);
      if (away.lengthSquared() < 1e-4F) {
        away = here - threat;
      }
      goal = line + away.normalized() * k_behind_line_distance;
    } else {
      QVector3D away = here - threat;
      away.setY(0.0F);
      if (away.lengthSquared() < 1e-4F) {
        continue;
      }
      goal = here + away.normalized() * k_open_ground_retreat;
    }
    goal.setY(transform_ref.position.y);

    m_last_withdraw[id] = m_clock;
    CommandService::MoveOptions options;
    options.kind = MoveOrderKind::PlayerMove;
    CommandService::move_unit(world, id, goal, options);
  }

  if (m_last_withdraw.size() > 256U) {
    std::erase_if(m_last_withdraw, [&](const auto& item) {
      return m_clock - item.second >= k_withdraw_cooldown;
    });
  }
}

auto SkirmishScreenSystem::access() const -> Engine::Core::SystemAccess {
  using namespace Engine::Core;
  return SystemAccess::declare(Reads<UnitComponent,
                                     TransformComponent,
                                     BuildingComponent,
                                     PendingRemovalComponent>{},
                               Writes<AttackTargetComponent,
                                      MovementComponent,
                                      AttackComponent,
                                      PatrolComponent,
                                      HoldModeComponent,
                                      GuardModeComponent,
                                      FormationModeComponent,
                                      PlayerOrderIntentComponent>{});
}

} // namespace Game::Systems
