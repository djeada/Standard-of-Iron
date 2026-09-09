#pragma once

#include "render/gl/backend/dead_tree_mesh.h"

namespace Render::GL::BackendPipelines {

inline constexpr float k_cursed_gold_vein_rock_crown = 0.46F;

[[nodiscard]] auto build_cursed_gold_vein_mesh() -> PropMeshData;

} // namespace Render::GL::BackendPipelines
