#include "skirmish_screen_system.h"

#include <QVector3D>

#include <cmath>
#include <limits>

#include "../core/component_gameplay.h"
#include "../core/system_context.h"
#include "../core/world.h"
#include "../units/spawn_type.h"
#include "combat_system/attack_control.h"
#include "combat_system/combat_utils.h"
#include "movement/command_service.h"
#include "owner_registry.h"

namespace Game::Systems {

namespace {

using Index = Engine::Core::WorldSpatialIndex;
constexpr float k_immediate_threat = 2.5F;
constexpr float k_screen_half_width = 2.6F;
constexpr float k_minimum_gain = 1.25F;

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

[[nodiscard]] auto is_tied_up_elsewhere(Engine::Core::World& world,
                                        Engine::Core::EntityID threat,
                                        Engine::Core::EntityID skirmisher) -> bool {
  auto const* attack = world.try_get<Engine::Core::AttackComponent>(threat);
  return attack != nullptr && attack->in_melee_lock &&
         attack->melee_lock_target_id != 0 &&
         attack->melee_lock_target_id != skirmisher;
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
        movement_ref.has_waypoints() || Combat::in_rts_melee_lock(entity) ||
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
      float const distance_sq = dx * dx + dz * dz;
      if (distance_sq > k_immediate_threat * k_immediate_threat &&
          is_tied_up_elsewhere(world, entry.id, id)) {
        return;
      }
      if (distance_sq < threat_dist_sq) {
        threat_dist_sq = distance_sq;
        threat = QVector3D(entry.x, 0.0F, entry.z);
      }
    });
    if (threat_dist_sq == std::numeric_limits<float>::max() || threat_dist_sq < 1e-4F) {
      continue;
    }

    QVector3D const here(x, 0.0F, z);
    float const threat_distance = std::sqrt(threat_dist_sq);
    QVector3D const away = (here - threat) / threat_distance;
    QVector3D goal = threat + away * k_firing_distance;
    float best_screen_step_sq = std::numeric_limits<float>::max();
    bool screened = false;
    index.for_each_in_radius(
        x, z, k_line_search_radius, [&](const Index::Entry& entry) {
          if (entry.id == id || entry.owner_id != owner || !living_unit(entry) ||
              !holds_the_line(world, entry.id)) {
            return;
          }
          QVector3D const line(entry.x, 0.0F, entry.z);
          QVector3D const from_threat = line - threat;
          float const span = QVector3D::dotProduct(from_threat, away);
          float const sideways_sq = (from_threat - away * span).lengthSquared();
          if (threat_distance > k_immediate_threat && span > 0.6F &&
              span < threat_distance - 0.65F &&
              sideways_sq < k_screen_half_width * k_screen_half_width) {
            screened = true;
          }
          if (span <= 0.6F || sideways_sq > k_screen_half_width * k_screen_half_width) {
            return;
          }
          QVector3D const candidate =
              line + from_threat.normalized() * k_behind_line_distance;
          QVector3D const step = candidate - here;
          float const separation = (candidate - threat).length();
          float const step_sq = step.lengthSquared();
          // A screen is useful only if reaching it opens space and leaves the
          // skirmishers near their firing line.
          if (QVector3D::dotProduct(step, away) >= k_minimum_gain &&
              separation >= k_threat_radius + k_minimum_gain &&
              separation <= k_firing_distance + 1.1F && step_sq < best_screen_step_sq) {
            best_screen_step_sq = step_sq;
            goal = candidate;
          }
        });
    if (screened) {
      continue;
    }
    goal.setY(transform_ref.position.y);

    m_last_withdraw[id] = m_clock;
    CommandService::MoveOptions options;
    options.kind = MoveOrderKind::PlayerMove;
    CommandService::move_unit(world, id, goal, options);
    if (movement_ref.get_has_target()) {
      Combat::clear_orphaned_rts_attack_presentation(entity);
    }
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
                                      CombatStateComponent,
                                      RpgCommanderActionComponent,
                                      PatrolComponent,
                                      HoldModeComponent,
                                      GuardModeComponent,
                                      FormationModeComponent,
                                      PlayerOrderIntentComponent>{});
}

} // namespace Game::Systems
