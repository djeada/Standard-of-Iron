#include "app/economy/construction_preview.h"

#include "game/core/component_gameplay.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/systems/navigation/wall_network_service.h"
#include "game/units/building_spawn_setup.h"
#include "game/visuals/building_asset_key.h"

namespace App::Economy {

namespace {

auto surface_position(Engine::Core::World& world,
                      const QVector3D& position) -> QVector3D {
  auto& terrain_service = Game::Session::session_for(world).terrain();
  if (!terrain_service.is_initialized()) {
    return position;
  }
  return terrain_service.resolve_surface_world_position(
      position.x(), position.z(), 0.0F, position.y());
}

void apply_transform(Engine::Core::TransformComponent& transform,
                     const QVector3D& position,
                     float rotation_y,
                     const QVector3D& scale) {
  transform.position = {position.x(), position.y(), position.z()};
  transform.rotation = {0.0F, rotation_y, 0.0F};
  transform.scale = {scale.x(), scale.y(), scale.z()};
}

} // namespace

ConstructionPreview::ConstructionPreview(Engine::Core::World* world, QObject* parent)
    : QObject(parent)
    , m_world(world) {
}

void ConstructionPreview::clear_entities() {
  if (m_world == nullptr) {
    m_entity_ids.clear();
    return;
  }

  for (auto entity_id : m_entity_ids) {
    if (m_world->get_entity(entity_id) != nullptr) {
      m_world->destroy_entity(entity_id);
    }
  }
  m_entity_ids.clear();
}

void ConstructionPreview::hand_to_construction_site(const QString& item_type) {
  if (m_world == nullptr) {
    clear_entities();
    return;
  }

  for (auto entity_id : m_entity_ids) {
    auto* preview =
        m_world->try_get<Engine::Core::ConstructionPreviewComponent>(entity_id);
    if (preview == nullptr) {
      m_world->destroy_entity(entity_id);
      continue;
    }
    preview->site_ghost = true;
    preview->valid = true;
    preview->progress = 0.0F;
    preview->product_type = item_type.toStdString();
  }
  m_entity_ids.clear();
}

void ConstructionPreview::show_structure(const QString& item_type,
                                         const QVector3D& world_position,
                                         float rotation_y,
                                         const PreviewOwner& owner) {
  clear_entities();
  if (m_world == nullptr) {
    return;
  }

  auto* entity = m_world->create_entity();
  if (entity == nullptr) {
    return;
  }

  auto* transform = entity->add_component<Engine::Core::TransformComponent>();
  auto* preview = entity->add_component<Engine::Core::ConstructionPreviewComponent>();
  if (transform == nullptr || preview == nullptr) {
    m_world->destroy_entity(entity->get_id());
    return;
  }

  const std::string type = item_type.toStdString();
  apply_transform(*transform,
                  surface_position(*m_world, world_position),
                  rotation_y,
                  Game::Units::building_transform_scale(type));

  auto* renderable =
      Game::Units::add_building_renderable(*entity, owner.nation_id, type);
  if (renderable == nullptr) {
    m_world->destroy_entity(entity->get_id());
    return;
  }
  renderable->visible = false;

  preview->owner_id = owner.owner_id;
  preview->nation_id = owner.nation_id;
  preview->valid = m_valid;
  preview->product_type = type;

  m_entity_ids.push_back(entity->get_id());
}

void ConstructionPreview::show_wall_plan(
    const std::vector<Game::Systems::PlannedWallSegment>& segments,
    bool gate,
    const PreviewOwner& owner,
    bool ladder) {
  using Game::Systems::WallNetworkService;
  clear_entities();
  if (m_world == nullptr || segments.empty()) {
    return;
  }

  const char* product_type = ladder ? "wall_ladder" : gate ? "wall_gate" : "wall_segment";
  const QVector3D scale = Game::Units::building_transform_scale(product_type);

  for (const auto& segment : segments) {
    auto* entity = m_world->create_entity();
    if (entity == nullptr) {
      continue;
    }

    auto* transform = entity->add_component<Engine::Core::TransformComponent>();
    auto* renderable = entity->add_component<Engine::Core::RenderableComponent>();
    auto* preview = entity->add_component<Engine::Core::ConstructionPreviewComponent>();
    if (transform == nullptr || renderable == nullptr || preview == nullptr) {
      m_world->destroy_entity(entity->get_id());
      continue;
    }

    apply_transform(*transform,
                    surface_position(*m_world, segment.world_position),
                    segment.rotation_y,
                    scale);

    renderable->visible = false;
    renderable->renderer_id =
        ladder ? Game::Visuals::building_asset_key(owner.nation_id, "wall_ladder")
               : (gate ? WallNetworkService::resolve_gate_appearance(
                             owner.nation_id, segment.connection_mask, segment.rotation_y)
                       : WallNetworkService::resolve_appearance(owner.nation_id,
                                                                segment.connection_mask))
                     .renderer_id;

    preview->owner_id = owner.owner_id;
    preview->nation_id = owner.nation_id;
    preview->grid_x = segment.grid_x;
    preview->grid_z = segment.grid_z;
    preview->valid = segment.valid;
    preview->product_type = product_type;

    m_entity_ids.push_back(entity->get_id());
  }
}

void ConstructionPreview::set_active(bool active) {
  if (m_active == active) {
    return;
  }
  m_active = active;
  if (!active) {
    set_reason({});
  }
  emit active_changed();
}

void ConstructionPreview::set_valid(bool valid) {
  if (m_valid == valid) {
    return;
  }
  m_valid = valid;
  emit valid_changed();
}

void ConstructionPreview::set_reason(const QString& reason) {
  if (m_reason == reason) {
    return;
  }
  m_reason = reason;
  emit reason_changed();
}

void ConstructionPreview::set_ruling(bool valid, const QString& reason) {
  set_valid(valid);
  set_reason(valid ? QString() : reason);
}

void ConstructionPreview::set_summary(int segment_count,
                                      int valid_segment_count,
                                      int total_cost) {
  if (m_segment_count == segment_count &&
      m_valid_segment_count == valid_segment_count && m_total_cost == total_cost) {
    return;
  }
  m_segment_count = segment_count;
  m_valid_segment_count = valid_segment_count;
  m_total_cost = total_cost;
  emit summary_changed();
}

} // namespace App::Economy
