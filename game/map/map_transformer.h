#pragma once

#include "map_definition.h"
#include "../visuals/visual_catalog.h"
#include <memory>

namespace Engine { namespace Core { class World; using EntityID = unsigned int; } }

namespace Game::Map {

struct MapRuntime {
    std::vector<Engine::Core::EntityID> unitIds;
};

class MapTransformer {
public:
    // Populates the world from a MapDefinition. Returns runtime info like created entity IDs.
    static MapRuntime applyToWorld(const MapDefinition& def, Engine::Core::World& world, const Game::Visuals::VisualCatalog* visuals = nullptr);
};

} // namespace Game::Map
