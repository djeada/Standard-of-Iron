#include "render_archetype.h"

#include "geom/transforms.h"
#include "gl/primitives.h"
#include "submitter.h"

#include <array>
#include <utility>

namespace Render::GL {
namespace {

auto lod_index(RenderArchetypeLod lod) -> std::size_t {
  return static_cast<std::size_t>(lod);
}

void expand_unit_bounds(BoundingBox &bounds, const QMatrix4x4 &transform) {
  static const std::array<QVector3D, 8> k_unit_corners = {
      QVector3D(-0.5F, -0.5F, -0.5F), QVector3D(0.5F, -0.5F, -0.5F),
      QVector3D(-0.5F, 0.5F, -0.5F),  QVector3D(0.5F, 0.5F, -0.5F),
      QVector3D(-0.5F, -0.5F, 0.5F),  QVector3D(0.5F, -0.5F, 0.5F),
      QVector3D(-0.5F, 0.5F, 0.5F),   QVector3D(0.5F, 0.5F, 0.5F)};

  for (const QVector3D &corner : k_unit_corners) {
    bounds.expand(transform.map(corner));
  }
}

} // namespace

auto empty_bounding_box() -> BoundingBox {
  BoundingBox bounds;
  float const inf = std::numeric_limits<float>::infinity();
  bounds.min = QVector3D(inf, inf, inf);
  bounds.max = QVector3D(-inf, -inf, -inf);
  return bounds;
}

auto box_local_model(const QVector3D &center, const QVector3D &scale)
    -> QMatrix4x4 {
  QMatrix4x4 model;
  model.translate(center);
  model.scale(scale);
  return model;
}

auto cylinder_local_model(const QVector3D &start, const QVector3D &end,
                          float radius) -> QMatrix4x4 {
  return Render::Geom::cylinder_between(start, end, radius);
}

auto select_render_archetype_lod(const RenderArchetype &archetype, float distance)
    -> RenderArchetypeLod {
  RenderArchetypeLod fallback = RenderArchetypeLod::Full;
  bool has_fallback = false;

  for (RenderArchetypeLod lod : {RenderArchetypeLod::Full,
                                 RenderArchetypeLod::Reduced,
                                 RenderArchetypeLod::Minimal}) {
    const RenderArchetypeSlice &slice = archetype.lods[lod_index(lod)];
    if (slice.draws.empty()) {
      continue;
    }
    if (!has_fallback) {
      fallback = lod;
      has_fallback = true;
    }
    if (distance <= slice.max_distance) {
      return lod;
    }
    fallback = lod;
  }

  return fallback;
}

void submit_render_instance(ISubmitter &out, const RenderInstance &instance) {
  if (instance.archetype == nullptr) {
    return;
  }

  const RenderArchetypeSlice &slice =
      instance.archetype->lods[lod_index(instance.lod)];
  for (const RenderArchetypeDraw &draw : slice.draws) {
    QVector3D color = draw.color;
    if (draw.palette_slot != kRenderArchetypeFixedColorSlot &&
        draw.palette_slot < instance.palette.size()) {
      color = instance.palette[draw.palette_slot];
    }

    QMatrix4x4 world = instance.world * draw.local_model;
    float const alpha = draw.alpha * instance.alpha_multiplier;
    Texture *texture =
        (draw.texture != nullptr) ? draw.texture : instance.default_texture;

    if (draw.material != nullptr) {
      out.part(draw.mesh, draw.material, world, color, texture, alpha,
               draw.material_id);
      continue;
    }
    out.mesh(draw.mesh, world, color, texture, alpha, draw.material_id);
  }
}

RenderArchetypeBuilder::RenderArchetypeBuilder(std::string name) {
  m_archetype.debug_name = std::move(name);
  for (RenderArchetypeSlice &slice : m_archetype.lods) {
    slice.local_bounds = empty_bounding_box();
  }
}

void RenderArchetypeBuilder::use_lod(RenderArchetypeLod lod) {
  m_active_lod = lod;
}

void RenderArchetypeBuilder::set_max_distance(float max_distance) {
  m_archetype.lods[lod_index(m_active_lod)].max_distance = max_distance;
}

void RenderArchetypeBuilder::add_mesh(Mesh *mesh, const QMatrix4x4 &local_model,
                                      const QVector3D &color, Texture *texture,
                                      float alpha, int material_id,
                                      Material *material) {
  RenderArchetypeDraw draw;
  draw.mesh = mesh;
  draw.material = material;
  draw.local_model = local_model;
  draw.color = color;
  draw.texture = texture;
  draw.alpha = alpha;
  draw.material_id = material_id;
  add_draw(std::move(draw));
}

void RenderArchetypeBuilder::add_palette_mesh(Mesh *mesh,
                                              const QMatrix4x4 &local_model,
                                              std::uint8_t palette_slot,
                                              Texture *texture, float alpha,
                                              int material_id,
                                              Material *material) {
  RenderArchetypeDraw draw;
  draw.mesh = mesh;
  draw.material = material;
  draw.local_model = local_model;
  draw.texture = texture;
  draw.alpha = alpha;
  draw.material_id = material_id;
  draw.palette_slot = palette_slot;
  add_draw(std::move(draw));
}

void RenderArchetypeBuilder::add_box(const QVector3D &center,
                                     const QVector3D &scale,
                                     const QVector3D &color, Texture *texture,
                                     float alpha, int material_id,
                                     Material *material) {
  add_mesh(get_unit_cube(), box_local_model(center, scale), color, texture,
           alpha, material_id, material);
}

void RenderArchetypeBuilder::add_palette_box(const QVector3D &center,
                                             const QVector3D &scale,
                                             std::uint8_t palette_slot,
                                             Texture *texture, float alpha,
                                             int material_id,
                                             Material *material) {
  add_palette_mesh(get_unit_cube(), box_local_model(center, scale), palette_slot,
                   texture, alpha, material_id, material);
}

void RenderArchetypeBuilder::add_cylinder(const QVector3D &start,
                                          const QVector3D &end, float radius,
                                          const QVector3D &color,
                                          Texture *texture, float alpha,
                                          int material_id,
                                          Material *material) {
  add_mesh(get_unit_cylinder(), cylinder_local_model(start, end, radius), color,
           texture, alpha, material_id, material);
}

void RenderArchetypeBuilder::add_palette_cylinder(const QVector3D &start,
                                                  const QVector3D &end,
                                                  float radius,
                                                  std::uint8_t palette_slot,
                                                  Texture *texture,
                                                  float alpha, int material_id,
                                                  Material *material) {
  add_palette_mesh(get_unit_cylinder(),
                   cylinder_local_model(start, end, radius), palette_slot,
                   texture, alpha, material_id, material);
}

auto RenderArchetypeBuilder::build() && -> RenderArchetype {
  return std::move(m_archetype);
}

void RenderArchetypeBuilder::add_draw(RenderArchetypeDraw draw) {
  RenderArchetypeSlice &slice = m_archetype.lods[lod_index(m_active_lod)];
  expand_unit_bounds(slice.local_bounds, draw.local_model);
  slice.draws.push_back(std::move(draw));
}

} // namespace Render::GL
