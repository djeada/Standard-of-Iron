#pragma once

#include <QMatrix4x4>

namespace Render::GL {

class Mesh;

auto torso_mesh_without_bottom_cap() -> Mesh *;
void align_torso_mesh_forward(QMatrix4x4 &model) noexcept;

} // namespace Render::GL
