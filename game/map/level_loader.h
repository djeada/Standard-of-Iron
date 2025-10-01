#pragma once

#include <QString>
#include <memory>

namespace Engine { namespace Core { class World; using EntityID = unsigned int; } }
namespace Render { namespace GL { class Renderer; class Camera; } }

namespace Game { namespace Map {

struct LevelLoadResult {
    bool ok = false;
    QString mapName;
    Engine::Core::EntityID playerUnitId = 0;
    float camFov = 45.0f;
    float camNear = 0.1f;
    float camFar = 1000.0f;
};

class LevelLoader {
public:
    // Loads visuals, installs unit factories, loads map, applies environment, populates world. Falls back to default env if needed.
    static LevelLoadResult loadFromAssets(const QString& mapPath,
                                          Engine::Core::World& world,
                                          Render::GL::Renderer& renderer,
                                          Render::GL::Camera& camera);
};

} } // namespace Game::Map
