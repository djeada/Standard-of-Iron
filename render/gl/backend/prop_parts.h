#pragma once

#include <cstdint>
#include <span>

#include "render/gl/backend/prop_mesh_builder.h"

namespace Render::GL::BackendPipelines {

// A constexpr-friendly point. QVector3D cannot be built at compile time, so
// authored part tables use this and convert on the way into the mesh builder.
struct PropPoint {
  float x{0.0F};
  float y{0.0F};
  float z{0.0F};
};

// One authored primitive, in the same terms as the append_* builders. Props are
// hand-placed assemblies of these, so writing them as tables keeps the numbers
// that describe a shape separate from the code that turns them into triangles.

// A horizontal slab with a square cross-section that tapers between two
// half-extents: plinth courses, cornices, steps.
struct PropSlabPart {
  float y0{0.0F};
  float y1{0.0F};
  float half_bottom{0.0F};
  float half_top{0.0F};
};

// A tapered cylinder between two points: arms, legs, torsos, columns.
struct PropLimbPart {
  PropPoint a;
  PropPoint b;
  float r0{0.0F};
  float r1{0.0F};
  int segments{8};
};

// A rectangular bar between two points: joists, rails, planks.
struct PropBeamPart {
  PropPoint a;
  PropPoint b;
  float half_width{0.0F};
  float half_depth{0.0F};
};

// Same shape as a beam, but built with the older normal convention. Kept apart
// so existing geometry is not silently re-shaded.
struct PropOrientedBoxPart {
  PropPoint a;
  PropPoint b;
  float half_width{0.0F};
  float half_depth{0.0F};
};

// An axis-aligned box between two corners.
struct PropBoxPart {
  PropPoint lo;
  PropPoint hi;
};

// An elliptical frustum standing on the Y axis, with independent X and Z radii
// at each end.
struct PropFrustumPart {
  float cx{0.0F};
  float y0{0.0F};
  float cz{0.0F};
  float rx0{0.0F};
  float rz0{0.0F};
  float rx1{0.0F};
  float rz1{0.0F};
  float height{0.0F};
  int segments{8};
};

// A circular taper standing on the Y axis.
struct PropTaperPart {
  float cx{0.0F};
  float y0{0.0F};
  float cz{0.0F};
  float r0{0.0F};
  float r1{0.0F};
  float height{0.0F};
  int segments{8};
};

// A vertical prism standing on the Y axis.
struct PropVertPrismPart {
  float cx{0.0F};
  float y0{0.0F};
  float cz{0.0F};
  float r{0.0F};
  float height{0.0F};
  int segments{6};
};

// Emits every part in an authored table into the mesh being built. Order within
// a table is preserved; order between tables does not matter, because a prop is
// one opaque mesh.
void append_parts(PropMeshVerts& verts,
                  PropMeshIndices& idx,
                  std::span<const PropSlabPart> parts);
void append_parts(PropMeshVerts& verts,
                  PropMeshIndices& idx,
                  std::span<const PropLimbPart> parts);
void append_parts(PropMeshVerts& verts,
                  PropMeshIndices& idx,
                  std::span<const PropBeamPart> parts);
void append_parts(PropMeshVerts& verts,
                  PropMeshIndices& idx,
                  std::span<const PropOrientedBoxPart> parts);
void append_parts(PropMeshVerts& verts,
                  PropMeshIndices& idx,
                  std::span<const PropBoxPart> parts);
void append_parts(PropMeshVerts& verts,
                  PropMeshIndices& idx,
                  std::span<const PropFrustumPart> parts);
void append_parts(PropMeshVerts& verts,
                  PropMeshIndices& idx,
                  std::span<const PropTaperPart> parts);
void append_parts(PropMeshVerts& verts,
                  PropMeshIndices& idx,
                  std::span<const PropVertPrismPart> parts);

} // namespace Render::GL::BackendPipelines
