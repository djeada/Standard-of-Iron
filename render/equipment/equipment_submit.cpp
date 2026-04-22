#include "equipment_submit.h"

#include "../render_archetype.h"
#include "horse/i_horse_equipment_renderer.h"
#include "i_equipment_renderer.h"

namespace Render::GL {

void append_equipment_archetype(
    EquipmentBatch &batch, const RenderArchetype &archetype,
    const QMatrix4x4 &world, std::span<const QVector3D> palette,
    Texture *default_texture, float alpha_multiplier, RenderArchetypeLod lod) {
  assert(palette.size() <= kEquipmentArchetypePaletteCapacity);

  EquipmentArchetypePrim prim;
  prim.archetype = &archetype;
  prim.world = world;
  prim.palette_count = static_cast<std::uint8_t>(palette.size());
  std::copy(palette.begin(), palette.end(), prim.palette.begin());
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
    RenderInstance instance;
    instance.archetype = a.archetype;
    instance.world = a.world;
    instance.palette =
        std::span<const QVector3D>(a.palette.data(), a.palette_count);
    instance.default_texture = a.default_texture;
    instance.alpha_multiplier = a.alpha_multiplier;
    instance.lod = a.lod;
    submit_render_instance(out, instance);
  }
  for (const auto &c : batch.cylinders) {
    out.cylinder(c.start, c.end, c.radius, c.color, c.alpha);
  }
}

void render_equipment(IEquipmentRenderer &renderer, const DrawContext &ctx,
                      const BodyFrames &frames, const HumanoidPalette &palette,
                      const HumanoidAnimationContext &anim,
                      ISubmitter &out) noexcept {
  EquipmentBatch batch;
  batch.reserve(64, 8, 8);
  renderer.render(ctx, frames, palette, anim, batch);
  submit_equipment_batch(batch, out);
}

void render_horse_equipment(const IHorseEquipmentRenderer &renderer,
                            const DrawContext &ctx,
                            const HorseBodyFrames &frames,
                            const HorseVariant &variant,
                            const HorseAnimationContext &anim,
                            ISubmitter &out) noexcept {
  EquipmentBatch batch;
  batch.reserve(32, 8, 8);
  renderer.render(ctx, frames, variant, anim, batch);
  submit_equipment_batch(batch, out);
}

} // namespace Render::GL
