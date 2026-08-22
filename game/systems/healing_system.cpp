#include "healing_system.h"

#include <QDebug>
#include <qvectornd.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "../core/component.h"
#include "../core/event_manager.h"
#include "../core/system_context.h"
#include "../core/world.h"
#include "../core/world_spatial_index.h"
#include "combat_rules.h"
#include "healing_beam_system.h"
#include "healing_colors.h"
#include "healing_rules.h"
#include "nation_id.h"

namespace Game::Systems {

namespace {

auto matches_heal_affinity(Engine::Core::SystemContext& context,
                           Engine::Core::EntityID target_id,
                           Engine::Core::HealerComponent::TargetAffinity affinity)
    -> bool {
  bool const target_is_undead = context.has<Engine::Core::UndeadComponent>(target_id);
  switch (affinity) {
  case Engine::Core::HealerComponent::TargetAffinity::UndeadAllies:
    return target_is_undead;
  case Engine::Core::HealerComponent::TargetAffinity::LivingAllies:
  default:
    return !target_is_undead;
  }
}

} // namespace

void HealingSystem::run(Engine::Core::SystemContext& context) {
  process_healing(context);
}

void HealingSystem::process_healing(Engine::Core::SystemContext& context) {
  const float delta_time = context.delta_time();
  auto* healing_beam_system = context.world().get_system<HealingBeamSystem>();

  auto& index = context.spatial_index();
  index.refresh(context.world());
  std::vector<Engine::Core::EntityID> candidates;

  for (auto [healer, healer_comp_ref, healer_unit_ref, healer_transform_ref] :
       context.entity_view<Engine::Core::HealerComponent,
                           Engine::Core::UnitComponent,
                           Engine::Core::TransformComponent>()) {
    const Engine::Core::EntityID healer_id = healer.get_id();
    if (context.has<Engine::Core::PendingRemovalComponent>(healer_id)) {
      continue;
    }

    auto* healer_comp = &healer_comp_ref;
    const auto* healer_unit = &healer_unit_ref;
    auto* healer_transform = &healer_transform_ref;

    if (healer_unit->health <= 0) {
      continue;
    }

    auto const* healer_attack =
        context.try_get<Engine::Core::AttackComponent>(healer_id);
    if (healer_attack != nullptr && healer_attack->in_melee_lock &&
        Game::Systems::CombatRules::participates_in_rts_melee_lock(&healer)) {
      healer_comp->is_healing_active = false;
      continue;
    }

    healer_comp->time_since_last_heal += delta_time;

    if (healer_comp->time_since_last_heal < healer_comp->healing_cooldown) {
      continue;
    }

    bool healed_any = false;
    candidates.clear();
    index.for_each_in_radius(
        healer_transform->position.x,
        healer_transform->position.z,
        healer_comp->healing_range,
        [&](const Engine::Core::WorldSpatialIndex::Entry& entry) {
          if (entry.owner_id != healer_unit->owner_id ||
              entry.is(Engine::Core::WorldSpatialIndex::k_pending_removal)) {
            return;
          }
          candidates.push_back(entry.id);
        });

    std::sort(candidates.begin(), candidates.end());

    for (const Engine::Core::EntityID candidate_id : candidates) {
      auto* target = context.world().get_entity(candidate_id);
      if (target == nullptr ||
          context.has<Engine::Core::PendingRemovalComponent>(candidate_id)) {
        continue;
      }

      auto* target_unit = context.try_get<Engine::Core::UnitComponent>(candidate_id);
      auto* target_transform =
          context.try_get<Engine::Core::TransformComponent>(candidate_id);

      if ((target_unit == nullptr) || (target_transform == nullptr)) {
        continue;
      }

      if (!HealingRules::can_receive_healing(*target)) {
        continue;
      }

      if (target_unit->owner_id != healer_unit->owner_id) {
        continue;
      }
      if (!matches_heal_affinity(context, candidate_id, healer_comp->target_affinity)) {
        continue;
      }

      float const dx = target_transform->position.x - healer_transform->position.x;
      float const dz = target_transform->position.z - healer_transform->position.z;
      float const dist = std::sqrt(dx * dx + dz * dz);

      if (dist <= healer_comp->healing_range) {
        target_unit->health =
            std::min(target_unit->health + healer_comp->healing_amount,
                     HealingRules::maximum_recoverable_health(*target));
        Engine::Core::EventManager::instance().publish(
            Engine::Core::AudioCueEvent("combat.heal"));

        healer_comp->healing_target_x = target_transform->position.x;
        healer_comp->healing_target_z = target_transform->position.z;

        if (dist > 0.1F) {
          float const target_yaw =
              std::atan2(dx, dz) * 180.0F / std::numbers::pi_v<float>;
          healer_transform->desired_yaw = target_yaw;
          healer_transform->has_desired_yaw = true;
        }

        if (healing_beam_system != nullptr) {
          QVector3D const healer_pos(healer_transform->position.x,
                                     healer_transform->position.y + 1.2F,
                                     healer_transform->position.z);
          QVector3D const target_pos(target_transform->position.x,
                                     target_transform->position.y + 0.8F,
                                     target_transform->position.z);

          QVector3D const heal_color = get_healing_color(healer_unit->nation_id);

          healing_beam_system->spawn_beam(healer_pos, target_pos, heal_color, 0.7F);
        }

        healed_any = true;
      }
    }

    if (healed_any) {
      healer_comp->time_since_last_heal = 0.0F;

      healer_comp->is_healing_active = true;
    } else {
      healer_comp->is_healing_active = false;
    }
  }
}

auto HealingSystem::access() const -> Engine::Core::SystemAccess {
  using namespace Engine::Core;
  return SystemAccess::declare(
      Reads<AttackComponent,
            AttackTargetComponent,
            CommanderComponent,
            UndeadComponent,
            RpgHealthComponent,
            FormationRosterPresentationComponent,
            PendingRemovalComponent>{},
      Writes<UnitComponent, TransformComponent, HealerComponent>{});
}

} // namespace Game::Systems
