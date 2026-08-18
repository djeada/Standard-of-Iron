#include "app/core/match_presentation_sync.h"

#include <QString>
#include <QVector3D>

#include <algorithm>

#include "app/input/cursor_manager.h"
#include "app/input/hover_tracker.h"
#include "app/orders/rts_action_model.h"
#include "app/world/unit_queries.h"
#include "game/core/world.h"
#include "game/map/visibility_service.h"
#include "game/systems/selection_system.h"
#include "scene/camera.h"

namespace App::Core::PresentationSync {

auto collect_attack_range_rings(const SelectionAttackContext& ctx)
    -> std::vector<Game::Systems::AttackRangeRing> {
  std::vector<Game::Systems::AttackRangeRing> rings;
  if (ctx.world == nullptr || ctx.spectator_mode) {
    return rings;
  }

  auto* selection_system = ctx.world->get_system<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    return rings;
  }

  const auto& selected = selection_system->get_selected_units();
  const auto hovered =
      (ctx.hover != nullptr) ? ctx.hover->get_last_hovered_entity() : 0;

  Game::Systems::AttackRangeRingRequest request;
  request.world = ctx.world;
  request.local_owner_id = ctx.local_owner_id;
  request.selection = selected;
  request.max_rings = Game::Systems::k_attack_range_max_rings;
  if (std::find(selected.begin(), selected.end(), hovered) != selected.end()) {
    request.focus_entity_id = hovered;
  }
  return Game::Systems::collect_attack_range_rings(request);
}

auto collect_attack_targeting(const SelectionAttackContext& ctx)
    -> AttackTargetingResult {
  AttackTargetingResult result;
  result.hint[QStringLiteral("state")] = QStringLiteral("none");
  result.hint[QStringLiteral("name")] = QString();
  result.hint[QStringLiteral("range")] = QStringLiteral("none");

  const bool attack_mode_active =
      (ctx.cursor != nullptr) && ctx.cursor->mode() == CursorMode::Attack;
  if (!attack_mode_active || ctx.world == nullptr || ctx.spectator_mode) {
    return result;
  }

  std::vector<Engine::Core::EntityID> attackers;
  if (auto* selection_system =
          ctx.world->get_system<Game::Systems::SelectionSystem>()) {
    attackers = App::Core::filter_selected_units_for_action(
        ctx.world, selection_system->get_selected_units(), QStringLiteral("attack"));
  }

  auto& visibility = Game::Map::VisibilityService::instance();
  const auto snapshot =
      visibility.is_initialized() ? visibility.snapshot_ptr() : nullptr;

  Game::Systems::AttackTargetingRequest request;
  request.world = ctx.world;
  request.local_owner_id = ctx.local_owner_id;
  request.has_attackers = !attackers.empty();
  request.hovered_entity_id =
      (ctx.hover != nullptr) ? ctx.hover->get_last_hovered_entity() : 0;
  if (ctx.camera != nullptr) {
    const QVector3D anchor = ctx.camera->get_target();
    request.anchor_x = anchor.x();
    request.anchor_z = anchor.z();
    request.max_distance = Game::Systems::k_attack_highlight_max_distance;
  }
  request.max_markers = Game::Systems::k_attack_highlight_max_markers;
  request.visibility = snapshot.get();

  result.highlights = Game::Systems::collect_attack_target_highlights(request);

  const auto verdict_key =
      Game::Systems::attack_target_verdict_key(result.highlights.hovered_verdict);
  result.hint[QStringLiteral("state")] = QString::fromLatin1(
      verdict_key.data(), static_cast<qsizetype>(verdict_key.size()));

  if (result.highlights.hovered_verdict == Game::Systems::AttackTargetVerdict::Valid) {
    App::World::UnitDescription hovered;
    if (App::World::describe_unit(
            ctx.world, result.highlights.hovered_entity_id, hovered)) {
      result.hint[QStringLiteral("name")] = hovered.name;
    }

    const auto range_verdict = Game::Systems::classify_range_to_target(
        ctx.world, attackers, result.highlights.hovered_entity_id);
    const auto range_key = Game::Systems::range_verdict_key(range_verdict);
    result.hint[QStringLiteral("range")] =
        QString::fromLatin1(range_key.data(), static_cast<qsizetype>(range_key.size()));
  }

  return result;
}

} // namespace App::Core::PresentationSync
