#include "combat_system.h"
#include "../core/component.h"
#include "../core/event_manager.h"
#include "../core/world.h"
#include "../units/troop_config.h"
#include "../visuals/team_colors.h"
#include "arrow_system.h"
#include "building_collision_registry.h"
#include "camera_visibility_service.h"
#include "command_service.h"
#include "owner_registry.h"
#include "units/spawn_type.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <qvectornd.h>
#include <random>
#include <vector>

namespace Game::Systems {

namespace {
thread_local std::mt19937 gen(std::random_device{}());

auto isUnitInHoldMode(Engine::Core::Entity *entity) -> bool {
  if (entity == nullptr) {
    return false;
  }
  auto *hold_mode = entity->get_component<Engine::Core::HoldModeComponent>();
  return (hold_mode != nullptr) && hold_mode->active;
}
} // namespace

void CombatSystem::update(Engine::Core::World *world, float delta_time) {
  process_hit_feedback(world, delta_time);
  process_combat_state(world, delta_time);
  process_attacks(world, delta_time);
  process_auto_engagement(world, delta_time);
}

void CombatSystem::process_attacks(Engine::Core::World *world,
                                   float delta_time) {
  auto units = world->get_entities_with<Engine::Core::UnitComponent>();

  auto *arrow_sys = world->get_system<ArrowSystem>();

  for (auto *attacker : units) {

    if (attacker->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto *attacker_unit =
        attacker->get_component<Engine::Core::UnitComponent>();
    auto *attacker_transform =
        attacker->get_component<Engine::Core::TransformComponent>();
    auto *attacker_atk =
        attacker->get_component<Engine::Core::AttackComponent>();

    if ((attacker_unit == nullptr) || (attacker_transform == nullptr)) {
      continue;
    }

    if (attacker_unit->health <= 0) {
      continue;
    }

    if ((attacker_atk != nullptr) && attacker_atk->in_melee_lock) {
      auto *lock_target = world->get_entity(attacker_atk->melee_lock_target_id);
      if ((lock_target == nullptr) ||
          lock_target->has_component<Engine::Core::PendingRemovalComponent>()) {

        attacker_atk->in_melee_lock = false;
        attacker_atk->melee_lock_target_id = 0;
      } else {
        auto *lock_target_unit =
            lock_target->get_component<Engine::Core::UnitComponent>();
        if ((lock_target_unit == nullptr) || lock_target_unit->health <= 0) {

          attacker_atk->in_melee_lock = false;
          attacker_atk->melee_lock_target_id = 0;
        } else {

          auto *att_t = attacker_transform;
          auto *tgt_t =
              lock_target->get_component<Engine::Core::TransformComponent>();
          if ((att_t != nullptr) && (tgt_t != nullptr)) {
            float const dx = tgt_t->position.x - att_t->position.x;
            float const dz = tgt_t->position.z - att_t->position.z;
            float const dist = std::sqrt(dx * dx + dz * dz);

            const float ideal_melee_distance = 0.6F;
            const float max_melee_separation = 0.9F;

            if (dist > max_melee_separation) {
              // Check if attacker is in hold mode - don't pull if so
              if (!isUnitInHoldMode(attacker)) {
                float const pull_amount =
                    (dist - ideal_melee_distance) * 0.3F * delta_time * 5.0F;

                if (dist > 0.001F) {
                  QVector3D const direction(dx / dist, 0.0F, dz / dist);

                  att_t->position.x += direction.x() * pull_amount;
                  att_t->position.z += direction.z() * pull_amount;
                }
              }
            }
          }
        }
      }
    }

    if ((attacker_atk != nullptr) && attacker_atk->in_melee_lock &&
        attacker_atk->melee_lock_target_id != 0) {
      auto *lock_target = world->get_entity(attacker_atk->melee_lock_target_id);
      if ((lock_target != nullptr) &&
          !lock_target
               ->has_component<Engine::Core::PendingRemovalComponent>()) {

        auto *attack_target =
            attacker->get_component<Engine::Core::AttackTargetComponent>();
        if (attack_target == nullptr) {
          attack_target =
              attacker->add_component<Engine::Core::AttackTargetComponent>();
        }
        if (attack_target != nullptr) {
          attack_target->target_id = attacker_atk->melee_lock_target_id;
          attack_target->should_chase = false;
        }
      }
    }

    float range = 2.0F;
    int damage = 10;
    float cooldown = 1.0F;
    float *t_accum = nullptr;
    float tmp_accum = 0.0F;

    if (attacker_atk != nullptr) {

      updateCombatMode(attacker, world, attacker_atk);

      range = attacker_atk->get_current_range();
      damage = attacker_atk->get_current_damage();
      cooldown = attacker_atk->get_current_cooldown();

      auto *hold_mode =
          attacker->get_component<Engine::Core::HoldModeComponent>();
      if ((hold_mode != nullptr) && hold_mode->active) {
        if (attacker_unit->spawn_type == Game::Units::SpawnType::Archer) {

          range *= 1.5F;
          damage = static_cast<int>(damage * 1.5F);
        } else if (attacker_unit->spawn_type ==
                   Game::Units::SpawnType::Spearman) {

          damage = static_cast<int>(damage * 2.0F);
        } else {
          // All other units in hold mode get a significant attack bonus
          damage = static_cast<int>(damage * 1.75F);
        }
      }

      attacker_atk->time_since_last += delta_time;
      t_accum = &attacker_atk->time_since_last;
    } else {
      tmp_accum += delta_time;
      t_accum = &tmp_accum;
    }

    if (*t_accum < cooldown) {
      continue;
    }

    auto *attack_target =
        attacker->get_component<Engine::Core::AttackTargetComponent>();
    Engine::Core::Entity *best_target = nullptr;

    if ((attack_target != nullptr) && attack_target->target_id != 0) {

      auto *target = world->get_entity(attack_target->target_id);
      if ((target != nullptr) &&
          !target->has_component<Engine::Core::PendingRemovalComponent>()) {
        auto *target_unit =
            target->get_component<Engine::Core::UnitComponent>();

        auto &owner_registry = Game::Systems::OwnerRegistry::instance();
        bool const is_ally = owner_registry.are_allies(attacker_unit->owner_id,
                                                       target_unit->owner_id);

        if ((target_unit != nullptr) && target_unit->health > 0 &&
            target_unit->owner_id != attacker_unit->owner_id && !is_ally) {

          if (is_in_range(attacker, target, range)) {
            best_target = target;

            bool is_ranged_unit = false;
            if ((attacker_atk != nullptr) && attacker_atk->can_ranged &&
                attacker_atk->current_mode ==
                    Engine::Core::AttackComponent::CombatMode::Ranged) {
              is_ranged_unit = true;
            }

            if (is_ranged_unit) {
              auto *movement =
                  attacker->get_component<Engine::Core::MovementComponent>();
              if ((movement != nullptr) && movement->has_target) {
                movement->has_target = false;
                movement->vx = 0.0F;
                movement->vz = 0.0F;
                movement->path.clear();
                if (attacker_transform != nullptr) {
                  movement->target_x = attacker_transform->position.x;
                  movement->target_y = attacker_transform->position.z;
                  movement->goal_x = attacker_transform->position.x;
                  movement->goal_y = attacker_transform->position.z;
                }
              }
            }

            if (auto *att_t =
                    attacker
                        ->get_component<Engine::Core::TransformComponent>()) {
              if (auto *tgt_t =
                      target
                          ->get_component<Engine::Core::TransformComponent>()) {
                float const dx = tgt_t->position.x - att_t->position.x;
                float const dz = tgt_t->position.z - att_t->position.z;
                float const yaw =
                    std::atan2(dx, dz) * 180.0F / std::numbers::pi_v<float>;
                att_t->desired_yaw = yaw;
                att_t->has_desired_yaw = true;
              }
            }
          } else if (attack_target->should_chase) {

            auto *hold_mode =
                attacker->get_component<Engine::Core::HoldModeComponent>();
            if ((hold_mode != nullptr) && hold_mode->active) {
              if (!is_in_range(attacker, target, range)) {
                attacker
                    ->remove_component<Engine::Core::AttackTargetComponent>();
              }
              continue;
            }

            bool is_ranged_unit = false;
            if ((attacker_atk != nullptr) && attacker_atk->can_ranged &&
                attacker_atk->current_mode ==
                    Engine::Core::AttackComponent::CombatMode::Ranged) {
              is_ranged_unit = true;
            }

            bool const currently_in_range =
                is_in_range(attacker, target, range);

            if (is_ranged_unit && currently_in_range) {
              auto *movement =
                  attacker->get_component<Engine::Core::MovementComponent>();
              if (movement != nullptr) {
                movement->has_target = false;
                movement->vx = 0.0F;
                movement->vz = 0.0F;
                movement->path.clear();
                auto *attacker_transform_component =
                    attacker->get_component<Engine::Core::TransformComponent>();
                if (attacker_transform_component != nullptr) {
                  movement->target_x = attacker_transform_component->position.x;
                  movement->target_y = attacker_transform_component->position.z;
                  movement->goal_x = attacker_transform_component->position.x;
                  movement->goal_y = attacker_transform_component->position.z;
                }
              }
              best_target = target;
            } else {

              auto *target_transform =
                  target->get_component<Engine::Core::TransformComponent>();
              auto *attacker_transform_component =
                  attacker->get_component<Engine::Core::TransformComponent>();
              if ((target_transform != nullptr) &&
                  (attacker_transform_component != nullptr)) {
                QVector3D const attacker_pos(
                    attacker_transform_component->position.x, 0.0F,
                    attacker_transform_component->position.z);
                QVector3D const target_pos(target_transform->position.x, 0.0F,
                                           target_transform->position.z);
                QVector3D desired_pos = target_pos;
                bool hold_position = false;

                bool const target_is_building =
                    target->has_component<Engine::Core::BuildingComponent>();
                if (target_is_building) {
                  float const scale_x = target_transform->scale.x;
                  float const scale_z = target_transform->scale.z;
                  float const target_radius = std::max(scale_x, scale_z) * 0.5F;
                  QVector3D direction = target_pos - attacker_pos;
                  float const distance_sq = direction.lengthSquared();
                  if (distance_sq > 0.000001F) {
                    float const distance = std::sqrt(distance_sq);
                    direction /= distance;
                    float const desired_distance =
                        target_radius + std::max(range - 0.2F, 0.2F);
                    if (distance > desired_distance + 0.15F) {
                      desired_pos = target_pos - direction * desired_distance;
                    } else {
                      hold_position = true;
                    }
                  }
                } else if (is_ranged_unit) {
                  QVector3D direction = target_pos - attacker_pos;
                  float const distance_sq = direction.lengthSquared();
                  if (distance_sq > 0.000001F) {
                    float const distance = std::sqrt(distance_sq);
                    direction /= distance;
                    float const optimal_range = range * 0.85F;
                    if (distance > optimal_range + 0.5F) {
                      desired_pos = target_pos - direction * optimal_range;
                    } else {
                      hold_position = true;
                    }
                  }
                }

                auto *movement =
                    attacker->get_component<Engine::Core::MovementComponent>();
                if (movement == nullptr) {
                  movement =
                      attacker
                          ->add_component<Engine::Core::MovementComponent>();
                }

                if (movement != nullptr) {
                  if (hold_position) {
                    movement->has_target = false;
                    movement->vx = 0.0F;
                    movement->vz = 0.0F;
                    movement->path.clear();
                    if (attacker_transform_component != nullptr) {
                      movement->target_x =
                          attacker_transform_component->position.x;
                      movement->target_y =
                          attacker_transform_component->position.z;
                      movement->goal_x =
                          attacker_transform_component->position.x;
                      movement->goal_y =
                          attacker_transform_component->position.z;
                    }
                  } else {
                    QVector3D planned_target(movement->target_x, 0.0F,
                                             movement->target_y);
                    if (!movement->path.empty()) {
                      const auto &final_node = movement->path.back();
                      planned_target =
                          QVector3D(final_node.first, 0.0F, final_node.second);
                    }

                    float const diff_sq =
                        (planned_target - desired_pos).lengthSquared();
                    bool need_new_command = !movement->path_pending;
                    if (movement->has_target && diff_sq <= 0.25F * 0.25F) {
                      need_new_command = false;
                    }

                    if (need_new_command) {
                      CommandService::MoveOptions options;
                      options.clear_attack_intent = false;

                      options.allow_direct_fallback = true;
                      std::vector<Engine::Core::EntityID> const unit_ids = {
                          attacker->get_id()};
                      std::vector<QVector3D> const move_targets = {desired_pos};
                      CommandService::moveUnits(*world, unit_ids, move_targets,
                                                options);
                    }
                  }
                }
              }

              if (is_in_range(attacker, target, range)) {
                best_target = target;
              }
            }
          } else {

            attacker->remove_component<Engine::Core::AttackTargetComponent>();
          }
        } else {

          attacker->remove_component<Engine::Core::AttackTargetComponent>();
        }
      } else {

        attacker->remove_component<Engine::Core::AttackTargetComponent>();
      }
    }

    if ((best_target == nullptr) && (attack_target == nullptr)) {

      auto &owner_registry = Game::Systems::OwnerRegistry::instance();

      for (auto *target : units) {
        if (target == attacker) {
          continue;
        }

        auto *target_unit =
            target->get_component<Engine::Core::UnitComponent>();
        if ((target_unit == nullptr) || target_unit->health <= 0) {
          continue;
        }

        if (target_unit->owner_id == attacker_unit->owner_id) {
          continue;
        }

        if (owner_registry.are_allies(attacker_unit->owner_id,
                                      target_unit->owner_id)) {
          continue;
        }

        if (target->has_component<Engine::Core::BuildingComponent>()) {
          continue;
        }

        if (is_in_range(attacker, target, range)) {
          best_target = target;
          break;
        }
      }
    }

    if (best_target != nullptr) {
      auto *best_target_unit =
          best_target->get_component<Engine::Core::UnitComponent>();

      if (!attacker->has_component<Engine::Core::AttackTargetComponent>()) {
        auto *new_target =
            attacker->add_component<Engine::Core::AttackTargetComponent>();
        new_target->target_id = best_target->get_id();
        new_target->should_chase = false;
      } else {
        auto *existing_target =
            attacker->get_component<Engine::Core::AttackTargetComponent>();
        if (existing_target->target_id != best_target->get_id()) {
          existing_target->target_id = best_target->get_id();
          existing_target->should_chase = false;
        }
      }

      bool is_ranged_unit = false;
      if ((attacker_atk != nullptr) && attacker_atk->can_ranged &&
          attacker_atk->current_mode ==
              Engine::Core::AttackComponent::CombatMode::Ranged) {
        is_ranged_unit = true;
      }

      if (is_ranged_unit) {
        auto *movement =
            attacker->get_component<Engine::Core::MovementComponent>();
        if ((movement != nullptr) && movement->has_target) {
          movement->has_target = false;
          movement->vx = 0.0F;
          movement->vz = 0.0F;
          movement->path.clear();
          if (attacker_transform != nullptr) {
            movement->target_x = attacker_transform->position.x;
            movement->target_y = attacker_transform->position.z;
            movement->goal_x = attacker_transform->position.x;
            movement->goal_y = attacker_transform->position.z;
          }
        }
      }

      if (auto *att_t =
              attacker->get_component<Engine::Core::TransformComponent>()) {
        if (auto *tgt_t =
                best_target
                    ->get_component<Engine::Core::TransformComponent>()) {
          float const dx = tgt_t->position.x - att_t->position.x;
          float const dz = tgt_t->position.z - att_t->position.z;
          float const yaw =
              std::atan2(dx, dz) * 180.0F / std::numbers::pi_v<float>;
          att_t->desired_yaw = yaw;
          att_t->has_desired_yaw = true;
        }
      }

      if (arrow_sys != nullptr) {
        auto *att_t =
            attacker->get_component<Engine::Core::TransformComponent>();
        auto *tgt_t =
            best_target->get_component<Engine::Core::TransformComponent>();
        auto *att_u = attacker->get_component<Engine::Core::UnitComponent>();

        bool const should_show_arrow_vfx =
            (att_u != nullptr &&
             att_u->spawn_type != Game::Units::SpawnType::Catapult &&
             att_u->spawn_type != Game::Units::SpawnType::Ballista);

        if (should_show_arrow_vfx &&
            ((attacker_atk == nullptr) ||
             attacker_atk->current_mode !=
                 Engine::Core::AttackComponent::CombatMode::Melee)) {
          QVector3D const a_pos(att_t->position.x, att_t->position.y,
                                att_t->position.z);
          QVector3D const t_pos(tgt_t->position.x, tgt_t->position.y,
                                tgt_t->position.z);
          QVector3D const dir = (t_pos - a_pos).normalized();
          QVector3D const color =
              (att_u != nullptr)
                  ? Game::Visuals::team_colorForOwner(att_u->owner_id)
                  : QVector3D(0.8F, 0.9F, 1.0F);

          int arrow_count = 1;
          if (att_u != nullptr) {
            int const troop_size =
                Game::Units::TroopConfig::instance().getIndividualsPerUnit(
                    att_u->spawn_type);
            int const max_arrows = std::max(1, troop_size / 3);

            static thread_local std::mt19937 gen(std::random_device{}());
            std::uniform_int_distribution<> dist(1, max_arrows);
            arrow_count = dist(gen);
          }

          for (int i = 0; i < arrow_count; ++i) {
            static thread_local std::mt19937 spread_gen(std::random_device{}());
            std::uniform_real_distribution<float> spread_dist(-0.15F, 0.15F);

            QVector3D const perpendicular(-dir.z(), 0.0F, dir.x());
            QVector3D const up_vector(0.0F, 1.0F, 0.0F);

            float const lateral_offset = spread_dist(spread_gen);
            float const vertical_offset = spread_dist(spread_gen) * 1.5F;
            float const depth_offset = spread_dist(spread_gen) * 1.3F;

            QVector3D const start_offset =
                perpendicular * lateral_offset + up_vector * vertical_offset;
            QVector3D const end_offset = perpendicular * lateral_offset +
                                         up_vector * vertical_offset +
                                         dir * depth_offset;

            QVector3D const start = a_pos + QVector3D(0.0F, 0.6F, 0.0F) +
                                    dir * 0.35F + start_offset;
            QVector3D const end =
                t_pos + QVector3D(0.5F, 0.5F, 0.0F) + end_offset;

            arrow_sys->spawnArrow(start, end, color, 14.0F);
          }
        }
      }

      if ((attacker_atk != nullptr) &&
          attacker_atk->current_mode ==
              Engine::Core::AttackComponent::CombatMode::Melee) {

        attacker_atk->in_melee_lock = true;
        attacker_atk->melee_lock_target_id = best_target->get_id();

        auto *combat_state =
            attacker->get_component<Engine::Core::CombatStateComponent>();
        if (combat_state == nullptr) {
          combat_state =
              attacker->add_component<Engine::Core::CombatStateComponent>();
        }
        if (combat_state != nullptr &&
            combat_state->animation_state ==
                Engine::Core::CombatAnimationState::Idle) {
          combat_state->animation_state =
              Engine::Core::CombatAnimationState::Advance;
          combat_state->state_time = 0.0F;
          combat_state->state_duration =
              Engine::Core::CombatStateComponent::kAdvanceDuration;
          std::uniform_real_distribution<float> offset_dist(0.0F, 0.15F);
          combat_state->attack_offset = offset_dist(gen);
          std::uniform_int_distribution<int> variant_dist(
              0, Engine::Core::CombatStateComponent::kMaxAttackVariants - 1);
          combat_state->attack_variant =
              static_cast<std::uint8_t>(variant_dist(gen));
        }

        auto *target_atk =
            best_target->get_component<Engine::Core::AttackComponent>();
        if (target_atk != nullptr) {
          target_atk->in_melee_lock = true;
          target_atk->melee_lock_target_id = attacker->get_id();
        }

        auto *att_t =
            attacker->get_component<Engine::Core::TransformComponent>();
        auto *tgt_t =
            best_target->get_component<Engine::Core::TransformComponent>();
        if ((att_t != nullptr) && (tgt_t != nullptr)) {
          float const dx = tgt_t->position.x - att_t->position.x;
          float const dz = tgt_t->position.z - att_t->position.z;
          float const dist = std::sqrt(dx * dx + dz * dz);

          const float ideal_melee_distance = 0.6F;

          if (dist > ideal_melee_distance + 0.1F) {

            float const move_amount = (dist - ideal_melee_distance) * 0.5F;

            if (dist > 0.001F) {
              QVector3D const direction(dx / dist, 0.0F, dz / dist);

              // Check hold mode - don't move units that are holding
              if (!isUnitInHoldMode(attacker)) {
                att_t->position.x += direction.x() * move_amount;
                att_t->position.z += direction.z() * move_amount;
              }

              if (!isUnitInHoldMode(best_target)) {
                tgt_t->position.x -= direction.x() * move_amount;
                tgt_t->position.z -= direction.z() * move_amount;
              }
            }
          }
        }
      }

      dealDamage(world, best_target, damage, attacker->get_id());
      *t_accum = 0.0F;
    } else {

      if ((attack_target == nullptr) &&
          attacker->has_component<Engine::Core::AttackTargetComponent>()) {
        attacker->remove_component<Engine::Core::AttackTargetComponent>();
      }
    }
  }
}

auto CombatSystem::is_in_range(Engine::Core::Entity *attacker,
                               Engine::Core::Entity *target,
                               float range) -> bool {
  auto *attacker_transform =
      attacker->get_component<Engine::Core::TransformComponent>();
  auto *target_transform =
      target->get_component<Engine::Core::TransformComponent>();

  if ((attacker_transform == nullptr) || (target_transform == nullptr)) {
    return false;
  }

  float const dx =
      target_transform->position.x - attacker_transform->position.x;
  float const dz =
      target_transform->position.z - attacker_transform->position.z;
  float const dy =
      target_transform->position.y - attacker_transform->position.y;
  float const distance_squared = dx * dx + dz * dz;

  float target_radius = 0.0F;
  if (target->has_component<Engine::Core::BuildingComponent>()) {

    float const scale_x = target_transform->scale.x;
    float const scale_z = target_transform->scale.z;
    target_radius = std::max(scale_x, scale_z) * 0.5F;
  } else {

    float const scale_x = target_transform->scale.x;
    float const scale_z = target_transform->scale.z;
    target_radius = std::max(scale_x, scale_z) * 0.5F;
  }

  float const effective_range = range + target_radius;

  if (distance_squared > effective_range * effective_range) {
    return false;
  }

  auto *attacker_atk = attacker->get_component<Engine::Core::AttackComponent>();
  if ((attacker_atk != nullptr) &&
      attacker_atk->current_mode ==
          Engine::Core::AttackComponent::CombatMode::Melee) {
    float const height_diff = std::abs(dy);
    if (height_diff > attacker_atk->max_height_difference) {
      return false;
    }
  }

  return true;
}

void CombatSystem::dealDamage(Engine::Core::World *world,
                              Engine::Core::Entity *target, int damage,
                              Engine::Core::EntityID attackerId) {
  auto *unit = target->get_component<Engine::Core::UnitComponent>();
  if (unit != nullptr) {
    bool const is_killing_blow = (unit->health > 0 && unit->health <= damage);
    unit->health = std::max(0, unit->health - damage);

    int attacker_owner_id = 0;
    std::optional<Game::Units::SpawnType> attacker_type_opt;
    if (attackerId != 0 && (world != nullptr)) {
      auto *attacker = world->get_entity(attackerId);
      if (attacker != nullptr) {
        auto *attacker_unit =
            attacker->get_component<Engine::Core::UnitComponent>();
        if (attacker_unit != nullptr) {
          attacker_owner_id = attacker_unit->owner_id;
          attacker_type_opt = attacker_unit->spawn_type;
        }
      }
    }

    Game::Units::SpawnType const attacker_type =
        attacker_type_opt.value_or(Game::Units::SpawnType::Knight);

    Engine::Core::EventManager::instance().publish(Engine::Core::CombatHitEvent(
        attackerId, target->get_id(), damage, attacker_type, is_killing_blow));

    if (unit->health > 0) {
      apply_hit_feedback(target, attackerId, world);
    }

    if (target->has_component<Engine::Core::BuildingComponent>() &&
        unit->health > 0) {
      Engine::Core::EventManager::instance().publish(
          Engine::Core::BuildingAttackedEvent(target->get_id(), unit->owner_id,
                                              unit->spawn_type, attackerId,
                                              attacker_owner_id, damage));
    }

    if (unit->health <= 0) {

      int const killer_owner_id = attacker_owner_id;

      Engine::Core::EventManager::instance().publish(
          Engine::Core::UnitDiedEvent(target->get_id(), unit->owner_id,
                                      unit->spawn_type, attackerId,
                                      killer_owner_id));

      auto *target_atk = target->get_component<Engine::Core::AttackComponent>();
      if ((target_atk != nullptr) && target_atk->in_melee_lock &&
          target_atk->melee_lock_target_id != 0) {

        if (world != nullptr) {
          auto *lock_partner =
              world->get_entity(target_atk->melee_lock_target_id);
          if ((lock_partner != nullptr) &&
              !lock_partner
                   ->has_component<Engine::Core::PendingRemovalComponent>()) {
            auto *partner_atk =
                lock_partner->get_component<Engine::Core::AttackComponent>();
            if ((partner_atk != nullptr) &&
                partner_atk->melee_lock_target_id == target->get_id()) {
              partner_atk->in_melee_lock = false;
              partner_atk->melee_lock_target_id = 0;
            }
          }
        }
      }

      if (target->has_component<Engine::Core::BuildingComponent>()) {
        BuildingCollisionRegistry::instance().unregister_building(
            target->get_id());
      }

      if (auto *r =
              target->get_component<Engine::Core::RenderableComponent>()) {
        r->visible = false;
      }

      if (auto *movement =
              target->get_component<Engine::Core::MovementComponent>()) {
        movement->has_target = false;
        movement->vx = 0.0F;
        movement->vz = 0.0F;
        movement->path.clear();
        movement->path_pending = false;
      }

      target->add_component<Engine::Core::PendingRemovalComponent>();
    }
  }
}

void CombatSystem::updateCombatMode(
    Engine::Core::Entity *attacker, Engine::Core::World *world,
    Engine::Core::AttackComponent *attack_comp) {
  if (attack_comp == nullptr) {
    return;
  }

  if (attack_comp->preferred_mode !=
      Engine::Core::AttackComponent::CombatMode::Auto) {
    attack_comp->current_mode = attack_comp->preferred_mode;
    return;
  }

  auto *attacker_transform =
      attacker->get_component<Engine::Core::TransformComponent>();
  if (attacker_transform == nullptr) {
    return;
  }

  auto *attacker_unit = attacker->get_component<Engine::Core::UnitComponent>();
  if (attacker_unit == nullptr) {
    return;
  }

  auto &owner_registry = Game::Systems::OwnerRegistry::instance();
  auto units = world->get_entities_with<Engine::Core::UnitComponent>();

  float closest_enemy_dist_sq = std::numeric_limits<float>::max();
  float closest_height_diff = 0.0F;

  for (auto *target : units) {
    if (target == attacker) {
      continue;
    }

    auto *target_unit = target->get_component<Engine::Core::UnitComponent>();
    if ((target_unit == nullptr) || target_unit->health <= 0) {
      continue;
    }

    if (owner_registry.are_allies(attacker_unit->owner_id,
                                  target_unit->owner_id)) {
      continue;
    }

    auto *target_transform =
        target->get_component<Engine::Core::TransformComponent>();
    if (target_transform == nullptr) {
      continue;
    }

    float const dx =
        target_transform->position.x - attacker_transform->position.x;
    float const dz =
        target_transform->position.z - attacker_transform->position.z;
    float const dy =
        target_transform->position.y - attacker_transform->position.y;
    float const dist_sq = dx * dx + dz * dz;

    if (dist_sq < closest_enemy_dist_sq) {
      closest_enemy_dist_sq = dist_sq;
      closest_height_diff = std::abs(dy);
    }
  }

  if (closest_enemy_dist_sq == std::numeric_limits<float>::max()) {
    if (attack_comp->can_ranged) {
      attack_comp->current_mode =
          Engine::Core::AttackComponent::CombatMode::Ranged;
    } else {
      attack_comp->current_mode =
          Engine::Core::AttackComponent::CombatMode::Melee;
    }
    return;
  }

  float const closest_dist = std::sqrt(closest_enemy_dist_sq);

  bool const in_melee_range =
      attack_comp->is_in_melee_range(closest_dist, closest_height_diff);
  bool const in_ranged_range = attack_comp->is_in_ranged_range(closest_dist);

  if (in_melee_range && attack_comp->can_melee) {
    attack_comp->current_mode =
        Engine::Core::AttackComponent::CombatMode::Melee;
  } else if (in_ranged_range && attack_comp->can_ranged) {
    attack_comp->current_mode =
        Engine::Core::AttackComponent::CombatMode::Ranged;
  } else if (attack_comp->can_ranged) {
    attack_comp->current_mode =
        Engine::Core::AttackComponent::CombatMode::Ranged;
  } else {
    attack_comp->current_mode =
        Engine::Core::AttackComponent::CombatMode::Melee;
  }
}

void CombatSystem::process_auto_engagement(Engine::Core::World *world,
                                           float delta_time) {
  auto units = world->get_entities_with<Engine::Core::UnitComponent>();

  for (auto it = m_engagementCooldowns.begin();
       it != m_engagementCooldowns.end();) {
    it->second -= delta_time;
    if (it->second <= 0.0F) {
      it = m_engagementCooldowns.erase(it);
    } else {
      ++it;
    }
  }

  for (auto *unit : units) {

    if (unit->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto *unit_comp = unit->get_component<Engine::Core::UnitComponent>();
    if ((unit_comp == nullptr) || unit_comp->health <= 0) {
      continue;
    }

    if (unit->has_component<Engine::Core::BuildingComponent>()) {
      continue;
    }

    auto *attack_comp = unit->get_component<Engine::Core::AttackComponent>();
    if ((attack_comp == nullptr) || !attack_comp->can_melee) {
      continue;
    }

    if (attack_comp->can_ranged &&
        attack_comp->preferred_mode !=
            Engine::Core::AttackComponent::CombatMode::Melee) {
      continue;
    }

    if (m_engagementCooldowns.find(unit->get_id()) !=
        m_engagementCooldowns.end()) {
      continue;
    }

    if (!is_unit_idle(unit)) {
      continue;
    }

    float const vision_range = unit_comp->vision_range;
    auto *nearest_enemy = findNearestEnemy(unit, world, vision_range);

    if (nearest_enemy != nullptr) {

      auto *attack_target =
          unit->get_component<Engine::Core::AttackTargetComponent>();
      if (attack_target == nullptr) {
        attack_target =
            unit->add_component<Engine::Core::AttackTargetComponent>();
      }
      if (attack_target != nullptr) {
        attack_target->target_id = nearest_enemy->get_id();
        attack_target->should_chase = true;

        m_engagementCooldowns[unit->get_id()] = ENGAGEMENT_COOLDOWN;
      }
    }
  }
}

auto CombatSystem::is_unit_idle(Engine::Core::Entity *unit) -> bool {

  auto *hold_mode = unit->get_component<Engine::Core::HoldModeComponent>();
  if ((hold_mode != nullptr) && hold_mode->active) {
    return false;
  }

  auto *attack_target =
      unit->get_component<Engine::Core::AttackTargetComponent>();
  if ((attack_target != nullptr) && attack_target->target_id != 0) {
    return false;
  }

  auto *movement = unit->get_component<Engine::Core::MovementComponent>();
  if ((movement != nullptr) && movement->has_target) {
    return false;
  }

  auto *attack_comp = unit->get_component<Engine::Core::AttackComponent>();
  if ((attack_comp != nullptr) && attack_comp->in_melee_lock) {
    return false;
  }

  auto *patrol = unit->get_component<Engine::Core::PatrolComponent>();
  return (patrol == nullptr) || !patrol->patrolling;
}

auto CombatSystem::findNearestEnemy(Engine::Core::Entity *unit,
                                    Engine::Core::World *world,
                                    float max_range) -> Engine::Core::Entity * {
  auto *unit_comp = unit->get_component<Engine::Core::UnitComponent>();
  auto *unit_transform =
      unit->get_component<Engine::Core::TransformComponent>();
  if ((unit_comp == nullptr) || (unit_transform == nullptr)) {
    return nullptr;
  }

  auto &owner_registry = Game::Systems::OwnerRegistry::instance();
  auto units = world->get_entities_with<Engine::Core::UnitComponent>();

  Engine::Core::Entity *nearest_enemy = nullptr;
  float nearest_dist_sq = max_range * max_range;

  for (auto *target : units) {
    if (target == unit) {
      continue;
    }

    if (target->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto *target_unit = target->get_component<Engine::Core::UnitComponent>();
    if ((target_unit == nullptr) || target_unit->health <= 0) {
      continue;
    }

    if (target_unit->owner_id == unit_comp->owner_id) {
      continue;
    }
    if (owner_registry.are_allies(unit_comp->owner_id, target_unit->owner_id)) {
      continue;
    }

    if (target->has_component<Engine::Core::BuildingComponent>()) {
      continue;
    }

    auto *target_transform =
        target->get_component<Engine::Core::TransformComponent>();
    if (target_transform == nullptr) {
      continue;
    }

    float const dx = target_transform->position.x - unit_transform->position.x;
    float const dz = target_transform->position.z - unit_transform->position.z;
    float const dist_sq = dx * dx + dz * dz;

    if (dist_sq < nearest_dist_sq) {
      nearest_dist_sq = dist_sq;
      nearest_enemy = target;
    }
  }

  return nearest_enemy;
}

void CombatSystem::process_hit_feedback(Engine::Core::World *world,
                                        float delta_time) {
  auto units = world->get_entities_with<Engine::Core::HitFeedbackComponent>();
  auto &visibility = CameraVisibilityService::instance();

  for (auto *unit : units) {
    if (unit->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto *feedback = unit->get_component<Engine::Core::HitFeedbackComponent>();
    if (feedback == nullptr || !feedback->is_reacting) {
      continue;
    }

    feedback->reaction_time += delta_time;
    float const progress =
        feedback->reaction_time /
        Engine::Core::HitFeedbackComponent::kReactionDuration;

    if (progress >= 1.0F) {
      feedback->is_reacting = false;
      feedback->reaction_time = 0.0F;
      feedback->reaction_intensity = 0.0F;
      feedback->knockback_x = 0.0F;
      feedback->knockback_z = 0.0F;
    } else {
      auto *transform = unit->get_component<Engine::Core::TransformComponent>();
      if (transform != nullptr) {
        if (!visibility.should_process_detailed_effects(
                transform->position.x, transform->position.y,
                transform->position.z)) {
          continue;
        }

        float const fade = 1.0F - progress;
        float const max_displacement_per_frame = 0.02F;
        float const dx = feedback->knockback_x * fade * delta_time;
        float const dz = feedback->knockback_z * fade * delta_time;
        float const displacement = std::sqrt(dx * dx + dz * dz);
        float const scale = (displacement > max_displacement_per_frame &&
                             displacement > 0.0001F)
                                ? max_displacement_per_frame / displacement
                                : 1.0F;
        transform->position.x += dx * scale;
        transform->position.z += dz * scale;
      }
    }
  }
}

void CombatSystem::process_combat_state(Engine::Core::World *world,
                                        float delta_time) {
  auto units = world->get_entities_with<Engine::Core::CombatStateComponent>();

  for (auto *unit : units) {
    if (unit->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto *combat_state =
        unit->get_component<Engine::Core::CombatStateComponent>();
    if (combat_state == nullptr) {
      continue;
    }

    if (combat_state->is_hit_paused) {
      combat_state->hit_pause_remaining -= delta_time;
      if (combat_state->hit_pause_remaining <= 0.0F) {
        combat_state->is_hit_paused = false;
        combat_state->hit_pause_remaining = 0.0F;
      }
      continue;
    }

    combat_state->state_time += delta_time;

    if (combat_state->state_time >= combat_state->state_duration) {
      using CS = Engine::Core::CombatAnimationState;
      using CSC = Engine::Core::CombatStateComponent;

      switch (combat_state->animation_state) {
      case CS::Advance:
        combat_state->animation_state = CS::WindUp;
        combat_state->state_duration = CSC::kWindUpDuration;
        break;
      case CS::WindUp:
        combat_state->animation_state = CS::Strike;
        combat_state->state_duration = CSC::kStrikeDuration;
        break;
      case CS::Strike:
        combat_state->animation_state = CS::Impact;
        combat_state->state_duration = CSC::kImpactDuration;
        break;
      case CS::Impact:
        combat_state->animation_state = CS::Recover;
        combat_state->state_duration = CSC::kRecoverDuration;
        break;
      case CS::Recover:
        combat_state->animation_state = CS::Reposition;
        combat_state->state_duration = CSC::kRepositionDuration;
        break;
      case CS::Reposition:
      case CS::Idle:
      default:
        combat_state->animation_state = CS::Idle;
        combat_state->state_duration = 0.0F;
        break;
      }
      combat_state->state_time = 0.0F;
    }
  }
}

void CombatSystem::apply_hit_feedback(Engine::Core::Entity *target,
                                      Engine::Core::EntityID attacker_id,
                                      Engine::Core::World *world) {
  if (target == nullptr) {
    return;
  }

  auto *feedback = target->get_component<Engine::Core::HitFeedbackComponent>();
  if (feedback == nullptr) {
    feedback = target->add_component<Engine::Core::HitFeedbackComponent>();
  }
  if (feedback == nullptr) {
    return;
  }

  feedback->is_reacting = true;
  feedback->reaction_time = 0.0F;
  feedback->reaction_intensity = 1.0F;

  auto *target_transform =
      target->get_component<Engine::Core::TransformComponent>();
  if (target_transform != nullptr && attacker_id != 0 && world != nullptr) {
    auto *attacker = world->get_entity(attacker_id);
    if (attacker != nullptr) {
      auto *attacker_transform =
          attacker->get_component<Engine::Core::TransformComponent>();
      if (attacker_transform != nullptr) {
        float const dx =
            target_transform->position.x - attacker_transform->position.x;
        float const dz =
            target_transform->position.z - attacker_transform->position.z;
        float const dist = std::sqrt(dx * dx + dz * dz);
        if (dist > 0.001F) {
          float const knockback =
              Engine::Core::HitFeedbackComponent::kMaxKnockback;
          feedback->knockback_x = (dx / dist) * knockback;
          feedback->knockback_z = (dz / dist) * knockback;
        }
      }
    }
  }

  auto *combat_state =
      target->get_component<Engine::Core::CombatStateComponent>();
  if (combat_state != nullptr) {
    combat_state->is_hit_paused = true;
    combat_state->hit_pause_remaining =
        Engine::Core::CombatStateComponent::kHitPauseDuration;
  }
}

} // namespace Game::Systems
