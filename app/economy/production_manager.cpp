#include "app/economy/production_manager.h"

#include <QCoreApplication>
#include <QPointF>

#include <algorithm>
#include <string>

#include "app/economy/construction_pointer.h"
#include "app/economy/harvest_targeting.h"
#include "app/economy/placement_orders.h"
#include "app/economy/resource_text.h"
#include "app/input/viewport_state.h"
#include "app/orders/order_submission.h"
#include "app/orders/worker_orders.h"
#include "game/audio/audio_cues.h"
#include "game/core/component_gameplay.h"
#include "game/core/world.h"
#include "game/render_bridge/picking_service.h"
#include "game/session/selection_service.h"
#include "game/session/session_context.h"
#include "game/systems/builder_product_types.h"
#include "game/systems/economy/food_targets.h"
#include "game/systems/nation_registry.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/structure_placement_service.h"
#include "game/units/spawn_type.h"
#include "scene/camera.h"

namespace {

using namespace App::Economy;

constexpr float k_construction_rotation_step_degrees = 5.0F;

} // namespace

ProductionManager::ProductionManager(Engine::Core::World* world,
                                     Game::Systems::PickingService* picking_service,
                                     Render::GL::Camera* camera,
                                     QObject* parent)
    : QObject(parent)
    , m_world(world)
    , m_picking_service(picking_service)
    , m_camera(camera)
    , m_preview(world) {
  connect(&m_preview,
          &ConstructionPreview::active_changed,
          this,
          &ProductionManager::construction_preview_active_changed);
  connect(&m_preview,
          &ConstructionPreview::valid_changed,
          this,
          &ProductionManager::construction_preview_valid_changed);
  connect(&m_preview,
          &ConstructionPreview::reason_changed,
          this,
          &ProductionManager::construction_preview_reason_changed);
  connect(&m_preview,
          &ConstructionPreview::summary_changed,
          this,
          &ProductionManager::construction_preview_summary_changed);
}

auto ProductionManager::placement_phase() const -> PlacementPhase {
  return App::Economy::placement_phase(
      m_session.active(), m_wall.drag_active(), m_preview.active());
}

auto ProductionManager::construction_preview_rotatable() const -> bool {
  return m_session.active() && m_preview.active() &&
         (m_session.is_wall() ||
          item_supports_preview_rotation(m_session.construction_type()));
}

auto ProductionManager::ready_for_pointer() const -> bool {
  return m_picking_service != nullptr && m_camera != nullptr;
}

auto ProductionManager::refusal_context() const -> RefusalContext {
  return {.economy = Game::Session::session_for(*m_world).economy(),
          .owner_id = m_session.owner_id(m_world),
          .construction_type = m_session.construction_type()};
}

auto ProductionManager::preview_owner() const -> PreviewOwner {
  return {.owner_id = m_session.owner_id(m_world),
          .nation_id = m_session.nation_id(m_world)};
}

auto ProductionManager::ground_refusal(const QVector3D& site) const -> QString {
  if (m_world == nullptr) {
    return {};
  }
  return ground_refusal_text(*m_world,
                             m_session.construction_type(),
                             site.x(),
                             site.z(),
                             m_session.effective_rotation_y(),
                             m_session.builders(),
                             refusal_context());
}

auto ProductionManager::nearest_legal_site(const QVector3D& wanted) const -> QVector3D {
  if (m_world == nullptr || m_session.construction_type().isEmpty() ||
      m_session.kind() == PlacementKind::Harvest || m_session.is_wall() ||
      ground_refusal(wanted).isEmpty()) {
    return wanted;
  }
  return nearest_clear_site(*m_world,
                            m_session.construction_type(),
                            wanted,
                            m_session.effective_rotation_y(),
                            m_session.builders());
}

auto ProductionManager::hover_refusal(const QVector3D& site) -> QString {
  const bool harvesting = m_session.kind() == PlacementKind::Harvest;
  if (m_session.food_target_id() != 0 && harvesting) {
    if (!Game::Systems::resolve_food_target(
             *m_world, m_session.food_target_id(), m_session.owner_id(m_world))
             .has_value()) {
      return QCoreApplication::translate("ProductionManager",
                                         "Nothing here is worth harvesting.");
    }
    return {};
  }

  if (harvesting) {
    const HarvestPlacement placement =
        evaluate_harvest_placement(m_world,
                                   m_session.builders(),
                                   site,
                                   m_session.construction_type(),
                                   m_session.harvest_target_id());
    if (!placement.valid()) {
      return QCoreApplication::translate("ProductionManager",
                                         "No resource here to work.");
    }
    return {};
  }

  if (const QString refusal = ground_refusal(site); !refusal.isEmpty()) {
    return refusal;
  }
  return affordability_refusal_text(refusal_context());
}

auto ProductionManager::pointer_hit(qreal sx, qreal sy, const ViewportState& viewport)
    -> std::optional<ConstructionPointerHit> {
  return resolve_construction_pointer_hit(m_world,
                                          m_session.construction_type(),
                                          m_session.owner_id(m_world),
                                          m_session.builders(),
                                          *m_camera,
                                          viewport,
                                          QPointF(sx, sy));
}

void ProductionManager::reject(const QString& reason) {
  emit construction_placement_rejected(reason);
}

void ProductionManager::reject_and_invalidate(const QString& reason) {
  m_preview.set_valid(false);
  emit construction_placement_rejected(reason);
}

void ProductionManager::submit_order(int owner_id, App::Core::OrderRequest request) {
  emit order_feedback(
      App::Core::submit_player_order(*m_world, owner_id, std::move(request)));
}

void ProductionManager::end_placement(Handoff handoff) {
  if (handoff == Handoff::BecomeSiteGhost) {
    m_preview.hand_to_construction_site(m_session.construction_type());
  } else {
    m_preview.clear_entities();
  }
  m_preview.clear_summary();
  m_wall.reset();
  m_session.end();
  m_preview.set_active(false);
  m_preview.set_valid(false);
  emit placing_construction_changed();
}

void ProductionManager::start_building_placement(const QString& building_type,
                                                 int local_owner_id) {
  if (building_type.isEmpty() || m_world == nullptr) {
    return;
  }

  if (m_session.active()) {
    on_construction_cancel();
  }

  auto& nation_registry = Game::Session::session_for(*m_world).nations();
  const auto* nation = nation_registry.get_nation_for_player(local_owner_id);
  m_session.begin_direct(building_type,
                         local_owner_id,
                         nation != nullptr ? nation->id
                                           : nation_registry.default_nation_id());

  m_wall.reset();
  m_preview.clear_entities();
  m_preview.clear_summary();
  m_preview.set_active(false);
  m_preview.set_valid(false);
  Game::Audio::play_cue(Game::Audio::Cue::k_build_placement_begin);
  emit placing_construction_changed();
}

void ProductionManager::place_building_at_screen(qreal sx,
                                                 qreal sy,
                                                 int local_owner_id,
                                                 const ViewportState& viewport) {
  if (m_session.pending_building_type().isEmpty() || !m_session.direct()) {
    return;
  }

  m_session.set_owner_id(local_owner_id);
  on_construction_mouse_move(sx, sy, viewport);
  if (m_preview.active()) {
    on_construction_confirm();
  }
}

void ProductionManager::cancel_building_placement() {
  if (m_session.direct()) {
    on_construction_cancel();
    return;
  }
  m_session.forget_pending_building();
}

void ProductionManager::reset_transient_state() {
  cancel_building_placement();

  if (m_session.active()) {
    on_construction_cancel();
  }

  m_session.end();
  m_session.set_position(QVector3D());
  m_wall.reset();
  m_preview.set_active(false);
  m_preview.set_valid(false);
  m_preview.clear_summary();
  m_preview.clear_entities();
}

void ProductionManager::on_construction_cancel() {
  if (!placement_allows(placement_phase(), m_session.kind(), PlacementEvent::Cancel)) {
    return;
  }
  end_placement();
}

void ProductionManager::start_builder_construction(const QString& item_type) {
  if (m_world == nullptr) {
    return;
  }

  if (m_session.active()) {
    on_construction_cancel();
  }

  const bool harvesting = is_harvest_construction_item(item_type);
  auto builders = collect_available_builders(harvesting);
  if (builders.empty()) {
    emit order_feedback(App::Core::rejected_order(
        harvesting ? App::Core::OrderKind::Gather : App::Core::OrderKind::Build,
        App::Core::unit_busy_reason()));
    return;
  }

  const QVector3D center = builder_center(builders);
  m_session.begin_with_builders(item_type, std::move(builders), center, m_world);
  m_wall.reset();
  m_preview.clear_entities();
  m_preview.clear_summary();
  m_preview.set_active(false);
  m_preview.set_valid(false);
  Game::Audio::play_cue(Game::Audio::Cue::k_build_placement_begin);
  emit placing_construction_changed();
}

void ProductionManager::on_construction_mouse_move(qreal sx,
                                                   qreal sy,
                                                   const ViewportState& viewport) {
  if (!placement_allows(
          placement_phase(), m_session.kind(), PlacementEvent::PointerMotion) ||
      !ready_for_pointer()) {
    return;
  }

  const auto hit = pointer_hit(sx, sy, viewport);
  if (m_session.is_wall()) {
    if (hit.has_value()) {
      hover_wall(hit->world_position);
      return;
    }
    m_preview.clear_entities();
    m_wall.hover_without_target();
    m_preview.clear_summary();
    m_preview.set_active(false);
    m_preview.set_valid(false);
    return;
  }

  if (hit.has_value()) {
    hover_structure(*hit);
    return;
  }
  hover_nothing();
}

void ProductionManager::hover_wall(const QVector3D& world_position) {
  m_session.set_position(world_position);
  m_wall.hover_at(world_position);
  replan_wall(world_position);
}

void ProductionManager::hover_structure(const ConstructionPointerHit& hit) {
  m_session.set_targets(hit.harvest_target_id, hit.food_target_id);
  m_session.set_position(hit.world_position);
  show_structure_hover(hit.world_position);
}

void ProductionManager::hover_nothing() {
  drop_structure_preview();
  m_preview.set_valid(false);
}

void ProductionManager::drop_structure_preview() {
  m_session.clear_targets();
  m_preview.clear_entities();
  m_preview.set_active(false);
}

void ProductionManager::show_structure_hover(const QVector3D& world_position) {
  if (m_world == nullptr || m_session.is_wall()) {
    return;
  }

  m_preview.set_active(true);
  const QVector3D site = nearest_legal_site(world_position);
  const QString reason = hover_refusal(site);
  m_preview.set_ruling(reason.isEmpty(), reason);
  show_structure_preview(site);
}

void ProductionManager::show_structure_preview(const QVector3D& world_position) {
  if (!is_previewable_structure_item(m_session.construction_type())) {
    m_preview.clear_entities();
    return;
  }
  m_preview.show_structure(m_session.construction_type(),
                           world_position,
                           m_session.effective_rotation_y(),
                           preview_owner());
}

void ProductionManager::replan_wall(const QVector3D& pointer_world) {
  m_wall.clear_plan();
  m_preview.clear_entities();

  if (m_world == nullptr || !m_wall.anchor_set()) {
    m_preview.set_active(false);
    m_preview.set_valid(false);
    m_preview.clear_summary();
    return;
  }

  const bool gate = m_session.is_gate();
  const auto summary =
      m_wall.plan(*m_world, pointer_world, m_session.owner_id(m_world), gate);
  const bool any_valid = summary.valid_segment_count > 0;

  m_preview.set_active(!m_wall.segments().empty());
  m_preview.set_ruling(
      any_valid,
      any_valid ? QString()
                : wall_plan_refusal_text(m_wall.segments(), refusal_context()));
  m_preview.set_summary(
      summary.segment_count, summary.valid_segment_count, summary.total_cost);
  m_preview.show_wall_plan(m_wall.segments(), gate, preview_owner());
}

void ProductionManager::on_construction_pointer_pressed(qreal sx,
                                                        qreal sy,
                                                        const ViewportState& viewport) {
  if (!placement_allows(
          placement_phase(), m_session.kind(), PlacementEvent::PointerPress) ||
      !ready_for_pointer()) {
    return;
  }

  const auto hit = pointer_hit(sx, sy, viewport);
  if (!hit.has_value()) {
    return;
  }

  m_wall.begin_drag(hit->world_position);
  m_session.set_position(hit->world_position);
  replan_wall(hit->world_position);
}

void ProductionManager::on_construction_pointer_released(
    qreal sx, qreal sy, const ViewportState& viewport) {
  if (!placement_allows(
          placement_phase(), m_session.kind(), PlacementEvent::PointerRelease)) {
    return;
  }

  if (!m_session.is_wall()) {
    release_structure(sx, sy, viewport);
    return;
  }
  release_wall(sx, sy, viewport);
}

void ProductionManager::release_structure(qreal sx,
                                          qreal sy,
                                          const ViewportState& viewport) {
  if (ready_for_pointer()) {
    if (const auto hit = pointer_hit(sx, sy, viewport); hit.has_value()) {
      hover_structure(*hit);
      m_release_position = hit->world_position;
    } else if (QVector3D ground;
               Game::Systems::PickingService::screen_to_ground(QPointF(sx, sy),
                                                               *m_camera,
                                                               viewport.width,
                                                               viewport.height,
                                                               ground)) {
      if (m_session.kind() == PlacementKind::Harvest) {
        drop_structure_preview();
        m_preview.set_valid(false);
      }
      m_release_position = ground;
    }
  }
  on_construction_confirm();
  m_release_position.reset();
}

void ProductionManager::release_wall(qreal sx,
                                     qreal sy,
                                     const ViewportState& viewport) {
  if (!m_wall.anchor_set()) {
    return;
  }

  QVector3D release_world = m_session.position();
  bool release_hit_valid = false;
  if (ready_for_pointer()) {
    if (const auto hit = pointer_hit(sx, sy, viewport); hit.has_value()) {
      release_world = hit->world_position;
      release_hit_valid = true;
    }
  }

  if (!release_hit_valid && !m_preview.active()) {
    return;
  }

  m_session.set_position(release_world);
  replan_wall(release_world);
  if (!m_preview.active()) {
    return;
  }
  confirm_wall();
}

void ProductionManager::on_construction_scroll(float delta) {
  if (!placement_allows(placement_phase(), m_session.kind(), PlacementEvent::Rotate)) {
    return;
  }

  if (m_session.is_wall()) {
    m_wall.rotate(delta);
    replan_wall(m_session.position());
    return;
  }

  if (!item_supports_preview_rotation(m_session.construction_type())) {
    return;
  }

  m_session.set_rotation_y(normalize_rotation_degrees(
      m_session.rotation_y() + delta * k_construction_rotation_step_degrees));
  show_structure_preview(m_session.position());
}

void ProductionManager::on_construction_confirm() {
  if (!m_session.active() || (!m_session.direct() && m_session.builders().empty())) {
    on_construction_cancel();
    return;
  }

  const bool harvesting = m_session.kind() == PlacementKind::Harvest;
  if (m_session.is_wall()) {
    if (!m_preview.active()) {
      reject(QCoreApplication::translate("ProductionManager",
                                         "Drag out a wall line first."));
      return;
    }
    confirm_wall();
    return;
  }

  if (!m_preview.active()) {
    reject(harvesting ? generic_collect_failure_reason()
                      : QCoreApplication::translate("ProductionManager",
                                                    "Choose a build location."));
    return;
  }

  if (m_session.direct()) {
    confirm_direct_building();
  } else if (harvesting && m_session.food_target_id() != 0) {
    confirm_food_harvest();
  } else if (harvesting) {
    confirm_harvest();
  } else {
    confirm_builder_structure();
  }
}

void ProductionManager::confirm_food_harvest() {
  const int owner_id = m_session.owner_id(m_world);
  const auto target = Game::Systems::resolve_food_target(
      *m_world, m_session.food_target_id(), owner_id);
  if (!target.has_value() ||
      Game::Systems::food_target_claimed(*m_world, m_session.food_target_id())) {
    reject_and_invalidate(QCoreApplication::translate(
        "ProductionManager", "That resource is already assigned."));
    return;
  }

  auto crew = m_session.builders();
  App::Orders::sort_nearest_first(*m_world, crew, target->x, target->z);
  submit_order(owner_id,
               App::Orders::harvest_order(std::move(crew),
                                          std::string(target->product_type),
                                          target->id,
                                          target->x,
                                          target->z));
  end_placement();
}

void ProductionManager::confirm_harvest() {
  const HarvestPlacement placement =
      evaluate_harvest_placement(m_world,
                                 m_session.builders(),
                                 m_session.position(),
                                 m_session.construction_type(),
                                 m_session.harvest_target_id());
  if (!placement.valid()) {
    reject_and_invalidate(placement.failure_reason);
    return;
  }

  if (prop_taken(Game::Session::session_for(*m_world).terrain(),
                 placement.target->id,
                 crew_claims(m_world, m_session.builders()))) {
    reject_and_invalidate(QCoreApplication::translate(
        "ProductionManager", "That resource is already assigned."));
    return;
  }

  std::vector<Engine::Core::EntityID> crew;
  crew.reserve(m_session.builders().size());
  crew.push_back(placement.builder_id);
  for (auto id : m_session.builders()) {
    if (id != placement.builder_id) {
      crew.push_back(id);
    }
  }
  submit_order(m_session.owner_id(m_world),
               App::Orders::harvest_order(std::move(crew),
                                          harvest_product_type(placement.kind),
                                          placement.target->id,
                                          placement.target->x,
                                          placement.target->z));
  end_placement();
}

void ProductionManager::confirm_builder_structure() {
  m_session.set_position(nearest_legal_site(m_session.position()));
  if (const QString refusal = ground_refusal(m_session.position());
      !refusal.isEmpty()) {
    reject(refusal);
    return;
  }

  const int owner_id = m_session.owner_id(m_world);
  if (const QString refusal = affordability_refusal_text(refusal_context());
      !refusal.isEmpty()) {
    reject(refusal);
    return;
  }

  submit_order(owner_id,
               structure_build_order(m_session.builders(),
                                     m_session.construction_type(),
                                     m_session.position(),
                                     m_session.rotation_y()));
  end_placement(Handoff::BecomeSiteGhost);
}

void ProductionManager::confirm_direct_building() {
  if (!m_session.direct() || m_world == nullptr) {
    return;
  }

  const int owner_id = m_session.owner_id(m_world);
  const std::string building_type = m_session.construction_type().toStdString();
  const float rotation_y = m_session.effective_rotation_y();
  if (const QString refusal = placement_refusal_text(
          Game::Systems::StructurePlacementService::ruling(
              *m_world, owner_id, building_type, m_session.position(), rotation_y),
          refusal_context());
      !refusal.isEmpty()) {
    reject(refusal);
    return;
  }

  submit_order(owner_id,
               direct_placement_order(building_type, m_session.position(), rotation_y));
  end_placement();
}

void ProductionManager::confirm_wall() {
  const int valid_segment_count = m_wall.valid_segment_count();
  if (valid_segment_count <= 0) {
    reject(wall_confirm_refusal_text(m_wall.segments()));
    m_preview.set_active(!m_wall.segments().empty());
    m_preview.set_valid(false);
    return;
  }

  const int owner_id = m_session.owner_id(m_world);
  Game::Systems::ResourceAmounts total_cost;
  total_cost.set(Game::Systems::ResourceType::Wood,
                 valid_segment_count *
                     wood_per_wall_segment(m_session.construction_type()));
  const auto& economy = Game::Session::session_for(*m_world).economy();
  if (!economy.has_at_least(owner_id, total_cost)) {
    reject(insufficient_resources_reason(economy, owner_id, total_cost));
    m_preview.set_active(!m_wall.segments().empty());
    m_preview.set_valid(false);
    return;
  }

  if (m_session.builders().empty()) {
    reject(QCoreApplication::translate("ProductionManager", "No available builder."));
    return;
  }

  submit_order(
      owner_id,
      wall_plan_order(m_session.builders(), m_wall.request(), m_session.position()));
  end_placement();
}

auto ProductionManager::set_rally_at_screen(qreal sx,
                                            qreal sy,
                                            int local_owner_id,
                                            const ViewportState& viewport) -> bool {
  if ((m_world == nullptr) || (m_picking_service == nullptr) || (m_camera == nullptr)) {
    return false;
  }

  QVector3D hit;
  if (!Game::Systems::PickingService::screen_to_surface(
          Game::Session::session_for(*m_world).terrain(),
          QPointF(sx, sy),
          *m_camera,
          viewport.width,
          viewport.height,
          hit)) {
    return false;
  }

  bool updated_any = false;
  for (auto id :
       Game::Session::session_for(*m_world).selection().get_selected_units()) {
    auto* e = m_world->get_entity(id);
    if (e == nullptr) {
      continue;
    }

    auto* unit = e->get_component<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->owner_id != local_owner_id ||
        !Game::Units::is_recruitment_building(unit->spawn_type)) {
      continue;
    }

    auto* prod = e->get_component<Engine::Core::ProductionComponent>();
    if (prod == nullptr) {
      prod = e->add_component<Engine::Core::ProductionComponent>();
    }
    if (prod == nullptr) {
      continue;
    }
    prod->rally_x = hit.x();
    prod->rally_z = hit.z();
    prod->rally_set = true;
    updated_any = true;
  }
  return updated_any;
}

auto ProductionManager::collect_available_builders(bool include_busy)
    -> std::vector<Engine::Core::EntityID> {
  std::vector<Engine::Core::EntityID> builders;

  for (auto id :
       Game::Session::session_for(*m_world).selection().get_selected_units()) {
    auto* e = m_world->get_entity(id);
    if (e == nullptr) {
      continue;
    }

    auto* builder_prod = e->get_component<Engine::Core::BuilderProductionComponent>();
    auto* unit = e->get_component<Engine::Core::UnitComponent>();
    if (builder_prod != nullptr && unit != nullptr &&
        unit->spawn_type == Game::Units::SpawnType::Builder && unit->health > 0 &&
        (include_busy ||
         Game::Systems::is_gather_builder_product(builder_prod->product_type) ||
         (!builder_prod->in_progress && !builder_prod->has_construction_site))) {
      builders.push_back(id);
    }
  }

  return builders;
}

auto ProductionManager::builder_center(
    const std::vector<Engine::Core::EntityID>& builder_ids) -> QVector3D {
  QVector3D sum;
  int valid_count = 0;

  for (auto id : builder_ids) {
    auto* e = m_world->get_entity(id);
    auto* transform =
        e != nullptr ? e->get_component<Engine::Core::TransformComponent>() : nullptr;
    if (transform == nullptr) {
      continue;
    }
    sum +=
        QVector3D(transform->position.x, transform->position.y, transform->position.z);
    valid_count++;
  }

  return valid_count > 0 ? sum / static_cast<float>(valid_count) : QVector3D();
}
