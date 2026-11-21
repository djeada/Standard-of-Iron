#pragma once

#include <span>
#include <string_view>

#include "render/entity/ambient_people.h"
#include "render/entity/building_render_common.h"
#include "render/entity/building_state.h"
#include "render/entity/building_torches.h"
#include "render/entity/registry.h"
#include "render/render_archetype.h"

namespace Render::GL {

inline constexpr float k_temple_mesh_scale = 2.0F;

using TempleArchetypeResolver = const RenderArchetype& (*)(BuildingState);

struct TempleRendererConfig {
  std::string_view nation_slug;
  TempleArchetypeResolver archetype;
  BuildingSelectionStyle selection;
  std::span<const TorchMount> torches{};
  std::span<const AmbientPerson> people{};
  std::span<const WalkSurface> walk_surfaces{};
  std::span<const QVector3D> incense{};
};

void register_temple_renderer_variant(EntityRendererRegistry& registry,
                                      const TempleRendererConfig& config);

} // namespace Render::GL
