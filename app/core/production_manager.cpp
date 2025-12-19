#include "production_manager.h"

#include "app/core/input_command_handler.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_transformer.h"
#include "game/systems/command_service.h"
#include "game/systems/nation_registry.h"
#include "game/systems/picking_service.h"
#include "game/systems/production_service.h"
#include "game/systems/selection_system.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_config.h"
#include "game/units/troop_type.h"
#include "render/gl/camera.h"
#include <QDebug>
#include <QPointF>
#include <algorithm>

ProductionManager::ProductionManager(
    Engine::Core::World *world, Game::Systems::PickingService *picking_service,
    Render::GL::Camera *camera, QObject *parent)
    : QObject(parent), m_world(world), m_picking_service(picking_service),
      m_camera(camera) {}

void ProductionManager::start_building_placement(const QString &building_type) {
  if (building_type.isEmpty()) {
    return;
  }
  m_pending_building_type = building_type;
}

void ProductionManager::place_building_at_screen(
    qreal sx, qreal sy, int local_owner_id, const ViewportState &viewport) {
  if (m_pending_building_type.isEmpty() || !m_world || !m_picking_service ||
      !m_camera) {
    return;
  }

  QVector3D hit;
  if (!m_picking_service->screen_to_ground(
          QPointF(sx, sy), *m_camera, viewport.width, viewport.height, hit)) {
    return;
  }

  Game::Units::SpawnParams params;
  params.position = hit;
  params.player_id = local_owner_id;
  params.ai_controlled = false;

  auto &nation_registry = Game::Systems::NationRegistry::instance();
  if (const auto *nation =
          nation_registry.get_nation_for_player(local_owner_id)) {
    params.nation_id = nation->id;
  } else {
    params.nation_id = nation_registry.default_nation_id();
  }

  if (m_pending_building_type == QStringLiteral("defense_tower")) {
    params.spawn_type = Game::Units::SpawnType::DefenseTower;

    auto registry = Game::Map::MapTransformer::getFactoryRegistry();
    if (registry) {
      auto unit = registry->create(params.spawn_type, *m_world, params);
      if (unit) {
        qInfo() << "Placed defense tower at" << hit.x() << hit.z();
      }
    }
  }

  m_pending_building_type.clear();
}

void ProductionManager::cancel_building_placement() {
  m_pending_building_type.clear();
}

void ProductionManager::on_construction_mouse_move(
    qreal sx, qreal sy, const ViewportState &viewport) {
  if (!m_is_placing_construction || !m_picking_service || !m_camera) {
    return;
  }

  QPointF screenPt(sx, sy);
  QVector3D hit;
  if (m_picking_service->screen_to_ground(screenPt, *m_camera, viewport.width,
                                          viewport.height, hit)) {
    m_construction_placement_position = hit;

    for (auto id : m_pending_construction_builders) {
      auto *e = m_world->get_entity(id);
      if (!e) {
        continue;
      }

      auto *builder_prod =
          e->get_component<Engine::Core::BuilderProductionComponent>();
      if (builder_prod) {
        builder_prod->construction_site_x = hit.x();
        builder_prod->construction_site_z = hit.z();
      }
    }
  }
}

void ProductionManager::on_construction_confirm() {
  if (!m_is_placing_construction || m_pending_construction_builders.empty()) {
    on_construction_cancel();
    return;
  }

  for (auto id : m_pending_construction_builders) {
    auto *e = m_world->get_entity(id);
    if (!e) {
      continue;
    }

    auto *builder_prod =
        e->get_component<Engine::Core::BuilderProductionComponent>();
    if (builder_prod) {
      builder_prod->is_placement_preview = false;
      builder_prod->construction_site_x = m_construction_placement_position.x();
      builder_prod->construction_site_z = m_construction_placement_position.z();
    }

    auto *mv = e->get_component<Engine::Core::MovementComponent>();
    if (mv) {
      mv->goal_x = m_construction_placement_position.x();
      mv->goal_y = m_construction_placement_position.z();
      mv->target_x = m_construction_placement_position.x();
      mv->target_y = m_construction_placement_position.z();
    }
  }

  m_is_placing_construction = false;
  m_pending_construction_type.clear();
  m_pending_construction_builders.clear();
  emit placing_construction_changed();
}

void ProductionManager::on_construction_cancel() {
  if (!m_is_placing_construction) {
    return;
  }

  for (auto id : m_pending_construction_builders) {
    auto *e = m_world->get_entity(id);
    if (!e) {
      continue;
    }

    auto *builder_prod =
        e->get_component<Engine::Core::BuilderProductionComponent>();
    if (builder_prod) {
      builder_prod->has_construction_site = false;
      builder_prod->construction_site_x = 0.0F;
      builder_prod->construction_site_z = 0.0F;
      builder_prod->at_construction_site = false;
      builder_prod->product_type = "";
      builder_prod->is_placement_preview = false;
    }
  }

  m_is_placing_construction = false;
  m_pending_construction_type.clear();
  m_pending_construction_builders.clear();
  emit placing_construction_changed();
}

void ProductionManager::start_builder_construction(const QString &item_type) {
  if (!m_world) {
    return;
  }

  m_pending_construction_builders = collect_available_builders();
  if (m_pending_construction_builders.empty()) {
    return;
  }

  m_pending_construction_type = item_type;
  m_is_placing_construction = true;
  m_construction_placement_position =
      calculate_builder_center_position(m_pending_construction_builders);

  std::string item_str = item_type.toStdString();
  float build_time = get_construction_build_time(item_str);

  for (auto id : m_pending_construction_builders) {
    auto *e = m_world->get_entity(id);
    if (!e) {
      continue;
    }

    auto *builder_prod =
        e->get_component<Engine::Core::BuilderProductionComponent>();
    if (!builder_prod) {
      continue;
    }

    builder_prod->product_type = item_str;
    builder_prod->build_time = build_time;
    builder_prod->time_remaining = build_time;
    builder_prod->has_construction_site = true;
    builder_prod->construction_site_x = m_construction_placement_position.x();
    builder_prod->construction_site_z = m_construction_placement_position.z();
    builder_prod->at_construction_site = false;
    builder_prod->in_progress = false;
    builder_prod->is_placement_preview = true;
  }

  emit placing_construction_changed();
}

auto ProductionManager::get_selected_production_state(int local_owner_id) const
    -> QVariantMap {
  QVariantMap m;
  m["has_barracks"] = false;
  m["in_progress"] = false;
  m["time_remaining"] = 0.0;
  m["build_time"] = 0.0;
  m["produced_count"] = 0;
  m["max_units"] = 0;
  m["villager_cost"] = 1;

  if (!m_world) {
    return m;
  }

  auto *selection_system =
      m_world->get_system<Game::Systems::SelectionSystem>();
  if (!selection_system) {
    return m;
  }

  Game::Systems::ProductionState st;
  Game::Systems::ProductionService::get_selected_barracks_state(
      *m_world, selection_system->get_selected_units(), local_owner_id, st);

  m["has_barracks"] = st.has_barracks;
  m["in_progress"] = st.in_progress;
  m["product_type"] =
      QString::fromStdString(Game::Units::troop_typeToString(st.product_type));
  m["time_remaining"] = st.time_remaining;
  m["build_time"] = st.build_time;
  m["produced_count"] = st.produced_count;
  m["max_units"] = st.max_units;
  m["villager_cost"] = st.villager_cost;
  m["queue_size"] = st.queue_size;
  m["nation_id"] =
      QString::fromStdString(Game::Systems::nation_id_to_string(st.nation_id));

  QVariantList queue_list;
  for (const auto &unit_type : st.production_queue) {
    queue_list.append(
        QString::fromStdString(Game::Units::troop_typeToString(unit_type)));
  }
  m["production_queue"] = queue_list;

  return m;
}

auto ProductionManager::get_selected_builder_production_state() const
    -> QVariantMap {
  QVariantMap m;
  m["in_progress"] = false;
  m["time_remaining"] = 0.0;
  m["build_time"] = 10.0;
  m["product_type"] = "";

  if (!m_world) {
    return m;
  }

  auto *selection_system =
      m_world->get_system<Game::Systems::SelectionSystem>();
  if (!selection_system) {
    return m;
  }

  const auto &selected = selection_system->get_selected_units();
  for (auto id : selected) {
    auto *e = m_world->get_entity(id);
    if (!e) {
      continue;
    }

    auto *builder_prod =
        e->get_component<Engine::Core::BuilderProductionComponent>();
    if (builder_prod) {
      m["in_progress"] =
          builder_prod->in_progress || builder_prod->is_placement_preview;
      m["time_remaining"] = builder_prod->time_remaining;
      m["build_time"] = builder_prod->build_time;
      m["product_type"] = QString::fromStdString(builder_prod->product_type);
      return m;
    }
  }

  return m;
}

auto ProductionManager::get_unit_production_info(const QString &unit_type)
    -> QVariantMap {
  QVariantMap info;
  const auto &config = Game::Units::TroopConfig::instance();
  std::string type_str = unit_type.toStdString();

  info["cost"] = config.getProductionCost(type_str);
  info["build_time"] = static_cast<double>(config.getBuildTime(type_str));
  info["individuals_per_unit"] = config.getIndividualsPerUnit(type_str);

  return info;
}

void ProductionManager::set_rally_at_screen(qreal sx, qreal sy,
                                            int local_owner_id,
                                            const ViewportState &viewport) {
  if (!m_world || !m_picking_service || !m_camera) {
    return;
  }

  QVector3D hit;
  if (!m_picking_service->screen_to_ground(
          QPointF(sx, sy), *m_camera, viewport.width, viewport.height, hit)) {
    return;
  }

  auto *selection_system =
      m_world->get_system<Game::Systems::SelectionSystem>();
  if (!selection_system) {
    return;
  }

  const auto &selected = selection_system->get_selected_units();
  for (auto id : selected) {
    auto *e = m_world->get_entity(id);
    if (!e) {
      continue;
    }

    auto *unit = e->get_component<Engine::Core::UnitComponent>();
    if (!unit || unit->owner_id != local_owner_id) {
      continue;
    }

    auto *prod = e->get_component<Engine::Core::ProductionComponent>();
    if (prod) {
      prod->rally_x = hit.x();
      prod->rally_z = hit.z();
      prod->rally_set = true;
    }
  }
}

auto ProductionManager::collect_available_builders()
    -> std::vector<Engine::Core::EntityID> {
  std::vector<Engine::Core::EntityID> builders;

  auto *selection_system =
      m_world->get_system<Game::Systems::SelectionSystem>();
  if (!selection_system) {
    return builders;
  }

  const auto &selected = selection_system->get_selected_units();
  for (auto id : selected) {
    auto *e = m_world->get_entity(id);
    if (!e) {
      continue;
    }

    auto *builder_prod =
        e->get_component<Engine::Core::BuilderProductionComponent>();
    if (builder_prod && !builder_prod->in_progress) {
      builders.push_back(id);
    }
  }

  return builders;
}

auto ProductionManager::calculate_builder_center_position(
    const std::vector<Engine::Core::EntityID> &builder_ids) -> QVector3D {
  float sum_x = 0.0F;
  float sum_y = 0.0F;
  float sum_z = 0.0F;
  int valid_count = 0;

  for (auto id : builder_ids) {
    auto *e = m_world->get_entity(id);
    if (!e) {
      continue;
    }

    auto *transform = e->get_component<Engine::Core::TransformComponent>();
    if (transform) {
      sum_x += transform->position.x;
      sum_y += transform->position.y;
      sum_z += transform->position.z;
      valid_count++;
    }
  }

  if (valid_count > 0) {
    return QVector3D(sum_x / static_cast<float>(valid_count),
                     sum_y / static_cast<float>(valid_count),
                     sum_z / static_cast<float>(valid_count));
  }

  return QVector3D(0.0F, 0.0F, 0.0F);
}

auto ProductionManager::get_construction_build_time(
    const std::string &item_type) -> float {
  constexpr float DEFAULT_BUILD_TIME = 10.0F;
  constexpr float CATAPULT_BUILD_TIME = 15.0F;
  constexpr float BALLISTA_BUILD_TIME = 12.0F;
  constexpr float DEFENSE_TOWER_BUILD_TIME = 20.0F;

  if (item_type == "catapult") {
    return CATAPULT_BUILD_TIME;
  }
  if (item_type == "ballista") {
    return BALLISTA_BUILD_TIME;
  }
  if (item_type == "defense_tower") {
    return DEFENSE_TOWER_BUILD_TIME;
  }
  return DEFAULT_BUILD_TIME;
}
