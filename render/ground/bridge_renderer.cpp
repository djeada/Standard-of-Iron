#include "bridge_renderer.h"
#include "../../game/map/visibility_service.h"
#include "../gl/mesh.h"
#include "../gl/render_constants.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"
#include "map/terrain.h"
#include <QVector2D>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <qglobal.h>
#include <qmatrix4x4.h>
#include <qvectornd.h>
#include <vector>

namespace Render::GL {

using namespace Render::GL::Geometry;

BridgeRenderer::BridgeRenderer() = default;
BridgeRenderer::~BridgeRenderer() = default;

void BridgeRenderer::configure(const std::vector<Game::Map::Bridge> &bridges,
                               float tile_size) {
  m_bridges = bridges;
  m_tile_size = tile_size;
  buildMeshes();
}

void BridgeRenderer::buildMeshes() {
  m_meshes.clear();

  if (m_bridges.empty()) {
    return;
  }

  for (const auto &bridge : m_bridges) {
    QVector3D dir = bridge.end - bridge.start;
    float const length = dir.length();
    if (length < 0.01F) {
      m_meshes.push_back(nullptr);
      continue;
    }

    dir.normalize();
    QVector3D const perpendicular(-dir.z(), 0.0F, dir.x());
    float const half_width = bridge.width * 0.5F;

    int length_segments =
        static_cast<int>(std::ceil(length / (m_tile_size * 0.3F)));
    length_segments = std::max(length_segments, MinLengthSegments);

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    constexpr int k_vertices_per_bridge_segment = 12;
    const float deck_thickness = std::clamp(bridge.width * 0.25F, 0.35F, 0.8F);
    const float parapet_height = std::clamp(bridge.width * 0.25F, 0.25F, 0.55F);
    const float parapet_offset = half_width * 1.05F;

    auto add_vertex = [&](const QVector3D &position, const QVector3D &normal,
                          float u, float v) {
      Vertex vtx{};
      vtx.position[0] = position.x();
      vtx.position[1] = position.y();
      vtx.position[2] = position.z();
      QVector3D const n = normal.normalized();
      vtx.normal[0] = n.x();
      vtx.normal[1] = n.y();
      vtx.normal[2] = n.z();
      vtx.tex_coord[0] = u;
      vtx.tex_coord[1] = v;
      vertices.push_back(vtx);
    };

    auto push_quad = [&](unsigned int a, unsigned int b, unsigned int c,
                         unsigned int d) {
      indices.push_back(a);
      indices.push_back(b);
      indices.push_back(c);
      indices.push_back(a);
      indices.push_back(c);
      indices.push_back(d);
    };

    for (int i = 0; i <= length_segments; ++i) {
      float const t =
          static_cast<float>(i) / static_cast<float>(length_segments);
      QVector3D const center_pos = bridge.start + dir * (length * t);

      float const arch_curve = 4.0F * t * (1.0F - t);
      float const arch_height = bridge.height * arch_curve * 0.8F;

      float const deck_height =
          bridge.start.y() + bridge.height + arch_height * 0.3F;

      float const stone_noise = std::sin(center_pos.x() * 3.0F) *
                                std::cos(center_pos.z() * 2.5F) * 0.02F;

      float const deck_y = deck_height + stone_noise;
      float const underside_y =
          deck_height - deck_thickness - arch_curve * bridge.height * 0.55F;
      float const rail_top_y = deck_y + parapet_height;

      QVector3D const left_normal = (-perpendicular).normalized();
      QVector3D const right_normal = perpendicular.normalized();

      QVector3D top_left = center_pos + perpendicular * (-half_width);
      top_left.setY(deck_y);
      QVector3D top_right = center_pos + perpendicular * (half_width);
      top_right.setY(deck_y);

      QVector3D bottom_left = top_left;
      bottom_left.setY(underside_y);
      QVector3D bottom_right = top_right;
      bottom_right.setY(underside_y);

      QVector3D const side_left_top = top_left;
      QVector3D const side_left_bottom = bottom_left;
      QVector3D const side_right_top = top_right;
      QVector3D const side_right_bottom = bottom_right;

      QVector3D parapet_left_bottom =
          center_pos + perpendicular * (-parapet_offset);
      parapet_left_bottom.setY(deck_y);
      QVector3D parapet_left_top = parapet_left_bottom;
      parapet_left_top.setY(rail_top_y);

      QVector3D parapet_right_bottom =
          center_pos + perpendicular * (parapet_offset);
      parapet_right_bottom.setY(deck_y);
      QVector3D parapet_right_top = parapet_right_bottom;
      parapet_right_top.setY(rail_top_y);

      float const tex_u0 = 0.0F;
      float const tex_u1 = 1.0F;
      float const tex_v = t * length * 0.4F;

      add_vertex(top_left, QVector3D(0.0F, 1.0F, 0.0F), tex_u0, tex_v);
      add_vertex(top_right, QVector3D(0.0F, 1.0F, 0.0F), tex_u1, tex_v);
      add_vertex(bottom_left, QVector3D(0.0F, -1.0F, 0.0F), tex_u0, tex_v);
      add_vertex(bottom_right, QVector3D(0.0F, -1.0F, 0.0F), tex_u1, tex_v);
      add_vertex(side_left_top, left_normal, tex_u0, tex_v);
      add_vertex(side_left_bottom, left_normal, tex_u0, tex_v);
      add_vertex(side_right_top, right_normal, tex_u1, tex_v);
      add_vertex(side_right_bottom, right_normal, tex_u1, tex_v);
      add_vertex(parapet_left_top, left_normal, tex_u0, tex_v);
      add_vertex(parapet_left_bottom, left_normal, tex_u0, tex_v);
      add_vertex(parapet_right_top, right_normal, tex_u1, tex_v);
      add_vertex(parapet_right_bottom, right_normal, tex_u1, tex_v);

      if (i < length_segments) {
        auto const base_idx =
            static_cast<unsigned int>(i * k_vertices_per_bridge_segment);
        unsigned int const next_idx = base_idx + k_vertices_per_bridge_segment;

        push_quad(base_idx + 0, base_idx + 1, next_idx + 1, next_idx + 0);

        push_quad(next_idx + 3, next_idx + 2, base_idx + 2, base_idx + 3);

        push_quad(base_idx + 4, base_idx + 5, next_idx + 5, next_idx + 4);

        push_quad(base_idx + 6, base_idx + 7, next_idx + 7, next_idx + 6);

        push_quad(base_idx + 9, base_idx + 8, next_idx + 8, next_idx + 9);

        push_quad(base_idx + 11, base_idx + 10, next_idx + 10, next_idx + 11);
      }
    }

    if (!vertices.empty()) {
      unsigned int const start_idx = 0;
      auto const end_idx = static_cast<unsigned int>(
          length_segments * k_vertices_per_bridge_segment);

      QVector3D const forward_normal = dir;

      auto add_cap = [&](unsigned int topL, unsigned int topR,
                         unsigned int bottomR, unsigned int bottomL,
                         const QVector3D &normal) {
        auto const cap_start = static_cast<unsigned int>(vertices.size());
        auto copy_vertex = [&](unsigned int source, const QVector3D &norm) {
          const Vertex &src = vertices[source];
          Vertex vtx = src;
          QVector3D const n = norm.normalized();
          vtx.normal[0] = n.x();
          vtx.normal[1] = n.y();
          vtx.normal[2] = n.z();
          vertices.push_back(vtx);
        };
        copy_vertex(topL, normal);
        copy_vertex(topR, normal);
        copy_vertex(bottomR, normal);
        copy_vertex(bottomL, normal);
        push_quad(cap_start + 0, cap_start + 1, cap_start + 2, cap_start + 3);
      };

      add_cap(start_idx + 0, start_idx + 1, start_idx + 3, start_idx + 2,
              -forward_normal);
      add_cap(end_idx + 0, end_idx + 1, end_idx + 3, end_idx + 2,
              forward_normal);
    }

    if (!vertices.empty() && !indices.empty()) {
      m_meshes.push_back(std::make_unique<Mesh>(vertices, indices));
    } else {
      m_meshes.push_back(nullptr);
    }
  }
}

void BridgeRenderer::submit(Renderer &renderer, ResourceManager *resources) {
  if (m_meshes.empty() || m_bridges.empty()) {
    return;
  }

  Q_UNUSED(resources);

  auto &visibility = Game::Map::VisibilityService::instance();
  const bool use_visibility = visibility.is_initialized();

  auto *shader = renderer.get_shader("bridge");
  if (shader == nullptr) {
    shader = renderer.get_shader("basic");
    if (shader == nullptr) {
      return;
    }
  }

  renderer.set_current_shader(shader);

  QMatrix4x4 model;
  model.setToIdentity();

  QVector3D const stone_color(0.55F, 0.52F, 0.48F);

  size_t mesh_index = 0;
  for (const auto &bridge : m_bridges) {
    if (mesh_index >= m_meshes.size()) {
      break;
    }

    auto *mesh = m_meshes[mesh_index].get();
    ++mesh_index;

    if (mesh == nullptr) {
      continue;
    }

    QVector3D dir = bridge.end - bridge.start;
    float const length = dir.length();

    float alpha = 1.0F;
    QVector3D color_multiplier(1.0F, 1.0F, 1.0F);

    if (use_visibility) {
      int max_visibility_state = 0;
      dir.normalize();

      int const samples_per_bridge = 5;
      for (int i = 0; i < samples_per_bridge; ++i) {
        float const t =
            static_cast<float>(i) / static_cast<float>(samples_per_bridge - 1);
        QVector3D const pos = bridge.start + dir * (length * t);

        if (visibility.isVisibleWorld(pos.x(), pos.z())) {
          max_visibility_state = 2;
          break;
        }
        if (visibility.isExploredWorld(pos.x(), pos.z())) {
          max_visibility_state = std::max(max_visibility_state, 1);
        }
      }

      if (max_visibility_state == 0) {
        continue;
      }
      if (max_visibility_state == 1) {
        alpha = 0.5F;
        color_multiplier = QVector3D(0.4F, 0.4F, 0.45F);
      }
    }

    QVector3D const final_color(stone_color.x() * color_multiplier.x(),
                                stone_color.y() * color_multiplier.y(),
                                stone_color.z() * color_multiplier.z());

    renderer.mesh(mesh, model, final_color, nullptr, alpha);
  }

  renderer.set_current_shader(nullptr);
}

} // namespace Render::GL
