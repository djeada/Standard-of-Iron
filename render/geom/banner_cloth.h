#pragma once

#include <QMatrix4x4>
#include <QVector3D>
#include <vector>

namespace Render::Geom {

/**
 * @brief Lightweight CPU-side helpers for banner geometry.
 *
 * Heavy cloth physics simulation is done on GPU via banner shaders.
 * This class provides simple geometry setup and decorative elements.
 */
class BannerCloth {
public:
  /**
   * @brief Generate a simple banner transform for shader-based animation.
   *
   * The actual cloth animation is performed in the banner vertex shader.
   * This just provides the base transform.
   */
  static auto generate_banner_transform(const QVector3D &center,
                                        float half_width, float half_height,
                                        float depth) -> QMatrix4x4;
};

/**
 * @brief Generates decorative tassel geometry for banner edges.
 *
 * Tassels use simple sine-based sway which is lightweight on CPU.
 */
class BannerTassels {
public:
  static constexpr int k_max_tassels = 8;

  struct TasselSet {
    std::vector<QMatrix4x4> thread_transforms;
    std::vector<QMatrix4x4> tip_transforms;
    QVector3D thread_color;
    QVector3D tip_color;
  };

  /**
   * @brief Generate tassels along the bottom edge of a banner.
   *
   * Uses simple sine wave for sway - lightweight CPU calculation.
   */
  static auto generate_bottom_tassels(const QVector3D &banner_center,
                                      float banner_width, float banner_height,
                                      float tassel_length, int tassel_count,
                                      float animation_time,
                                      const QVector3D &thread_color,
                                      const QVector3D &tip_color) -> TasselSet;
};

/**
 * @brief Generates embroidered pattern overlays for banners.
 */
class BannerEmbroidery {
public:
  struct EmbroideryLayer {
    QMatrix4x4 transform;
    QVector3D color;
    float alpha;
  };

  /**
   * @brief Generate a border/trim pattern for a banner.
   */
  static auto generate_border_trim(const QVector3D &banner_center,
                                   float half_width, float half_height,
                                   float border_thickness,
                                   const QVector3D &trim_color)
      -> std::vector<EmbroideryLayer>;
};

} // namespace Render::Geom
