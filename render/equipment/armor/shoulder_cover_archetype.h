#pragma once

#include "../equipment_submit.h"

#include "../../entity/registry.h"
#include "../../gl/primitives.h"
#include "../../render_archetype.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace Render::GL {

struct ShoulderCoverLobe {
  QVector3D center{0.0F, 0.0F, 0.0F};
  QVector3D scale{1.0F, 1.0F, 1.0F};
  std::uint8_t palette_slot{0U};
  float alpha{1.0F};
  int material_id{1};
};

inline auto build_shoulder_cover_archetype(
    std::string_view debug_name,
    std::span<const ShoulderCoverLobe> lobes) -> RenderArchetype {
  RenderArchetypeBuilder builder{std::string(debug_name)};
  for (const auto &lobe : lobes) {
    builder.add_palette_mesh(
        get_unit_sphere(), box_local_model(lobe.center, lobe.scale),
        lobe.palette_slot, nullptr, lobe.alpha, lobe.material_id);
  }
  return std::move(builder).build();
}

template <std::size_t Count>
inline auto build_shoulder_cover_archetype(
    std::string_view debug_name,
    const std::array<ShoulderCoverLobe, Count> &lobes) -> RenderArchetype {
  return build_shoulder_cover_archetype(
      debug_name,
      std::span<const ShoulderCoverLobe>(lobes.data(), lobes.size()));
}

inline auto make_shoulder_cover_transform(const QMatrix4x4 &parent,
                                          const QVector3D &origin,
                                          const QVector3D &outward,
                                          const QVector3D &up) -> QMatrix4x4 {
  QVector3D right_axis = outward.normalized();
  QVector3D up_axis = up.normalized();
  QVector3D forward_axis = QVector3D::crossProduct(right_axis, up_axis);
  if (forward_axis.lengthSquared() < 1e-5F) {
    forward_axis = QVector3D(0.0F, 0.0F, 1.0F);
  } else {
    forward_axis.normalize();
  }

  QMatrix4x4 local;
  local.setColumn(0, QVector4D(right_axis, 0.0F));
  local.setColumn(1, QVector4D(up_axis, 0.0F));
  local.setColumn(2, QVector4D(forward_axis, 0.0F));
  local.setColumn(3, QVector4D(origin, 1.0F));
  return parent * local;
}

inline void append_shoulder_cover_archetype(
    EquipmentBatch &batch, const DrawContext &ctx, const QVector3D &origin,
    const QVector3D &outward, const QVector3D &up,
    const RenderArchetype &archetype, std::span<const QVector3D> palette = {},
    Texture *default_texture = nullptr, float alpha_multiplier = 1.0F,
    RenderArchetypeLod lod = RenderArchetypeLod::Full) {
  append_equipment_archetype(
      batch, archetype,
      make_shoulder_cover_transform(ctx.model, origin, outward, up), palette,
      default_texture, alpha_multiplier, lod);
}

} // namespace Render::GL
