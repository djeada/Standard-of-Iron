#include "camera_follow_system.h"
#include "selection_system.h"
#include "../core/world.h"
#include "../core/component.h"
#include "../../render/gl/camera.h"

namespace Game { namespace Systems {

void CameraFollowSystem::update(Engine::Core::World& world,
                                SelectionSystem& selection,
                                Render::GL::Camera& camera) {
    const auto& sel = selection.getSelectedUnits();
    if (sel.empty()) return;
    QVector3D sum(0,0,0); int count = 0;
    for (auto id : sel) {
        if (auto* e = world.getEntity(id)) {
            if (auto* t = e->getComponent<Engine::Core::TransformComponent>()) {
                sum += QVector3D(t->position.x, t->position.y, t->position.z);
                ++count;
            }
        }
    }
    if (count > 0) {
        QVector3D center = sum / float(count);
        camera.setTarget(center);
        camera.updateFollow(center);
    }
}

void CameraFollowSystem::snapToSelection(Engine::Core::World& world,
                                         SelectionSystem& selection,
                                         Render::GL::Camera& camera) {
    const auto& sel = selection.getSelectedUnits();
    if (sel.empty()) return;
    QVector3D sum(0,0,0); int count = 0;
    for (auto id : sel) {
        if (auto* e = world.getEntity(id)) {
            if (auto* t = e->getComponent<Engine::Core::TransformComponent>()) {
                sum += QVector3D(t->position.x, t->position.y, t->position.z);
                ++count;
            }
        }
    }
    if (count > 0) {
        QVector3D target = sum / float(count);
        camera.setTarget(target);
        camera.captureFollowOffset();
    }
}

} } // namespace Game::Systems
