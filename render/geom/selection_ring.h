#pragma once

#include "../gl/mesh.h"
#include <memory>

namespace Render::Geom {

// Lazily creates and caches a selection ring mesh (annulus) shared across renderers
class SelectionRing {
public:
    static Render::GL::Mesh* get();
private:
    static std::unique_ptr<Render::GL::Mesh> s_mesh;
};

} // namespace Render::Geom
