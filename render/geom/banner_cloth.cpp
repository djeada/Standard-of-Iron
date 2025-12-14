#include "banner_cloth.h"
#include <cmath>

namespace Render::Geom {

namespace {
constexpr float k_pi = 3.14159265358979F;
}

auto BannerCloth::generate_banner_transform(const QVector3D &center,
                                            float half_width, float half_height,
                                            float depth) -> QMatrix4x4 {
  QMatrix4x4 transform;
  transform.translate(center);
  transform.scale(half_width, half_height, depth);
  return transform;
}

auto BannerTassels::generate_bottom_tassels(
    const QVector3D &banner_center, float banner_width, float banner_height,
    float tassel_length, int tassel_count, float animation_time,
    const QVector3D &thread_color, const QVector3D &tip_color) -> TasselSet {
  TasselSet result;
  result.thread_color = thread_color;
  result.tip_color = tip_color;

  int const count = std::min(tassel_count, k_max_tassels);
  if (count <= 0) {
    return result;
  }

  result.thread_transforms.reserve(count);
  result.tip_transforms.reserve(count);

  float const bottom_y = banner_center.y() - banner_height * 0.5F;
  float const spacing = banner_width / static_cast<float>(count + 1);
  float const start_x = banner_center.x() - banner_width * 0.5F + spacing;

  constexpr float thread_radius = 0.008F;
  constexpr float tip_size = 0.015F;

  for (int i = 0; i < count; ++i) {
    float const x = start_x + spacing * static_cast<float>(i);

    float const phase = animation_time * 2.5F + static_cast<float>(i) * 0.7F;
    float const sway = std::sin(phase) * 0.04F;

    QVector3D thread_top(x, bottom_y, banner_center.z());
    QVector3D thread_bottom(x + sway, bottom_y - tassel_length,
                            banner_center.z());

    QMatrix4x4 thread_transform;
    QVector3D const thread_center = (thread_top + thread_bottom) * 0.5F;
    thread_transform.translate(thread_center);
    thread_transform.scale(thread_radius, tassel_length * 0.5F, thread_radius);
    result.thread_transforms.push_back(thread_transform);

    QMatrix4x4 tip_transform;
    tip_transform.translate(thread_bottom);
    tip_transform.scale(tip_size, tip_size, tip_size);
    result.tip_transforms.push_back(tip_transform);
  }

  return result;
}

auto BannerEmbroidery::generate_border_trim(
    const QVector3D &banner_center, float half_width, float half_height,
    float border_thickness,
    const QVector3D &trim_color) -> std::vector<EmbroideryLayer> {
  std::vector<EmbroideryLayer> layers;
  layers.reserve(4);

  float const z_offset = 0.005F;

  {
    EmbroideryLayer layer;
    layer.transform.translate(banner_center.x(),
                              banner_center.y() + half_height -
                                  border_thickness * 0.5F,
                              banner_center.z() + z_offset);
    layer.transform.scale(half_width + border_thickness,
                          border_thickness * 0.5F, 0.002F);
    layer.color = trim_color;
    layer.alpha = 1.0F;
    layers.push_back(layer);
  }

  {
    EmbroideryLayer layer;
    layer.transform.translate(banner_center.x(),
                              banner_center.y() - half_height +
                                  border_thickness * 0.5F,
                              banner_center.z() + z_offset);
    layer.transform.scale(half_width + border_thickness,
                          border_thickness * 0.5F, 0.002F);
    layer.color = trim_color;
    layer.alpha = 1.0F;
    layers.push_back(layer);
  }

  {
    EmbroideryLayer layer;
    layer.transform.translate(banner_center.x() - half_width +
                                  border_thickness * 0.5F,
                              banner_center.y(), banner_center.z() + z_offset);
    layer.transform.scale(border_thickness * 0.5F,
                          half_height - border_thickness, 0.002F);
    layer.color = trim_color;
    layer.alpha = 1.0F;
    layers.push_back(layer);
  }

  {
    EmbroideryLayer layer;
    layer.transform.translate(banner_center.x() + half_width -
                                  border_thickness * 0.5F,
                              banner_center.y(), banner_center.z() + z_offset);
    layer.transform.scale(border_thickness * 0.5F,
                          half_height - border_thickness, 0.002F);
    layer.color = trim_color;
    layer.alpha = 1.0F;
    layers.push_back(layer);
  }

  return layers;
}

} // namespace Render::Geom
