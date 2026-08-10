#include "render/gl/backend/prop_parts.h"

namespace Render::GL::BackendPipelines {

namespace {
[[nodiscard]] auto to_vector(const PropPoint& point) noexcept -> QVector3D {
  return {point.x, point.y, point.z};
}
} // namespace

void append_parts(PropMeshVerts& verts,
                  PropMeshIndices& idx,
                  std::span<const PropSlabPart> parts) {
  for (const auto& part : parts) {
    append_prop_slab(verts, idx, part.y0, part.y1, part.half_bottom, part.half_top);
  }
}

void append_parts(PropMeshVerts& verts,
                  PropMeshIndices& idx,
                  std::span<const PropLimbPart> parts) {
  for (const auto& part : parts) {
    append_prop_limb(verts,
                     idx,
                     to_vector(part.a),
                     to_vector(part.b),
                     part.r0,
                     part.r1,
                     part.segments);
  }
}

void append_parts(PropMeshVerts& verts,
                  PropMeshIndices& idx,
                  std::span<const PropBeamPart> parts) {
  for (const auto& part : parts) {
    append_prop_beam(verts,
                     idx,
                     to_vector(part.a),
                     to_vector(part.b),
                     part.half_width,
                     part.half_depth);
  }
}

void append_parts(PropMeshVerts& verts,
                  PropMeshIndices& idx,
                  std::span<const PropOrientedBoxPart> parts) {
  for (const auto& part : parts) {
    append_oriented_box(verts,
                        idx,
                        to_vector(part.a),
                        to_vector(part.b),
                        part.half_width,
                        part.half_depth);
  }
}

void append_parts(PropMeshVerts& verts,
                  PropMeshIndices& idx,
                  std::span<const PropBoxPart> parts) {
  for (const auto& part : parts) {
    append_box(verts, idx, to_vector(part.lo), to_vector(part.hi));
  }
}

void append_parts(PropMeshVerts& verts,
                  PropMeshIndices& idx,
                  std::span<const PropFrustumPart> parts) {
  for (const auto& part : parts) {
    append_prop_frustum(verts,
                        idx,
                        part.cx,
                        part.y0,
                        part.cz,
                        part.rx0,
                        part.rz0,
                        part.rx1,
                        part.rz1,
                        part.height,
                        part.segments);
  }
}

void append_parts(PropMeshVerts& verts,
                  PropMeshIndices& idx,
                  std::span<const PropTaperPart> parts) {
  for (const auto& part : parts) {
    append_prop_taper(verts,
                      idx,
                      part.cx,
                      part.y0,
                      part.cz,
                      part.r0,
                      part.r1,
                      part.height,
                      part.segments);
  }
}

void append_parts(PropMeshVerts& verts,
                  PropMeshIndices& idx,
                  std::span<const PropVertPrismPart> parts) {
  for (const auto& part : parts) {
    append_vert_prism(
        verts, idx, part.cx, part.y0, part.cz, part.r, part.height, part.segments);
  }
}

} // namespace Render::GL::BackendPipelines
