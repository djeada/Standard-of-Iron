#include "camera_follow_system.h"
#include "../../render/gl/camera.h"
#include "../core/component.h"
#include "../core/world.h"
#include "selection_system.h"
#include <qvectornd.h>

namespace Game::Systems {

void CameraFollowSystem::update(Engine::Core::World &world,
                                SelectionSystem &selection,
                                Render::GL::Camera &camera) {
  const auto &sel = selection.get_selected_units();
  if (sel.empty()) {
    return;
  }
  QVector3D sum(0, 0, 0);
  int count = 0;
  for (auto id : sel) {
    if (auto *e = world.get_entity(id)) {
      if (auto *t = e->get_component<Engine::Core::TransformComponent>()) {
        sum += QVector3D(t->position.x, t->position.y, t->position.z);
        ++count;
      }
    }
  }
  if (count > 0) {
    QVector3D const center = sum / float(count);
    camera.update_follow(center);
  }
}

void CameraFollowSystem::snapToSelection(Engine::Core::World &world,
                                         SelectionSystem &selection,
                                         Render::GL::Camera &camera) {
  const auto &sel = selection.get_selected_units();
  if (sel.empty()) {
    return;
  }
  QVector3D sum(0, 0, 0);
  int count = 0;
  for (auto id : sel) {
    if (auto *e = world.get_entity(id)) {
      if (auto *t = e->get_component<Engine::Core::TransformComponent>()) {
        sum += QVector3D(t->position.x, t->position.y, t->position.z);
        ++count;
      }
    }
  }
  if (count > 0) {
    QVector3D const target = sum / float(count);
    camera.setTarget(target);
    camera.capture_follow_offset();
  }
}

} // namespace Game::Systems
