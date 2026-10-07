#include "movement_system_heading.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "core/component_economy.h"
#include "formation/army_formation_registry.h"
#include "map/terrain_service.h"
#include "systems/defensive_unit_layout_service.h"
#include "systems/formation_combat_geometry.h"
#include "units/spawn_type.h"
#include "util/planar_math.h"

namespace Game::Systems::MovementHeading {

namespace {

constexpr float desired_yaw_turn_speed_degrees = 720.0F;

constexpr float k_swing_turn_speed_degrees = 200.0F;

constexpr float k_formation_heading_min_speed = 0.4F;
constexpr float k_formation_heading_speed_fraction = 0.25F;
constexpr float k_formation_intent_min_distance = 1.0F;

constexpr float full_translation_heading_error_degrees = 20.0F;
constexpr float k_formation_about_face_degrees = 100.0F;
constexpr float k_about_face_cooldown_seconds = 0.5F;

constexpr float k_formation_align_distance = 11.0F;
constexpr float k_formation_align_max_deviation_degrees = 55.0F;

constexpr float k_formation_heading_deadband_degrees = 6.0F;
constexpr float k_heading_hold_speed = 0.25F;

constexpr float k_formation_outer_file_speed_floor = 1.8F;
constexpr float k_formation_outer_file_speed_scale = 1.0F;

struct HeadingReference {
  bool valid{false};
  float yaw{0.0F};
};

auto aligned_with_slot(float travel_yaw,
                       const Engine::Core::TransformComponent& transform,
                       float remaining_distance) -> float {
  if (!transform.has_desired_yaw || remaining_distance >= k_formation_align_distance) {
    return travel_yaw;
  }
  float const closeness =
      std::clamp(1.0F - (remaining_distance / k_formation_align_distance), 0.0F, 1.0F);
  float const to_slot =
      Game::Systems::signed_yaw_delta(travel_yaw, transform.desired_yaw);
  float const blended = std::clamp(to_slot * closeness,
                                   -k_formation_align_max_deviation_degrees,
                                   k_formation_align_max_deviation_degrees);
  return travel_yaw + blended;
}

auto heading_reference(const Engine::Core::Entity& entity,
                       const Engine::Core::TransformComponent& transform,
                       const Engine::Core::MovementComponent& movement,
                       const Engine::Core::MovementFactsComponent* facts,
                       const Engine::Core::UnitComponent* unit) -> HeadingReference {
  bool const formation =
      unit != nullptr && FormationCombat::has_formation_slots(entity);
  if (movement.get_following_formation_slot()) {
    const auto* membership =
        entity.get_component<Engine::Core::ArmyFormationMembershipComponent>();
    const auto* group = membership != nullptr
                            ? Game::Formation::ArmyFormationRegistry::instance().find(
                                  membership->group_id)
                            : nullptr;
    if (group != nullptr && group->maintains_formation() &&
        (group->has_destination || movement.get_has_target())) {
      const auto* slot = group->find_slot_for(entity.get_id());
      if (slot != nullptr &&
          std::hypot(transform.position.x - slot->world_position.x(),
                     transform.position.z - slot->world_position.z()) <= 3.0F &&
          movement.remaining_waypoints() <= 1U) {

        return {true, slot->facing};
      }
    }
  }
  if (formation && movement.get_has_target()) {
    float const to_target_x = movement.get_target_x() - transform.position.x;
    float const to_target_z = movement.get_target_y() - transform.position.z;
    float const remaining =
        std::sqrt((to_target_x * to_target_x) + (to_target_z * to_target_z));
    if (facts != nullptr && facts->desired.valid) {
      float dx = facts->desired.heading_x;
      float dz = facts->desired.heading_z;
      if (dx * dx + dz * dz <= 1.0e-5F) {
        dx = facts->desired.velocity_x;
        dz = facts->desired.velocity_z;
      }
      if (dx * dx + dz * dz > 1.0e-5F) {
        return {true,
                aligned_with_slot(Game::Systems::yaw_degrees_from_direction(dx, dz),
                                  transform,
                                  remaining)};
      }
    }
    if (remaining > k_formation_intent_min_distance) {
      return {true,
              aligned_with_slot(std::atan2(to_target_x, to_target_z) * 180.0F /
                                    std::numbers::pi_v<float>,
                                transform,
                                remaining)};
    }
  }
  float const vx = movement.get_vx();
  float const vz = movement.get_vz();
  float const speed2 = vx * vx + vz * vz;
  float const min_speed =
      formation ? std::max(k_formation_heading_min_speed,
                           unit->speed * k_formation_heading_speed_fraction)
                : 0.0F;
  if (speed2 <= std::max(1.0e-5F, min_speed * min_speed)) {
    return {};
  }
  return {true, std::atan2(vx, vz) * 180.0F / std::numbers::pi_v<float>};
}

class FacingPass {
public:
  FacingPass(Engine::Core::World& world,
             Engine::Core::Entity& entity,
             Engine::Core::TransformComponent& transform,
             Engine::Core::MovementComponent& movement,
             const Engine::Core::MovementFactsComponent* facts,
             const Engine::Core::UnitComponent* unit,
             float delta_time)
      : m_entity(entity)
      , m_transform(transform)
      , m_movement(movement)
      , m_facts(facts)
      , m_unit(unit)
      , m_delta_time(delta_time) {
    float const body_turn_speed =
        unit != nullptr ? Game::Units::body_turn_speed_degrees(unit->spawn_type)
                        : desired_yaw_turn_speed_degrees;
    m_turn_speed =
        (unit != nullptr ? formation_turn_speed_degrees(entity, *unit, body_turn_speed)
                         : body_turn_speed) *
        DefensiveUnitLayoutService::turn_speed_multiplier(entity);

    m_shell_holds_its_face =
        DefensiveUnitLayoutService::holds_position(entity) && transform.has_desired_yaw;
    m_formation = unit != nullptr && FormationCombat::has_formation_slots(entity);
    if (!m_formation) {
      if (const auto* combat =
              world.try_get<Engine::Core::CombatStateComponent>(entity.get_id());
          combat != nullptr &&
          (combat->animation_state == Engine::Core::CombatAnimationState::Strike ||
           combat->animation_state == Engine::Core::CombatAnimationState::Impact)) {
        m_turn_speed = std::min(m_turn_speed, k_swing_turn_speed_degrees);
      }
    }

    m_traversal =
        world.try_get<Engine::Core::UnitTraversalLayoutStateComponent>(entity.get_id());
    if (m_traversal != nullptr && m_traversal->about_face_cooldown_seconds > 0.0F) {
      m_traversal->about_face_cooldown_seconds =
          std::max(0.0F, m_traversal->about_face_cooldown_seconds - delta_time);
    }
  }

  [[nodiscard]] auto faced_about() const -> bool { return m_faced_about; }

  void run() {
    auto const reference =
        heading_reference(m_entity, m_transform, m_movement, m_facts, m_unit);
    if (reference.valid && !m_shell_holds_its_face) {
      turn_toward_heading(reference);
    } else if (m_transform.has_desired_yaw) {
      turn_toward_desired_yaw();
    }
  }

private:
  void face_about_toward(float yaw) {
    bool const may_face_about =
        m_traversal == nullptr || m_traversal->about_face_cooldown_seconds <= 0.0F;
    m_faced_about =
        m_formation && may_face_about && !m_shell_holds_its_face &&
        std::fabs(Game::Systems::signed_yaw_delta(m_transform.rotation.y, yaw)) >
            k_formation_about_face_degrees &&
        FormationCombat::face_about_in_place(m_entity);
    if (m_faced_about && m_traversal != nullptr) {
      m_traversal->about_face_cooldown_seconds = k_about_face_cooldown_seconds;
    }
  }

  void turn_toward_heading(const HeadingReference& reference) {
    face_about_toward(reference.yaw);

    bool const standing_still = m_formation && m_facts != nullptr &&
                                m_facts->last_accepted_speed < k_heading_hold_speed;
    bool const jostled = m_formation && m_facts != nullptr &&
                         (m_facts->steering.contact_push_x != 0.0F ||
                          m_facts->steering.contact_push_z != 0.0F);
    float const deadband = m_movement.get_following_formation_slot() ? 0.5F
                           : (standing_still || jostled)
                               ? full_translation_heading_error_degrees
                           : m_formation ? k_formation_heading_deadband_degrees
                                         : 0.0F;
    if (std::fabs(Game::Systems::signed_yaw_delta(m_transform.rotation.y,
                                                  reference.yaw)) > deadband) {
      m_transform.rotation.y = Game::Systems::turn_yaw_toward(
          m_transform.rotation.y, reference.yaw, m_turn_speed * m_delta_time);
    }
  }

  void turn_toward_desired_yaw() {
    float const target_yaw = m_transform.desired_yaw;
    face_about_toward(target_yaw);
    float const diff =
        Game::Systems::signed_yaw_delta(m_transform.rotation.y, target_yaw);
    m_transform.rotation.y = Game::Systems::turn_yaw_toward(
        m_transform.rotation.y, target_yaw, m_turn_speed * m_delta_time);
    if (std::fabs(diff) < 0.5F && !m_shell_holds_its_face) {
      m_transform.has_desired_yaw = false;
    }
  }

  Engine::Core::Entity& m_entity;
  Engine::Core::TransformComponent& m_transform;
  Engine::Core::MovementComponent& m_movement;
  const Engine::Core::MovementFactsComponent* m_facts;
  const Engine::Core::UnitComponent* m_unit;
  Engine::Core::UnitTraversalLayoutStateComponent* m_traversal{nullptr};
  float m_delta_time;
  float m_turn_speed{0.0F};
  bool m_shell_holds_its_face{false};
  bool m_formation{false};
  bool m_faced_about{false};
};

} // namespace

auto formation_turn_speed_degrees(const Engine::Core::Entity& entity,
                                  const Engine::Core::UnitComponent& unit,
                                  float single_body_turn_speed) -> float {
  float const turn_radius = FormationCombat::formation_turn_radius(entity);
  if (!FormationCombat::has_formation_slots(entity) || turn_radius <= 0.5F) {
    return single_body_turn_speed;
  }

  float const max_outer_speed =
      std::max(k_formation_outer_file_speed_floor,
               unit.speed * k_formation_outer_file_speed_scale);
  float const derived =
      max_outer_speed / turn_radius * 180.0F / std::numbers::pi_v<float>;
  return std::min(derived, std::min(75.0F, single_body_turn_speed));
}

void apply_desired_yaw(Engine::Core::TransformComponent* transform,
                       float delta_time,
                       float turn_speed_degrees) {
  if ((transform == nullptr) || !transform->has_desired_yaw) {
    return;
  }

  float const target_yaw = transform->desired_yaw;
  transform->rotation.y = Game::Systems::turn_yaw_toward(
      transform->rotation.y, target_yaw, turn_speed_degrees * delta_time);

  float const remaining_diff =
      Game::Systems::signed_yaw_delta(transform->rotation.y, target_yaw);
  if (std::fabs(remaining_diff) < 0.5F) {
    transform->rotation.y = target_yaw;
    transform->has_desired_yaw = false;
  }
}

auto face_locked_opponent(Engine::Core::World* world,
                          const Engine::Core::Entity& entity,
                          Engine::Core::TransformComponent& transform,
                          const Engine::Core::AttackComponent& attack,
                          const Engine::Core::UnitComponent* unit,
                          float delta_time) -> bool {
  if (world == nullptr || attack.melee_lock_target_id == 0) {
    return false;
  }
  auto* structure = world->get_entity(attack.melee_lock_target_id);
  if (structure == nullptr) {
    return false;
  }
  bool const faces_a_structure =
      world->has<Engine::Core::BuildingComponent>(structure->get_id());
  bool const faces_an_animal =
      world->has<Engine::Core::WildlifeComponent>(structure->get_id()) &&
      !FormationCombat::has_formation_slots(entity);
  if (!faces_a_structure && !faces_an_animal) {
    return false;
  }
  auto const* structure_transform =
      structure->get_component<Engine::Core::TransformComponent>();
  if (structure_transform == nullptr) {
    return false;
  }

  float const dx = structure_transform->position.x - transform.position.x;
  float const dz = structure_transform->position.z - transform.position.z;
  if ((dx * dx) + (dz * dz) < 0.000001F) {
    return false;
  }

  transform.desired_yaw = std::atan2(dx, dz) * 180.0F / std::numbers::pi_v<float>;
  transform.has_desired_yaw = true;
  apply_desired_yaw(&transform,
                    delta_time,
                    unit != nullptr
                        ? Game::Units::body_turn_speed_degrees(unit->spawn_type)
                        : desired_yaw_turn_speed_degrees);
  return true;
}

void clamp_to_map_bounds(Engine::Core::TransformComponent& transform) {
  auto& terrain = Game::Map::TerrainService::instance();
  if (!terrain.is_initialized()) {
    return;
  }
  const Game::Map::TerrainHeightMap* hm = terrain.get_height_map();
  if (hm == nullptr) {
    return;
  }
  const float tile = hm->get_tile_size();
  const int w = hm->get_width();
  const int h = hm->get_height();
  if (w <= 0 || h <= 0) {
    return;
  }
  const float half_w = w * 0.5F - 0.5F;
  const float half_h = h * 0.5F - 0.5F;
  transform.position.x =
      std::clamp(transform.position.x, -half_w * tile, half_w * tile);
  transform.position.z =
      std::clamp(transform.position.z, -half_h * tile, half_h * tile);
}

auto finalize_orientation(Engine::Core::World& world,
                          Engine::Core::Entity* entity,
                          Engine::Core::TransformComponent* transform,
                          Engine::Core::MovementComponent* movement,
                          const Engine::Core::MovementFactsComponent* facts,
                          float delta_time) -> bool {
  clamp_to_map_bounds(*transform);
  Engine::Core::EntityID const id = entity->get_id();

  auto* terrain_ctx = world.try_get<Engine::Core::TerrainContextComponent>(id);
  if (terrain_ctx != nullptr && terrain_ctx->audio_cooldown > 0.0F) {
    terrain_ctx->audio_cooldown =
        std::max(0.0F, terrain_ctx->audio_cooldown - delta_time);
  }

  if (world.has<Engine::Core::BuildingComponent>(id)) {
    return false;
  }

  FacingPass pass(world,
                  *entity,
                  *transform,
                  *movement,
                  facts,
                  world.try_get<Engine::Core::UnitComponent>(id),
                  delta_time);
  pass.run();
  return pass.faced_about();
}

} // namespace Game::Systems::MovementHeading
