#include "equipment_submit.h"

#include "../render_archetype.h"

namespace Render::GL {

void append_equipment_archetype(
    EquipmentBatch &batch, const RenderArchetype &archetype,
    const QMatrix4x4 &world, std::span<const QVector3D> palette,
    Texture *default_texture, float alpha_multiplier, RenderArchetypeLod lod) {
  assert(palette.size() <= k_equipment_archetype_palette_capacity);

  EquipmentArchetypePrim prim;
  prim.archetype = &archetype;
  prim.world = world;
  prim.set_palette(palette);
  prim.default_texture = default_texture;
  prim.alpha_multiplier = alpha_multiplier;
  prim.lod = lod;
  batch.archetypes.push_back(prim);
}

void submit_equipment_batch(const EquipmentBatch &batch,
                            ISubmitter &out) noexcept {
  for (const auto &p : batch.meshes) {
    if (p.mesh == nullptr) {
      continue;
    }
    if (p.material != nullptr) {
      out.part(p.mesh, p.material, p.model, p.color, p.texture, p.alpha,
               p.material_id);
    } else {
      out.mesh(p.mesh, p.model, p.color, p.texture, p.alpha, p.material_id);
    }
  }
  for (const auto &a : batch.archetypes) {
    if (a.archetype == nullptr) {
      continue;
    }
    submit_render_instance(out, a.render_instance());
  }
  for (const auto &c : batch.cylinders) {
    out.cylinder(c.start, c.end, c.radius, c.color, c.alpha);
  }
}

} // namespace Render::GL
