#include "linear_feature_geometry.h"
#include "ground_utils.h"
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <vector>

namespace {

auto value_noise(float x, float y) -> float {
  float const ix = std::floor(x);
  float const iy = std::floor(y);
  float fx = x - ix;
  float fy = y - iy;

  fx = fx * fx * (3.0F - 2.0F * fx);
  fy = fy * fy * (3.0F - 2.0F * fy);

  float const a = Render::Ground::noise_hash(ix, iy);
  float const b = Render::Ground::noise_hash(ix + 1.0F, iy);
  float const c = Render::Ground::noise_hash(ix, iy + 1.0F);
  float const d = Render::Ground::noise_hash(ix + 1.0F, iy + 1.0F);

  return a * (1.0F - fx) * (1.0F - fy) + b * fx * (1.0F - fy) +
         c * (1.0F - fx) * fy + d * fx * fy;
}

} // namespace

namespace Render::Ground {

auto build_linear_ribbon_mesh(const LinearFeatureRibbonSegment &segment,
                              float tile_size,
                              const LinearFeatureRibbonSettings &settings)
    -> std::unique_ptr<Render::GL::Mesh> {
  QVector3D dir = segment.end - segment.start;
  float const length = dir.length();
  if (length < 0.01F) {
    return nullptr;
  }

  dir.normalize();
  QVector3D const perpendicular(-dir.z(), 0.0F, dir.x());
  float const half_width = segment.width * 0.5F;
  float const step = std::max(tile_size * settings.sample_step, 0.001F);

  int length_steps = static_cast<int>(std::ceil(length / step)) + 1;
  length_steps = std::max(length_steps, settings.min_length_steps);

  std::vector<Render::GL::Vertex> vertices;
  std::vector<unsigned int> indices;

  vertices.reserve(static_cast<size_t>(length_steps * 2));
  indices.reserve(static_cast<size_t>((length_steps - 1) * 6));

  for (int i = 0; i < length_steps; ++i) {
    float const t = static_cast<float>(i) / static_cast<float>(length_steps - 1);
    QVector3D center_pos = segment.start + dir * (length * t);

    float combined_noise = 0.0F;
    float weight_sum = 0.0F;
    for (size_t noise_index = 0;
         noise_index < settings.edge_noise_frequencies.size(); ++noise_index) {
      float const frequency = settings.edge_noise_frequencies[noise_index];
      float const weight = settings.edge_noise_weights[noise_index];
      if (frequency <= 0.0F || weight <= 0.0F) {
        continue;
      }

      combined_noise +=
          value_noise(center_pos.x() * frequency, center_pos.z() * frequency) *
          weight;
      weight_sum += weight;
    }
    if (weight_sum > 0.0F) {
      combined_noise /= weight_sum;
    }
    combined_noise = (combined_noise - 0.5F) * 2.0F;

    float const width_variation =
        combined_noise * half_width * settings.width_variation_scale;

    if (settings.meander_amplitude > 0.0F &&
        settings.meander_frequency > 0.0F) {
      float const meander =
          value_noise(t * settings.meander_frequency,
                      length * settings.meander_length_scale) *
          settings.meander_amplitude;
      center_pos += perpendicular * meander;
    }

    QVector3D const left =
        center_pos - perpendicular * (half_width + width_variation);
    QVector3D const right =
        center_pos + perpendicular * (half_width + width_variation);

    float const normal[3] = {0.0F, 1.0F, 0.0F};

    Render::GL::Vertex left_vertex{};
    left_vertex.position[0] = left.x();
    left_vertex.position[1] = left.y() + settings.y_offset;
    left_vertex.position[2] = left.z();
    left_vertex.normal[0] = normal[0];
    left_vertex.normal[1] = normal[1];
    left_vertex.normal[2] = normal[2];
    left_vertex.tex_coord[0] = 0.0F;
    left_vertex.tex_coord[1] = t;
    vertices.push_back(left_vertex);

    Render::GL::Vertex right_vertex{};
    right_vertex.position[0] = right.x();
    right_vertex.position[1] = right.y() + settings.y_offset;
    right_vertex.position[2] = right.z();
    right_vertex.normal[0] = normal[0];
    right_vertex.normal[1] = normal[1];
    right_vertex.normal[2] = normal[2];
    right_vertex.tex_coord[0] = 1.0F;
    right_vertex.tex_coord[1] = t;
    vertices.push_back(right_vertex);

    if (i < length_steps - 1) {
      unsigned int const idx0 = static_cast<unsigned int>(i * 2);
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

  if (vertices.empty() || indices.empty()) {
    return nullptr;
  }

  return std::make_unique<Render::GL::Mesh>(vertices, indices);
}

} // namespace Render::Ground
