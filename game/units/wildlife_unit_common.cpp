#include "wildlife_unit_common.h"

#include "../core/component.h"
#include "../core/entity.h"
#include "../core/ownership_constants.h"
#include "../systems/troop_profile_service.h"

namespace Game::Units {

auto setup_wildlife_unit(Engine::Core::Entity& entity,
                         const SpawnParams& params,
                         Game::Wildlife::Species species,
                         TroopType troop_type) -> WildlifeUnitComponents {
  auto profile = Game::Systems::TroopProfileService::instance().get_profile(
      Game::Systems::NationID::RomanRepublic, troop_type);

  WildlifeUnitComponents out;

  out.transform = entity.add_component<Engine::Core::TransformComponent>();
  out.transform->position = {
      params.position.x(), params.position.y(), params.position.z()};
  float const scale = profile.visuals.render_scale;
  out.transform->scale = {scale, scale, scale};
  out.transform->rotation.y = params.rotation_y;

  out.renderable = entity.add_component<Engine::Core::RenderableComponent>();
  out.renderable->visible = true;
  out.renderable->renderer_id = profile.visuals.renderer_id;

  out.unit = entity.add_component<Engine::Core::UnitComponent>();
  out.unit->spawn_type = spawn_typeFromTroopType(troop_type);
  out.unit->health = profile.combat.health;
  out.unit->max_health = profile.combat.max_health;
  out.unit->speed = profile.combat.speed;
  out.unit->owner_id = Game::Core::NEUTRAL_OWNER_ID;
  out.unit->vision_range = profile.combat.vision_range;
  out.unit->uses_nation_formation_profile = false;

  out.movement = entity.add_component<Engine::Core::MovementComponent>();
  if (out.movement != nullptr) {
    out.movement->set_rest_position(params.position.x(), params.position.z());
  }

  if (profile.combat.can_melee) {
    out.attack = entity.add_component<Engine::Core::AttackComponent>();
    if (out.attack != nullptr) {
      out.attack->range = profile.combat.melee_range;
      out.attack->damage = profile.combat.melee_damage;
      out.attack->cooldown = profile.combat.melee_cooldown;
      out.attack->melee_range = profile.combat.melee_range;
      out.attack->melee_damage = profile.combat.melee_damage;
      out.attack->melee_cooldown = profile.combat.melee_cooldown;
      out.attack->preferred_mode = Engine::Core::AttackComponent::CombatMode::Melee;
      out.attack->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;
      out.attack->can_ranged = false;
      out.attack->can_melee = true;
    }
  }

  auto* wildlife = entity.add_component<Engine::Core::WildlifeComponent>();
  wildlife->species = species;
  wildlife->home_x = params.position.x();
  wildlife->home_z = params.position.z();
  wildlife->target_x = params.position.x();
  wildlife->target_z = params.position.z();
  wildlife->behavior = species == Game::Wildlife::Species::Wolf
                           ? Game::Wildlife::Behavior::Roam
                           : Game::Wildlife::Behavior::Graze;

  return out;
}

} // namespace Game::Units
