#pragma once

#include <QMatrix4x4>
#include <QVector3D>
#include <vector>

namespace Render::Geom {

class BannerCloth {
public:
  static auto generate_banner_transform(const QVector3D &center,
                                        float half_width, float half_height,
                                        float depth) -> QMatrix4x4;
};

class BannerTassels {
public:
  static constexpr int k_max_tassels = 8;

  struct TasselSet {
    std::vector<QMatrix4x4> thread_transforms;
    std::vector<QMatrix4x4> tip_transforms;
    QVector3D thread_color;
    QVector3D tip_color;
  };

  static auto generate_bottom_tassels(const QVector3D &banner_center,
                                      float banner_width, float banner_height,
                                      float tassel_length, int tassel_count,
                                      float animation_time,
                                      const QVector3D &thread_color,
                                      const QVector3D &tip_color) -> TasselSet;
};

class BannerEmbroidery {
public:
  struct EmbroideryLayer {
    QMatrix4x4 transform;
    QVector3D color;
    float alpha;
  };

  static auto generate_border_trim(
      const QVector3D &banner_center, float half_width, float half_height,
      float border_thickness,
      const QVector3D &trim_color) -> std::vector<EmbroideryLayer>;
};

} // namespace Render::Geom
