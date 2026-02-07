#include "unit_render_cache.h"

#include "../game/core/component.h"
#include "../game/core/entity.h"
#include "../game/units/spawn_type.h"
#include <QMatrix4x4>

namespace Render {

auto UnitRenderCache::get_or_create(std::uint32_t entity_id,
                                    Engine::Core::Entity *entity,
                                    std::uint32_t frame) -> CachedUnitData & {
  auto [it, inserted] = m_cache.emplace(entity_id, CachedUnitData{});
  auto &data = it->second;

  data.entity_id = entity_id;
  data.last_seen_frame = frame;

  if (inserted || data.entity != entity) {
    data.entity = entity;
    data.model_matrix_valid = false;
  }

  if (entity == nullptr) {
    data.transform = nullptr;
    data.unit = nullptr;
    data.renderable = nullptr;
    data.movement = nullptr;
    data.renderer_key.clear();
    return data;
  }

  auto *new_transform =
      entity->get_component<Engine::Core::TransformComponent>();
  auto *new_unit = entity->get_component<Engine::Core::UnitComponent>();
  auto *new_renderable =
      entity->get_component<Engine::Core::RenderableComponent>();
  auto *new_movement = entity->get_component<Engine::Core::MovementComponent>();

  if (data.transform != new_transform) {
    data.model_matrix_valid = false;
  }

  data.transform = new_transform;
  data.unit = new_unit;
  data.renderable = new_renderable;
  data.movement = new_movement;

  if (data.renderable != nullptr && !data.renderable->renderer_id.empty()) {
    data.renderer_key = data.renderable->renderer_id;
  } else if (data.unit != nullptr) {
    data.renderer_key = Game::Units::spawn_typeToString(data.unit->spawn_type);
  } else {
    data.renderer_key.clear();
  }

  return data;
}

void UnitRenderCache::prune(std::uint32_t current_frame,
                            std::uint32_t max_age) {
  auto it = m_cache.begin();
  while (it != m_cache.end()) {
    if (current_frame - it->second.last_seen_frame > max_age) {
      it = m_cache.erase(it);
    } else {
      ++it;
    }
  }
}

auto UnitRenderCache::update_model_matrix(CachedUnitData &data) -> bool {
  if (data.transform == nullptr) {
    return false;
  }

  const auto &pos = data.transform->position;
  const auto &rot = data.transform->rotation;
  const auto &sc = data.transform->scale;

  if (data.model_matrix_valid && pos.x == data.last_pos_x &&
      pos.y == data.last_pos_y && pos.z == data.last_pos_z &&
      rot.x == data.last_rot_x && rot.y == data.last_rot_y &&
      rot.z == data.last_rot_z && sc.x == data.last_scale_x &&
      sc.y == data.last_scale_y && sc.z == data.last_scale_z) {
    return false;
  }

  QMatrix4x4 m;
  m.translate(pos.x, pos.y, pos.z);
  m.rotate(rot.x, 1.0F, 0.0F, 0.0F);
  m.rotate(rot.y, 0.0F, 1.0F, 0.0F);
  m.rotate(rot.z, 0.0F, 0.0F, 1.0F);
  m.scale(sc.x, sc.y, sc.z);
  data.model_matrix = m;

  data.last_pos_x = pos.x;
  data.last_pos_y = pos.y;
  data.last_pos_z = pos.z;
  data.last_rot_x = rot.x;
  data.last_rot_y = rot.y;
  data.last_rot_z = rot.z;
  data.last_scale_x = sc.x;
  data.last_scale_y = sc.y;
  data.last_scale_z = sc.z;
  data.model_matrix_valid = true;
  return true;
}

auto ModelMatrixCache::get_or_create(
    std::uint32_t entity_id, Engine::Core::TransformComponent *transform,
    std::uint32_t frame) -> const QMatrix4x4 & {
  auto &entry = m_cache[entity_id];
  entry.last_seen_frame = frame;

  if (transform == nullptr) {
    return entry.matrix;
  }

  const auto &pos = transform->position;
  const auto &rot = transform->rotation;
  const auto &sc = transform->scale;

  if (entry.valid && pos.x == entry.last_pos_x && pos.y == entry.last_pos_y &&
      pos.z == entry.last_pos_z && rot.x == entry.last_rot_x &&
      rot.y == entry.last_rot_y && rot.z == entry.last_rot_z &&
      sc.x == entry.last_scale_x && sc.y == entry.last_scale_y &&
      sc.z == entry.last_scale_z) {
    return entry.matrix;
  }

  QMatrix4x4 m;
  m.translate(pos.x, pos.y, pos.z);
  m.rotate(rot.x, 1.0F, 0.0F, 0.0F);
  m.rotate(rot.y, 0.0F, 1.0F, 0.0F);
  m.rotate(rot.z, 0.0F, 0.0F, 1.0F);
  m.scale(sc.x, sc.y, sc.z);
  entry.matrix = m;

  entry.last_pos_x = pos.x;
  entry.last_pos_y = pos.y;
  entry.last_pos_z = pos.z;
  entry.last_rot_x = rot.x;
  entry.last_rot_y = rot.y;
  entry.last_rot_z = rot.z;
  entry.last_scale_x = sc.x;
  entry.last_scale_y = sc.y;
  entry.last_scale_z = sc.z;
  entry.valid = true;
  return entry.matrix;
}

void ModelMatrixCache::prune(std::uint32_t current_frame,
                             std::uint32_t max_age) {
  auto it = m_cache.begin();
  while (it != m_cache.end()) {
    if (current_frame - it->second.last_seen_frame > max_age) {
      it = m_cache.erase(it);
    } else {
      ++it;
    }
  }
}

} // namespace Render
