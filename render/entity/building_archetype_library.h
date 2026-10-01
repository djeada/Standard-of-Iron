#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "building_archetype_desc.h"
#include "building_state.h"
#include "render/static_mesh_pack.h"

namespace Render::GL {

[[nodiscard]] auto
building_archetype_set(std::string_view catalog_name) -> const BuildingArchetypeSet&;

[[nodiscard]] auto building_mesh_key(std::string_view catalog_name,
                                     BuildingState state) -> std::string;

void request_nation_buildings(std::string_view nation_slug);

[[nodiscard]] auto
requested_building_meshes() -> std::vector<std::shared_ptr<const MergedBuildingMesh>>;

[[nodiscard]] auto
building_source_parts(std::string_view mesh_key) -> const RenderArchetype*;

[[nodiscard]] auto bake_building_meshes() -> std::vector<StaticMeshPackEntry>;

inline constexpr std::string_view k_building_mesh_pack = "buildings.smpk";

} // namespace Render::GL
