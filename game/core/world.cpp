#include "world.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <mutex>
#include <numbers>
#include <utility>
#include <vector>

#include "../systems/owner_registry.h"
#include "../systems/troop_count_registry.h"
#include "component.h"
#include "core/entity.h"
#include "core/system.h"

namespace Engine::Core {

namespace {

constexpr float k_motion_displacement_epsilon_sq = 1.0e-6F;
constexpr float k_motion_velocity_epsilon_sq = 1.0e-4F;
constexpr float k_goal_intent_distance_sq = 0.35F * 0.35F;
constexpr float k_recent_path_request_window = 0.35F;

[[nodiscard]] auto
forward_xz_from_yaw(float yaw_degrees) noexcept -> std::pair<float, float> {
  float const yaw_rad = yaw_degrees * std::numbers::pi_v<float> / 180.0F;
  return {std::sin(yaw_rad), std::cos(yaw_rad)};
}

void normalize_xz(float& x, float& z) noexcept {
  float const len_sq = x * x + z * z;
  if (len_sq <= 1.0e-8F) {
    x = 0.0F;
    z = 1.0F;
    return;
  }
  float const inv_len = 1.0F / std::sqrt(len_sq);
  x *= inv_len;
  z *= inv_len;
}

[[nodiscard]] auto
movement_goal_is_active(const MovementComponent* movement,
                        const TransformComponent* transform) noexcept -> bool {
  if (movement == nullptr || transform == nullptr) {
    return false;
  }

  float const dx = movement->goal_x - transform->position.x;
  float const dz = movement->goal_y - transform->position.z;
  if (dx * dx + dz * dz <= k_goal_intent_distance_sq) {
    return false;
  }

  bool const has_recent_request =
      movement->time_since_last_path_request < k_recent_path_request_window &&
      (movement->last_goal_x != 0.0F || movement->last_goal_y != 0.0F);
  return movement->repath_cooldown > 0.0F || has_recent_request ||
         movement->time_stuck > 0.0F || movement->unstuck_cooldown > 0.0F;
}

[[nodiscard]] auto
attack_target_is_in_range(World& world,
                          const AttackComponent* attack,
                          const AttackTargetComponent* attack_target,
                          const TransformComponent* transform) -> bool {
  if (attack == nullptr || attack_target == nullptr || attack_target->target_id == 0 ||
      transform == nullptr) {
    return false;
  }

  auto* target = world.get_entity(attack_target->target_id);
  if (target == nullptr) {
    return false;
  }
  auto* target_transform = target->get_component<TransformComponent>();
  if (target_transform == nullptr) {
    return false;
  }

  float const dx = target_transform->position.x - transform->position.x;
  float const dz = target_transform->position.z - transform->position.z;
  float const dist_sq = dx * dx + dz * dz;
  float target_radius =
      std::max(target_transform->scale.x, target_transform->scale.z) * 0.5F;
  if (auto* elephant = target->get_component<ElephantComponent>()) {
    target_radius = std::max(target_radius, elephant->trample_radius);
  }
  float const effective_range = attack->range + target_radius + 0.25F;
  return dist_sq <= effective_range * effective_range;
}

struct MotionPresentationSample {
  bool displaced{false};
  bool has_component_velocity{false};
  bool has_navigation_intent{false};
  bool direct_control_moving{false};
  bool builder_bypass{false};
  bool has_chase_intent{false};
  bool has_active_navigation_segment{false};
  bool is_running{false};
};

[[nodiscard]] auto resolve_motion_presentation_state(
    const MotionPresentationSample& sample) noexcept -> MotionPresentationState {
  bool const moving = sample.displaced || sample.direct_control_moving ||
                      sample.builder_bypass || sample.has_component_velocity ||
                      sample.has_navigation_intent || sample.has_chase_intent ||
                      sample.has_active_navigation_segment;
  if (!moving) {
    return MotionPresentationState::Idle;
  }
  return sample.is_running ? MotionPresentationState::Run
                           : MotionPresentationState::Walk;
}

void begin_motion_presentation_frame(World& world, float delta_time) {
  const std::lock_guard<std::recursive_mutex> lock(world.get_entity_mutex());
  for (const auto& [entity_id, entity] : world.get_entities()) {
    (void)entity_id;
    auto* transform = entity->get_component<TransformComponent>();
    auto* unit = entity->get_component<UnitComponent>();
    if (transform == nullptr || unit == nullptr) {
      continue;
    }
    auto* motion = get_or_add_component<MotionPresentationComponent>(entity.get());
    if (motion == nullptr) {
      continue;
    }
    motion->previous_x = transform->position.x;
    motion->previous_y = transform->position.y;
    motion->previous_z = transform->position.z;
    motion->tick_delta_time = std::max(0.0F, delta_time);
    motion->snapshot_valid = false;
    motion->initialized = true;
  }
}

void finalize_motion_presentation_frame(World& world, float delta_time) {
  const float safe_dt = std::max(delta_time, 1.0e-5F);
  const std::lock_guard<std::recursive_mutex> lock(world.get_entity_mutex());
  for (const auto& [entity_id, entity] : world.get_entities()) {
    (void)entity_id;
    auto* motion = entity->get_component<MotionPresentationComponent>();
    auto* transform = entity->get_component<TransformComponent>();
    auto* unit = entity->get_component<UnitComponent>();
    if (motion == nullptr || transform == nullptr || unit == nullptr) {
      continue;
    }

    auto* movement = entity->get_component<MovementComponent>();
    auto* attack = entity->get_component<AttackComponent>();
    auto* attack_target = entity->get_component<AttackTargetComponent>();
    auto* commander = entity->get_component<CommanderComponent>();
    auto* builder_prod = entity->get_component<BuilderProductionComponent>();
    auto* stamina = entity->get_component<StaminaComponent>();

    float const displacement_x = transform->position.x - motion->previous_x;
    float const displacement_z = transform->position.z - motion->previous_z;
    float const displacement_sq =
        displacement_x * displacement_x + displacement_z * displacement_z;
    bool const displaced = displacement_sq > k_motion_displacement_epsilon_sq;

    float movement_speed_sq = 0.0F;
    if (movement != nullptr) {
      movement_speed_sq = movement->vx * movement->vx + movement->vz * movement->vz;
    }
    bool const has_component_velocity =
        movement_speed_sq > k_motion_velocity_epsilon_sq;

    bool const has_active_navigation_segment =
        movement != nullptr &&
        (movement->has_target || movement->has_waypoints() || movement->path_pending ||
         movement->pending_request_id != 0);
    bool const has_navigation_intent = has_active_navigation_segment ||
                                       has_component_velocity ||
                                       movement_goal_is_active(movement, transform);

    bool const direct_control_moving =
        commander != nullptr && commander->fpv_controlled &&
        ((commander->fpv_motion_vx * commander->fpv_motion_vx +
          commander->fpv_motion_vz * commander->fpv_motion_vz) >
             k_motion_velocity_epsilon_sq ||
         has_component_velocity);
    bool const builder_bypass =
        builder_prod != nullptr && builder_prod->bypass_movement_active;

    motion->attack_target_in_range =
        attack_target_is_in_range(world, attack, attack_target, transform);
    motion->has_chase_intent =
        attack_target != nullptr && attack_target->target_id > 0 &&
        attack_target->should_chase && !motion->attack_target_in_range;

    MotionPresentationSample sample{};
    sample.displaced = displaced;
    sample.has_component_velocity = has_component_velocity;
    sample.has_navigation_intent = has_navigation_intent;
    sample.direct_control_moving = direct_control_moving;
    sample.builder_bypass = builder_bypass;
    sample.has_chase_intent = motion->has_chase_intent;
    sample.has_active_navigation_segment = has_active_navigation_segment;
    sample.is_running = stamina != nullptr && stamina->is_running;

    const MotionPresentationState next_state =
        resolve_motion_presentation_state(sample);
    motion->set_state(next_state);
    motion->state_time =
        motion->state_changed ? 0.0F : motion->state_time + std::max(0.0F, delta_time);
    motion->has_velocity = displaced || has_component_velocity;
    motion->has_navigation_intent = has_navigation_intent || builder_bypass;

    motion->displacement_x = displacement_x;
    motion->displacement_z = displacement_z;
    if (has_component_velocity && movement != nullptr) {
      motion->velocity_x = movement->vx;
      motion->velocity_z = movement->vz;
      motion->speed = std::sqrt(movement_speed_sq);
    } else if (displaced) {
      motion->velocity_x = displacement_x / safe_dt;
      motion->velocity_z = displacement_z / safe_dt;
      motion->speed = std::sqrt(displacement_sq) / safe_dt;
    } else {
      motion->velocity_x = 0.0F;
      motion->velocity_z = 0.0F;
      motion->speed = next_state != MotionPresentationState::Idle
                          ? std::max(0.1F, unit->speed)
                          : 0.0F;
      if (next_state == MotionPresentationState::Run && stamina != nullptr) {
        motion->speed *= StaminaComponent::k_run_speed_multiplier;
      }
    }

    motion->has_movement_target = false;
    if (builder_bypass) {
      motion->movement_target_x = builder_prod->bypass_target_x;
      motion->movement_target_z = builder_prod->bypass_target_z;
      motion->has_movement_target = true;
    } else if (movement != nullptr && movement->has_waypoints()) {
      auto const& waypoint = movement->current_waypoint();
      motion->movement_target_x = waypoint.first;
      motion->movement_target_z = waypoint.second;
      motion->has_movement_target = true;
    } else if (movement != nullptr && movement->has_target) {
      motion->movement_target_x = movement->target_x;
      motion->movement_target_z = movement->target_y;
      motion->has_movement_target = true;
    } else if (movement_goal_is_active(movement, transform)) {
      motion->movement_target_x = movement->goal_x;
      motion->movement_target_z = movement->goal_y;
      motion->has_movement_target = true;
    } else if (motion->has_chase_intent && attack_target != nullptr) {
      if (auto* target = world.get_entity(attack_target->target_id)) {
        if (auto* target_transform = target->get_component<TransformComponent>()) {
          motion->movement_target_x = target_transform->position.x;
          motion->movement_target_z = target_transform->position.z;
          motion->has_movement_target = true;
        }
      }
    }

    if (has_component_velocity && movement != nullptr) {
      motion->direction_x = movement->vx;
      motion->direction_z = movement->vz;
    } else if (displaced) {
      motion->direction_x = displacement_x;
      motion->direction_z = displacement_z;
    } else if (motion->has_movement_target) {
      motion->direction_x = motion->movement_target_x - transform->position.x;
      motion->direction_z = motion->movement_target_z - transform->position.z;
    } else {
      auto [forward_x, forward_z] = forward_xz_from_yaw(transform->rotation.y);
      motion->direction_x = forward_x;
      motion->direction_z = forward_z;
    }
    normalize_xz(motion->direction_x, motion->direction_z);

    if (direct_control_moving) {
      motion->source = MotionPresentationSource::DirectControl;
    } else if (builder_bypass) {
      motion->source = MotionPresentationSource::BuilderBypass;
    } else if (motion->has_chase_intent) {
      motion->source = MotionPresentationSource::Chase;
    } else if (has_navigation_intent) {
      motion->source = MotionPresentationSource::Navigation;
    } else if (displaced) {
      motion->source = MotionPresentationSource::ForcedDisplacement;
    } else {
      motion->source = MotionPresentationSource::None;
    }

    motion->seconds_since_motion =
        motion->has_locomotion()
            ? 0.0F
            : motion->seconds_since_motion + std::max(0.0F, delta_time);
    motion->snapshot_valid = true;
  }
}

} // namespace

World::World() = default;
World::~World() = default;

void World::on_component_changed(EntityID entity_id,
                                 std::type_index component_type,
                                 bool added) {

  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);

  if (added) {
    m_component_index[component_type].insert(entity_id);
  } else {
    auto it = m_component_index.find(component_type);
    if (it != m_component_index.end()) {
      it->second.erase(entity_id);
      if (it->second.empty()) {
        m_component_index.erase(it);
      }
    }
  }

  const auto observers = m_component_observers;
  for (const auto& observer : observers) {
    observer.callback(entity_id, component_type, added);
  }
}

void World::setup_entity_callback(Entity* entity) {
  entity->set_component_change_callback(
      [this](EntityID entity_id, std::type_index component_type, bool added) {
        this->on_component_changed(entity_id, component_type, added);
      });
}

auto World::create_entity() -> Entity* {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  EntityID const id = m_next_entity_id++;
  auto entity = std::make_unique<Entity>(id);
  auto* ptr = entity.get();
  setup_entity_callback(ptr);
  m_entities[id] = std::move(entity);
  return ptr;
}

auto World::create_entity_with_id(EntityID entity_id) -> Entity* {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  if (entity_id == NULL_ENTITY) {
    return nullptr;
  }

  if (m_entities.contains(entity_id)) {
    for (auto it = m_component_index.begin(); it != m_component_index.end();) {
      it->second.erase(entity_id);
      if (it->second.empty()) {
        it = m_component_index.erase(it);
      } else {
        ++it;
      }
    }
  }

  auto entity = std::make_unique<Entity>(entity_id);
  auto* ptr = entity.get();
  setup_entity_callback(ptr);
  m_entities[entity_id] = std::move(entity);

  if (entity_id >= m_next_entity_id) {
    m_next_entity_id = entity_id + 1;
  }

  return ptr;
}

void World::destroy_entity(EntityID entity_id) {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);

  auto it = m_entities.find(entity_id);

  for (auto& [type, entity_set] : m_component_index) {
    entity_set.erase(entity_id);
  }

  if (it != m_entities.end()) {
    m_entities.erase(it);
  } else {
    m_entities.erase(entity_id);
  }

  const auto observers = m_entity_destroyed_observers;
  for (const auto& observer : observers) {
    observer.callback(entity_id);
  }
}

void World::clear() {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  m_entities.clear();
  m_component_index.clear();
  m_next_entity_id = 1;

  const auto observers = m_world_cleared_observers;
  for (const auto& observer : observers) {
    observer.callback();
  }
}

auto World::get_entity(EntityID entity_id) -> Entity* {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  auto it = m_entities.find(entity_id);
  return it != m_entities.end() ? it->second.get() : nullptr;
}

void World::add_system(std::unique_ptr<System> system) {
  m_systems.push_back(std::move(system));
}

void World::update(float delta_time) {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  begin_motion_presentation_frame(*this, delta_time);
  for (auto& system : m_systems) {
    system->update(this, delta_time);
  }
  finalize_motion_presentation_frame(*this, delta_time);
}

auto World::get_units_owned_by(int owner_id) const -> std::vector<Entity*> {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  std::vector<Entity*> result;
  result.reserve(m_entities.size());
  for (const auto& [entity_id, entity] : m_entities) {
    auto* unit = entity->get_component<UnitComponent>();
    if (unit == nullptr) {
      continue;
    }
    if (unit->owner_id == owner_id) {
      result.push_back(entity.get());
    }
  }
  return result;
}

auto World::get_units_not_owned_by(int owner_id) const -> std::vector<Entity*> {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  std::vector<Entity*> result;
  result.reserve(m_entities.size());
  for (const auto& [entity_id, entity] : m_entities) {
    auto* unit = entity->get_component<UnitComponent>();
    if (unit == nullptr) {
      continue;
    }
    if (unit->owner_id != owner_id) {
      result.push_back(entity.get());
    }
  }
  return result;
}

auto World::get_allied_units(int owner_id) const -> std::vector<Entity*> {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  std::vector<Entity*> result;
  result.reserve(m_entities.size());
  auto& owner_registry = Game::Systems::OwnerRegistry::instance();

  for (const auto& [entity_id, entity] : m_entities) {
    auto* unit = entity->get_component<UnitComponent>();
    if (unit == nullptr) {
      continue;
    }

    if (unit->owner_id == owner_id ||
        owner_registry.are_allies(owner_id, unit->owner_id)) {
      result.push_back(entity.get());
    }
  }
  return result;
}

auto World::get_enemy_units(int owner_id) const -> std::vector<Entity*> {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  std::vector<Entity*> result;
  result.reserve(m_entities.size());
  auto& owner_registry = Game::Systems::OwnerRegistry::instance();

  for (const auto& [entity_id, entity] : m_entities) {
    auto* unit = entity->get_component<UnitComponent>();
    if (unit == nullptr) {
      continue;
    }

    if (owner_registry.are_enemies(owner_id, unit->owner_id)) {
      result.push_back(entity.get());
    }
  }
  return result;
}

auto World::count_troops_for_player(int owner_id) -> int {
  return Game::Systems::TroopCountRegistry::instance().get_troop_count(owner_id);
}

auto World::get_next_entity_id() const -> EntityID {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  return m_next_entity_id;
}

void World::set_next_entity_id(EntityID next_id) {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  m_next_entity_id = std::max(next_id, m_next_entity_id);
}

auto World::add_component_observer(ComponentObserverCallback callback)
    -> ObserverHandle {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  const ObserverHandle handle = m_next_observer_handle++;
  m_component_observers.push_back({handle, std::move(callback)});
  return handle;
}

auto World::add_entity_destroyed_observer(EntityDestroyedCallback callback)
    -> ObserverHandle {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  const ObserverHandle handle = m_next_observer_handle++;
  m_entity_destroyed_observers.push_back({handle, std::move(callback)});
  return handle;
}

auto World::add_world_cleared_observer(WorldClearedCallback callback)
    -> ObserverHandle {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  const ObserverHandle handle = m_next_observer_handle++;
  m_world_cleared_observers.push_back({handle, std::move(callback)});
  return handle;
}

void World::remove_component_observer(ObserverHandle handle) {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  std::erase_if(m_component_observers,
                [handle](const auto& entry) { return entry.handle == handle; });
}

void World::remove_entity_destroyed_observer(ObserverHandle handle) {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  std::erase_if(m_entity_destroyed_observers,
                [handle](const auto& entry) { return entry.handle == handle; });
}

void World::remove_world_cleared_observer(ObserverHandle handle) {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  std::erase_if(m_world_cleared_observers,
                [handle](const auto& entry) { return entry.handle == handle; });
}

} // namespace Engine::Core
