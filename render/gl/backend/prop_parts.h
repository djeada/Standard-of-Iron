#pragma once

#include <cstdint>
#include <span>

#include "render/gl/backend/prop_mesh_builder.h"

namespace Render::GL::BackendPipelines {

struct PropPoint {
  float x{0.0F};
  float y{0.0F};
  float z{0.0F};
};

struct PropSlabPart {
  float y0{0.0F};
  float y1{0.0F};
  float half_bottom{0.0F};
  float half_top{0.0F};
};

struct PropLimbPart {
  PropPoint a;
  PropPoint b;
  float r0{0.0F};
  float r1{0.0F};
  int segments{8};
};

struct PropBeamPart {
  PropPoint a;
  PropPoint b;
  float half_width{0.0F};
  float half_depth{0.0F};
};

struct PropOrientedBoxPart {
  PropPoint a;
  PropPoint b;
  float half_width{0.0F};
  float half_depth{0.0F};
};

struct PropBoxPart {
  PropPoint lo;
  PropPoint hi;
};

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

struct PropTaperPart {
  float cx{0.0F};
  float y0{0.0F};
  float cz{0.0F};
  float r0{0.0F};
  float r1{0.0F};
  float height{0.0F};
  int segments{8};
};

struct PropVertPrismPart {
  float cx{0.0F};
  float y0{0.0F};
  float cz{0.0F};
  float r{0.0F};
  float height{0.0F};
  int segments{6};
};

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
