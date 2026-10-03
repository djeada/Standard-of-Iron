#include "world_render_snapshot.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <functional>
#include <string>
#include <type_traits>

#include "../formation/unit_layout_state.h"
#include "component.h"
#include "core/entity.h"
#include "world.h"

namespace Engine::Core {

namespace {

template <typename ComponentType>
void copy_snapshot_component(const Entity& source, Entity& destination) {
  auto const* component = source.get_component<ComponentType>();
  if (component == nullptr) {
    if (destination.get_component<ComponentType>() != nullptr) {
      destination.remove_component<ComponentType>();
    }
    return;
  }
  static_assert(std::is_copy_constructible_v<ComponentType>);
  static_assert(std::is_copy_assignable_v<ComponentType>);
  if (auto* existing = destination.get_component<ComponentType>()) {
    *existing = *component;
    return;
  }
  destination.add_component<ComponentType>(*component);
}

template <typename ComponentType>
auto copy_revisioned_snapshot_component(const Entity& source,
                                        Entity& destination) -> bool {
  auto const* component = source.get_component<ComponentType>();
  if (component == nullptr) {
    if (destination.get_component<ComponentType>() != nullptr) {
      destination.remove_component<ComponentType>();
    }
    return false;
  }
  if (auto* existing = destination.get_component<ComponentType>()) {
    if (existing->revision == component->revision) {
      return true;
    }
    *existing = *component;
    return false;
  }
  destination.add_component<ComponentType>(*component);
  return false;
}

} // namespace
void copy_authoritative_snapshot_components(const Entity& source, Entity& destination) {
  copy_snapshot_component<TransformComponent>(source, destination);
  copy_snapshot_component<UnitComponent>(source, destination);
  copy_snapshot_component<RenderableComponent>(source, destination);
  copy_snapshot_component<MovementComponent>(source, destination);
  copy_snapshot_component<MovementFactsComponent>(source, destination);
  copy_snapshot_component<BuildingComponent>(source, destination);
  copy_snapshot_component<PendingRemovalComponent>(source, destination);
  copy_snapshot_component<AttackComponent>(source, destination);
  copy_snapshot_component<SiegeTowerComponent>(source, destination);
  copy_snapshot_component<WallWalkerComponent>(source, destination);
  copy_snapshot_component<WallSegmentComponent>(source, destination);
  copy_snapshot_component<AttackTargetComponent>(source, destination);
  copy_snapshot_component<ForestCoverComponent>(source, destination);
  copy_snapshot_component<CombatStateComponent>(source, destination);
  copy_snapshot_component<FormationContactComponent>(source, destination);
  copy_snapshot_component<WildlifeComponent>(source, destination);
  copy_snapshot_component<BuilderProductionComponent>(source, destination);
  copy_snapshot_component<ProductionComponent>(source, destination);
  copy_snapshot_component<CaptureComponent>(source, destination);
  copy_snapshot_component<CommanderComponent>(source, destination);
  copy_snapshot_component<CommanderAuraBuffComponent>(source, destination);
  copy_snapshot_component<RpgCommanderActionComponent>(source, destination);
  copy_snapshot_component<RpgCommanderTargetComponent>(source, destination);
  copy_snapshot_component<HealerComponent>(source, destination);
  copy_snapshot_component<PatrolComponent>(source, destination);
  copy_snapshot_component<GuardModeComponent>(source, destination);
  copy_snapshot_component<HoldModeComponent>(source, destination);
  copy_snapshot_component<FormationModeComponent>(source, destination);
  copy_snapshot_component<UnitLayoutStateComponent>(source, destination);
  copy_snapshot_component<UnitTraversalLayoutStateComponent>(source, destination);
  copy_snapshot_component<SpearBraceComponent>(source, destination);
  copy_snapshot_component<StaminaComponent>(source, destination);
  copy_snapshot_component<MoraleComponent>(source, destination);
  copy_snapshot_component<BurningStatusComponent>(source, destination);
  copy_snapshot_component<StaggerComponent>(source, destination);
  copy_snapshot_component<PoiseComponent>(source, destination);
  copy_snapshot_component<CombatLaunchComponent>(source, destination);
  copy_snapshot_component<HitFeedbackComponent>(source, destination);
  copy_snapshot_component<WallConstructionSiteComponent>(source, destination);
  copy_snapshot_component<DismantleSiteComponent>(source, destination);
  copy_snapshot_component<FirePatchComponent>(source, destination);
  copy_snapshot_component<StructureFireComponent>(source, destination);
  copy_snapshot_component<ElephantComponent>(source, destination);
  copy_snapshot_component<ElephantStompImpactComponent>(source, destination);
  copy_snapshot_component<CatapultLoadingComponent>(source, destination);
  copy_snapshot_component<GateComponent>(source, destination);
  copy_snapshot_component<ResourceCarryComponent>(source, destination);
  copy_snapshot_component<FarmComponent>(source, destination);
}

auto copy_presentation_snapshot_components(const Entity& source,
                                           Entity& destination) -> std::uint64_t {
  std::uint64_t skipped = 0;
  copy_snapshot_component<MotionPresentationComponent>(source, destination);
  copy_snapshot_component<CommanderPresentationSampleComponent>(source, destination);
  copy_snapshot_component<CreaturePresentationComponent>(source, destination);
  skipped += copy_revisioned_snapshot_component<FormationRosterPresentationComponent>(
                 source, destination)
                 ? 1U
                 : 0U;
  skipped += copy_revisioned_snapshot_component<FormationPresentationComponent>(
                 source, destination)
                 ? 1U
                 : 0U;
  skipped += copy_revisioned_snapshot_component<FormationHitPresentationComponent>(
                 source, destination)
                 ? 1U
                 : 0U;
  copy_snapshot_component<SoldierCasualtyAnimationComponent>(source, destination);
  copy_snapshot_component<DeathAnimationComponent>(source, destination);
  copy_snapshot_component<ConstructionPreviewComponent>(source, destination);
  copy_snapshot_component<StructureDamagePresentationComponent>(source, destination);
  copy_snapshot_component<StructureRepairPresentationComponent>(source, destination);
  copy_snapshot_component<RpgContactPresentationComponent>(source, destination);
  copy_snapshot_component<CommanderSignaturePresentationComponent>(source, destination);
  copy_snapshot_component<BloodStainComponent>(source, destination);
  copy_snapshot_component<StockpileComponent>(source, destination);
  copy_snapshot_component<ProductionCompletionComponent>(source, destination);
  return skipped;
}

auto copy_render_components(const Entity& source,
                            Entity& destination) -> std::uint64_t {
  copy_authoritative_snapshot_components(source, destination);
  const std::uint64_t skipped =
      copy_presentation_snapshot_components(source, destination);

  auto const* traversal = source.get_component<UnitTraversalLayoutStateComponent>();
  auto const* formation = source.get_component<FormationPresentationComponent>();
  auto* transform = destination.get_component<TransformComponent>();
  bool const formation_handles_squeeze =
      formation != nullptr && formation->soldiers.size() > 1U;
  if (transform != nullptr && traversal != nullptr && traversal->active &&
      !formation_handles_squeeze) {
    transform->scale.x *= std::clamp(traversal->lateral_scale, 0.1F, 1.0F);
  }
  return skipped;
}

namespace {

void render_hash_combine(std::uint64_t& seed, std::uint64_t value) {
  seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U);
}

void render_hash_float(std::uint64_t& seed, float value) {
  render_hash_combine(seed, std::bit_cast<std::uint32_t>(value));
}

} // namespace

auto render_entity_is_stable(const Entity& entity) -> bool {
  auto const* movement = entity.get_component<MovementComponent>();
  auto const* motion = entity.get_component<MotionPresentationComponent>();
  auto const* traversal = entity.get_component<UnitTraversalLayoutStateComponent>();
  auto const* creature = entity.get_component<CreaturePresentationComponent>();
  auto const* target = entity.get_component<AttackTargetComponent>();
  auto const* combat = entity.get_component<CombatStateComponent>();
  auto const* contact = entity.get_component<FormationContactComponent>();
  auto const* casualties = entity.get_component<SoldierCasualtyAnimationComponent>();
  bool const moving = (movement != nullptr &&
                       (movement->get_has_target() || movement->has_waypoints() ||
                        std::hypot(movement->get_vx(), movement->get_vz()) > 0.001F)) ||
                      (motion != nullptr && motion->has_locomotion()) ||
                      (traversal != nullptr && traversal->active);
  bool const active_creature =
      creature != nullptr &&
      (creature->combat_active || creature->is_constructing || creature->is_healing ||
       creature->is_dying || creature->is_dead || creature->jump_active ||
       creature->flag_rally_planting || creature->authored_action_running);
  bool const active_combat =
      (target != nullptr && target->target_id != 0) ||
      (combat != nullptr && combat->animation_state != CombatAnimationState::Idle) ||
      (contact != nullptr && (contact->in_contact || !contact->fronts.empty())) ||
      (casualties != nullptr && !casualties->entries.empty());

  auto const* wildlife = entity.get_component<WildlifeComponent>();
  bool const wildlife_reacting =
      wildlife != nullptr &&
      (wildlife->dazed_timer > 0.0F || wildlife->flinch_timer > 0.0F ||
       wildlife->bite_timer > 0.0F);
  bool const transient =
      wildlife_reacting || entity.has_component<PendingRemovalComponent>() ||
      entity.has_component<DeathAnimationComponent>() ||
      entity.has_component<BuilderProductionComponent>() ||
      entity.has_component<ProductionComponent>() ||
      entity.has_component<CaptureComponent>() ||
      entity.has_component<CommanderComponent>() ||
      entity.has_component<CommanderAuraBuffComponent>() ||
      entity.has_component<RpgCommanderActionComponent>() ||
      entity.has_component<RpgCommanderTargetComponent>() ||
      entity.has_component<HealerComponent>() ||
      entity.has_component<BurningStatusComponent>() ||
      entity.has_component<StaggerComponent>() ||
      entity.has_component<HitFeedbackComponent>() ||
      entity.has_component<FormationHitPresentationComponent>() ||
      entity.has_component<ConstructionPreviewComponent>() ||
      entity.has_component<WallConstructionSiteComponent>() ||
      entity.has_component<DismantleSiteComponent>() ||
      entity.has_component<WallWalkerComponent>() ||
      entity.has_component<StructureDamagePresentationComponent>() ||
      entity.has_component<StructureRepairPresentationComponent>() ||
      entity.has_component<ProductionCompletionComponent>() ||
      entity.has_component<RpgContactPresentationComponent>() ||
      entity.has_component<CommanderSignaturePresentationComponent>() ||
      entity.has_component<BloodStainComponent>() ||
      entity.has_component<FirePatchComponent>() ||
      entity.has_component<StructureFireComponent>() ||
      entity.has_component<ElephantStompImpactComponent>() ||
      entity.has_component<CatapultLoadingComponent>();
  return !moving && !active_creature && !active_combat && !transient;
}

auto render_entity_signature(const Entity& entity) -> std::uint64_t {
  std::uint64_t signature = 0xcbf29ce484222325ULL;
  if (auto const* transform = entity.get_component<TransformComponent>()) {
    render_hash_float(signature, transform->position.x);
    render_hash_float(signature, transform->position.y);
    render_hash_float(signature, transform->position.z);
    render_hash_float(signature, transform->rotation.x);
    render_hash_float(signature, transform->rotation.y);
    render_hash_float(signature, transform->rotation.z);
    render_hash_float(signature, transform->scale.x);
    render_hash_float(signature, transform->scale.y);
    render_hash_float(signature, transform->scale.z);
  }
  if (auto const* unit = entity.get_component<UnitComponent>()) {
    render_hash_combine(signature, static_cast<std::uint64_t>(unit->health));
    render_hash_combine(signature, static_cast<std::uint64_t>(unit->max_health));
    render_hash_float(signature, unit->speed);
    render_hash_combine(signature, static_cast<std::uint64_t>(unit->spawn_type));
    render_hash_combine(signature, static_cast<std::uint64_t>(unit->owner_id));
    render_hash_combine(signature, static_cast<std::uint64_t>(unit->nation_id));
    render_hash_combine(
        signature,
        static_cast<std::uint64_t>(unit->render_individuals_per_unit_override));
    render_hash_combine(signature, static_cast<std::uint64_t>(unit->squad_strength));
  }
  if (auto const* farm = entity.get_component<FarmComponent>()) {
    render_hash_combine(signature, static_cast<std::uint64_t>(farm->growth_stage()));
  }
  if (auto const* wildlife = entity.get_component<WildlifeComponent>()) {
    render_hash_combine(signature,
                        (wildlife->dazed_timer > 0.0F ? 1U : 0U) |
                            (wildlife->flinch_timer > 0.0F ? 2U : 0U) |
                            (wildlife->bite_timer > 0.0F ? 4U : 0U));
  }
  if (auto const* cover = entity.get_component<ForestCoverComponent>()) {
    render_hash_combine(signature, cover->concealed ? 1U : 0U);
    render_hash_combine(signature, cover->seen_by);
  }
  if (auto const* stockpile = entity.get_component<StockpileComponent>()) {

    render_hash_float(signature, stockpile->wood_fill);
    render_hash_float(signature, stockpile->stone_fill);
    render_hash_float(signature, stockpile->iron_fill);
    render_hash_float(signature, stockpile->food_fill);
    render_hash_float(signature, stockpile->deposit_flash);
  }
  if (auto const* tower = entity.get_component<SiegeTowerComponent>()) {
    render_hash_float(signature, tower->ramp);
    render_hash_combine(signature, static_cast<std::uint64_t>(tower->state));
  }
  if (auto const* walker = entity.get_component<WallWalkerComponent>()) {
    render_hash_float(signature, walker->elevation);
    render_hash_combine(signature, static_cast<std::uint64_t>(walker->phase));
  }
  if (auto const* wall = entity.get_component<WallSegmentComponent>()) {
    render_hash_combine(signature,
                        static_cast<std::uint64_t>(wall->connection_mask) |
                            (static_cast<std::uint64_t>(wall->inner_x + 1) << 8U) |
                            (static_cast<std::uint64_t>(wall->inner_z + 1) << 12U) |
                            (wall->has_stair ? (1ULL << 16U) : 0ULL));
  }
  if (auto const* gate = entity.get_component<GateComponent>()) {

    render_hash_float(signature, gate->open_amount);
    render_hash_combine(signature, static_cast<std::uint64_t>(gate->state));
    render_hash_combine(signature, static_cast<std::uint64_t>(gate->manual_mode));
  }
  if (auto const* renderable = entity.get_component<RenderableComponent>()) {
    render_hash_combine(signature, std::hash<std::string>{}(renderable->renderer_id));
    render_hash_combine(signature, renderable->visible ? 1U : 0U);
  }
  if (auto const* layout = entity.get_component<UnitLayoutStateComponent>()) {
    render_hash_combine(signature, layout->state);
    render_hash_combine(signature, layout->phase);
    render_hash_combine(signature, layout->layout_id);
    render_hash_float(signature, layout->transition_progress);
  }
  if (auto const* traversal =
          entity.get_component<UnitTraversalLayoutStateComponent>()) {
    render_hash_combine(signature, traversal->route_id);
    render_hash_combine(signature, traversal->portal_id);
    render_hash_combine(signature, static_cast<std::uint64_t>(traversal->mode));
    render_hash_combine(signature, static_cast<std::uint64_t>(traversal->target_mode));
    render_hash_combine(signature, traversal->current_files);
    render_hash_combine(signature, traversal->target_files);
    render_hash_float(signature, traversal->transition_curve);
    render_hash_float(signature, traversal->lateral_scale);
    render_hash_combine(signature, traversal->active ? 1U : 0U);
  }
  if (auto const* morale = entity.get_component<MoraleComponent>()) {
    render_hash_float(signature, morale->morale);
    render_hash_float(signature, morale->commander_aura_bonus);
    render_hash_combine(signature, morale->wavering ? 1U : 0U);
    render_hash_combine(signature, morale->routing ? 1U : 0U);
  }
  if (auto const* creature = entity.get_component<CreaturePresentationComponent>()) {
    render_hash_combine(signature, creature->revision);
  }
  if (auto const* formation = entity.get_component<FormationPresentationComponent>()) {
    render_hash_combine(signature, formation->revision);
  }
  if (auto const* roster =
          entity.get_component<FormationRosterPresentationComponent>()) {
    render_hash_combine(signature, roster->revision);
  }
  render_hash_combine(signature, entity.has_component<UnitComponent>() ? 1U : 0U);
  render_hash_combine(signature, entity.has_component<BuildingComponent>() ? 1U : 0U);
  return signature;
}

} // namespace Engine::Core
