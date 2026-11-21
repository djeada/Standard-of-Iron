#include "formation_presentation_publisher.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

#include "../../core/component.h"
#include "../../core/world.h"
#include "../../map/scatter/world_prop_clearance_index.h"
#include "../../units/spawn_type.h"
#include "../building_collision_registry.h"
#include "combat_utils.h"
#include "formation_combat_roles.h"
#include "formation_local_frame.h"
#include "formation_slot_directive.h"
#include "formation_structure_facade.h"
#include "formation_surround.h"
#include "game/util/planar_math.h"
#include "target_rules.h"

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
    return;
  }
  ++hit->revision;
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

auto soldier_reach(const EntityFrame& frame) -> float {
  float reach_sq = 0.0F;
  for (auto const& slot : frame.layout.all_slots) {
    reach_sq =
        std::max(reach_sq, slot.local_x * slot.local_x + slot.local_z * slot.local_z);
  }
  for (auto const& soldier : frame.presentation.soldiers) {
    if (soldier.alive && soldier.world_motion_valid) {
      float const dx = soldier.world_x - frame.actor->position.x;
      float const dz = soldier.world_z - frame.actor->position.z;
      reach_sq = std::max(reach_sq, dx * dx + dz * dz);
    }
  }
  return std::sqrt(reach_sq);
}

void join_touching_bodies(std::vector<SolidBody>& bodies) {
  constexpr float k_touching = 1.0e-3F;
  auto const same = [](float a, float b) {
    return std::abs(a - b) <= k_touching;
  };
  auto const join_runs =
      [&](auto lane_low, auto lane_high, auto run_low, auto run_high) {
        std::sort(
            bodies.begin(), bodies.end(), [&](const SolidBody& a, const SolidBody& b) {
              if (!same(a.*lane_low, b.*lane_low)) {
                return a.*lane_low < b.*lane_low;
              }
              if (!same(a.*lane_high, b.*lane_high)) {
                return a.*lane_high < b.*lane_high;
              }
              return a.*run_low < b.*run_low;
            });
        std::size_t kept = 0;
        for (std::size_t index = 1; index < bodies.size(); ++index) {
          SolidBody& run = bodies[kept];
          SolidBody const& next = bodies[index];
          if (same(run.*lane_low, next.*lane_low) &&
              same(run.*lane_high, next.*lane_high) &&
              next.*run_low <= run.*run_high + k_touching) {
            run.*run_high = std::max(run.*run_high, next.*run_high);
          } else {
            bodies[++kept] = next;
          }
        }
        bodies.resize(kept + 1U);
      };
  if (bodies.size() < 2U) {
    return;
  }
  join_runs(&SolidBody::min_z, &SolidBody::max_z, &SolidBody::min_x, &SolidBody::max_x);
  join_runs(&SolidBody::min_x, &SolidBody::max_x, &SolidBody::min_z, &SolidBody::max_z);
}

auto buildings_around(const EntityFrame& frame,
                      float reach) -> const std::vector<SolidBody>* {
  if (frame.wall_walker != nullptr || frame.structure.attacks_structure ||
      frame.unit.spawn_type == Game::Units::SpawnType::Builder) {
    return nullptr;
  }
  thread_local std::vector<SolidBody> bodies;
  bodies.clear();
  Pathfinding::building_collisions().for_each_solid_body_near(
      frame.actor->position.x,
      frame.actor->position.z,
      reach * 2.0F + k_soldier_building_awareness,
      [](float center_x, float center_z, float half_x, float half_z) {
        bodies.push_back({center_x - half_x,
                          center_x + half_x,
                          center_z - half_z,
                          center_z + half_z});
      });
  join_touching_bodies(bodies);
  return bodies.empty() ? nullptr : &bodies;
}

void survey_surroundings(EntityFrame& frame,
                         const Game::Map::WorldPropClearanceIndex& props) {
  if (frame.actor == nullptr) {
    return;
  }
  float const reach = soldier_reach(frame);
  frame.solid_bodies = buildings_around(frame, reach);
  float const prop_reach = reach + frame.layout.body_radius + k_soldier_prop_awareness;
  if (!props.anything_near(
          frame.actor->position.x, frame.actor->position.z, prop_reach)) {
    return;
  }
  thread_local Game::Map::WorldPropClearanceIndex nearby;
  props.gather_near(
      frame.actor->position.x, frame.actor->position.z, prop_reach, nearby);
  frame.props = nearby.empty() ? nullptr : &nearby;
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

auto single_body_opponent(Engine::Core::World& world,
                          Engine::Core::EntityID id) -> Engine::Core::Entity* {
  auto* opponent = world.get_entity(id);
  if (opponent == nullptr || is_building(opponent) ||
      FormationCombat::has_formation_slots(*opponent) ||
      world.has<Engine::Core::ElephantComponent>(id) ||
      world.has<Engine::Core::WildlifeComponent>(id)) {
    return nullptr;
  }
  return opponent;
}

auto surrounds_its_opponent(Engine::Core::World& world,
                            Engine::Core::Entity& entity,
                            const FrontFacts& facts) -> Engine::Core::Entity* {
  auto const id = entity.get_id();
  if (!facts.melee_ordered || facts.display_target == 0U ||
      world.has<Engine::Core::WallWalkerComponent>(id) ||
      world.has<Engine::Core::RaftRiderComponent>(id)) {
    return nullptr;
  }
  auto* opponent = single_body_opponent(world, facts.display_target);
  auto const* actor = world.try_get<Engine::Core::TransformComponent>(id);
  auto const* target =
      opponent != nullptr
          ? world.try_get<Engine::Core::TransformComponent>(opponent->get_id())
          : nullptr;
  if (actor == nullptr || target == nullptr ||
      Game::Systems::planar_length(actor->position.x - target->position.x,
                                   actor->position.z - target->position.z) >
          k_surround_reach) {
    return nullptr;
  }
  return opponent;
}

void enlist_surrounders(Engine::Core::World& world,
                        LayoutCache& layouts,
                        SurroundBoard& board) {
  board.clear();
  for (auto [entity_ref, entity_unit] :
       world.entity_view<Engine::Core::UnitComponent>()) {
    if (entity_unit.health <= 0 || !FormationCombat::has_formation_slots(entity_ref)) {
      continue;
    }
    auto const id = entity_ref.get_id();
    auto const facts =
        read_front_facts(world.try_get<Engine::Core::AttackComponent>(id),
                         world.try_get<Engine::Core::AttackTargetComponent>(id),
                         world.try_get<Engine::Core::FormationContactComponent>(id));
    auto* opponent = surrounds_its_opponent(world, entity_ref, facts);
    if (opponent == nullptr) {
      continue;
    }
    auto const& layout = layouts.for_entity(entity_ref);
    if (layout.live_slots.size() < 2U) {
      continue;
    }
    auto const& actor = *world.try_get<Engine::Core::TransformComponent>(id);
    auto const& target =
        *world.try_get<Engine::Core::TransformComponent>(opponent->get_id());
    board.enlist(
        opponent->get_id(),
        id,
        surround_approach(
            target.position.x, target.position.z, actor.position.x, actor.position.z),
        static_cast<int>(layout.live_slots.size()));
  }
  board.allot();
}

auto squad_speed_of(Engine::Core::World& world, Engine::Core::EntityID id) -> float {
  if (auto const* movement = world.try_get<Engine::Core::MovementComponent>(id)) {
    return Game::Systems::planar_length(movement->get_vx(), movement->get_vz());
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
                    const SurroundBoard& board,
                    const Game::Map::WorldPropClearanceIndex& props,
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
  frame.wall_walker = world.try_get<Engine::Core::WallWalkerComponent>(id);
  frame.raft_rider = world.try_get<Engine::Core::RaftRiderComponent>(id);
  advance_squad_reform(frame.reform, frame.actor, delta_time);
  frame.squad_speed = squad_speed_of(world, id);
  frame.passability = passability_of(world, id);
  frame.mounted = Game::Units::is_cavalry(unit.spawn_type);
  survey_surroundings(frame, props);
  frame.frame_sign = FormationCombat::layout_frame_sign(
      layout, traversal != nullptr && traversal->about_faced);
  thread_local std::vector<SurroundSpot> surround_spots;
  if (auto const sector = board.sector_of(facts.display_target, id);
      sector.has_value() && frame.actor != nullptr &&
      frame.display_opponent != nullptr) {
    auto const& target = *world.try_get<Engine::Core::TransformComponent>(
        frame.display_opponent->get_id());
    auto const& opponent_layout = layouts.for_entity(*frame.display_opponent);
    float const weapon_reach = attack != nullptr ? attack->melee_range : 1.5F;
    plan_surround(
        {.actor = *frame.actor,
         .layout = layout,
         .previous = presentation->soldiers,
         .sector = *sector,
         .target_x = target.position.x,
         .target_z = target.position.z,
         .first_ring_radius =
             layout.body_radius + opponent_layout.body_radius + weapon_reach * 0.35F,
         .frame_sign = frame.frame_sign},
        surround_spots);
    frame.surround = &surround_spots;
  }

  bool const soldiers_changed = publish_soldiers(frame);
  finish_squad_reform(world, entity, frame.reform, layout);
  commit_presentation(frame, soldiers_changed);
}

} // namespace

void publish_formation_presentation(Engine::Core::World& world, float delta_time) {
  thread_local LayoutCache layouts;
  thread_local ForeignFighters foreign;
  thread_local SurroundBoard board;
  layouts.begin_tick();
  collect_foreign_fighters(world, foreign.grid);
  enlist_surrounders(world, layouts, board);
  auto const props = Game::Map::shared_world_prop_clearance_index();
  for (auto [entity_ref, entity_unit] :
       world.entity_view<Engine::Core::UnitComponent>()) {
    publish_entity(
        world, entity_ref, entity_unit, layouts, foreign, board, *props, delta_time);
  }
}

} // namespace Game::Systems::Combat
