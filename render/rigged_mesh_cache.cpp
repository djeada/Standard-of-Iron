#include "rigged_mesh_cache.h"

#include "creature/spec.h"

namespace Render::GL {

auto RiggedMeshCache::get_or_bake(
    const Render::Creature::CreatureSpec &spec,
    Render::Creature::CreatureLOD lod, std::span<const QMatrix4x4> rest_palette,
    std::uint16_t variant_bucket) -> const RiggedMeshEntry * {
  Key const key{&spec, lod, variant_bucket};
  if (auto it = m_entries.find(key); it != m_entries.end()) {
    return &it->second;
  }

  RiggedMeshEntry entry;

  Render::Creature::BakeInput input{};
  input.graph = &Render::Creature::part_graph_for(spec, lod);
  input.bind_pose = rest_palette;

  entry.mesh = Render::Creature::bake_rigged_mesh(input);

  entry.inverse_bind.reserve(rest_palette.size());
  for (const auto &m : rest_palette) {
    entry.inverse_bind.push_back(m.inverted());
  }

  auto [it, _] = m_entries.emplace(key, std::move(entry));
  return &it->second;
}

} // namespace Render::GL
