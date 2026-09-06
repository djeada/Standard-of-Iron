#include "app/core/frame_ui_coordinator.h"

#include <algorithm>
#include <vector>

#include "app/economy/production_manager.h"
#include "app/input/cursor_manager.h"
#include "app/orders/command_controller.h"
#include "app/orders/order_markers.h"
#include "app/orders/rts_action_model.h"
#include "game/accessibility/motion_settings.h"
#include "game/core/component_core.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/systems/arrow_system.h"
#include "game/systems/healing_beam_system.h"
#include "game/systems/nation_registry.h"
#include "game/systems/projectile_system.h"
#include "game/systems/selection_system.h"
#include "game/units/spawn_type.h"
#include "render/entity/combat_dust_renderer.h"
#include "render/entity/commander_aura_renderer.h"
#include "render/entity/healer_aura_renderer.h"
#include "render/entity/healing_beam_renderer.h"
#include "render/entity/healing_waves_renderer.h"
#include "render/geom/arrow.h"
#include "render/geom/attack_target_markers.h"
#include "render/geom/formation_arrow.h"
#include "render/geom/interaction_target_markers.h"
#include "render/geom/patrol_flags.h"
#include "render/geom/projectile_renderer.h"
#include "render/geom/range_rings.h"
#include "render/geom/target_focus_rings.h"
#include "render/scene_renderer.h"

namespace {

auto state_enabled(const QVariantMap& states, const QString& action_id) -> bool {
  return states.value(action_id).toMap().value(QStringLiteral("enabled")).toBool();
}

auto has_selected_local_barracks(Engine::Core::World* world,
                                 int local_owner_id) -> bool {
  if (world == nullptr) {
    return false;
  }

  auto* selection_system = world->get_system<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    return false;
  }

  for (const auto entity_id : selection_system->get_selected_units()) {
    auto* entity = world->get_entity(entity_id);
    if (entity == nullptr) {
      continue;
    }

    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if ((unit != nullptr) && unit->owner_id == local_owner_id &&
        unit->spawn_type == Game::Units::SpawnType::Barracks) {
      return true;
    }
  }

  return false;
}

void render_attack_targeting(
    Render::GL::Renderer* renderer,
    Render::GL::ResourceManager* resources,
    const Game::Systems::AttackTargetingHighlights& targeting) {
  if (targeting.markers.empty()) {
    return;
  }

  std::vector<Render::GL::AttackTargetMarkerVisual> visuals;
  visuals.reserve(targeting.markers.size());
  for (const auto& marker : targeting.markers) {
    visuals.push_back(
        {.position = QVector3D(marker.world_x, marker.world_y, marker.world_z),
         .radius = marker.radius,
         .hovered = marker.hovered,
         .attackable = marker.attackable});
  }
  Render::GL::render_attack_target_markers(renderer, resources, visuals);
}

auto glyph_for_interaction(const Game::Systems::InteractionTargetMarker& marker)
    -> Game::Systems::ActivityKind {
  using Game::Systems::ActivityKind;
  switch (marker.action) {
  case Game::Systems::InteractionAction::Deliver:
    return ActivityKind::Deliver;
  case Game::Systems::InteractionAction::Repair:
    return ActivityKind::Repair;
  case Game::Systems::InteractionAction::Harvest:
    return ActivityKind::HarvestGrain;
  case Game::Systems::InteractionAction::Slaughter:
    return ActivityKind::SlaughterSheep;
  case Game::Systems::InteractionAction::Gather:
    break;
  case Game::Systems::InteractionAction::None:
    return ActivityKind::Idle;
  }

  if (Game::Map::is_boulder_world_prop_type(marker.prop_type)) {
    return ActivityKind::MineStone;
  }
  if (Game::Map::is_iron_ore_world_prop_type(marker.prop_type)) {
    return ActivityKind::MineIron;
  }
  return ActivityKind::ChopWood;
}

void render_interaction_targeting(
    Render::GL::Renderer* renderer,
    Render::GL::ResourceManager* resources,
    const Game::Systems::InteractionTargetingHighlights& targeting) {
  if (targeting.markers.empty()) {
    return;
  }

  std::vector<Render::GL::InteractionTargetMarkerVisual> visuals;
  visuals.reserve(targeting.markers.size());
  for (const auto& marker : targeting.markers) {
    visuals.push_back(
        {.position = QVector3D(marker.world_x, marker.world_y, marker.world_z),
         .radius = marker.radius,
         .hovered = marker.hovered,
         .action = glyph_for_interaction(marker)});
  }
  Render::GL::render_interaction_target_markers(renderer, resources, visuals);
}

constexpr float k_order_marker_base_radius = 0.9F;
constexpr float k_order_marker_shrink = 0.35F;
constexpr float k_order_marker_thickness = 0.12F;
constexpr float k_order_marker_lift = 0.04F;
constexpr float k_order_marker_alpha = 0.95F;

constexpr float k_objective_marker_radius = 2.4F;
constexpr float k_objective_marker_thickness = 0.22F;
constexpr float k_objective_marker_lift = 0.05F;
constexpr QVector3D k_objective_marker_color{0.95F, 0.76F, 0.36F};

auto order_marker_pattern(App::Core::OrderKind kind,
                          bool rejected) -> Game::Accessibility::TeamPattern {
  if (rejected) {
    return Game::Accessibility::TeamPattern::Dashed;
  }
  switch (kind) {
  case App::Core::OrderKind::Attack:
    return Game::Accessibility::TeamPattern::DoubleRing;
  case App::Core::OrderKind::Guard:
  case App::Core::OrderKind::Hold:
    return Game::Accessibility::TeamPattern::Notched;
  case App::Core::OrderKind::Patrol:
    return Game::Accessibility::TeamPattern::Dotted;
  default:
    return Game::Accessibility::TeamPattern::Solid;
  }
}

auto focus_visual_role(Game::Systems::TargetFocusRole role)
    -> Render::GL::TargetFocusVisualRole {
  switch (role) {
  case Game::Systems::TargetFocusRole::Inspected:
    return Render::GL::TargetFocusVisualRole::Inspected;
  case Game::Systems::TargetFocusRole::LockedTarget:
    return Render::GL::TargetFocusVisualRole::LockedTarget;
  case Game::Systems::TargetFocusRole::IncomingAttacker:
    return Render::GL::TargetFocusVisualRole::IncomingAttacker;
  }
  return Render::GL::TargetFocusVisualRole::LockedTarget;
}

void render_target_focus(Render::GL::Renderer* renderer,
                         Render::GL::ResourceManager* resources,
                         const std::vector<Game::Systems::TargetFocusMarker>& markers) {
  if (markers.empty()) {
    return;
  }
  std::vector<Render::GL::TargetFocusVisual> visuals;
  visuals.reserve(markers.size());
  for (const auto& marker : markers) {
    visuals.push_back(
        {.position = QVector3D(marker.world_x, marker.world_y, marker.world_z),
         .radius = marker.radius,
         .role = focus_visual_role(marker.role),
         .hostile = marker.hostile,
         .is_building = marker.is_building,
         .weight = marker.weight});
  }
  Render::GL::TargetFocusStyle style;
  style.reduced_motion = Game::Accessibility::MotionSettings::reduced_motion();
  Render::GL::render_target_focus_rings(renderer, resources, visuals, style);
}

void render_objective_marker(Render::GL::Renderer* renderer,
                             const QVector3D& position) {
  if (renderer == nullptr) {
    return;
  }
  const auto& terrain = renderer->world_view().terrain_or_empty();
  const QVector3D grounded =
      terrain.resolve_surface_world_position(position.x(), position.z(), 0.0F, 0.0F);

  Render::GL::GroundMarkerCmd ring;
  ring.center =
      QVector3D(position.x(), grounded.y() + k_objective_marker_lift, position.z());
  ring.outer_radius = k_objective_marker_radius;
  ring.thickness = k_objective_marker_thickness;
  ring.color = k_objective_marker_color;
  ring.alpha = 0.85F;
  ring.focused = true;
  renderer->ground_marker(ring);

  Render::GL::GroundMarkerCmd inner = ring;
  inner.outer_radius = k_objective_marker_radius * 0.45F;
  inner.thickness = k_objective_marker_thickness * 0.7F;
  inner.alpha = 0.6F;
  inner.focused = false;
  renderer->ground_marker(inner);
}

void render_order_markers(Render::GL::Renderer* renderer,
                          const std::vector<App::Core::OrderMarker>& markers) {
  if (renderer == nullptr || markers.empty()) {
    return;
  }
  const auto& terrain = renderer->world_view().terrain_or_empty();
  for (const auto& marker : markers) {
    const float t = std::clamp(marker.progress(), 0.0F, 1.0F);
    const float ease = 1.0F - (1.0F - t) * (1.0F - t);

    Render::GL::GroundMarkerCmd ring;
    const QVector3D grounded = terrain.resolve_surface_world_position(
        marker.position.x(), marker.position.z(), 0.0F, marker.position.y());
    ring.center = QVector3D(
        marker.position.x(), grounded.y() + k_order_marker_lift, marker.position.z());

    const float base_radius = marker.target != 0 ? k_order_marker_base_radius * 1.15F
                                                 : k_order_marker_base_radius;
    ring.outer_radius = marker.rejected
                            ? base_radius * (1.0F + 0.25F * ease)
                            : base_radius * (1.0F - k_order_marker_shrink * ease);
    ring.thickness = k_order_marker_thickness * (1.0F - 0.4F * ease);
    ring.color = App::Core::order_marker_color(marker.kind, marker.rejected);
    ring.alpha = k_order_marker_alpha * (1.0F - ease);
    ring.pattern = order_marker_pattern(marker.kind, marker.rejected);
    ring.focused = !marker.rejected && marker.kind == App::Core::OrderKind::Attack;
    renderer->ground_marker(ring);
  }
}

} // namespace

namespace App::Core::FrameUiCoordinator {

void render_effects(const RenderEffectsContext& context,
                    const std::function<void()>& render_runtime_mode_effects) {
  if (context.renderer == nullptr || context.world == nullptr) {
    return;
  }

  auto* res = context.renderer->resources();
  if (res == nullptr) {
    return;
  }

  if (auto* arrow_system = context.world->get_system<Game::Systems::ArrowSystem>()) {
    Render::GL::render_arrows(context.renderer, res, *arrow_system);
  }

  if (auto* projectile_system =
          context.world->get_system<Game::Systems::ProjectileSystem>()) {
    Render::GL::ProjectileViewContext view;
    view.local_owner_id = context.local_owner_id;
    view.reduced_effects = Game::Accessibility::MotionSettings::reduced_motion();
    Engine::Core::World* world = context.world;
    view.owner_of = [world](std::uint64_t id) -> int {
      auto* entity = world->get_entity(id);
      const auto* unit = entity != nullptr
                             ? entity->get_component<Engine::Core::UnitComponent>()
                             : nullptr;
      return unit != nullptr ? unit->owner_id : 0;
    };
    Render::GL::render_projectiles(context.renderer, res, *projectile_system, &view);
  }

  if (auto* healing_beam_system =
          context.world->get_system<Game::Systems::HealingBeamSystem>()) {
    if (auto* resources = context.renderer->resources()) {
      Render::GL::render_healing_beams(
          context.renderer, resources, *healing_beam_system);
      Render::GL::render_healing_waves(
          context.renderer, resources, *healing_beam_system);
    }
  }

  Render::GL::render_healer_auras(context.renderer, res, context.world);
  Render::GL::render_commander_auras(context.renderer, res, context.world);
  Render::GL::render_combat_dust(context.renderer, res, context.world);
  Render::GL::render_blood_stains(context.renderer, res, context.world);

  if (render_runtime_mode_effects) {
    render_runtime_mode_effects();
  }

  if (context.attack_targeting != nullptr) {
    render_attack_targeting(context.renderer, res, *context.attack_targeting);
  }

  if (context.interaction_targeting != nullptr) {
    render_interaction_targeting(context.renderer, res, *context.interaction_targeting);
  }

  if (context.attack_range_rings != nullptr) {
    Render::GL::render_attack_range_rings(
        context.renderer, res, *context.attack_range_rings);
  }

  if (context.target_focus != nullptr) {
    render_target_focus(context.renderer, res, *context.target_focus);
  }

  if (context.order_markers != nullptr) {
    render_order_markers(context.renderer, *context.order_markers);
  }

  if (context.objective_marker.has_value()) {
    render_objective_marker(context.renderer, *context.objective_marker);
  }

  std::optional<QVector3D> preview_waypoint;
  if (context.command_controller != nullptr &&
      context.command_controller->has_patrol_first_waypoint()) {
    preview_waypoint = context.command_controller->get_patrol_first_waypoint();
  }
  Render::GL::render_patrol_flags(
      context.renderer, res, *context.world, preview_waypoint);

  Render::GL::render_commander_rally_flags(context.renderer,
                                           res,
                                           context.world,
                                           context.local_owner_id,
                                           context.commander_rally_preview_pos);

  if (context.command_controller != nullptr &&
      context.command_controller->formation().is_placing_formation()) {
    auto& session = Game::Session::session_for(*context.world);
    Render::GL::FormationPlacementInfo placement;
    placement.position =
        context.command_controller->formation().get_formation_placement_position();
    placement.position.setY(session.terrain().get_terrain_height(
        placement.position.x(), placement.position.z()));
    placement.angle_degrees =
        context.command_controller->formation().get_formation_facing_degrees();
    placement.aim_distance =
        context.command_controller->formation().get_formation_aim_distance();
    placement.active = true;

    const auto* nation =
        session.nations().get_nation_for_player(context.local_owner_id);
    if (nation != nullptr) {
      switch (nation->id) {
      case Game::Systems::NationID::RomanRepublic:
        placement.accent_color = QVector3D(0.85F, 0.12F, 0.10F);
        break;
      case Game::Systems::NationID::Carthage:
        placement.accent_color = QVector3D(0.62F, 0.08F, 0.78F);
        break;
      case Game::Systems::NationID::IronSepulcher:
        placement.accent_color = QVector3D(0.58F, 0.60F, 0.66F);
        break;
      default:
        break;
      }
    }

    const auto& preview = context.command_controller->formation().formation_preview();
    placement.slot_markers.reserve(preview.slot_list.size());
    for (const auto& slot : preview.slot_list) {
      Render::GL::FormationSlotMarker marker;
      marker.position = slot.world_position;
      marker.position.setY(session.terrain().get_terrain_height(
          slot.world_position.x(), slot.world_position.z()));
      marker.radius = std::max(0.6F, preview.spacing * 0.45F);
      marker.facing_degrees = slot.facing;
      marker.blocked = slot.status == Game::Formation::SlotStatus::Blocked;
      marker.adjusted = slot.status == Game::Formation::SlotStatus::Adjusted;
      placement.slot_markers.push_back(marker);
    }

    Render::GL::render_formation_slot_preview(context.renderer, res, placement);
    Render::GL::render_formation_arrow(context.renderer, res, placement);
  }
}

auto prune_selection_action_context(const SelectionPruneContext& context)
    -> SelectionPruneEffects {
  SelectionPruneEffects effects;

  if (context.production_manager != nullptr &&
      context.production_manager->is_placing_construction()) {
    const QString action_id =
        (context.production_manager->pending_builder_construction_type() ==
         QStringLiteral("collect"))
            ? QStringLiteral("collect")
            : QStringLiteral("build");
    if (!state_enabled(context.hud_action_states, action_id)) {
      effects.cancel_construction = true;
    }
  }

  if (context.command_controller != nullptr &&
      context.command_controller->formation().is_placing_formation() &&
      !state_enabled(context.hud_action_states, QStringLiteral("formation"))) {
    effects.cancel_formation = true;
  }

  if (context.cursor_manager == nullptr) {
    return effects;
  }

  if (context.cursor_manager->mode() == CursorMode::PlaceBarracksRally) {
    if (!has_selected_local_barracks(context.world, context.local_owner_id)) {
      effects.cursor_resolution = CursorResolution::CancelBarracksRallyPlacement;
    }
    return effects;
  }

  const QString cursor_action =
      App::Core::action_id_for_cursor_mode(context.cursor_manager->mode());
  if (cursor_action.isEmpty() ||
      state_enabled(context.hud_action_states, cursor_action)) {
    return effects;
  }

  if (context.cursor_manager->mode() == CursorMode::PlaceCommanderRally) {
    effects.cursor_resolution = CursorResolution::CancelCommanderFlagRally;
    return effects;
  }

  if ((context.cursor_manager->mode() == CursorMode::Patrol) &&
      context.command_controller != nullptr &&
      context.command_controller->has_patrol_first_waypoint()) {
    effects.clear_patrol_first_waypoint = true;
  }

  effects.cursor_resolution = CursorResolution::ResetToNormal;
  return effects;
}

} // namespace App::Core::FrameUiCoordinator
