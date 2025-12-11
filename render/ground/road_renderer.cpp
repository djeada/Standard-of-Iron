#include "road_renderer.h"
#include "../../game/map/terrain.h"
#include "../../game/map/visibility_service.h"
#include "../gl/mesh.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"
#include "ground_utils.h"
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

RoadRenderer::RoadRenderer() = default;
RoadRenderer::~RoadRenderer() = default;

void RoadRenderer::configure(
    const std::vector<Game::Map::RoadSegment> &road_segments, float tile_size) {
  m_road_segments = road_segments;
  m_tile_size = tile_size;
  build_meshes();
}

void RoadRenderer::build_meshes() {
  m_meshes.clear();

  if (m_road_segments.empty()) {
    return;
  }

  auto noise = [](float x, float y) -> float {
    float const ix = std::floor(x);
    float const iy = std::floor(y);
    float fx = x - ix;
    float fy = y - iy;

    fx = fx * fx * (3.0F - 2.0F * fx);
    fy = fy * fy * (3.0F - 2.0F * fy);

    float const a = Ground::noise_hash(ix, iy);
    float const b = Ground::noise_hash(ix + 1.0F, iy);
    float const c = Ground::noise_hash(ix, iy + 1.0F);
    float const d = Ground::noise_hash(ix + 1.0F, iy + 1.0F);

    return a * (1.0F - fx) * (1.0F - fy) + b * fx * (1.0F - fy) +
           c * (1.0F - fx) * fy + d * fx * fy;
  };

  for (const auto &segment : m_road_segments) {
    QVector3D dir = segment.end - segment.start;
    float const length = dir.length();
    if (length < 0.01F) {
      m_meshes.push_back(nullptr);
      continue;
    }

    dir.normalize();
    QVector3D const perpendicular(-dir.z(), 0.0F, dir.x());
    float const half_width = segment.width * 0.5F;

    int length_steps =
        static_cast<int>(std::ceil(length / (m_tile_size * 0.5F))) + 1;
    length_steps = std::max(length_steps, 8);

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    for (int i = 0; i < length_steps; ++i) {
      float const t =
          static_cast<float>(i) / static_cast<float>(length_steps - 1);
      QVector3D center_pos = segment.start + dir * (length * t);

      constexpr float k_edge_noise_freq_1 = 1.5F;
      constexpr float k_edge_noise_freq_2 = 4.0F;

      float const edge_noise1 = noise(center_pos.x() * k_edge_noise_freq_1,
                                      center_pos.z() * k_edge_noise_freq_1);
      float const edge_noise2 = noise(center_pos.x() * k_edge_noise_freq_2,
                                      center_pos.z() * k_edge_noise_freq_2);

      float combined_noise = edge_noise1 * 0.6F + edge_noise2 * 0.4F;
      combined_noise = (combined_noise - 0.5F) * 2.0F;

      float const width_variation = combined_noise * half_width * 0.15F;

      constexpr float road_y_offset = 0.02F;

      QVector3D const left =
          center_pos - perpendicular * (half_width + width_variation);
      QVector3D const right =
          center_pos + perpendicular * (half_width + width_variation);

      float const normal[3] = {0.0F, 1.0F, 0.0F};

      Vertex left_vertex;
      left_vertex.position[0] = left.x();
      left_vertex.position[1] = left.y() + road_y_offset;
      left_vertex.position[2] = left.z();
      left_vertex.normal[0] = normal[0];
      left_vertex.normal[1] = normal[1];
      left_vertex.normal[2] = normal[2];
      left_vertex.tex_coord[0] = 0.0F;
      left_vertex.tex_coord[1] = t;
      vertices.push_back(left_vertex);

      Vertex right_vertex;
      right_vertex.position[0] = right.x();
      right_vertex.position[1] = right.y() + road_y_offset;
      right_vertex.position[2] = right.z();
      right_vertex.normal[0] = normal[0];
      right_vertex.normal[1] = normal[1];
      right_vertex.normal[2] = normal[2];
      right_vertex.tex_coord[0] = 1.0F;
      right_vertex.tex_coord[1] = t;
      vertices.push_back(right_vertex);

      if (i < length_steps - 1) {
        unsigned int const idx0 = i * 2;
        unsigned int const idx1 = idx0 + 1;
        unsigned int const idx2 = idx0 + 2;
        unsigned int const idx3 = idx0 + 3;

        indices.push_back(idx0);
        indices.push_back(idx2);
        indices.push_back(idx1);

        indices.push_back(idx1);
        indices.push_back(idx2);
        indices.push_back(idx3);
      }
    }

    if (!vertices.empty() && !indices.empty()) {
      m_meshes.push_back(std::make_unique<Mesh>(vertices, indices));
    } else {
      m_meshes.push_back(nullptr);
    }
  }
}

void RoadRenderer::submit(Renderer &renderer, ResourceManager *resources) {
  if (m_meshes.empty() || m_road_segments.empty()) {
    return;
  }

  Q_UNUSED(resources);

  auto &visibility = Game::Map::VisibilityService::instance();
  const bool use_visibility = visibility.isInitialized();

  auto *shader = renderer.get_shader("road");
  if (shader == nullptr) {

    shader = renderer.get_shader("terrain");
    if (shader == nullptr) {
      return;
    }
  }

  renderer.set_current_shader(shader);

  QMatrix4x4 model;
  model.setToIdentity();

  QVector3D const road_base_color(0.45F, 0.42F, 0.38F);

  size_t mesh_index = 0;
  for (const auto &segment : m_road_segments) {
    if (mesh_index >= m_meshes.size()) {
      break;
    }

    auto *mesh = m_meshes[mesh_index].get();
    ++mesh_index;

    if (mesh == nullptr) {
      continue;
    }

    QVector3D dir = segment.end - segment.start;
    float const length = dir.length();

    float alpha = 1.0F;
    QVector3D color_multiplier(1.0F, 1.0F, 1.0F);

    if (use_visibility) {
      int max_visibility_state = 0;
      dir.normalize();

      int const samples_per_segment = 5;
      for (int i = 0; i < samples_per_segment; ++i) {
        float const t =
            static_cast<float>(i) / static_cast<float>(samples_per_segment - 1);
        QVector3D const pos = segment.start + dir * (length * t);

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

    QVector3D const final_color(road_base_color.x() * color_multiplier.x(),
                                road_base_color.y() * color_multiplier.y(),
                                road_base_color.z() * color_multiplier.z());

    renderer.mesh(mesh, model, final_color, nullptr, alpha);
  }

  renderer.set_current_shader(nullptr);
}

} // namespace Render::GL
