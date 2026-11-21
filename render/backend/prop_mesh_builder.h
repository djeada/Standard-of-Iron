#pragma once

#include <qvectornd.h>

#include <cstdint>
#include <utility>
#include <vector>

namespace Render::GL::BackendPipelines {

using PropMeshVerts = std::vector<std::pair<QVector3D, QVector3D>>;
using PropMeshIndices = std::vector<std::uint16_t>;

void append_box(PropMeshVerts& verts,
                PropMeshIndices& idx,
                const QVector3D& lo,
                const QVector3D& hi);

void append_quad(PropMeshVerts& verts,
                 PropMeshIndices& idx,
                 const QVector3D& p0,
                 const QVector3D& p1,
                 const QVector3D& p2,
                 const QVector3D& p3,
                 const QVector3D& n);

void append_oriented_box(PropMeshVerts& verts,
                         PropMeshIndices& idx,
                         const QVector3D& a,
                         const QVector3D& b,
                         float half_width,
                         float half_depth);

void append_prop_taper(PropMeshVerts& verts,
                       PropMeshIndices& idx,
                       float cx,
                       float y0,
                       float cz,
                       float r0,
                       float r1,
                       float height,
                       int segs);

void append_prop_beam(PropMeshVerts& verts,
                      PropMeshIndices& idx,
                      const QVector3D& a,
                      const QVector3D& b,
                      float half_width,
                      float half_depth);

void append_prop_slab(PropMeshVerts& verts,
                      PropMeshIndices& idx,
                      float y0,
                      float y1,
                      float half_bottom,
                      float half_top);

void append_prop_frustum(PropMeshVerts& verts,
                         PropMeshIndices& idx,
                         float cx,
                         float y0,
                         float cz,
                         float rx0,
                         float rz0,
                         float rx1,
                         float rz1,
                         float height,
                         int segs);

void append_prop_limb(PropMeshVerts& verts,
                      PropMeshIndices& idx,
                      const QVector3D& a,
                      const QVector3D& b,
                      float r0,
                      float r1,
                      int segs);

void append_disc_xaxis(PropMeshVerts& verts,
                       PropMeshIndices& idx,
                       float cx,
                       float cy,
                       float cz,
                       float r,
                       float hthk,
                       int segs);

void append_spoked_wheel_xaxis(PropMeshVerts& verts,
                               PropMeshIndices& idx,
                               float cx,
                               float cy,
                               float cz,
                               float r,
                               float hthk,
                               int segs,
                               int spokes);

void append_vert_prism(PropMeshVerts& verts,
                       PropMeshIndices& idx,
                       float cx,
                       float y0,
                       float cz,
                       float r,
                       float height,
                       int segs);

void append_barrel_yaxis(PropMeshVerts& verts,
                         PropMeshIndices& idx,
                         float cx,
                         float y0,
                         float cz,
                         float r,
                         float height,
                         int segs);

} // namespace Render::GL::BackendPipelines
