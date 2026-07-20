#include "formation_contact_processor.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "../../core/component.h"
#include "../../core/world.h"
#include "../formation_combat_geometry.h"

namespace Game::Systems::Combat {
namespace {

void clear_contact(Engine::Core::FormationContactComponent& contact) {
  if (contact.target_id == 0 && !contact.in_contact &&
      contact.engaged_soldier_indices.empty() && contact.engagement_pairs.empty()) {
    return;
  }
  contact.target_id = 0;
  contact.surface_gap = 0.0F;
  contact.in_contact = false;
  contact.engaged_soldier_indices.clear();
  contact.engagement_pairs.clear();
  ++contact.revision;
}

void publish_formation_presentation(Engine::Core::World& world) {
  for (auto* entity : world.get_entities_with<Engine::Core::UnitComponent>()) {
    if (entity == nullptr || !FormationCombat::has_formation_slots(*entity)) {
      continue;
    }
    auto const layout = FormationCombat::resolve_layout(*entity);
    auto* presentation = Engine::Core::get_or_add_component<
        Engine::Core::FormationPresentationComponent>(entity);
    if (presentation == nullptr) {
      continue;
    }

    auto const* attack = entity->get_component<Engine::Core::AttackComponent>();
    auto const* target_ref =
        entity->get_component<Engine::Core::AttackTargetComponent>();
    auto const* contact =
        entity->get_component<Engine::Core::FormationContactComponent>();
    Engine::Core::EntityID const target_id =
        target_ref != nullptr ? target_ref->target_id : 0U;
    auto* target = target_id != 0U ? world.get_entity(target_id) : nullptr;
    auto const* target_unit = target != nullptr
                                  ? target->get_component<Engine::Core::UnitComponent>()
                                  : nullptr;
    bool const target_alive =
        target_unit != nullptr && target_unit->health > 0 &&
        !target->has_component<Engine::Core::PendingRemovalComponent>();
    bool const melee_ordered =
        attack != nullptr && target_id != 0U &&
        attack->current_mode == Engine::Core::AttackComponent::CombatMode::Melee;
    auto const* actor_transform =
        entity->get_component<Engine::Core::TransformComponent>();
    auto const* target_transform =
        target != nullptr ? target->get_component<Engine::Core::TransformComponent>()
                          : nullptr;
    bool const owns_contact_facing =
        contact != nullptr && contact->in_contact && contact->target_id == target_id &&
        actor_transform != nullptr && target_transform != nullptr;
    float desired_contact_local_yaw = 0.0F;
    if (owns_contact_facing) {
      float const dx = target_transform->position.x - actor_transform->position.x;
      float const dz = target_transform->position.z - actor_transform->position.z;
      float const target_world_yaw =
          std::atan2(dx, dz) * 180.0F / std::numbers::pi_v<float>;
      desired_contact_local_yaw =
          std::remainder(target_world_yaw - actor_transform->rotation.y, 360.0F);
    }

    std::vector<bool> live(static_cast<std::size_t>(layout.total_count), false);
    for (auto const& slot : layout.live_slots) {
      if (slot.index < live.size()) {
        live[slot.index] = true;
      }
    }
    std::vector<Engine::Core::FormationSoldierPresentation> directives;
    directives.reserve(layout.all_slots.size());
    for (auto const& slot : layout.all_slots) {
      Engine::Core::FormationSoldierPresentation directive;
      directive.slot_index = slot.index;
      directive.row = slot.row;
      directive.col = slot.col;
      directive.local_x = slot.local_x;
      directive.local_z = slot.local_z;
      directive.local_yaw = slot.local_yaw;
      if (owns_contact_facing) {
        float previous_yaw = slot.local_yaw;
        if (presentation->target_id == target_id &&
            slot.index < presentation->soldiers.size()) {
          previous_yaw = presentation->soldiers[slot.index].local_yaw;
        }
        float const yaw_delta =
            std::remainder(desired_contact_local_yaw - previous_yaw, 360.0F);
        constexpr float k_max_contact_turn_per_tick = 4.0F;
        directive.local_yaw = previous_yaw + std::clamp(yaw_delta,
                                                        -k_max_contact_turn_per_tick,
                                                        k_max_contact_turn_per_tick);
      }
      directive.alive = slot.index < live.size() && live[slot.index];
      directive.action = melee_ordered
                             ? Engine::Core::FormationSoldierAction::MeleeReady
                             : Engine::Core::FormationSoldierAction::FollowUnit;
      if (contact != nullptr && contact->in_contact &&
          contact->target_id == target_id) {
        auto const pair = std::find_if(contact->engagement_pairs.begin(),
                                       contact->engagement_pairs.end(),
                                       [&](auto const& candidate) {
                                         return candidate.attacker_slot == slot.index;
                                       });
        if (pair != contact->engagement_pairs.end()) {
          directive.action = Engine::Core::FormationSoldierAction::MeleeEngaged;
          directive.target_slot = pair->target_slot;
          directive.engagement_surface_gap = pair->surface_gap;
        }
      }
      if (directive.action == Engine::Core::FormationSoldierAction::MeleeReady &&
          presentation->target_id == target_id &&
          slot.index < presentation->soldiers.size()) {
        auto const& previous = presentation->soldiers[slot.index];
        if (previous.action == Engine::Core::FormationSoldierAction::MeleeEngaged ||
            previous.action ==
                Engine::Core::FormationSoldierAction::MeleeFollowThrough) {
          directive.action = Engine::Core::FormationSoldierAction::MeleeFollowThrough;
          directive.target_slot = previous.target_slot;
          directive.engagement_surface_gap = previous.engagement_surface_gap;
        }
      }
      directives.push_back(directive);
    }

    bool const changed = presentation->formation_seed != layout.seed ||
                         presentation->rows != layout.rows ||
                         presentation->cols != layout.cols ||
                         presentation->spacing != layout.spacing ||
                         presentation->target_id != target_id ||
                         presentation->target_alive != target_alive ||
                         presentation->melee_ordered != melee_ordered ||
                         presentation->allow_full_body_hit_reaction ||
                         presentation->soldiers != directives;
    presentation->formation_seed = layout.seed;
    presentation->rows = static_cast<std::uint16_t>(layout.rows);
    presentation->cols = static_cast<std::uint16_t>(layout.cols);
    presentation->spacing = layout.spacing;
    presentation->target_id = target_id;
    presentation->target_alive = target_alive;
    presentation->melee_ordered = melee_ordered;

    presentation->allow_full_body_hit_reaction = layout.live_slots.size() == 1U;
    presentation->soldiers = std::move(directives);
    if (changed) {
      ++presentation->revision;
    }
  }
}

} // namespace

void update_formation_contacts(Engine::Core::World* world) {
  if (world == nullptr) {
    return;
  }

  for (auto* attacker :
       world->get_entities_with<Engine::Core::FormationContactComponent>()) {
    auto* attack = attacker->get_component<Engine::Core::AttackComponent>();
    auto* target_ref = attacker->get_component<Engine::Core::AttackTargetComponent>();
    if (attack == nullptr || target_ref == nullptr || target_ref->target_id == 0 ||
        attack->current_mode != Engine::Core::AttackComponent::CombatMode::Melee) {
      clear_contact(
          *attacker->get_component<Engine::Core::FormationContactComponent>());
    }
  }

  for (auto* attacker :
       world->get_entities_with<Engine::Core::AttackTargetComponent>()) {
    auto* attack = attacker->get_component<Engine::Core::AttackComponent>();
    auto* target_ref = attacker->get_component<Engine::Core::AttackTargetComponent>();
    if (attack == nullptr || target_ref == nullptr || target_ref->target_id == 0 ||
        attack->current_mode != Engine::Core::AttackComponent::CombatMode::Melee) {
      continue;
    }
    auto* target = world->get_entity(target_ref->target_id);
    auto* attacker_unit = attacker->get_component<Engine::Core::UnitComponent>();
    auto* target_unit = target != nullptr
                            ? target->get_component<Engine::Core::UnitComponent>()
                            : nullptr;
    if (target == nullptr || attacker_unit == nullptr || target_unit == nullptr ||
        attacker_unit->health <= 0 || target_unit->health <= 0 ||
        !FormationCombat::has_formation_slots(*attacker) &&
            !FormationCombat::has_formation_slots(*target)) {
      if (auto* existing =
              attacker->get_component<Engine::Core::FormationContactComponent>()) {
        clear_contact(*existing);
      }
      continue;
    }

    auto const geometry = FormationCombat::contact_geometry(*attacker, *target);
    auto* contact =
        Engine::Core::get_or_add_component<Engine::Core::FormationContactComponent>(
            attacker);
    if (contact == nullptr) {
      continue;
    }
    bool const same_target = contact->target_id == target->get_id();
    bool const in_contact =
        FormationCombat::contact_is_active(*attacker, *target, geometry);
    auto pairs = in_contact ? FormationCombat::engagement_pairs(*attacker, *target)
                            : std::vector<Engine::Core::FormationEngagementPair>{};
    std::vector<std::uint16_t> engaged;
    engaged.reserve(pairs.size());
    for (auto const& pair : pairs) {
      engaged.push_back(pair.attacker_slot);
    }
    auto same_pair_ids = [](auto const& lhs, auto const& rhs) {
      if (lhs.size() != rhs.size()) {
        return false;
      }
      for (std::size_t index = 0; index < lhs.size(); ++index) {
        if (lhs[index].attacker_slot != rhs[index].attacker_slot ||
            lhs[index].target_slot != rhs[index].target_slot) {
          return false;
        }
      }
      return true;
    };
    bool const changed = !same_target || contact->in_contact != in_contact ||
                         contact->engaged_soldier_indices != engaged ||
                         !same_pair_ids(contact->engagement_pairs, pairs);
    contact->target_id = target->get_id();
    contact->surface_gap = geometry.surface_gap;
    contact->in_contact = in_contact;
    contact->engaged_soldier_indices = std::move(engaged);
    contact->engagement_pairs = std::move(pairs);
    if (changed) {
      ++contact->revision;
    }
  }

  publish_formation_presentation(*world);
}

} // namespace Game::Systems::Combat
