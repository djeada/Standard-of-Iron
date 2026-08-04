#include "rpg_bow_shot.h"

#include <algorithm>
#include <cmath>
#include <optional>

#include "../../core/world.h"
#include "../../visuals/team_colors.h"
#include "../combat_actions/combat_action_definition.h"
#include "../combat_system/combat_types.h"
#include "../projectile_system.h"
#include "rpg_bow_draw.h"

namespace Game::Systems::RpgCombat {

namespace {

constexpr float k_aimed_arc_scale = 0.10F;

constexpr float k_overshoot_range = 12.0F;

} // namespace

auto loose_aimed_arrow(Engine::Core::World& world,
                       Engine::Core::Entity& commander,
                       const Game::Systems::CombatActions::CombatActionDefinition&
                           definition) -> LoosedArrow {
  LoosedArrow result;

  auto* aim = commander.get_component<Engine::Core::RpgCommanderAimComponent>();
  auto* attack = commander.get_component<Engine::Core::AttackComponent>();
  auto const* unit = commander.get_component<Engine::Core::UnitComponent>();
  auto* projectiles = world.get_system<Game::Systems::ProjectileSystem>();
  if (aim == nullptr || attack == nullptr || unit == nullptr ||
      projectiles == nullptr) {
    return result;
  }

  float const power = aim->shot_power > 0.0F
                          ? aim->shot_power
                          : Engine::Core::RpgCommanderAimComponent::k_min_shot_power;
  float const range = std::max(1.0F, attack->range) + k_overshoot_range;
  auto const sight = scatter_ray(
      commander_aim_ray(commander, *aim), aim->spread_degrees, ++aim->shot_sequence);
  auto const shot = resolve_bow_shot(world, commander, *aim, sight, range);

  int const damage = std::max(
      1,
      static_cast<int>(std::lround(static_cast<float>(attack->damage) *
                                   definition.damage.base_multiplier * power)));

  std::optional<QVector3D> target_origin;
  if (shot.hit_body) {
    auto* target = world.get_entity(shot.hit.entity_id);
    auto const* target_transform =
        target != nullptr ? target->get_component<Engine::Core::TransformComponent>()
                          : nullptr;
    if (target_transform != nullptr) {
      target_origin = QVector3D(target_transform->position.x,
                                target_transform->position.y,
                                target_transform->position.z);
    }
  }

  projectiles->spawn_arrow(shot.start,
                           shot.end,
                           Game::Visuals::team_colorForOwner(unit->owner_id),
                           Game::Systems::Combat::Constants::k_arrow_speed *
                               (1.35F + (1.05F * power)),
                           false,
                           Game::Systems::ProjectileKind::Arrow,
                           shot.hit_body,
                           damage,
                           commander.get_id(),
                           shot.hit.entity_id,
                           0.0F,
                           0.6F,
                           false,
                           Game::Systems::ArrowVisualStyle::Aimed,
                           target_origin,
                           k_aimed_arc_scale,
                           power);

  result.released = true;
  result.target_id = shot.hit.entity_id;
  result.soldier_slot = shot.hit.soldier_slot;
  result.damage = shot.hit_body ? damage : 0;
  result.power = power;
  result.shot = shot;
  return result;
}

} // namespace Game::Systems::RpgCombat
