#include "attack_processor.h"
#include "../../core/component.h"
#include "../../core/world.h"
#include "../../units/spawn_type.h"
#include "../../units/troop_config.h"
#include "../../visuals/team_colors.h"
#include "../arrow_system.h"
#include "../command_service.h"
#include "../owner_registry.h"
#include "../pathfinding.h"
#include "../troop_profile_service.h"
#include "combat_mode_processor.h"
#include "combat_types.h"
#include "combat_utils.h"
#include "damage_processor.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>
#include <qvectornd.h>
#include <random>

namespace Game::Systems::Combat {

namespace {
thread_local std::mt19937 gen(std::random_device{}());

void stop_unit_movement(Engine::Core::Entity *unit,
                        Engine::Core::TransformComponent *transform) {
  auto *movement = unit->get_component<Engine::Core::MovementComponent>();
  if ((movement != nullptr) && movement->has_target) {
    movement->has_target = false;
    movement->vx = 0.0F;
    movement->vz = 0.0F;
    movement->clear_path();
    if (transform != nullptr) {
      movement->target_x = transform->position.x;
      movement->target_y = transform->position.z;
      movement->goal_x = transform->position.x;
      movement->goal_y = transform->position.z;
    }
  }
}

void face_target(Engine::Core::TransformComponent *attacker_transform,
                 Engine::Core::TransformComponent *target_transform) {
  if ((attacker_transform == nullptr) || (target_transform == nullptr)) {
    return;
  }
  float const dx =
      target_transform->position.x - attacker_transform->position.x;
  float const dz =
      target_transform->position.z - attacker_transform->position.z;
  float const yaw = std::atan2(dx, dz) * 180.0F / std::numbers::pi_v<float>;
  attacker_transform->desired_yaw = yaw;
  attacker_transform->has_desired_yaw = true;
}

auto is_ranged_mode(Engine::Core::AttackComponent *attack_comp) -> bool {
  return (attack_comp != nullptr) && attack_comp->can_ranged &&
         attack_comp->current_mode ==
             Engine::Core::AttackComponent::CombatMode::Ranged;
}

auto get_base_max_health(const Engine::Core::UnitComponent *unit)
    -> std::optional<int> {
  if (unit == nullptr) {
    return std::nullopt;
  }
  auto troop_type_opt = Game::Units::spawn_typeToTroopType(unit->spawn_type);
  if (!troop_type_opt) {
    return std::nullopt;
  }
  auto profile = Game::Systems::TroopProfileService::instance().get_profile(
      unit->nation_id, *troop_type_opt);
  return profile.combat.max_health;
}

auto is_high_ground_advantage(
    const Engine::Core::TransformComponent *high_transform,
    const Engine::Core::TransformComponent *low_transform) -> bool {
  if ((high_transform == nullptr) || (low_transform == nullptr)) {
    return false;
  }
  float const height_diff =
      high_transform->position.y - low_transform->position.y;
  return height_diff > Constants::kHighGroundHeightThreshold;
}

void process_melee_lock(Engine::Core::Entity *attacker,
                        Engine::Core::AttackComponent *attack_comp,
                        Engine::Core::World *world, float delta_time) {
  if (attack_comp == nullptr || !attack_comp->in_melee_lock) {
    return;
  }

  auto *lock_target = world->get_entity(attack_comp->melee_lock_target_id);
  if ((lock_target == nullptr) ||
      lock_target->has_component<Engine::Core::PendingRemovalComponent>()) {
    attack_comp->in_melee_lock = false;
    attack_comp->melee_lock_target_id = 0;
    return;
  }

  auto *lock_target_unit =
      lock_target->get_component<Engine::Core::UnitComponent>();
  if ((lock_target_unit == nullptr) || lock_target_unit->health <= 0) {
    attack_comp->in_melee_lock = false;
    attack_comp->melee_lock_target_id = 0;
    return;
  }

  auto *att_t = attacker->get_component<Engine::Core::TransformComponent>();
  auto *tgt_t = lock_target->get_component<Engine::Core::TransformComponent>();
  if ((att_t == nullptr) || (tgt_t == nullptr)) {
    return;
  }

  face_target(att_t, tgt_t);
  face_target(tgt_t, att_t);

  float const dx = tgt_t->position.x - att_t->position.x;
  float const dz = tgt_t->position.z - att_t->position.z;
  float const dist = std::sqrt(dx * dx + dz * dz);

  if (dist > Constants::kMaxMeleeSeparation) {
    if (!is_unit_in_hold_mode(attacker) && !is_building(attacker)) {
      float const pull_amount = (dist - Constants::kIdealMeleeDistance) *
                                Constants::kMeleePullFactor * delta_time *
                                Constants::kMeleePullSpeed;

      if (dist > Constants::kMinDistance) {
        QVector3D const direction(dx / dist, 0.0F, dz / dist);
        float const new_x = att_t->position.x + direction.x() * pull_amount;
        float const new_z = att_t->position.z + direction.z() * pull_amount;

        auto *pathfinder = CommandService::get_pathfinder();
        if (pathfinder != nullptr) {
          Point const new_grid = CommandService::world_to_grid(new_x, new_z);
          if (pathfinder->is_walkable(new_grid.x, new_grid.y)) {
            att_t->position.x = new_x;
            att_t->position.z = new_z;
          } else {

            attack_comp->in_melee_lock = false;
            attack_comp->melee_lock_target_id = 0;
          }
        } else {

          att_t->position.x = new_x;
          att_t->position.z = new_z;
        }
      }
    }
  }
}

void sync_melee_lock_target(Engine::Core::Entity *attacker,
                            Engine::Core::AttackComponent *attack_comp) {
  if (attack_comp == nullptr || !attack_comp->in_melee_lock ||
      attack_comp->melee_lock_target_id == 0) {
    return;
  }

  auto *attack_target =
      attacker->get_component<Engine::Core::AttackTargetComponent>();
  if (attack_target == nullptr) {
    attack_target =
        attacker->add_component<Engine::Core::AttackTargetComponent>();
  }
  if (attack_target != nullptr) {
    attack_target->target_id = attack_comp->melee_lock_target_id;
    attack_target->should_chase = false;
  }
}

void apply_health_bonus(Engine::Core::UnitComponent *unit_comp) {
  auto base_max_health_opt = get_base_max_health(unit_comp);
  int const base_max_health =
      base_max_health_opt.value_or(std::max(1, unit_comp->max_health));
  int const max_health_bonus = static_cast<int>(
      static_cast<float>(base_max_health) * Constants::kHealthMultiplierHold);
  if (unit_comp->max_health < max_health_bonus) {
    int const safe_max_health = std::max(1, unit_comp->max_health);
    int const health_percentage = (unit_comp->health * 100) / safe_max_health;
    unit_comp->max_health = max_health_bonus;
    unit_comp->health = (max_health_bonus * health_percentage) / 100;
  }
}

void apply_hold_mode_bonuses(Engine::Core::Entity *attacker,
                             Engine::Core::UnitComponent *unit_comp,
                             float &range, int &damage) {
  auto *hold_mode = attacker->get_component<Engine::Core::HoldModeComponent>();
  if ((hold_mode == nullptr) || !hold_mode->active) {
    return;
  }

  if (unit_comp->spawn_type == Game::Units::SpawnType::Archer) {
    range *= Constants::kRangeMultiplierHold;
    damage = static_cast<int>(static_cast<float>(damage) *
                              Constants::kDamageMultiplierArcherHold);
    apply_health_bonus(unit_comp);
  } else if (unit_comp->spawn_type == Game::Units::SpawnType::Spearman) {
    range *= Constants::kRangeMultiplierSpearmanHold;
    damage = static_cast<int>(static_cast<float>(damage) *
                              Constants::kDamageMultiplierSpearmanHold);
    apply_health_bonus(unit_comp);
  } else {
    damage = static_cast<int>(static_cast<float>(damage) *
                              Constants::kDamageMultiplierDefaultHold);
  }
}

void apply_high_ground_defense_bonuses(Engine::Core::Entity *attacker,
                                       Engine::Core::Entity *target,
                                       Engine::Core::UnitComponent *target_unit,
                                       int &damage) {
  if (target_unit == nullptr) {
    return;
  }

  if (target_unit->spawn_type != Game::Units::SpawnType::Archer &&
      target_unit->spawn_type != Game::Units::SpawnType::Spearman) {
    return;
  }

  auto *attacker_transform =
      attacker->get_component<Engine::Core::TransformComponent>();
  auto *target_transform =
      target->get_component<Engine::Core::TransformComponent>();
  if (!is_high_ground_advantage(target_transform, attacker_transform)) {
    return;
  }

  damage = std::max(1, static_cast<int>(static_cast<float>(damage) *
                                        Constants::kHighGroundArmorMultiplier));

  auto base_max_health_opt = get_base_max_health(target_unit);
  if (!base_max_health_opt || *base_max_health_opt <= 0) {
    return;
  }

  int const max_health_bonus =
      static_cast<int>(static_cast<float>(*base_max_health_opt) *
                       Constants::kHighGroundHealthMultiplier);
  if (target_unit->max_health < max_health_bonus) {
    int const safe_max_health = std::max(1, target_unit->max_health);
    int const health_percentage = (target_unit->health * 100) / safe_max_health;
    target_unit->max_health = max_health_bonus;
    target_unit->health = (max_health_bonus * health_percentage) / 100;
  }
}

auto calculate_tactical_damage_multiplier(
    Engine::Core::Entity *attacker, Engine::Core::Entity *target,
    Engine::Core::UnitComponent *attacker_unit,
    Engine::Core::UnitComponent *target_unit) -> float {
  float multiplier = 1.0F;

  if (attacker_unit->spawn_type == Game::Units::SpawnType::Spearman) {
    if (target_unit->spawn_type == Game::Units::SpawnType::HorseArcher ||
        target_unit->spawn_type == Game::Units::SpawnType::HorseSpearman ||
        target_unit->spawn_type == Game::Units::SpawnType::MountedKnight) {
      multiplier *= Constants::kSpearmanVsCavalryMultiplier;
    }
  }

  if (attacker_unit->spawn_type == Game::Units::SpawnType::Archer ||
      attacker_unit->spawn_type == Game::Units::SpawnType::HorseArcher) {
    // Archer bonus against elephants
    if (target->has_component<Engine::Core::ElephantComponent>()) {
      multiplier *= Constants::kArcherVsElephantMultiplier;
    }

    auto *attacker_transform =
        attacker->get_component<Engine::Core::TransformComponent>();
    auto *target_transform =
        target->get_component<Engine::Core::TransformComponent>();

    if (is_high_ground_advantage(attacker_transform, target_transform)) {
      multiplier *= Constants::kArcherHighGroundMultiplier;
    }
  } else if (attacker_unit->spawn_type == Game::Units::SpawnType::Spearman) {
    auto *attacker_transform =
        attacker->get_component<Engine::Core::TransformComponent>();
    auto *target_transform =
        target->get_component<Engine::Core::TransformComponent>();

    if (is_high_ground_advantage(attacker_transform, target_transform)) {
      multiplier *= Constants::kSpearmanHighGroundMultiplier;
    }
  }

  return multiplier;
}

void spawn_arrows(Engine::Core::Entity *attacker, Engine::Core::Entity *target,
                  ArrowSystem *arrow_sys) {
  if (arrow_sys == nullptr) {
    return;
  }

  auto *att_t = attacker->get_component<Engine::Core::TransformComponent>();
  auto *tgt_t = target->get_component<Engine::Core::TransformComponent>();
  auto *att_u = attacker->get_component<Engine::Core::UnitComponent>();

  if ((att_t == nullptr) || (tgt_t == nullptr)) {
    return;
  }

  QVector3D const a_pos(att_t->position.x, att_t->position.y,
                        att_t->position.z);
  QVector3D const t_pos(tgt_t->position.x, tgt_t->position.y,
                        tgt_t->position.z);
  QVector3D const dir = (t_pos - a_pos).normalized();
  QVector3D const color =
      (att_u != nullptr) ? Game::Visuals::team_colorForOwner(att_u->owner_id)
                         : QVector3D(0.8F, 0.9F, 1.0F);

  int arrow_count = 1;
  if (att_u != nullptr) {
    int const troop_size =
        Game::Units::TroopConfig::instance().getIndividualsPerUnit(
            att_u->spawn_type);

    int const max_arrows = std::max(2, (troop_size * 2) / 3);

    static thread_local std::mt19937 arrow_gen(std::random_device{}());
    std::uniform_int_distribution<> dist(max_arrows / 2, max_arrows);
    arrow_count = dist(arrow_gen);
  }

  for (int i = 0; i < arrow_count; ++i) {
    static thread_local std::mt19937 spread_gen(std::random_device{}());
    std::uniform_real_distribution<float> spread_dist(
        Constants::kArrowSpreadMin, Constants::kArrowSpreadMax);

    QVector3D const perpendicular(-dir.z(), 0.0F, dir.x());
    QVector3D const up_vector(0.0F, 1.0F, 0.0F);

    float const lateral_offset = spread_dist(spread_gen);
    float const vertical_offset =
        spread_dist(spread_gen) * Constants::kArrowVerticalSpreadFactor;
    float const depth_offset =
        spread_dist(spread_gen) * Constants::kArrowDepthSpreadFactor;

    QVector3D const start_offset =
        perpendicular * lateral_offset + up_vector * vertical_offset;
    QVector3D const end_offset = perpendicular * lateral_offset +
                                 up_vector * vertical_offset +
                                 dir * depth_offset;

    QVector3D const start =
        a_pos + QVector3D(0.0F, Constants::kArrowStartHeight, 0.0F) +
        dir * Constants::kArrowStartOffset + start_offset;
    QVector3D const end = t_pos +
                          QVector3D(Constants::kArrowTargetOffset,
                                    Constants::kArrowTargetOffset, 0.0F) +
                          end_offset;

    arrow_sys->spawn_arrow(start, end, color, Constants::kArrowSpeed);
  }
}

void initiate_melee_combat(Engine::Core::Entity *attacker,
                           Engine::Core::Entity *target,
                           Engine::Core::AttackComponent *attack_comp,
                           Engine::Core::World *world) {
  attack_comp->in_melee_lock = true;
  attack_comp->melee_lock_target_id = target->get_id();

  auto *combat_state =
      attacker->get_component<Engine::Core::CombatStateComponent>();
  if (combat_state == nullptr) {
    combat_state =
        attacker->add_component<Engine::Core::CombatStateComponent>();
  }
  if (combat_state != nullptr && combat_state->animation_state ==
                                     Engine::Core::CombatAnimationState::Idle) {
    combat_state->animation_state = Engine::Core::CombatAnimationState::Advance;
    combat_state->state_time = 0.0F;
    combat_state->state_duration =
        Engine::Core::CombatStateComponent::kAdvanceDuration;
    std::uniform_real_distribution<float> offset_dist(0.0F, 0.15F);
    combat_state->attack_offset = offset_dist(gen);
    std::uniform_int_distribution<int> variant_dist(
        0, Engine::Core::CombatStateComponent::kMaxAttackVariants - 1);
    combat_state->attack_variant = static_cast<std::uint8_t>(variant_dist(gen));
  }

  auto *target_atk = target->get_component<Engine::Core::AttackComponent>();
  if (target_atk != nullptr) {
    target_atk->in_melee_lock = true;
    target_atk->melee_lock_target_id = attacker->get_id();
  }

  auto *att_t = attacker->get_component<Engine::Core::TransformComponent>();
  auto *tgt_t = target->get_component<Engine::Core::TransformComponent>();
  if ((att_t != nullptr) && (tgt_t != nullptr)) {
    face_target(att_t, tgt_t);
    face_target(tgt_t, att_t);

    float const dx = tgt_t->position.x - att_t->position.x;
    float const dz = tgt_t->position.z - att_t->position.z;
    float const dist = std::sqrt(dx * dx + dz * dz);

    if (dist > Constants::kIdealMeleeDistance + 0.1F) {
      float const move_amount = (dist - Constants::kIdealMeleeDistance) *
                                Constants::kMoveAmountFactor;

      if (dist > Constants::kMinDistance) {
        QVector3D const direction(dx / dist, 0.0F, dz / dist);

        if (!is_unit_in_hold_mode(attacker) && !is_building(attacker)) {
          att_t->position.x += direction.x() * move_amount;
          att_t->position.z += direction.z() * move_amount;
        }

        if (!is_unit_in_hold_mode(target) && !is_building(target)) {
          tgt_t->position.x -= direction.x() * move_amount;
          tgt_t->position.z -= direction.z() * move_amount;
        }
      }
    }
  }
}

} // namespace

void process_attacks(Engine::Core::World *world, float delta_time) {
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

    process_melee_lock(attacker, attacker_atk, world, delta_time);
    sync_melee_lock_target(attacker, attacker_atk);

    float range = 2.0F;
    int damage = 10;
    float cooldown = 1.0F;
    float *t_accum = nullptr;
    float tmp_accum = 0.0F;

    if (attacker_atk != nullptr) {
      update_combat_mode(attacker, world, attacker_atk);

      range = attacker_atk->get_current_range();
      damage = attacker_atk->get_current_damage();
      cooldown = attacker_atk->get_current_cooldown();

      apply_hold_mode_bonuses(attacker, attacker_unit, range, damage);

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
          bool const ranged_unit = is_ranged_mode(attacker_atk);

          if (is_in_range(attacker, target, range)) {
            best_target = target;
            if (ranged_unit) {
              stop_unit_movement(attacker, attacker_transform);
            }
            face_target(
                attacker_transform,
                target->get_component<Engine::Core::TransformComponent>());
          } else if (attack_target->should_chase) {
            auto *hold_mode =
                attacker->get_component<Engine::Core::HoldModeComponent>();
            if ((hold_mode != nullptr) && hold_mode->active) {
              attacker->remove_component<Engine::Core::AttackTargetComponent>();
              continue;
            }

            auto *guard_mode =
                attacker->get_component<Engine::Core::GuardModeComponent>();
            if ((guard_mode != nullptr) && guard_mode->active) {
              float guard_x = guard_mode->guard_position_x;
              float guard_z = guard_mode->guard_position_z;
              if (guard_mode->guarded_entity_id != 0) {
                auto *guarded_entity =
                    world->get_entity(guard_mode->guarded_entity_id);
                if (guarded_entity != nullptr) {
                  auto *guarded_transform =
                      guarded_entity
                          ->get_component<Engine::Core::TransformComponent>();
                  if (guarded_transform != nullptr) {
                    guard_x = guarded_transform->position.x;
                    guard_z = guarded_transform->position.z;
                  }
                }
              }
              auto *target_transform =
                  target->get_component<Engine::Core::TransformComponent>();
              if (target_transform != nullptr) {
                float const dx = target_transform->position.x - guard_x;
                float const dz = target_transform->position.z - guard_z;
                float const dist_sq = dx * dx + dz * dz;
                float const guard_radius_sq =
                    guard_mode->guard_radius * guard_mode->guard_radius;
                if (dist_sq > guard_radius_sq) {
                  attacker
                      ->remove_component<Engine::Core::AttackTargetComponent>();
                  continue;
                }
              }
            }

            if (ranged_unit && is_in_range(attacker, target, range)) {
              stop_unit_movement(attacker, attacker_transform);
              best_target = target;
            } else {
              auto *target_transform =
                  target->get_component<Engine::Core::TransformComponent>();
              if ((target_transform != nullptr) &&
                  (attacker_transform != nullptr)) {
                QVector3D const attacker_pos(attacker_transform->position.x,
                                             0.0F,
                                             attacker_transform->position.z);
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
                } else if (ranged_unit) {
                  QVector3D direction = target_pos - attacker_pos;
                  float const distance_sq = direction.lengthSquared();
                  if (distance_sq > 0.000001F) {
                    float const distance = std::sqrt(distance_sq);
                    direction /= distance;
                    float const optimal_range =
                        range * Constants::kOptimalRangeFactor;
                    if (distance >
                        optimal_range + Constants::kOptimalRangeBuffer) {
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
                    movement->clear_path();
                    movement->target_x = attacker_transform->position.x;
                    movement->target_y = attacker_transform->position.z;
                    movement->goal_x = attacker_transform->position.x;
                    movement->goal_y = attacker_transform->position.z;
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
                    float const threshold = Constants::kNewCommandThreshold *
                                            Constants::kNewCommandThreshold;
                    if (movement->has_target && diff_sq <= threshold) {
                      need_new_command = false;
                    }

                    if (need_new_command) {
                      CommandService::MoveOptions options;
                      options.clear_attack_intent = false;
                      options.allow_direct_fallback = true;
                      std::vector<Engine::Core::EntityID> const unit_ids = {
                          attacker->get_id()};
                      std::vector<QVector3D> const move_targets = {desired_pos};
                      CommandService::move_units(*world, unit_ids, move_targets,
                                                 options);
                    }
                  }
                }

                if (is_in_range(attacker, target, range)) {
                  best_target = target;
                }
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

      bool const ranged_unit = is_ranged_mode(attacker_atk);

      if (ranged_unit) {
        stop_unit_movement(attacker, attacker_transform);
      }

      face_target(
          attacker_transform,
          best_target->get_component<Engine::Core::TransformComponent>());

      auto *att_u = attacker->get_component<Engine::Core::UnitComponent>();
      bool const should_show_arrow_vfx =
          (att_u != nullptr &&
           att_u->spawn_type != Game::Units::SpawnType::Catapult &&
           att_u->spawn_type != Game::Units::SpawnType::Ballista);

      if (should_show_arrow_vfx &&
          ((attacker_atk == nullptr) ||
           attacker_atk->current_mode !=
               Engine::Core::AttackComponent::CombatMode::Melee)) {
        spawn_arrows(attacker, best_target, arrow_sys);
      }

      if ((attacker_atk != nullptr) &&
          attacker_atk->current_mode ==
              Engine::Core::AttackComponent::CombatMode::Melee) {

        if (is_unit_in_hold_mode(attacker)) {
          attacker->remove_component<Engine::Core::AttackTargetComponent>();
          attacker_atk->in_melee_lock = false;
          attacker_atk->melee_lock_target_id = 0;
          continue;
        }
        initiate_melee_combat(attacker, best_target, attacker_atk, world);
      }

      auto *target_unit =
          best_target->get_component<Engine::Core::UnitComponent>();
      if (target_unit != nullptr) {
        float const tactical_multiplier = calculate_tactical_damage_multiplier(
            attacker, best_target, attacker_unit, target_unit);
        damage =
            static_cast<int>(static_cast<float>(damage) * tactical_multiplier);
        apply_high_ground_defense_bonuses(attacker, best_target, target_unit,
                                          damage);
      }

      deal_damage(world, best_target, damage, attacker->get_id());
      *t_accum = 0.0F;

      auto *guard_mode =
          attacker->get_component<Engine::Core::GuardModeComponent>();
      if ((guard_mode != nullptr) && guard_mode->active) {
        guard_mode->returning_to_guard_position = false;
      }
    } else {
      if ((attack_target == nullptr) &&
          attacker->has_component<Engine::Core::AttackTargetComponent>()) {
        attacker->remove_component<Engine::Core::AttackTargetComponent>();
      }

      auto *guard_mode =
          attacker->get_component<Engine::Core::GuardModeComponent>();
      if ((guard_mode != nullptr) && guard_mode->active &&
          !guard_mode->returning_to_guard_position) {
        float guard_x = guard_mode->guard_position_x;
        float guard_z = guard_mode->guard_position_z;

        if (guard_mode->guarded_entity_id != 0) {
          auto *guarded_entity =
              world->get_entity(guard_mode->guarded_entity_id);
          if (guarded_entity != nullptr) {
            auto *guarded_transform =
                guarded_entity
                    ->get_component<Engine::Core::TransformComponent>();
            if (guarded_transform != nullptr) {
              guard_x = guarded_transform->position.x;
              guard_z = guarded_transform->position.z;
            }
          }
        }

        float const dx = guard_x - attacker_transform->position.x;
        float const dz = guard_z - attacker_transform->position.z;
        float const dist_sq = dx * dx + dz * dz;

        float const kReturnThresholdSq =
            Engine::Core::Defaults::kGuardReturnThreshold *
            Engine::Core::Defaults::kGuardReturnThreshold;
        if (dist_sq > kReturnThresholdSq) {
          guard_mode->returning_to_guard_position = true;
          CommandService::MoveOptions options;
          options.clear_attack_intent = true;
          options.allow_direct_fallback = true;
          std::vector<Engine::Core::EntityID> const unit_ids = {
              attacker->get_id()};
          std::vector<QVector3D> const move_targets = {
              QVector3D(guard_x, 0.0F, guard_z)};
          CommandService::move_units(*world, unit_ids, move_targets, options);
        }
      }
    }
  }
}

} // namespace Game::Systems::Combat
