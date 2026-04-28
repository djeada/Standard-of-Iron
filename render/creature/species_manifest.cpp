#include "species_manifest.h"

namespace Render::Creature {

namespace {

auto is_excluded_node(
    std::string_view debug_name,
    std::span<const std::string_view> excluded_prefixes) noexcept -> bool {
  for (std::string_view prefix : excluded_prefixes) {
    if (!prefix.empty() && debug_name.rfind(prefix, 0) == 0) {
      return true;
    }
  }
  return false;
}

} // namespace

auto compile_whole_mesh_lod(const WholeMeshLodManifest &manifest)
    -> CompiledWholeMeshLod {
  CompiledWholeMeshLod compiled;
  std::vector<Render::Creature::Quadruped::MeshNode> filtered_nodes;
  filtered_nodes.reserve(manifest.mesh_nodes.size());
  for (auto const &node : manifest.mesh_nodes) {
    if (is_excluded_node(node.debug_name, manifest.excluded_node_name_prefixes)) {
      continue;
    }
    filtered_nodes.push_back(node);
  }

  compiled.mesh =
      Render::Creature::Quadruped::compile_combined_mesh_graph(filtered_nodes);
  compiled.primitives.reserve(1U + manifest.overlay_primitives.size());

  if (compiled.mesh != nullptr && !compiled.mesh->get_indices().empty()) {
    PrimitiveInstance primitive{};
    primitive.debug_name = manifest.primitive_name;
    primitive.shape = PrimitiveShape::Mesh;
    primitive.params.anchor_bone = manifest.anchor_bone;
    primitive.params.half_extents = QVector3D(1.0F, 1.0F, 1.0F);
    primitive.custom_mesh = compiled.mesh.get();
    primitive.mesh_skinning = manifest.mesh_skinning;
    primitive.color_role = manifest.color_role;
    primitive.material_id = manifest.material_id;
    primitive.lod_mask = manifest.lod_mask;
    compiled.primitives.push_back(primitive);
  }

  compiled.primitives.insert(compiled.primitives.end(),
                             manifest.overlay_primitives.begin(),
                             manifest.overlay_primitives.end());
  return compiled;
}

} // namespace Render::Creature
