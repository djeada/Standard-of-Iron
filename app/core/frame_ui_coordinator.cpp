#include "frame_ui_coordinator.h"

#include <algorithm>
#include <vector>

#include "../controllers/command_controller.h"
#include "../models/cursor_manager.h"
#include "../models/hover_tracker.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/systems/arrow_system.h"
#include "game/systems/civilian_delivery_system.h"
#include "game/systems/healing_beam_system.h"
#include "game/systems/nation_registry.h"
#include "game/systems/projectile_system.h"
#include "game/systems/selection_system.h"
#include "game/units/spawn_type.h"
#include "order_markers.h"
#include "production_manager.h"
#include "render/entity/combat_dust_renderer.h"
#include "render/entity/commander_aura_renderer.h"
#include "render/entity/healer_aura_renderer.h"
#include "render/entity/healing_beam_renderer.h"
#include "render/entity/healing_waves_renderer.h"
#include "render/geom/arrow.h"
#include "render/geom/attack_target_markers.h"
#include "render/geom/formation_arrow.h"
#include "render/geom/patrol_flags.h"
#include "render/geom/projectile_renderer.h"
#include "render/geom/range_rings.h"
#include "render/scene_renderer.h"
#include "rts_action_model.h"

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

constexpr float k_order_marker_base_radius = 0.9F;
constexpr float k_order_marker_shrink = 0.35F;
constexpr float k_order_marker_thickness = 0.12F;
constexpr float k_order_marker_lift = 0.04F;
constexpr float k_order_marker_alpha = 0.95F;

const QVector3D k_order_move_color(0.55F, 0.95F, 0.55F);
const QVector3D k_order_attack_color(1.0F, 0.32F, 0.22F);
const QVector3D k_order_guard_color(0.45F, 0.70F, 1.0F);
const QVector3D k_order_patrol_color(0.35F, 1.0F, 0.55F);
const QVector3D k_order_neutral_color(0.95F, 0.90F, 0.70F);
const QVector3D k_order_rejected_color(0.72F, 0.72F, 0.74F);

auto order_marker_color(App::Core::OrderKind kind, bool rejected) -> QVector3D {
  if (rejected) {
    return k_order_rejected_color;
  }
  switch (kind) {
  case App::Core::OrderKind::Move:
    return k_order_move_color;
  case App::Core::OrderKind::Attack:
    return k_order_attack_color;
  case App::Core::OrderKind::Guard:
  case App::Core::OrderKind::Hold:
    return k_order_guard_color;
  case App::Core::OrderKind::Patrol:
    return k_order_patrol_color;
  default:
    return k_order_neutral_color;
  }
}

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
    ring.color = order_marker_color(marker.kind, marker.rejected);
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
    Render::GL::render_projectiles(context.renderer, res, *projectile_system);
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

  if (context.attack_range_rings != nullptr) {
    Render::GL::render_attack_range_rings(
        context.renderer, res, *context.attack_range_rings);
  }

  if (context.order_markers != nullptr) {
    render_order_markers(context.renderer, *context.order_markers);
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
      context.command_controller->is_placing_formation()) {
    Render::GL::FormationPlacementInfo placement;
    placement.position = context.command_controller->get_formation_placement_position();
    placement.position.setY(Game::Map::TerrainService::instance().get_terrain_height(
        placement.position.x(), placement.position.z()));
    placement.angle_degrees =
        context.command_controller->get_formation_facing_degrees();
    placement.active = true;

    const auto* nation =
        Game::Systems::NationRegistry::instance().get_nation_for_player(
            context.local_owner_id);
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

    const auto& preview = context.command_controller->formation_preview();
    placement.slot_markers.reserve(preview.slot_list.size());
    for (const auto& slot : preview.slot_list) {
      Render::GL::FormationSlotMarker marker;
      marker.position = slot.world_position;
      marker.position.setY(Game::Map::TerrainService::instance().get_terrain_height(
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
      context.command_controller->is_placing_formation() &&
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

auto civilian_delivery_available(const CivilianDeliveryContext& context) -> bool {
  if (context.world == nullptr || context.hover_tracker == nullptr) {
    return false;
  }

  const auto hovered_id = context.hover_tracker->get_last_hovered_entity();
  auto* hovered = hovered_id != 0 ? context.world->get_entity(hovered_id) : nullptr;
  auto* hovered_unit = (hovered != nullptr)
                           ? hovered->get_component<Engine::Core::UnitComponent>()
                           : nullptr;
  const bool hovered_friendly_barracks =
      (hovered_unit != nullptr) && hovered_unit->owner_id == context.local_owner_id &&
      hovered_unit->spawn_type == Game::Units::SpawnType::Barracks;
  auto* hovered_production =
      hovered != nullptr ? hovered->get_component<Engine::Core::ProductionComponent>()
                         : nullptr;
  const bool barracks_has_room =
      (hovered_production != nullptr) &&
      (hovered_production->manpower_available +
           Game::Systems::k_civilian_delivery_population_grant <=
       hovered_production->max_units);

  if (!hovered_friendly_barracks || !barracks_has_room) {
    return false;
  }

  auto* selection_system = context.world->get_system<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    return false;
  }

  for (const auto id : selection_system->get_selected_units()) {
    auto* selected_entity = context.world->get_entity(id);
    auto* selected_unit =
        (selected_entity != nullptr)
            ? selected_entity->get_component<Engine::Core::UnitComponent>()
            : nullptr;
    if ((selected_unit != nullptr) &&
        (selected_unit->owner_id == context.local_owner_id) &&
        (selected_unit->spawn_type == Game::Units::SpawnType::Civilian)) {
      return true;
    }
  }

  return false;
}

} // namespace App::Core::FrameUiCoordinator
