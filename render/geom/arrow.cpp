#include "arrow.h"
#include "../../game/systems/arrow_system.h"
#include "../entity/registry.h"
#include "../gl/mesh.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <qmatrix4x4.h>
#include <qvectornd.h>
#include <vector>

namespace Render {
namespace Geom {

static auto createArrowMesh() -> GL::Mesh * {
  using GL::Vertex;
  std::vector<GL::Vertex> verts;
  std::vector<unsigned int> idx;

  constexpr int k_arrow_radial_segments = 12;
  const float shaft_radius = 0.05F;
  const float shaft_len = 0.85F;
  const float tip_len = 0.15F;
  const float tip_start_z = shaft_len;
  const float tip_end_z = shaft_len + tip_len;

  int const base_index = 0;
  for (int ring = 0; ring < 2; ++ring) {
    float z = (ring == 0) ? 0.0F : shaft_len;
    for (int i = 0; i < k_arrow_radial_segments; ++i) {
      float const a = (float(i) / k_arrow_radial_segments) * 6.2831853F;
      float x = std::cos(a) * shaft_radius;
      float y = std::sin(a) * shaft_radius;
      QVector3D n(x, y, 0.0F);
      n.normalize();
      verts.push_back({{x, y, z},
                       {n.x(), n.y(), n.z()},
                       {float(i) / k_arrow_radial_segments, z}});
    }
  }

  for (int i = 0; i < k_arrow_radial_segments; ++i) {
    int const next = (i + 1) % k_arrow_radial_segments;
    int a = base_index + i;
    int b = base_index + next;
    int c = base_index + k_arrow_radial_segments + next;
    int d = base_index + k_arrow_radial_segments + i;
    idx.push_back(a);
    idx.push_back(b);
    idx.push_back(c);
    idx.push_back(c);
    idx.push_back(d);
    idx.push_back(a);
  }

  int const ring_start = verts.size();
  for (int i = 0; i < k_arrow_radial_segments; ++i) {
    float const a = (float(i) / k_arrow_radial_segments) * 6.2831853F;
    float x = std::cos(a) * shaft_radius * 1.4F;
    float y = std::sin(a) * shaft_radius * 1.4F;
    QVector3D n(x, y, 0.2F);
    n.normalize();
    verts.push_back({{x, y, tip_start_z},
                     {n.x(), n.y(), n.z()},
                     {float(i) / k_arrow_radial_segments, 0.0F}});
  }

  int apex_index = verts.size();
  verts.push_back({{0.0F, 0.0F, tip_end_z}, {0.0F, 0.0F, 1.0F}, {0.5F, 1.0F}});
  for (int i = 0; i < k_arrow_radial_segments; ++i) {
    int const next = (i + 1) % k_arrow_radial_segments;
    idx.push_back(ring_start + i);
    idx.push_back(apex_index);
    idx.push_back(ring_start + next);
  }

  return new GL::Mesh(verts, idx);
}

auto Arrow::get() -> GL::Mesh * {
  static GL::Mesh *mesh = createArrowMesh();
  return mesh;
}

} // namespace Geom

namespace GL {

void renderArrows(Renderer *renderer, ResourceManager *resources,
                  const Game::Systems::ArrowSystem &arrow_system) {
  if ((renderer == nullptr) || (resources == nullptr)) {
    return;
  }
  auto *arrow_mesh = Render::GL::ResourceManager::arrow();
  if (arrow_mesh == nullptr) {
    return;
  }

  const auto &arrows = arrow_system.arrows();
  for (const auto &arrow : arrows) {
    if (!arrow.active) {
      continue;
    }

    const QVector3D delta = arrow.end - arrow.start;
    const float dist = std::max(0.001F, delta.length());
    QVector3D pos = arrow.start + delta * arrow.t;
    float const h = arrow.arcHeight * 4.0F * arrow.t * (1.0F - arrow.t);
    pos.setY(pos.y() + h);

    QMatrix4x4 model;
    model.translate(pos.x(), pos.y(), pos.z());

    constexpr float k_rad_to_deg = 180.0F / std::numbers::pi_v<float>;
    QVector3D const dir = delta.normalized();
    float const yaw_deg = std::atan2(dir.x(), dir.z()) * k_rad_to_deg;
    model.rotate(yaw_deg, QVector3D(0, 1, 0));

    constexpr float k_arc_height_multiplier = 8.0F;
    constexpr float k_arc_center_offset = 0.5F;
    float const vy = (arrow.end.y() - arrow.start.y()) / dist;
    float const pitch_deg =
        -std::atan2(vy - (k_arc_height_multiplier * arrow.arcHeight *
                          (arrow.t - k_arc_center_offset) / dist),
                    1.0F) *
        k_rad_to_deg;
    model.rotate(pitch_deg, QVector3D(1, 0, 0));

    constexpr float arrow_z_scale = 0.40F;
    constexpr float arrow_xy_scale = 0.26F;
    constexpr float arrow_z_translate_factor = 0.5F;
    model.translate(0.0F, 0.0F, -arrow_z_scale * arrow_z_translate_factor);
    model.scale(arrow_xy_scale, arrow_xy_scale, arrow_z_scale);

    renderer->mesh(arrow_mesh, model, arrow.color, nullptr, 1.0F);
  }
}

} // namespace GL
} // namespace Render
