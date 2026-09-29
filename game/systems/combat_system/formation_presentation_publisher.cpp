#include "formation_presentation_publisher.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

#include "../../core/component.h"
#include "../../core/world.h"
#include "../../units/spawn_type.h"
#include "combat_utils.h"
#include "formation_combat_roles.h"
#include "formation_local_frame.h"
#include "formation_slot_directive.h"
#include "formation_structure_facade.h"

namespace Game::Systems::Combat {
namespace {

using Soldier = Engine::Core::FormationSoldierPresentation;

void collect_foreign_fighters(Engine::Core::World& world, ForeignSoldierGrid& grid) {
  grid.clear();
  for (auto [entity_ref, entity_unit] :
       world.entity_view<Engine::Core::UnitComponent>()) {
    auto const* published = world.try_get<Engine::Core::FormationPresentationComponent>(
        entity_ref.get_id());
    if (published == nullptr || !published->melee_ordered) {
      continue;
    }
    auto const* published_attack =
        world.try_get<Engine::Core::AttackComponent>(entity_ref.get_id());
    auto const* published_contact =
        world.try_get<Engine::Core::FormationContactComponent>(entity_ref.get_id());
    bool const fighting =
        (published_attack != nullptr && published_attack->in_melee_lock) ||
        (published_contact != nullptr &&
         std::any_of(published_contact->fronts.begin(),
                     published_contact->fronts.end(),
                     [](auto const& front) { return front.in_contact; }));
    if (!fighting) {
      continue;
    }
    for (auto const& soldier : published->soldiers) {
      if (soldier.alive && soldier.world_motion_valid) {
        grid.insert({entity_ref.get_id(),
                     soldier.slot_index,
                     soldier.world_x,
                     soldier.world_z});
      }
    }
  }
}

void tick_formation_hit(Engine::Core::Entity& entity, float delta_time) {
  auto* hit = entity.get_component<Engine::Core::FormationHitPresentationComponent>();
  if (hit == nullptr) {
    return;
  }
  hit->remaining = std::max(0.0F, hit->remaining - std::max(0.0F, delta_time));
  if (hit->remaining <= 0.0F) {
    entity.remove_component<Engine::Core::FormationHitPresentationComponent>();
  }
}

auto collect_damage_carriers(Engine::Core::Entity& entity,
                             const Engine::Core::FormationContactComponent* contact)
    -> std::vector<DamageCarrier> {
  std::vector<DamageCarrier> carriers;
  if (contact == nullptr) {
    return carriers;
  }
  carriers.reserve(contact->fronts.size());
  for (auto const& front : contact->fronts) {
    if (!front.outgoing || !front.in_contact) {
      continue;
    }
    auto const carrier = FormationCombat::select_damage_engagement_pair(
        entity, front.opponent_id, front.engagement_pairs);
    carriers.push_back({&front,
                        carrier.has_value()
                            ? std::optional<std::uint16_t>{carrier->attacker_slot}
                            : std::nullopt});
  }
  return carriers;
}

void advance_squad_reform(Engine::Core::SquadReformComponent* reform,
                          const Engine::Core::TransformComponent* actor,
                          float delta_time) {
  if (reform == nullptr) {
    return;
  }
  reform->remaining_seconds -= std::max(0.0F, delta_time);
  if (reform->remaining_seconds <= 0.0F || actor == nullptr) {
    reform->soldiers.clear();
  }
}

void finish_squad_reform(Engine::Core::World& world,
                         Engine::Core::Entity& entity,
                         Engine::Core::SquadReformComponent* reform,
                         const FormationCombat::FormationLayout& layout) {
  if (reform == nullptr) {
    return;
  }
  std::erase_if(reform->soldiers, [&layout](auto const& entry) {
    return find_live_slot(layout, entry.slot_index) == nullptr;
  });
  if (reform->soldiers.empty()) {
    world.remove<Engine::Core::SquadReformComponent>(entity.get_id());
  }
}

struct FrontFacts {
  Engine::Core::EntityID outgoing_target{0U};
  bool outgoing_melee{false};
  bool melee_ordered{false};
  bool in_melee_contact{false};
  Engine::Core::EntityID display_target{0U};
};

auto read_front_facts(const Engine::Core::AttackComponent* attack,
                      const Engine::Core::AttackTargetComponent* target_ref,
                      const Engine::Core::FormationContactComponent* contact)
    -> FrontFacts {
  FrontFacts facts;
  facts.outgoing_target = target_ref != nullptr ? target_ref->target_id : 0U;
  facts.outgoing_melee = is_melee_mode(attack) && facts.outgoing_target != 0U;
  bool const incoming_contact =
      contact != nullptr && std::any_of(contact->fronts.begin(),
                                        contact->fronts.end(),
                                        [](auto const& front) {
                                          return !front.outgoing && front.in_contact;
                                        });
  facts.melee_ordered = facts.outgoing_melee || incoming_contact;
  facts.in_melee_contact = (attack != nullptr && attack->in_melee_lock) ||
                           (contact != nullptr && std::any_of(contact->fronts.begin(),
                                                              contact->fronts.end(),
                                                              [](auto const& front) {
                                                                return front.in_contact;
                                                              }));

  facts.display_target = facts.outgoing_target;
  if (facts.display_target == 0U && contact != nullptr) {
    auto const first_contact =
        std::find_if(contact->fronts.begin(),
                     contact->fronts.end(),
                     [](auto const& front) { return front.in_contact; });
    if (first_contact != contact->fronts.end()) {
      facts.display_target = first_contact->opponent_id;
    }
  }
  return facts;
}

auto squad_speed_of(Engine::Core::World& world, Engine::Core::EntityID id) -> float {
  if (auto const* movement = world.try_get<Engine::Core::MovementComponent>(id)) {
    return std::hypot(movement->get_vx(), movement->get_vz());
  }
  return 0.0F;
}

auto passability_of(Engine::Core::World& world,
                    Engine::Core::EntityID id) -> Pathfinding::Passability {
  auto const* movement = world.try_get<Engine::Core::MovementComponent>(id);
  return movement != nullptr && movement->get_can_enter_forest()
             ? Pathfinding::Passability::Light
             : Pathfinding::Passability::Heavy;
}

void commit_presentation(const EntityFrame& frame, bool soldiers_changed) {
  auto& presentation = frame.presentation;
  auto const& layout = frame.layout;
  bool const changed =
      presentation.formation_seed != layout.seed || presentation.rows != layout.rows ||
      presentation.cols != layout.cols || presentation.spacing != layout.spacing ||
      presentation.target_id != frame.display_target ||
      presentation.target_alive != frame.target_alive ||
      presentation.melee_ordered != frame.melee_ordered ||
      presentation.allow_full_body_hit_reaction || soldiers_changed;
  presentation.formation_seed = layout.seed;
  presentation.rows = static_cast<std::uint16_t>(layout.rows);
  presentation.cols = static_cast<std::uint16_t>(layout.cols);
  presentation.spacing = layout.spacing;
  presentation.target_id = frame.display_target;
  presentation.target_alive = frame.target_alive;
  presentation.melee_ordered = frame.melee_ordered;
  presentation.allow_full_body_hit_reaction = layout.live_slots.size() == 1U;
  presentation.combat_motion_time = frame.combat_motion_time;
  if (frame.actor != nullptr) {
    presentation.motion_root_x = frame.actor->position.x;
    presentation.motion_root_z = frame.actor->position.z;
    presentation.motion_root_yaw = frame.actor->rotation.y;
    presentation.motion_root_valid = true;
  }
  if (changed) {
    ++presentation.revision;
  }
}

void publish_entity(Engine::Core::World& world,
                    Engine::Core::Entity& entity,
                    const Engine::Core::UnitComponent& unit,
                    LayoutCache& layouts,
                    ForeignFighters& foreign,
                    float delta_time) {
  if (!FormationCombat::has_formation_slots(entity)) {
    return;
  }
  tick_formation_hit(entity, delta_time);

  auto const& layout = layouts.for_entity(entity);
  auto* presentation =
      Engine::Core::get_or_add_component<Engine::Core::FormationPresentationComponent>(
          &entity);
  if (presentation == nullptr) {
    return;
  }

  auto const id = entity.get_id();
  auto const* attack = world.try_get<Engine::Core::AttackComponent>(id);
  auto const* target_ref = world.try_get<Engine::Core::AttackTargetComponent>(id);
  auto const* contact = world.try_get<Engine::Core::FormationContactComponent>(id);
  auto const* traversal =
      world.try_get<Engine::Core::UnitTraversalLayoutStateComponent>(id);
  auto const facts = read_front_facts(attack, target_ref, contact);

  EntityFrame frame{.world = world,
                    .entity = entity,
                    .unit = unit,
                    .layout = layout,
                    .layouts = layouts,
                    .foreign = foreign,
                    .presentation = *presentation};
  frame.contact = contact;
  frame.traversal = traversal;
  frame.delta_time = delta_time;
  frame.melee_ordered = facts.melee_ordered;
  frame.in_melee_contact = facts.in_melee_contact;
  frame.display_target = facts.display_target;
  frame.target_alive = opponent_alive(world, facts.display_target);
  frame.combat_motion_time = facts.melee_ordered ? presentation->combat_motion_time +
                                                       std::max(0.0F, delta_time)
                                                 : presentation->combat_motion_time;
  frame.actor = world.try_get<Engine::Core::TransformComponent>(id);
  frame.display_opponent = world.get_entity(facts.display_target);
  frame.structure = build_structure_facade(entity,
                                           layout,
                                           frame.actor,
                                           traversal,
                                           frame.display_opponent,
                                           facts.outgoing_melee);
  frame.damage_carriers = collect_damage_carriers(entity, contact);

  frame.reform = world.try_get<Engine::Core::SquadReformComponent>(id);
  advance_squad_reform(frame.reform, frame.actor, delta_time);
  frame.squad_speed = squad_speed_of(world, id);
  frame.passability = passability_of(world, id);
  frame.mounted = Game::Units::is_cavalry(unit.spawn_type);
  frame.frame_sign = traversal != nullptr && traversal->about_faced ? -1.0F : 1.0F;

  bool const soldiers_changed = publish_soldiers(frame);
  finish_squad_reform(world, entity, frame.reform, layout);
  commit_presentation(frame, soldiers_changed);
}

} // namespace

void publish_formation_presentation(Engine::Core::World& world, float delta_time) {
  thread_local LayoutCache layouts;
  thread_local ForeignFighters foreign;
  layouts.begin_tick();
  collect_foreign_fighters(world, foreign.grid);
  for (auto [entity_ref, entity_unit] :
       world.entity_view<Engine::Core::UnitComponent>()) {
    publish_entity(world, entity_ref, entity_unit, layouts, foreign, delta_time);
  }
}

} // namespace Game::Systems::Combat
