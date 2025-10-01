#pragma once

#include <QVector3D>

namespace Engine { namespace Core { class World; } }
namespace Render { namespace GL { class Camera; } }
namespace Game { namespace Systems { class SelectionSystem; } }

namespace Game { namespace Systems {

class CameraFollowSystem {
public:
    void update(Engine::Core::World& world,
                SelectionSystem& selection,
                Render::GL::Camera& camera);

    void snapToSelection(Engine::Core::World& world,
                         SelectionSystem& selection,
                         Render::GL::Camera& camera);
};

} } // namespace Game::Systems
