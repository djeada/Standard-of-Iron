#include "riverbank_renderer.h"
#include "../../game/map/visibility_service.h"
#include "../gl/mesh.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"
#include "ground_utils.h"
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
#include <utility>
#include <vector>

namespace Render::GL {

RiverbankRenderer::RiverbankRenderer() = default;
RiverbankRenderer::~RiverbankRenderer() = default;

void RiverbankRenderer::configure(
    const std::vector<Game::Map::RiverSegment> &riverSegments,
    const Game::Map::TerrainHeightMap &height_map) {
  m_riverSegments = riverSegments;
  m_tile_size = height_map.getTileSize();
  m_grid_width = height_map.getWidth();
  m_grid_height = height_map.getHeight();
  m_heights = height_map.getHeightData();
  build_meshes();
}

void RiverbankRenderer::build_meshes() {
  m_meshes.clear();
  m_visibilitySamples.clear();

  if (m_riverSegments.empty()) {
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

  auto sample_height = [&](float world_x, float world_z) -> float {
    if (m_heights.empty() || m_grid_width == 0 || m_grid_height == 0) {
      return 0.0F;
    }

    float const half_width = m_grid_width * 0.5F - 0.5F;
    float const half_height = m_grid_height * 0.5F - 0.5F;
    float gx = (world_x / m_tile_size) + half_width;
    float gz = (world_z / m_tile_size) + half_height;

    gx = std::clamp(gx, 0.0F, float(m_grid_width - 1));
    gz = std::clamp(gz, 0.0F, float(m_grid_height - 1));

    int const x0 = int(std::floor(gx));
    int const z0 = int(std::floor(gz));
    int const x1 = std::min(x0 + 1, m_grid_width - 1);
    int const z1 = std::min(z0 + 1, m_grid_height - 1);

    float const tx = gx - float(x0);
    float const tz = gz - float(z0);

    float const h00 = m_heights[z0 * m_grid_width + x0];
    float const h10 = m_heights[z0 * m_grid_width + x1];
    float const h01 = m_heights[z1 * m_grid_width + x0];
    float const h11 = m_heights[z1 * m_grid_width + x1];

    float const h0 = h00 * (1.0F - tx) + h10 * tx;
    float const h1 = h01 * (1.0F - tx) + h11 * tx;
    return h0 * (1.0F - tz) + h1 * tz;
  };

  for (const auto &segment : m_riverSegments) {
    QVector3D dir = segment.end - segment.start;
    float const length = dir.length();
    if (length < 0.01F) {
      m_meshes.push_back(nullptr);
      m_visibilitySamples.emplace_back();
      continue;
    }

    dir.normalize();
    QVector3D const perpendicular(-dir.z(), 0.0F, dir.x());
    float const half_width = segment.width * 0.5F;

    // Multi-ring cross-section for volumetric bank geometry
    // Each ring has a distance from water edge and height profile
    constexpr int k_rings_per_side = 5; // water_edge, inner_mid, crest, outer_mid, terrain_blend
    constexpr int k_total_rings = k_rings_per_side * 2; // both sides
    
    int length_steps =
        static_cast<int>(std::ceil(length / (m_tile_size * 0.5F))) + 1;
    length_steps = std::max(length_steps, 8);

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<QVector3D> samples;

    for (int i = 0; i < length_steps; ++i) {
      float const t =
          static_cast<float>(i) / static_cast<float>(length_steps - 1);
      QVector3D center_pos = segment.start + dir * (length * t);

      constexpr float k_edge_noise_freq_1 = 2.0F;
      constexpr float k_edge_noise_freq_2 = 5.0F;
      constexpr float k_edge_noise_freq_3 = 10.0F;

      float const edge_noise1 = noise(center_pos.x() * k_edge_noise_freq_1,
                                      center_pos.z() * k_edge_noise_freq_1);
      float const edge_noise2 = noise(center_pos.x() * k_edge_noise_freq_2,
                                      center_pos.z() * k_edge_noise_freq_2);
      float const edge_noise3 = noise(center_pos.x() * k_edge_noise_freq_3,
                                      center_pos.z() * k_edge_noise_freq_3);

      float combined_noise =
          edge_noise1 * 0.5F + edge_noise2 * 0.3F + edge_noise3 * 0.2F;
      combined_noise = (combined_noise - 0.5F) * 2.0F;

      float const width_variation = combined_noise * half_width * 0.35F;

      float const meander = noise(t * 3.0F, length * 0.1F) * 0.3F;
      QVector3D const center_offset = perpendicular * meander;
      center_pos += center_offset;

      // Define cross-section profile for both sides
      // Heights: water edge slightly above water, crest raised, outer blends to terrain
      struct RingProfile {
        float distance_from_water; // perpendicular distance from water edge
        float height_offset;        // height above/below terrain
      };
      
      constexpr RingProfile k_left_rings[k_rings_per_side] = {
        {0.0F, 0.02F},    // water edge - just above water (halved from 0.05)
        {0.125F, 0.175F}, // inner midslope (halved from 0.25, 0.35)
        {0.25F, 0.3F},    // crest - peak of bank (halved from 0.5, 0.6)
        {0.375F, 0.125F}, // outer midslope (halved from 0.75, 0.25)
        {0.5F, -0.15F}    // terrain blend - well below terrain to avoid z-fighting on hills
      };
      
      // Add some noise-based width variation to ring distances
      float const ring_noise = noise(center_pos.x() * 3.0F, center_pos.z() * 3.0F) * 0.075F;
      float const base_bank_width = 0.5F + ring_noise; // Halved from 1.0F

      // Build vertices for left bank rings
      unsigned int const ring_start_idx = static_cast<unsigned int>(vertices.size());
      
      for (int ring = 0; ring < k_rings_per_side; ++ring) {
        float const ring_dist = k_left_rings[ring].distance_from_water * base_bank_width;
        float const ring_height = k_left_rings[ring].height_offset;
        
        QVector3D const ring_pos = center_pos - perpendicular * (half_width + width_variation + ring_dist);
        float const terrain_height = sample_height(ring_pos.x(), ring_pos.z());
        
        if (ring == 0) { // water edge
          samples.push_back(ring_pos);
        }
        
        Vertex vtx{};
        vtx.position[0] = ring_pos.x();
        vtx.position[1] = terrain_height + ring_height;
        vtx.position[2] = ring_pos.z();
        
        // Calculate normal based on slope between rings
        QVector3D normal;
        if (ring == 0) {
          // Water edge: use normal pointing toward next ring
          QVector3D const next_ring_pos = center_pos - perpendicular * 
            (half_width + width_variation + k_left_rings[1].distance_from_water * base_bank_width);
          float const next_terrain = sample_height(next_ring_pos.x(), next_ring_pos.z());
          QVector3D slope_vec(
            next_ring_pos.x() - ring_pos.x(),
            (next_terrain + k_left_rings[1].height_offset) - (terrain_height + ring_height),
            next_ring_pos.z() - ring_pos.z()
          );
          normal = QVector3D::crossProduct(slope_vec, dir).normalized();
        } else if (ring == k_rings_per_side - 1) {
          // Outer edge: use normal from previous ring
          unsigned int prev_idx = ring_start_idx + ring - 1;
          QVector3D prev_pos(vertices[prev_idx].position[0], 
                            vertices[prev_idx].position[1], 
                            vertices[prev_idx].position[2]);
          QVector3D slope_vec(ring_pos.x() - prev_pos.x(),
                             (terrain_height + ring_height) - prev_pos.y(),
                             ring_pos.z() - prev_pos.z());
          normal = QVector3D::crossProduct(slope_vec, dir).normalized();
        } else {
          // Middle rings: average of adjacent slopes
          unsigned int prev_idx = ring_start_idx + ring - 1;
          QVector3D prev_pos(vertices[prev_idx].position[0], 
                            vertices[prev_idx].position[1], 
                            vertices[prev_idx].position[2]);
          
          QVector3D const next_ring_pos = center_pos - perpendicular * 
            (half_width + width_variation + k_left_rings[ring + 1].distance_from_water * base_bank_width);
          float const next_terrain = sample_height(next_ring_pos.x(), next_ring_pos.z());
          
          QVector3D slope_from_prev(ring_pos.x() - prev_pos.x(),
                                   (terrain_height + ring_height) - prev_pos.y(),
                                   ring_pos.z() - prev_pos.z());
          QVector3D slope_to_next(next_ring_pos.x() - ring_pos.x(),
                                 (next_terrain + k_left_rings[ring + 1].height_offset) - (terrain_height + ring_height),
                                 next_ring_pos.z() - ring_pos.z());
          
          QVector3D n1 = QVector3D::crossProduct(slope_from_prev, dir).normalized();
          QVector3D n2 = QVector3D::crossProduct(slope_to_next, dir).normalized();
          normal = ((n1 + n2) * 0.5F).normalized();
        }
        
        vtx.normal[0] = normal.x();
        vtx.normal[1] = normal.y();
        vtx.normal[2] = normal.z();
        vtx.tex_coord[0] = static_cast<float>(ring) / (k_rings_per_side - 1);
        vtx.tex_coord[1] = t;
        vertices.push_back(vtx);
      }
      
      // Build vertices for right bank rings (mirror of left)
      for (int ring = 0; ring < k_rings_per_side; ++ring) {
        float const ring_dist = k_left_rings[ring].distance_from_water * base_bank_width;
        float const ring_height = k_left_rings[ring].height_offset;
        
        QVector3D const ring_pos = center_pos + perpendicular * (half_width + width_variation + ring_dist);
        float const terrain_height = sample_height(ring_pos.x(), ring_pos.z());
        
        if (ring == 0) { // water edge
          samples.push_back(ring_pos);
        }
        
        Vertex vtx{};
        vtx.position[0] = ring_pos.x();
        vtx.position[1] = terrain_height + ring_height;
        vtx.position[2] = ring_pos.z();
        
        // Calculate normal (mirror logic from left side)
        QVector3D normal;
        if (ring == 0) {
          QVector3D const next_ring_pos = center_pos + perpendicular * 
            (half_width + width_variation + k_left_rings[1].distance_from_water * base_bank_width);
          float const next_terrain = sample_height(next_ring_pos.x(), next_ring_pos.z());
          QVector3D slope_vec(
            next_ring_pos.x() - ring_pos.x(),
            (next_terrain + k_left_rings[1].height_offset) - (terrain_height + ring_height),
            next_ring_pos.z() - ring_pos.z()
          );
          normal = QVector3D::crossProduct(dir, slope_vec).normalized();
        } else if (ring == k_rings_per_side - 1) {
          unsigned int prev_idx = ring_start_idx + k_rings_per_side + ring - 1;
          QVector3D prev_pos(vertices[prev_idx].position[0], 
                            vertices[prev_idx].position[1], 
                            vertices[prev_idx].position[2]);
          QVector3D slope_vec(ring_pos.x() - prev_pos.x(),
                             (terrain_height + ring_height) - prev_pos.y(),
                             ring_pos.z() - prev_pos.z());
          normal = QVector3D::crossProduct(dir, slope_vec).normalized();
        } else {
          unsigned int prev_idx = ring_start_idx + k_rings_per_side + ring - 1;
          QVector3D prev_pos(vertices[prev_idx].position[0], 
                            vertices[prev_idx].position[1], 
                            vertices[prev_idx].position[2]);
          
          QVector3D const next_ring_pos = center_pos + perpendicular * 
            (half_width + width_variation + k_left_rings[ring + 1].distance_from_water * base_bank_width);
          float const next_terrain = sample_height(next_ring_pos.x(), next_ring_pos.z());
          
          QVector3D slope_from_prev(ring_pos.x() - prev_pos.x(),
                                   (terrain_height + ring_height) - prev_pos.y(),
                                   ring_pos.z() - prev_pos.z());
          QVector3D slope_to_next(next_ring_pos.x() - ring_pos.x(),
                                 (next_terrain + k_left_rings[ring + 1].height_offset) - (terrain_height + ring_height),
                                 next_ring_pos.z() - ring_pos.z());
          
          QVector3D n1 = QVector3D::crossProduct(dir, slope_from_prev).normalized();
          QVector3D n2 = QVector3D::crossProduct(dir, slope_to_next).normalized();
          normal = ((n1 + n2) * 0.5F).normalized();
        }
        
        vtx.normal[0] = normal.x();
        vtx.normal[1] = normal.y();
        vtx.normal[2] = normal.z();
        vtx.tex_coord[0] = static_cast<float>(ring) / (k_rings_per_side - 1);
        vtx.tex_coord[1] = t;
        vertices.push_back(vtx);
      }
      
      // Add water-level skirts (vertical geometry down to water surface)
      // Left water skirt
      {
        Vertex const &water_edge_left = vertices[ring_start_idx];
        Vertex skirt_vtx = water_edge_left;
        skirt_vtx.position[1] = -0.05F; // Water surface level
        skirt_vtx.normal[0] = -perpendicular.x();
        skirt_vtx.normal[1] = 0.0F;
        skirt_vtx.normal[2] = -perpendicular.z();
        vertices.push_back(skirt_vtx);
      }
      
      // Right water skirt
      {
        Vertex const &water_edge_right = vertices[ring_start_idx + k_rings_per_side];
        Vertex skirt_vtx = water_edge_right;
        skirt_vtx.position[1] = -0.05F; // Water surface level
        skirt_vtx.normal[0] = perpendicular.x();
        skirt_vtx.normal[1] = 0.0F;
        skirt_vtx.normal[2] = perpendicular.z();
        vertices.push_back(skirt_vtx);
      }
      
      // Connect rings with triangle strips
      if (i < length_steps - 1) {
        unsigned int const base_idx = ring_start_idx;
        unsigned int const next_base_idx = base_idx + k_total_rings + 2; // +2 for skirts
        
        // Left bank strips
        for (int ring = 0; ring < k_rings_per_side - 1; ++ring) {
          unsigned int idx0 = base_idx + ring;
          unsigned int idx1 = base_idx + ring + 1;
          unsigned int idx2 = next_base_idx + ring;
          unsigned int idx3 = next_base_idx + ring + 1;
          
          indices.push_back(idx0);
          indices.push_back(idx2);
          indices.push_back(idx1);
          
          indices.push_back(idx1);
          indices.push_back(idx2);
          indices.push_back(idx3);
        }
        
        // Right bank strips
        for (int ring = 0; ring < k_rings_per_side - 1; ++ring) {
          unsigned int idx0 = base_idx + k_rings_per_side + ring;
          unsigned int idx1 = base_idx + k_rings_per_side + ring + 1;
          unsigned int idx2 = next_base_idx + k_rings_per_side + ring;
          unsigned int idx3 = next_base_idx + k_rings_per_side + ring + 1;
          
          indices.push_back(idx0);
          indices.push_back(idx1);
          indices.push_back(idx2);
          
          indices.push_back(idx1);
          indices.push_back(idx3);
          indices.push_back(idx2);
        }
        
        // Water skirt strips
        {
          unsigned int left_top = base_idx;
          unsigned int left_bottom = base_idx + k_total_rings;
          unsigned int left_top_next = next_base_idx;
          unsigned int left_bottom_next = next_base_idx + k_total_rings;
          
          indices.push_back(left_top);
          indices.push_back(left_bottom);
          indices.push_back(left_top_next);
          
          indices.push_back(left_bottom);
          indices.push_back(left_bottom_next);
          indices.push_back(left_top_next);
          
          unsigned int right_top = base_idx + k_rings_per_side;
          unsigned int right_bottom = base_idx + k_total_rings + 1;
          unsigned int right_top_next = next_base_idx + k_rings_per_side;
          unsigned int right_bottom_next = next_base_idx + k_total_rings + 1;
          
          indices.push_back(right_top);
          indices.push_back(right_top_next);
          indices.push_back(right_bottom);
          
          indices.push_back(right_bottom);
          indices.push_back(right_top_next);
          indices.push_back(right_bottom_next);
        }
      }
    }

    if (!vertices.empty() && !indices.empty()) {
      m_meshes.push_back(std::make_unique<Mesh>(vertices, indices));
      m_visibilitySamples.push_back(std::move(samples));
    } else {
      m_meshes.push_back(nullptr);
      m_visibilitySamples.emplace_back();
    }
  }
}

void RiverbankRenderer::submit(Renderer &renderer, ResourceManager *resources) {
  if (m_meshes.empty() || m_riverSegments.empty()) {
    return;
  }

  Q_UNUSED(resources);

  auto *shader = renderer.get_shader("riverbank");
  if (shader == nullptr) {
    return;
  }

  renderer.set_current_shader(shader);

  QMatrix4x4 model;
  model.setToIdentity();

  size_t mesh_index = 0;
  for (const auto &segment : m_riverSegments) {
    if (mesh_index >= m_meshes.size()) {
      break;
    }

    auto *mesh = m_meshes[mesh_index].get();
    ++mesh_index;

    if (mesh == nullptr) {
      continue;
    }

    // Always render - fog overlay handles visibility naturally (like terrain)
    renderer.mesh(mesh, model, QVector3D(1.0F, 1.0F, 1.0F), nullptr, 1.0F);
  }

  renderer.set_current_shader(nullptr);
}

} // namespace Render::GL
