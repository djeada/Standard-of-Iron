#include "combat_system.h"
#include "../core/component.h"
#include "../core/event_manager.h"
#include "../core/world.h"
#include "../units/troop_config.h"
#include "../visuals/team_colors.h"
#include "arrow_system.h"
#include "building_collision_registry.h"
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

void CombatSystem::update(Engine::Core::World *world, float deltaTime) {
  processAttacks(world, deltaTime);
  processAutoEngagement(world, deltaTime);
}

void CombatSystem::processAttacks(Engine::Core::World *world, float deltaTime) {
  auto units = world->getEntitiesWith<Engine::Core::UnitComponent>();

  auto *arrow_sys = world->getSystem<ArrowSystem>();

  for (auto *attacker : units) {

    if (attacker->hasComponent<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto *attacker_unit = attacker->getComponent<Engine::Core::UnitComponent>();
    auto *attacker_transform =
        attacker->getComponent<Engine::Core::TransformComponent>();
    auto *attacker_atk =
        attacker->getComponent<Engine::Core::AttackComponent>();

    if ((attacker_unit == nullptr) || (attacker_transform == nullptr)) {
      continue;
    }

    if (attacker_unit->health <= 0) {
      continue;
    }

    if ((attacker_atk != nullptr) && attacker_atk->inMeleeLock) {
      auto *lock_target = world->getEntity(attacker_atk->meleeLockTargetId);
      if ((lock_target == nullptr) ||
          lock_target->hasComponent<Engine::Core::PendingRemovalComponent>()) {

        attacker_atk->inMeleeLock = false;
        attacker_atk->meleeLockTargetId = 0;
      } else {
        auto *lock_target_unit =
            lock_target->getComponent<Engine::Core::UnitComponent>();
        if ((lock_target_unit == nullptr) || lock_target_unit->health <= 0) {

          attacker_atk->inMeleeLock = false;
          attacker_atk->meleeLockTargetId = 0;
        } else {

          auto *att_t = attacker_transform;
          auto *tgt_t =
              lock_target->getComponent<Engine::Core::TransformComponent>();
          if ((att_t != nullptr) && (tgt_t != nullptr)) {
            float const dx = tgt_t->position.x - att_t->position.x;
            float const dz = tgt_t->position.z - att_t->position.z;
            float const dist = std::sqrt(dx * dx + dz * dz);

            const float ideal_melee_distance = 0.6F;
            const float max_melee_separation = 0.9F;

            if (dist > max_melee_separation) {
              float const pull_amount =
                  (dist - ideal_melee_distance) * 0.3F * deltaTime * 5.0F;

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

    if ((attacker_atk != nullptr) && attacker_atk->inMeleeLock &&
        attacker_atk->meleeLockTargetId != 0) {
      auto *lock_target = world->getEntity(attacker_atk->meleeLockTargetId);
      if ((lock_target != nullptr) &&
          !lock_target->hasComponent<Engine::Core::PendingRemovalComponent>()) {

        auto *attack_target =
            attacker->getComponent<Engine::Core::AttackTargetComponent>();
        if (attack_target == nullptr) {
          attack_target =
              attacker->addComponent<Engine::Core::AttackTargetComponent>();
        }
        if (attack_target != nullptr) {
          attack_target->target_id = attacker_atk->meleeLockTargetId;
          attack_target->shouldChase = false;
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

      range = attacker_atk->getCurrentRange();
      damage = attacker_atk->getCurrentDamage();
      cooldown = attacker_atk->getCurrentCooldown();

      auto *hold_mode =
          attacker->getComponent<Engine::Core::HoldModeComponent>();
      if ((hold_mode != nullptr) && hold_mode->active) {
        if (attacker_unit->spawn_type == Game::Units::SpawnType::Archer) {

          range *= 1.5F;
          damage = static_cast<int>(damage * 1.3F);
        } else if (attacker_unit->spawn_type ==
                   Game::Units::SpawnType::Spearman) {

          damage = static_cast<int>(damage * 1.4F);
        }
      }

      attacker_atk->timeSinceLast += deltaTime;
      t_accum = &attacker_atk->timeSinceLast;
    } else {
      tmp_accum += deltaTime;
      t_accum = &tmp_accum;
    }

    if (*t_accum < cooldown) {
      continue;
    }

    auto *attack_target =
        attacker->getComponent<Engine::Core::AttackTargetComponent>();
    Engine::Core::Entity *best_target = nullptr;

    if ((attack_target != nullptr) && attack_target->target_id != 0) {

      auto *target = world->getEntity(attack_target->target_id);
      if ((target != nullptr) &&
          !target->hasComponent<Engine::Core::PendingRemovalComponent>()) {
        auto *target_unit = target->getComponent<Engine::Core::UnitComponent>();

        auto &owner_registry = Game::Systems::OwnerRegistry::instance();
        bool const is_ally = owner_registry.areAllies(attacker_unit->owner_id,
                                                      target_unit->owner_id);

        if ((target_unit != nullptr) && target_unit->health > 0 &&
            target_unit->owner_id != attacker_unit->owner_id && !is_ally) {

          if (isInRange(attacker, target, range)) {
            best_target = target;

            bool is_ranged_unit = false;
            if ((attacker_atk != nullptr) && attacker_atk->canRanged &&
                attacker_atk->currentMode ==
                    Engine::Core::AttackComponent::CombatMode::Ranged) {
              is_ranged_unit = true;
            }

            if (is_ranged_unit) {
              auto *movement =
                  attacker->getComponent<Engine::Core::MovementComponent>();
              if ((movement != nullptr) && movement->hasTarget) {
                movement->hasTarget = false;
                movement->vx = 0.0F;
                movement->vz = 0.0F;
                movement->path.clear();
                if (attacker_transform != nullptr) {
                  movement->target_x = attacker_transform->position.x;
                  movement->target_y = attacker_transform->position.z;
                  movement->goalX = attacker_transform->position.x;
                  movement->goalY = attacker_transform->position.z;
                }
              }
            }

            if (auto *att_t =
                    attacker
                        ->getComponent<Engine::Core::TransformComponent>()) {
              if (auto *tgt_t =
                      target
                          ->getComponent<Engine::Core::TransformComponent>()) {
                float const dx = tgt_t->position.x - att_t->position.x;
                float const dz = tgt_t->position.z - att_t->position.z;
                float const yaw =
                    std::atan2(dx, dz) * 180.0F / std::numbers::pi_v<float>;
                att_t->desiredYaw = yaw;
                att_t->hasDesiredYaw = true;
              }
            }
          } else if (attack_target->shouldChase) {

            auto *hold_mode =
                attacker->getComponent<Engine::Core::HoldModeComponent>();
            if ((hold_mode != nullptr) && hold_mode->active) {
              if (!isInRange(attacker, target, range)) {
                attacker
                    ->removeComponent<Engine::Core::AttackTargetComponent>();
              }
              continue;
            }

            bool is_ranged_unit = false;
            if ((attacker_atk != nullptr) && attacker_atk->canRanged &&
                attacker_atk->currentMode ==
                    Engine::Core::AttackComponent::CombatMode::Ranged) {
              is_ranged_unit = true;
            }

            bool const currently_in_range = isInRange(attacker, target, range);

            if (is_ranged_unit && currently_in_range) {
              auto *movement =
                  attacker->getComponent<Engine::Core::MovementComponent>();
              if (movement != nullptr) {
                movement->hasTarget = false;
                movement->vx = 0.0F;
                movement->vz = 0.0F;
                movement->path.clear();
                auto *attacker_transform_component =
                    attacker->getComponent<Engine::Core::TransformComponent>();
                if (attacker_transform_component != nullptr) {
                  movement->target_x = attacker_transform_component->position.x;
                  movement->target_y = attacker_transform_component->position.z;
                  movement->goalX = attacker_transform_component->position.x;
                  movement->goalY = attacker_transform_component->position.z;
                }
              }
              best_target = target;
            } else {

              auto *target_transform =
                  target->getComponent<Engine::Core::TransformComponent>();
              auto *attacker_transform_component =
                  attacker->getComponent<Engine::Core::TransformComponent>();
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
                    target->hasComponent<Engine::Core::BuildingComponent>();
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
                    attacker->getComponent<Engine::Core::MovementComponent>();
                if (movement == nullptr) {
                  movement =
                      attacker->addComponent<Engine::Core::MovementComponent>();
                }

                if (movement != nullptr) {
                  if (hold_position) {
                    movement->hasTarget = false;
                    movement->vx = 0.0F;
                    movement->vz = 0.0F;
                    movement->path.clear();
                    if (attacker_transform_component != nullptr) {
                      movement->target_x =
                          attacker_transform_component->position.x;
                      movement->target_y =
                          attacker_transform_component->position.z;
                      movement->goalX =
                          attacker_transform_component->position.x;
                      movement->goalY =
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
                    bool need_new_command = !movement->pathPending;
                    if (movement->hasTarget && diff_sq <= 0.25F * 0.25F) {
                      need_new_command = false;
                    }

                    if (need_new_command) {
                      CommandService::MoveOptions options;
                      options.clearAttackIntent = false;

                      options.allowDirectFallback = true;
                      std::vector<Engine::Core::EntityID> const unit_ids = {
                          attacker->getId()};
                      std::vector<QVector3D> const move_targets = {desired_pos};
                      CommandService::moveUnits(*world, unit_ids, move_targets,
                                                options);
                    }
                  }
                }
              }

              if (isInRange(attacker, target, range)) {
                best_target = target;
              }
            }
          } else {

            attacker->removeComponent<Engine::Core::AttackTargetComponent>();
          }
        } else {

          attacker->removeComponent<Engine::Core::AttackTargetComponent>();
        }
      } else {

        attacker->removeComponent<Engine::Core::AttackTargetComponent>();
      }
    }

    if ((best_target == nullptr) && (attack_target == nullptr)) {

      auto &owner_registry = Game::Systems::OwnerRegistry::instance();

      for (auto *target : units) {
        if (target == attacker) {
          continue;
        }

        auto *target_unit = target->getComponent<Engine::Core::UnitComponent>();
        if ((target_unit == nullptr) || target_unit->health <= 0) {
          continue;
        }

        if (target_unit->owner_id == attacker_unit->owner_id) {
          continue;
        }

        if (owner_registry.areAllies(attacker_unit->owner_id,
                                     target_unit->owner_id)) {
          continue;
        }

        if (target->hasComponent<Engine::Core::BuildingComponent>()) {
          continue;
        }

        if (isInRange(attacker, target, range)) {
          best_target = target;
          break;
        }
      }
    }

    if (best_target != nullptr) {
      auto *best_target_unit =
          best_target->getComponent<Engine::Core::UnitComponent>();

      if (!attacker->hasComponent<Engine::Core::AttackTargetComponent>()) {
        auto *new_target =
            attacker->addComponent<Engine::Core::AttackTargetComponent>();
        new_target->target_id = best_target->getId();
        new_target->shouldChase = false;
      } else {
        auto *existing_target =
            attacker->getComponent<Engine::Core::AttackTargetComponent>();
        if (existing_target->target_id != best_target->getId()) {
          existing_target->target_id = best_target->getId();
          existing_target->shouldChase = false;
        }
      }

      bool is_ranged_unit = false;
      if ((attacker_atk != nullptr) && attacker_atk->canRanged &&
          attacker_atk->currentMode ==
              Engine::Core::AttackComponent::CombatMode::Ranged) {
        is_ranged_unit = true;
      }

      if (is_ranged_unit) {
        auto *movement =
            attacker->getComponent<Engine::Core::MovementComponent>();
        if ((movement != nullptr) && movement->hasTarget) {
          movement->hasTarget = false;
          movement->vx = 0.0F;
          movement->vz = 0.0F;
          movement->path.clear();
          if (attacker_transform != nullptr) {
            movement->target_x = attacker_transform->position.x;
            movement->target_y = attacker_transform->position.z;
            movement->goalX = attacker_transform->position.x;
            movement->goalY = attacker_transform->position.z;
          }
        }
      }

      if (auto *att_t =
              attacker->getComponent<Engine::Core::TransformComponent>()) {
        if (auto *tgt_t =
                best_target->getComponent<Engine::Core::TransformComponent>()) {
          float const dx = tgt_t->position.x - att_t->position.x;
          float const dz = tgt_t->position.z - att_t->position.z;
          float const yaw =
              std::atan2(dx, dz) * 180.0F / std::numbers::pi_v<float>;
          att_t->desiredYaw = yaw;
          att_t->hasDesiredYaw = true;
        }
      }

      if (arrow_sys != nullptr) {
        auto *att_t =
            attacker->getComponent<Engine::Core::TransformComponent>();
        auto *tgt_t =
            best_target->getComponent<Engine::Core::TransformComponent>();
        auto *att_u = attacker->getComponent<Engine::Core::UnitComponent>();

        if ((attacker_atk == nullptr) ||
            attacker_atk->currentMode !=
                Engine::Core::AttackComponent::CombatMode::Melee) {
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
          attacker_atk->currentMode ==
              Engine::Core::AttackComponent::CombatMode::Melee) {

        attacker_atk->inMeleeLock = true;
        attacker_atk->meleeLockTargetId = best_target->getId();

        auto *target_atk =
            best_target->getComponent<Engine::Core::AttackComponent>();
        if (target_atk != nullptr) {
          target_atk->inMeleeLock = true;
          target_atk->meleeLockTargetId = attacker->getId();
        }

        auto *att_t =
            attacker->getComponent<Engine::Core::TransformComponent>();
        auto *tgt_t =
            best_target->getComponent<Engine::Core::TransformComponent>();
        if ((att_t != nullptr) && (tgt_t != nullptr)) {
          float const dx = tgt_t->position.x - att_t->position.x;
          float const dz = tgt_t->position.z - att_t->position.z;
          float const dist = std::sqrt(dx * dx + dz * dz);

          const float ideal_melee_distance = 0.6F;

          if (dist > ideal_melee_distance + 0.1F) {

            float const move_amount = (dist - ideal_melee_distance) * 0.5F;

            if (dist > 0.001F) {
              QVector3D const direction(dx / dist, 0.0F, dz / dist);

              att_t->position.x += direction.x() * move_amount;
              att_t->position.z += direction.z() * move_amount;

              tgt_t->position.x -= direction.x() * move_amount;
              tgt_t->position.z -= direction.z() * move_amount;
            }
          }
        }
      }

      dealDamage(world, best_target, damage, attacker->getId());
      *t_accum = 0.0F;
    } else {

      if ((attack_target == nullptr) &&
          attacker->hasComponent<Engine::Core::AttackTargetComponent>()) {
        attacker->removeComponent<Engine::Core::AttackTargetComponent>();
      }
    }
  }
}

auto CombatSystem::isInRange(Engine::Core::Entity *attacker,
                             Engine::Core::Entity *target,
                             float range) -> bool {
  auto *attacker_transform =
      attacker->getComponent<Engine::Core::TransformComponent>();
  auto *target_transform =
      target->getComponent<Engine::Core::TransformComponent>();

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
  if (target->hasComponent<Engine::Core::BuildingComponent>()) {

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

  auto *attacker_atk = attacker->getComponent<Engine::Core::AttackComponent>();
  if ((attacker_atk != nullptr) &&
      attacker_atk->currentMode ==
          Engine::Core::AttackComponent::CombatMode::Melee) {
    float const height_diff = std::abs(dy);
    if (height_diff > attacker_atk->max_heightDifference) {
      return false;
    }
  }

  return true;
}

void CombatSystem::dealDamage(Engine::Core::World *world,
                              Engine::Core::Entity *target, int damage,
                              Engine::Core::EntityID attackerId) {
  auto *unit = target->getComponent<Engine::Core::UnitComponent>();
  if (unit != nullptr) {
    unit->health = std::max(0, unit->health - damage);

    int attacker_owner_id = 0;
    if (attackerId != 0 && (world != nullptr)) {
      auto *attacker = world->getEntity(attackerId);
      if (attacker != nullptr) {
        auto *attacker_unit =
            attacker->getComponent<Engine::Core::UnitComponent>();
        if (attacker_unit != nullptr) {
          attacker_owner_id = attacker_unit->owner_id;
        }
      }
    }

    if (target->hasComponent<Engine::Core::BuildingComponent>() &&
        unit->health > 0) {
      Engine::Core::EventManager::instance().publish(
          Engine::Core::BuildingAttackedEvent(target->getId(), unit->owner_id,
                                              unit->spawn_type, attackerId,
                                              attacker_owner_id, damage));
    }

    if (unit->health <= 0) {

      int const killer_owner_id = attacker_owner_id;

      Engine::Core::EventManager::instance().publish(
          Engine::Core::UnitDiedEvent(target->getId(), unit->owner_id,
                                      unit->spawn_type, attackerId,
                                      killer_owner_id));

      auto *target_atk = target->getComponent<Engine::Core::AttackComponent>();
      if ((target_atk != nullptr) && target_atk->inMeleeLock &&
          target_atk->meleeLockTargetId != 0) {

        if (world != nullptr) {
          auto *lock_partner = world->getEntity(target_atk->meleeLockTargetId);
          if ((lock_partner != nullptr) &&
              !lock_partner
                   ->hasComponent<Engine::Core::PendingRemovalComponent>()) {
            auto *partner_atk =
                lock_partner->getComponent<Engine::Core::AttackComponent>();
            if ((partner_atk != nullptr) &&
                partner_atk->meleeLockTargetId == target->getId()) {
              partner_atk->inMeleeLock = false;
              partner_atk->meleeLockTargetId = 0;
            }
          }
        }
      }

      if (target->hasComponent<Engine::Core::BuildingComponent>()) {
        BuildingCollisionRegistry::instance().unregisterBuilding(
            target->getId());
      }

      if (auto *r = target->getComponent<Engine::Core::RenderableComponent>()) {
        r->visible = false;
      }

      if (auto *movement =
              target->getComponent<Engine::Core::MovementComponent>()) {
        movement->hasTarget = false;
        movement->vx = 0.0F;
        movement->vz = 0.0F;
        movement->path.clear();
        movement->pathPending = false;
      }

      target->addComponent<Engine::Core::PendingRemovalComponent>();
    }
  }
}

void CombatSystem::updateCombatMode(
    Engine::Core::Entity *attacker, Engine::Core::World *world,
    Engine::Core::AttackComponent *attack_comp) {
  if (attack_comp == nullptr) {
    return;
  }

  if (attack_comp->preferredMode !=
      Engine::Core::AttackComponent::CombatMode::Auto) {
    attack_comp->currentMode = attack_comp->preferredMode;
    return;
  }

  auto *attacker_transform =
      attacker->getComponent<Engine::Core::TransformComponent>();
  if (attacker_transform == nullptr) {
    return;
  }

  auto *attacker_unit = attacker->getComponent<Engine::Core::UnitComponent>();
  if (attacker_unit == nullptr) {
    return;
  }

  auto &owner_registry = Game::Systems::OwnerRegistry::instance();
  auto units = world->getEntitiesWith<Engine::Core::UnitComponent>();

  float closest_enemy_dist_sq = std::numeric_limits<float>::max();
  float closest_height_diff = 0.0F;

  for (auto *target : units) {
    if (target == attacker) {
      continue;
    }

    auto *target_unit = target->getComponent<Engine::Core::UnitComponent>();
    if ((target_unit == nullptr) || target_unit->health <= 0) {
      continue;
    }

    if (owner_registry.areAllies(attacker_unit->owner_id,
                                 target_unit->owner_id)) {
      continue;
    }

    auto *target_transform =
        target->getComponent<Engine::Core::TransformComponent>();
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
    if (attack_comp->canRanged) {
      attack_comp->currentMode =
          Engine::Core::AttackComponent::CombatMode::Ranged;
    } else {
      attack_comp->currentMode =
          Engine::Core::AttackComponent::CombatMode::Melee;
    }
    return;
  }

  float const closest_dist = std::sqrt(closest_enemy_dist_sq);

  bool const in_melee_range =
      attack_comp->isInMeleeRange(closest_dist, closest_height_diff);
  bool const in_ranged_range = attack_comp->isInRangedRange(closest_dist);

  if (in_melee_range && attack_comp->canMelee) {
    attack_comp->currentMode = Engine::Core::AttackComponent::CombatMode::Melee;
  } else if (in_ranged_range && attack_comp->canRanged) {
    attack_comp->currentMode =
        Engine::Core::AttackComponent::CombatMode::Ranged;
  } else if (attack_comp->canRanged) {
    attack_comp->currentMode =
        Engine::Core::AttackComponent::CombatMode::Ranged;
  } else {
    attack_comp->currentMode = Engine::Core::AttackComponent::CombatMode::Melee;
  }
}

void CombatSystem::processAutoEngagement(Engine::Core::World *world,
                                         float deltaTime) {
  auto units = world->getEntitiesWith<Engine::Core::UnitComponent>();

  for (auto it = m_engagementCooldowns.begin();
       it != m_engagementCooldowns.end();) {
    it->second -= deltaTime;
    if (it->second <= 0.0F) {
      it = m_engagementCooldowns.erase(it);
    } else {
      ++it;
    }
  }

  for (auto *unit : units) {

    if (unit->hasComponent<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto *unit_comp = unit->getComponent<Engine::Core::UnitComponent>();
    if ((unit_comp == nullptr) || unit_comp->health <= 0) {
      continue;
    }

    if (unit->hasComponent<Engine::Core::BuildingComponent>()) {
      continue;
    }

    auto *attack_comp = unit->getComponent<Engine::Core::AttackComponent>();
    if ((attack_comp == nullptr) || !attack_comp->canMelee) {
      continue;
    }

    if (attack_comp->canRanged &&
        attack_comp->preferredMode !=
            Engine::Core::AttackComponent::CombatMode::Melee) {
      continue;
    }

    if (m_engagementCooldowns.find(unit->getId()) !=
        m_engagementCooldowns.end()) {
      continue;
    }

    if (!isUnitIdle(unit)) {
      continue;
    }

    float const vision_range = unit_comp->vision_range;
    auto *nearest_enemy = findNearestEnemy(unit, world, vision_range);

    if (nearest_enemy != nullptr) {

      auto *attack_target =
          unit->getComponent<Engine::Core::AttackTargetComponent>();
      if (attack_target == nullptr) {
        attack_target =
            unit->addComponent<Engine::Core::AttackTargetComponent>();
      }
      if (attack_target != nullptr) {
        attack_target->target_id = nearest_enemy->getId();
        attack_target->shouldChase = true;

        m_engagementCooldowns[unit->getId()] = ENGAGEMENT_COOLDOWN;
      }
    }
  }
}

auto CombatSystem::isUnitIdle(Engine::Core::Entity *unit) -> bool {

  auto *hold_mode = unit->getComponent<Engine::Core::HoldModeComponent>();
  if ((hold_mode != nullptr) && hold_mode->active) {
    return false;
  }

  auto *attack_target =
      unit->getComponent<Engine::Core::AttackTargetComponent>();
  if ((attack_target != nullptr) && attack_target->target_id != 0) {
    return false;
  }

  auto *movement = unit->getComponent<Engine::Core::MovementComponent>();
  if ((movement != nullptr) && movement->hasTarget) {
    return false;
  }

  auto *attack_comp = unit->getComponent<Engine::Core::AttackComponent>();
  if ((attack_comp != nullptr) && attack_comp->inMeleeLock) {
    return false;
  }

  auto *patrol = unit->getComponent<Engine::Core::PatrolComponent>();
  return (patrol == nullptr) || !patrol->patrolling;
}

auto CombatSystem::findNearestEnemy(Engine::Core::Entity *unit,
                                    Engine::Core::World *world,
                                    float maxRange) -> Engine::Core::Entity * {
  auto *unit_comp = unit->getComponent<Engine::Core::UnitComponent>();
  auto *unit_transform = unit->getComponent<Engine::Core::TransformComponent>();
  if ((unit_comp == nullptr) || (unit_transform == nullptr)) {
    return nullptr;
  }

  auto &owner_registry = Game::Systems::OwnerRegistry::instance();
  auto units = world->getEntitiesWith<Engine::Core::UnitComponent>();

  Engine::Core::Entity *nearest_enemy = nullptr;
  float nearest_dist_sq = maxRange * maxRange;

  for (auto *target : units) {
    if (target == unit) {
      continue;
    }

    if (target->hasComponent<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto *target_unit = target->getComponent<Engine::Core::UnitComponent>();
    if ((target_unit == nullptr) || target_unit->health <= 0) {
      continue;
    }

    if (target_unit->owner_id == unit_comp->owner_id) {
      continue;
    }
    if (owner_registry.areAllies(unit_comp->owner_id, target_unit->owner_id)) {
      continue;
    }

    if (target->hasComponent<Engine::Core::BuildingComponent>()) {
      continue;
    }

    auto *target_transform =
        target->getComponent<Engine::Core::TransformComponent>();
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

} // namespace Game::Systems