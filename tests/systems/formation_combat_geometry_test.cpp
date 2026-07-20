#include <algorithm>
#include <gtest/gtest.h>

#include "core/component.h"
#include "core/world.h"
#include "systems/combat_system/attack_processor.h"
#include "systems/combat_system/combat_action_processor.h"
#include "systems/combat_system/combat_utils.h"
#include "systems/combat_system/damage_application.h"
#include "systems/combat_system/formation_contact_processor.h"
#include "systems/formation_combat_geometry.h"
#include "systems/movement_system.h"
#include "systems/troop_profile_service.h"

namespace {

auto add_spearmen(Engine::Core::World& world,
                  int owner,
                  float z,
                  float yaw) -> Engine::Core::Entity* {
  auto* entity = world.create_entity();
  auto* transform = entity->add_component<Engine::Core::TransformComponent>();
  auto* unit =
      entity->add_component<Engine::Core::UnitComponent>(120, 120, 2.5F, 15.0F);
  auto* attack = entity->add_component<Engine::Core::AttackComponent>();
  transform->position = {0.0F, 0.0F, z};
  transform->rotation.y = yaw;
  transform->scale = {0.55F, 0.55F, 0.55F};
  unit->owner_id = owner;
  unit->spawn_type = Game::Units::SpawnType::Spearman;
  unit->render_individuals_per_unit_override = 12;
  attack->can_melee = true;
  attack->can_ranged = false;
  attack->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;
  attack->preferred_mode = Engine::Core::AttackComponent::CombatMode::Melee;
  attack->melee_range = 2.5F;
  return entity;
}

} // namespace

TEST(FormationCombatGeometry, NationTroopProfileOwnsFormationShape) {
  Engine::Core::UnitComponent roman;
  roman.spawn_type = Game::Units::SpawnType::Spearman;
  roman.nation_id = Game::Systems::NationID::RomanRepublic;
  Game::Systems::TroopProfile roman_profile;
  roman_profile.individuals_per_unit = 26;
  roman_profile.max_units_per_row = 7;
  roman_profile.visuals.formation_spacing = 1.05F;
  roman_profile.formation_type = Game::Systems::FormationType::Roman;
  auto const roman_definition =
      Game::Systems::FormationCombat::resolve_definition(roman, roman_profile);

  Engine::Core::UnitComponent carthaginian = roman;
  carthaginian.nation_id = Game::Systems::NationID::Carthage;
  Game::Systems::TroopProfile carthage_profile = roman_profile;
  carthage_profile.individuals_per_unit = 28;
  carthage_profile.formation_type = Game::Systems::FormationType::Carthage;
  auto const carthage_definition = Game::Systems::FormationCombat::resolve_definition(
      carthaginian, carthage_profile);

  EXPECT_EQ(roman_definition.total_count, 26);
  EXPECT_EQ(roman_definition.max_per_row, 7);
  EXPECT_EQ(roman_definition.type, Game::Systems::FormationType::Roman);
  EXPECT_EQ(carthage_definition.total_count, 28);
  EXPECT_EQ(carthage_definition.max_per_row, 7);
  EXPECT_EQ(carthage_definition.type, Game::Systems::FormationType::Carthage);
}

TEST(FormationCombatGeometry, CompactFixtureOverrideKeepsProfileFrontage) {
  Engine::Core::UnitComponent unit;
  unit.spawn_type = Game::Units::SpawnType::Spearman;
  unit.nation_id = Game::Systems::NationID::RomanRepublic;
  unit.render_individuals_per_unit_override = 3;
  Game::Systems::TroopProfile profile;
  profile.individuals_per_unit = 26;
  profile.max_units_per_row = 7;
  profile.visuals.formation_spacing = 1.05F;

  auto const definition =
      Game::Systems::FormationCombat::resolve_definition(unit, profile);

  EXPECT_EQ(definition.total_count, 3);
  EXPECT_EQ(definition.max_per_row, 3);
}

TEST(FormationCombatGeometry, MeleeUsesVisibleFormationContactNotEntityRange) {
  Engine::Core::World world;
  auto* attacker = add_spearmen(world, 1, 0.0F, 0.0F);
  auto* target = add_spearmen(world, 2, 3.0F, 180.0F);

  auto geometry = Game::Systems::FormationCombat::contact_geometry(*attacker, *target);
  ASSERT_TRUE(geometry.uses_formation_slots);
  EXPECT_GT(geometry.surface_gap, 0.5F);
  EXPECT_FALSE(Game::Systems::Combat::is_in_range(attacker, target, 2.5F));

  auto* target_transform = target->get_component<Engine::Core::TransformComponent>();
  target_transform->position.z = geometry.contact_center_distance;
  geometry = Game::Systems::FormationCombat::contact_geometry(*attacker, *target);
  EXPECT_NEAR(geometry.surface_gap, 0.0F, 0.0001F);
  EXPECT_FALSE(Game::Systems::Combat::is_in_range(attacker, target, 2.5F));

  target_transform->position.z = geometry.engagement_center_distance;
  geometry = Game::Systems::FormationCombat::contact_geometry(*attacker, *target);
  EXPECT_TRUE(geometry.formation_overlap_required);
  EXPECT_LT(geometry.center_distance, geometry.contact_center_distance);
  EXPECT_TRUE(Game::Systems::Combat::is_in_range(attacker, target, 2.5F));
}

TEST(FormationCombatGeometry, DeepOverlapEngagesEveryLivingSoldier) {
  Engine::Core::World world;
  auto* attacker = add_spearmen(world, 1, 0.0F, 0.0F);
  auto* target = add_spearmen(world, 2, 3.0F, 180.0F);
  auto const geometry =
      Game::Systems::FormationCombat::contact_geometry(*attacker, *target);
  target->get_component<Engine::Core::TransformComponent>()->position.z =
      geometry.engagement_center_distance;

  auto* target_ref = attacker->add_component<Engine::Core::AttackTargetComponent>();
  target_ref->target_id = target->get_id();
  Game::Systems::Combat::update_formation_contacts(&world);

  auto const* contact =
      attacker->get_component<Engine::Core::FormationContactComponent>();
  ASSERT_NE(contact, nullptr);
  EXPECT_TRUE(contact->in_contact);
  EXPECT_EQ(contact->target_id, target->get_id());
  ASSERT_EQ(contact->engaged_soldier_indices.size(), 12U);
  ASSERT_EQ(contact->engagement_pairs.size(), 12U);
  EXPECT_LE(Game::Systems::FormationCombat::contact_geometry(*attacker, *target)
                .center_distance,
            geometry.engagement_center_distance + 0.001F);

  auto const* presentation =
      attacker->get_component<Engine::Core::FormationPresentationComponent>();
  ASSERT_NE(presentation, nullptr);
  ASSERT_EQ(presentation->soldiers.size(), 12U);
  EXPECT_EQ(presentation->target_id, target->get_id());
  EXPECT_TRUE(presentation->target_alive);
  EXPECT_TRUE(presentation->melee_ordered);
  EXPECT_FALSE(presentation->allow_full_body_hit_reaction);
  for (auto const& directive : presentation->soldiers) {
    auto const pair =
        std::find_if(contact->engagement_pairs.begin(),
                     contact->engagement_pairs.end(),
                     [&](auto const& candidate) {
                       return candidate.attacker_slot == directive.slot_index;
                     });
    EXPECT_EQ(directive.action == Engine::Core::FormationSoldierAction::MeleeEngaged,
              pair != contact->engagement_pairs.end());
    if (pair != contact->engagement_pairs.end()) {
      EXPECT_EQ(directive.target_slot, pair->target_slot);
      EXPECT_EQ(directive.engagement_surface_gap, pair->surface_gap);
    }
  }
}

TEST(FormationCombatGeometry, EngagementWaitsForFormationFootprintsToMerge) {
  Engine::Core::World world;
  auto* attacker = add_spearmen(world, 1, 0.0F, 0.0F);
  auto* target = add_spearmen(world, 2, 6.0F, 180.0F);

  auto const geometry =
      Game::Systems::FormationCombat::contact_geometry(*attacker, *target);
  auto const attacker_layout =
      Game::Systems::FormationCombat::resolve_layout(*attacker);
  auto const target_layout = Game::Systems::FormationCombat::resolve_layout(*target);
  float const minimum_spacing =
      std::min(attacker_layout.spacing, target_layout.spacing);

  EXPECT_LT(geometry.engagement_center_distance,
            geometry.contact_center_distance - minimum_spacing * 0.45F);
  EXPECT_LE(geometry.engagement_center_distance, minimum_spacing * 0.5F);

  target->get_component<Engine::Core::TransformComponent>()->position.z =
      geometry.engagement_center_distance;
  auto const engaged =
      Game::Systems::FormationCombat::contact_geometry(*attacker, *target);
  EXPECT_TRUE(
      Game::Systems::FormationCombat::contact_is_active(*attacker, *target, engaged));
  EXPECT_LE(engaged.center_distance, minimum_spacing * 0.5F);
}

TEST(FormationCombatGeometry, LastLivingFormationBodyPublishesOwnedHitReaction) {
  Engine::Core::World world;
  auto* attacker = add_spearmen(world, 1, 0.0F, 0.0F);
  auto* target = add_spearmen(world, 2, 3.0F, 180.0F);
  auto* attacker_unit = attacker->get_component<Engine::Core::UnitComponent>();
  auto* target_unit = target->get_component<Engine::Core::UnitComponent>();
  attacker_unit->render_individuals_per_unit_override = 2;
  target_unit->render_individuals_per_unit_override = 2;
  attacker_unit->health = attacker_unit->max_health / 2;
  target_unit->health = target_unit->max_health / 2;
  auto const geometry =
      Game::Systems::FormationCombat::contact_geometry(*attacker, *target);
  target->get_component<Engine::Core::TransformComponent>()->position.z =
      geometry.engagement_center_distance;

  auto* target_ref = attacker->add_component<Engine::Core::AttackTargetComponent>();
  target_ref->target_id = target->get_id();
  Game::Systems::Combat::update_formation_contacts(&world);

  auto const* presentation =
      attacker->get_component<Engine::Core::FormationPresentationComponent>();
  ASSERT_NE(presentation, nullptr);
  EXPECT_EQ(std::count_if(presentation->soldiers.begin(),
                          presentation->soldiers.end(),
                          [](auto const& soldier) { return soldier.alive; }),
            1);
  EXPECT_TRUE(presentation->allow_full_body_hit_reaction);
}

TEST(FormationCombatGeometry, PresentationPublishesStableSlotsBeforeContact) {
  Engine::Core::World world;
  auto* attacker = add_spearmen(world, 1, 0.0F, 0.0F);
  auto* target = add_spearmen(world, 2, 6.0F, 180.0F);
  auto* target_ref = attacker->add_component<Engine::Core::AttackTargetComponent>();
  target_ref->target_id = target->get_id();

  Game::Systems::Combat::update_formation_contacts(&world);

  auto const* presentation =
      attacker->get_component<Engine::Core::FormationPresentationComponent>();
  ASSERT_NE(presentation, nullptr);
  ASSERT_EQ(presentation->soldiers.size(), 12U);
  for (std::size_t index = 0; index < presentation->soldiers.size(); ++index) {
    auto const& directive = presentation->soldiers[index];
    EXPECT_EQ(directive.slot_index, index);
    EXPECT_TRUE(directive.alive);
    EXPECT_EQ(directive.action, Engine::Core::FormationSoldierAction::MeleeReady);
  }
}

TEST(FormationCombatGeometry,
     TargetDeathPublishesFollowThroughWithoutNewAttackPermission) {
  Engine::Core::World world;
  auto* attacker = add_spearmen(world, 1, 0.0F, 0.0F);
  auto* target = add_spearmen(world, 2, 3.0F, 180.0F);
  auto const geometry =
      Game::Systems::FormationCombat::contact_geometry(*attacker, *target);
  target->get_component<Engine::Core::TransformComponent>()->position.z =
      geometry.engagement_center_distance;
  auto* target_ref = attacker->add_component<Engine::Core::AttackTargetComponent>();
  target_ref->target_id = target->get_id();
  auto* combat = attacker->add_component<Engine::Core::CombatStateComponent>();
  combat->animation_state = Engine::Core::CombatAnimationState::Strike;

  Game::Systems::Combat::update_formation_contacts(&world);
  auto const* before =
      attacker->get_component<Engine::Core::FormationPresentationComponent>();
  ASSERT_NE(before, nullptr);
  auto const engaged_before = static_cast<std::size_t>(std::count_if(
      before->soldiers.begin(), before->soldiers.end(), [](auto const& soldier) {
        return soldier.action == Engine::Core::FormationSoldierAction::MeleeEngaged;
      }));
  ASSERT_GT(engaged_before, 0U);

  target->get_component<Engine::Core::UnitComponent>()->health = 0;
  Game::Systems::Combat::update_formation_contacts(&world);

  auto const* after =
      attacker->get_component<Engine::Core::FormationPresentationComponent>();
  ASSERT_NE(after, nullptr);
  auto const follow_through = static_cast<std::size_t>(std::count_if(
      after->soldiers.begin(), after->soldiers.end(), [](auto const& soldier) {
        return soldier.action ==
               Engine::Core::FormationSoldierAction::MeleeFollowThrough;
      }));
  EXPECT_EQ(follow_through, engaged_before);
}

TEST(CreaturePresentation, PublishesOneCombatContractForEveryCreaturePipeline) {
  for (auto const spawn_type : {Game::Units::SpawnType::Spearman,
                                Game::Units::SpawnType::MountedKnight,
                                Game::Units::SpawnType::Elephant}) {
    Engine::Core::World world;
    auto* entity = world.create_entity();
    entity->add_component<Engine::Core::TransformComponent>();
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    unit->spawn_type = spawn_type;
    auto* attack = entity->add_component<Engine::Core::AttackComponent>();
    attack->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;
    auto* combat = entity->add_component<Engine::Core::CombatStateComponent>();
    combat->animation_state = Engine::Core::CombatAnimationState::Strike;
    combat->attack_family = Engine::Core::CombatAttackFamily::Spear;
    combat->state_time = 0.15F;
    combat->state_duration = 0.30F;

    Engine::Core::publish_creature_presentation(entity, &world);

    auto const* presentation =
        entity->get_component<Engine::Core::CreaturePresentationComponent>();
    ASSERT_NE(presentation, nullptr);
    EXPECT_TRUE(presentation->snapshot_valid);
    EXPECT_TRUE(presentation->combat_active);
    EXPECT_TRUE(presentation->is_attacking);
    EXPECT_TRUE(presentation->is_melee);
    EXPECT_EQ(presentation->combat_phase, Engine::Core::CombatAnimationState::Strike);
    EXPECT_FLOAT_EQ(presentation->combat_phase_progress, 0.5F);
  }
}

TEST(CreaturePresentation, FormationAggregateHitCannotPublishAFullBodyInterrupt) {
  Engine::Core::World world;
  auto* entity = add_spearmen(world, 1, 0.0F, 0.0F);
  auto* formation =
      entity->add_component<Engine::Core::FormationPresentationComponent>();
  formation->allow_full_body_hit_reaction = false;
  auto* hit = entity->add_component<Engine::Core::HitFeedbackComponent>();
  hit->is_reacting = true;
  hit->reaction_intensity = 1.0F;

  Engine::Core::publish_creature_presentation(entity, &world);

  auto const* presentation =
      entity->get_component<Engine::Core::CreaturePresentationComponent>();
  ASSERT_NE(presentation, nullptr);
  EXPECT_FALSE(presentation->allow_full_body_hit_reaction);
  EXPECT_FALSE(presentation->is_hit_reacting);
  EXPECT_FLOAT_EQ(presentation->hit_reaction_intensity, 0.0F);
}

TEST(FormationCombatGeometry, UnequalFormationsGiveEveryAttackerAnOpponentLane) {
  Engine::Core::World world;
  auto* attacker = add_spearmen(world, 1, 0.0F, 0.0F);
  auto* target = add_spearmen(world, 2, 3.0F, 180.0F);
  target->get_component<Engine::Core::UnitComponent>()->spawn_type =
      Game::Units::SpawnType::Knight;
  target->get_component<Engine::Core::TransformComponent>()->scale = {0.6F, 0.6F, 0.6F};
  target->get_component<Engine::Core::UnitComponent>()->health = 40;
  auto const geometry =
      Game::Systems::FormationCombat::contact_geometry(*attacker, *target);
  target->get_component<Engine::Core::TransformComponent>()->position.z =
      geometry.engagement_center_distance;

  auto* target_ref = attacker->add_component<Engine::Core::AttackTargetComponent>();
  target_ref->target_id = target->get_id();
  Game::Systems::Combat::update_formation_contacts(&world);

  auto const* contact =
      attacker->get_component<Engine::Core::FormationContactComponent>();
  ASSERT_NE(contact, nullptr);
  ASSERT_TRUE(contact->in_contact);
  ASSERT_EQ(contact->engagement_pairs.size(), 12U);
  std::vector<std::uint16_t> target_slots;
  for (auto const& pair : contact->engagement_pairs) {
    EXPECT_EQ(std::count(contact->engaged_soldier_indices.begin(),
                         contact->engaged_soldier_indices.end(),
                         pair.attacker_slot),
              1);
    target_slots.push_back(pair.target_slot);
  }
  std::sort(target_slots.begin(), target_slots.end());
  target_slots.erase(std::unique(target_slots.begin(), target_slots.end()),
                     target_slots.end());
  EXPECT_LT(target_slots.size(), contact->engagement_pairs.size())
      << "opponent lanes may be shared when the target has fewer survivors";
}

TEST(FormationCombatGeometry, EngagementLeasePersistsUntilTargetDeath) {
  Engine::Core::World world;
  auto* attacker = add_spearmen(world, 1, 0.0F, 0.0F);
  auto* target = add_spearmen(world, 2, 3.0F, 180.0F);
  auto const geometry =
      Game::Systems::FormationCombat::contact_geometry(*attacker, *target);
  auto* target_transform = target->get_component<Engine::Core::TransformComponent>();
  target_transform->position.z = geometry.engagement_center_distance;
  auto* target_ref = attacker->add_component<Engine::Core::AttackTargetComponent>();
  target_ref->target_id = target->get_id();
  Game::Systems::Combat::update_formation_contacts(&world);

  target_transform->position.z += 5.0F;
  Game::Systems::Combat::update_formation_contacts(&world);
  auto const* contact =
      attacker->get_component<Engine::Core::FormationContactComponent>();
  ASSERT_NE(contact, nullptr);
  EXPECT_TRUE(contact->in_contact);
  EXPECT_TRUE(Game::Systems::Combat::is_in_range(attacker, target, 2.5F));

  target->get_component<Engine::Core::UnitComponent>()->health = 0;
  Game::Systems::Combat::update_formation_contacts(&world);
  EXPECT_FALSE(contact->in_contact);
  EXPECT_FALSE(Game::Systems::Combat::is_in_range(attacker, target, 2.5F));
}

TEST(FormationCombatGeometry, ReinforcementJoinsAnExistingFormationFightAtItsEdge) {
  Engine::Core::World world;
  auto* primary = add_spearmen(world, 1, -6.0F, 0.0F);
  auto* defender = add_spearmen(world, 2, 0.0F, 180.0F);
  auto* reinforcement = add_spearmen(world, 1, 6.0F, 180.0F);

  auto const primary_geometry =
      Game::Systems::FormationCombat::contact_geometry(*primary, *defender);
  primary->get_component<Engine::Core::TransformComponent>()->position.z =
      -primary_geometry.engagement_center_distance;
  auto const reinforcement_geometry =
      Game::Systems::FormationCombat::contact_geometry(*reinforcement, *defender);
  reinforcement->get_component<Engine::Core::TransformComponent>()->position.z =
      reinforcement_geometry.contact_center_distance -
      reinforcement_geometry.contact_tolerance * 1.5F;

  auto* primary_attack = primary->get_component<Engine::Core::AttackComponent>();
  auto* defender_attack = defender->get_component<Engine::Core::AttackComponent>();
  primary_attack->in_melee_lock = true;
  primary_attack->melee_lock_target_id = defender->get_id();
  defender_attack->in_melee_lock = true;
  defender_attack->melee_lock_target_id = primary->get_id();

  auto const occupied_edge_geometry =
      Game::Systems::FormationCombat::contact_geometry(*reinforcement, *defender);
  EXPECT_GT(occupied_edge_geometry.center_distance,
            occupied_edge_geometry.engagement_center_distance);
  EXPECT_LT(occupied_edge_geometry.surface_gap,
            -occupied_edge_geometry.contact_tolerance);
  EXPECT_TRUE(Game::Systems::FormationCombat::contact_is_active(
      *reinforcement, *defender, occupied_edge_geometry));

  auto* reinforcement_attack =
      reinforcement->get_component<Engine::Core::AttackComponent>();
  reinforcement_attack->melee_cooldown = 0.0F;
  reinforcement_attack->time_since_last = 1.0F;
  auto* target_ref =
      reinforcement->add_component<Engine::Core::AttackTargetComponent>();
  target_ref->target_id = defender->get_id();
  target_ref->should_chase = true;
  auto* movement = reinforcement->add_component<Engine::Core::MovementComponent>();
  movement->engage_manual_move(0.0F, 0.0F);
  movement->set_manual_velocity(0.0F, -2.0F);

  Game::Systems::Combat::process_attacks(
      &world, Game::Systems::Combat::build_combat_query_context(&world), 0.016F);
  Game::Systems::Combat::update_formation_contacts(&world);

  EXPECT_TRUE(reinforcement_attack->in_melee_lock);
  EXPECT_EQ(reinforcement_attack->melee_lock_target_id, defender->get_id());
  EXPECT_FALSE(movement->get_has_target());
  EXPECT_EQ(defender_attack->melee_lock_target_id, primary->get_id())
      << "joining a fight must not steal the defender's existing commitment";
  auto const* contact =
      reinforcement->get_component<Engine::Core::FormationContactComponent>();
  ASSERT_NE(contact, nullptr);
  EXPECT_TRUE(contact->in_contact);
  EXPECT_EQ(contact->engagement_pairs.size(), 12U);
  auto const* presentation =
      reinforcement->get_component<Engine::Core::FormationPresentationComponent>();
  ASSERT_NE(presentation, nullptr);
  EXPECT_TRUE(presentation->melee_ordered);
  EXPECT_TRUE(std::all_of(
      presentation->soldiers.begin(),
      presentation->soldiers.end(),
      [](auto const& soldier) {
        return !soldier.alive ||
               soldier.action == Engine::Core::FormationSoldierAction::MeleeEngaged;
      }));
}

TEST(FormationCombatGeometry, RtsMeleeLockSurvivesSeparationUntilTargetDeath) {
  Engine::Core::World world;
  auto* attacker = add_spearmen(world, 1, 0.0F, 0.0F);
  auto* target = add_spearmen(world, 2, 3.0F, 180.0F);
  auto const geometry =
      Game::Systems::FormationCombat::contact_geometry(*attacker, *target);
  auto* target_transform = target->get_component<Engine::Core::TransformComponent>();
  target_transform->position.z = geometry.engagement_center_distance;
  auto* attack = attacker->get_component<Engine::Core::AttackComponent>();
  attack->time_since_last = attack->melee_cooldown;
  auto* target_ref = attacker->add_component<Engine::Core::AttackTargetComponent>();
  target_ref->target_id = target->get_id();
  target_ref->should_chase = true;

  Game::Systems::Combat::process_attacks(
      &world, Game::Systems::Combat::build_combat_query_context(&world), 0.016F);
  ASSERT_TRUE(attack->in_melee_lock);
  ASSERT_EQ(attack->melee_lock_target_id, target->get_id());

  target_transform->position.z += 8.0F;
  attack->time_since_last = 0.0F;
  Game::Systems::Combat::process_attacks(
      &world, Game::Systems::Combat::build_combat_query_context(&world), 0.016F);
  EXPECT_TRUE(attack->in_melee_lock);
  EXPECT_EQ(attack->melee_lock_target_id, target->get_id());

  target->get_component<Engine::Core::UnitComponent>()->health = 0;
  Game::Systems::Combat::process_attacks(
      &world, Game::Systems::Combat::build_combat_query_context(&world), 0.016F);
  EXPECT_FALSE(attack->in_melee_lock);
  EXPECT_EQ(attack->melee_lock_target_id, 0U);
}

TEST(FormationCombatGeometry, MovementAndFacingAreFrozenDuringRtsMeleeLock) {
  Engine::Core::World world;
  auto* attacker = add_spearmen(world, 1, 0.0F, 12.0F);
  auto* target = add_spearmen(world, 2, 1.0F, 180.0F);
  auto* attack = attacker->get_component<Engine::Core::AttackComponent>();
  attack->in_melee_lock = true;
  attack->melee_lock_target_id = target->get_id();
  auto* movement = attacker->add_component<Engine::Core::MovementComponent>();
  movement->engage_manual_move(5.0F, 5.0F);
  movement->set_manual_velocity(2.0F, 1.0F);
  auto* transform = attacker->get_component<Engine::Core::TransformComponent>();
  QVector3D const initial_position(
      transform->position.x, transform->position.y, transform->position.z);
  float const initial_yaw = transform->rotation.y;
  transform->desired_yaw = 90.0F;
  transform->has_desired_yaw = true;

  Game::Systems::MovementSystem movement_system;
  movement_system.update(&world, 0.25F);

  EXPECT_FALSE(movement->get_has_target());
  EXPECT_FLOAT_EQ(movement->get_vx(), 0.0F);
  EXPECT_FLOAT_EQ(movement->get_vz(), 0.0F);
  EXPECT_FLOAT_EQ(transform->position.x, initial_position.x());
  EXPECT_FLOAT_EQ(transform->position.z, initial_position.z());
  EXPECT_FLOAT_EQ(transform->rotation.y, initial_yaw);
  EXPECT_FALSE(transform->has_desired_yaw);
}

TEST(FormationCombatGeometry, PlayerOrdersCannotCancelALiveMeleeOpponent) {
  Engine::Core::World world;
  auto* attacker = add_spearmen(world, 1, 0.0F, 0.0F);
  auto* target = add_spearmen(world, 2, 1.0F, 180.0F);
  auto* replacement = add_spearmen(world, 2, 4.0F, 180.0F);
  auto* attack = attacker->get_component<Engine::Core::AttackComponent>();
  attack->in_melee_lock = true;
  attack->melee_lock_target_id = target->get_id();
  auto* target_ref = attacker->add_component<Engine::Core::AttackTargetComponent>();
  target_ref->target_id = target->get_id();
  target_ref->should_chase = true;

  Game::Systems::CommandService::move_unit(
      world, attacker->get_id(), QVector3D(20.0F, 0.0F, 20.0F));
  EXPECT_TRUE(attack->in_melee_lock);
  EXPECT_EQ(attack->melee_lock_target_id, target->get_id());
  EXPECT_EQ(attacker->get_component<Engine::Core::AttackTargetComponent>()->target_id,
            target->get_id());
  auto const* movement = attacker->get_component<Engine::Core::MovementComponent>();
  EXPECT_TRUE(movement == nullptr || !movement->get_has_target());

  Game::Systems::CommandService::attack_target(
      world, {attacker->get_id()}, replacement->get_id(), true);
  EXPECT_TRUE(attack->in_melee_lock);
  EXPECT_EQ(attack->melee_lock_target_id, target->get_id());
  EXPECT_EQ(attacker->get_component<Engine::Core::AttackTargetComponent>()->target_id,
            target->get_id());

  target->get_component<Engine::Core::UnitComponent>()->health = 0;
  Game::Systems::CommandService::attack_target(
      world, {attacker->get_id()}, replacement->get_id(), true);
  EXPECT_FALSE(attack->in_melee_lock);
  EXPECT_EQ(attacker->get_component<Engine::Core::AttackTargetComponent>()->target_id,
            replacement->get_id());
}

TEST(FormationCombatGeometry, AuthoredImpactDamagesAtVisibleFormationContact) {
  Engine::Core::World world;
  auto* attacker = add_spearmen(world, 1, 0.0F, 0.0F);
  auto* target = add_spearmen(world, 2, 3.0F, 180.0F);
  auto const geometry =
      Game::Systems::FormationCombat::contact_geometry(*attacker, *target);
  target->get_component<Engine::Core::TransformComponent>()->position.z =
      geometry.engagement_center_distance;

  auto* attack = attacker->get_component<Engine::Core::AttackComponent>();
  attack->melee_damage = 10;
  attack->melee_cooldown = 1.0F;
  attack->time_since_last = 1.0F;
  auto* target_ref = attacker->add_component<Engine::Core::AttackTargetComponent>();
  target_ref->target_id = target->get_id();
  target_ref->should_chase = false;

  Game::Systems::Combat::process_attacks(
      &world, Game::Systems::Combat::build_combat_query_context(&world), 0.016F);
  auto* action = attacker->get_component<Engine::Core::RpgCommanderActionComponent>();
  ASSERT_NE(action, nullptr);
  auto* target_unit = target->get_component<Engine::Core::UnitComponent>();
  int const initial_health = target_unit->health;
  Game::Systems::Combat::process_authored_combat_action(
      &world,
      *attacker,
      attacker->get_component<Engine::Core::CombatStateComponent>(),
      0.45F);

  EXPECT_LT(target_unit->health, initial_health);
  EXPECT_EQ(action->last_hit_target_id, target->get_id());
}

TEST(FormationCombatGeometry, ContactMaintenanceRunsDuringWeaponCooldown) {
  Engine::Core::World world;
  auto* attacker = add_spearmen(world, 1, 0.0F, 0.0F);
  auto* target = add_spearmen(world, 2, 3.0F, 180.0F);
  auto const geometry =
      Game::Systems::FormationCombat::contact_geometry(*attacker, *target);
  target->get_component<Engine::Core::TransformComponent>()->position.z =
      geometry.contact_center_distance + 0.15F;

  auto* attack = attacker->get_component<Engine::Core::AttackComponent>();
  attack->melee_cooldown = 1.0F;
  attack->time_since_last = 0.0F;
  auto* target_ref = attacker->add_component<Engine::Core::AttackTargetComponent>();
  target_ref->target_id = target->get_id();
  target_ref->should_chase = true;

  Game::Systems::Combat::process_attacks(
      &world, Game::Systems::Combat::build_combat_query_context(&world), 0.016F);

  auto const* movement = attacker->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(movement, nullptr);
  EXPECT_TRUE(movement->get_has_target() || movement->has_waypoints());
  EXPECT_EQ(attacker->get_component<Engine::Core::RpgCommanderActionComponent>(),
            nullptr);
}

TEST(FormationCombatGeometry, CasualtyCleanupNeverReusesADeadSoldierIdentity) {
  Engine::Core::World world;
  auto* attacker = add_spearmen(world, 1, 0.0F, 0.0F);
  auto* target = add_spearmen(world, 2, 3.0F, 180.0F);
  auto const initial =
      Game::Systems::FormationCombat::contact_geometry(*attacker, *target);
  target->get_component<Engine::Core::TransformComponent>()->position.z =
      initial.contact_center_distance;

  auto* target_unit = target->get_component<Engine::Core::UnitComponent>();
  auto const full_layout = Game::Systems::FormationCombat::resolve_layout(*target);
  int const retired_front_rank = full_layout.cols;
  int const expected_survivors = full_layout.total_count - retired_front_rank;
  target_unit->health =
      target_unit->max_health * expected_survivors / full_layout.total_count;
  auto* casualties =
      target->add_component<Engine::Core::SoldierCasualtyAnimationComponent>();
  for (std::uint16_t slot = 0; slot < retired_front_rank; ++slot) {
    Engine::Core::SoldierCasualtyAnimationComponent::Entry entry;
    entry.slot_index = slot;
    casualties->entries.push_back(entry);
  }

  auto layout = Game::Systems::FormationCombat::resolve_layout(*target);
  EXPECT_EQ(layout.live_slots.size(), static_cast<std::size_t>(expected_survivors));
  EXPECT_EQ(layout.occupied_slots.size(), 12U);
  auto geometry = Game::Systems::FormationCombat::contact_geometry(*attacker, *target);
  EXPECT_LE(geometry.surface_gap, geometry.contact_tolerance);

  casualties->entries.clear();
  layout = Game::Systems::FormationCombat::resolve_layout(*target);
  EXPECT_EQ(layout.occupied_slots.size(), layout.live_slots.size());
  ASSERT_FALSE(layout.live_slots.empty());
  EXPECT_EQ(layout.live_slots.front().index, retired_front_rank);
  for (auto const& slot : layout.live_slots) {
    EXPECT_GE(slot.index, retired_front_rank);
  }
  geometry = Game::Systems::FormationCombat::contact_geometry(*attacker, *target);
  EXPECT_GT(geometry.surface_gap, geometry.contact_tolerance);
}

TEST(FormationCombatGeometry, FormationHitDoesNotSteerTheSharedRoot) {
  Engine::Core::World world;
  auto* attacker = add_spearmen(world, 1, 0.0F, 0.0F);
  auto* target = add_spearmen(world, 2, 3.0F, 180.0F);
  auto* transform = target->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);
  transform->has_desired_yaw = false;

  Game::Systems::Combat::apply_hit_feedback(target, attacker->get_id(), &world);

  EXPECT_FALSE(transform->has_desired_yaw);
  auto const* feedback = target->get_component<Engine::Core::HitFeedbackComponent>();
  ASSERT_NE(feedback, nullptr);
  EXPECT_TRUE(feedback->is_reacting);
  EXPECT_EQ(feedback->source_attacker_id, attacker->get_id());
}
