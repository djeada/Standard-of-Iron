#include "roman_greaves.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_specs.h"
#include "../../humanoid/style_palette.h"
#include "../../submitter.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <cmath>

namespace Render::GL {

using Render::Geom::cylinderBetween;
using Render::Geom::sphereAt;
using Render::GL::Humanoid::saturate_color;

void RomanGreavesRenderer::render(const DrawContext &ctx,
                                  const BodyFrames &frames,
                                  const HumanoidPalette &palette,
                                  const HumanoidAnimationContext &anim,
                                  ISubmitter &submitter) {
  (void)anim;

  using HP = HumanProportions;

  QVector3D const greaves_color =
      saturate_color(palette.metal * QVector3D(0.95F, 0.88F, 0.68F));

  auto render_greave = [&](const AttachmentFrame &shin) {
    float const shin_r = shin.radius;
    float const shin_length = HP::LOWER_LEG_LEN;

    float const greave_start = shin_length * 0.10F;
    float const greave_end = shin_length * 0.92F;
    float const greave_len = greave_end - greave_start;

    QVector3D greave_top = shin.origin + shin.up * (shin_length - greave_start);
    QVector3D greave_bottom =
        shin.origin + shin.up * (shin_length - greave_end);

    QVector3D const &plate_up = shin.up;
    QVector3D const &plate_forward = shin.forward;
    QVector3D const &plate_right = shin.right;

    constexpr int NUM_SEGMENTS = 3;
    float const angles[NUM_SEGMENTS] = {-0.8F, 0.0F, 0.8F};

    float const greave_offset = shin_r * 1.08F;
    float const greave_thickness = 0.006F;
    float const segment_width = shin_r * 0.55F;

    for (int seg = 0; seg < NUM_SEGMENTS; ++seg) {
      float angle = angles[seg];
      float cos_a = std::cos(angle);
      float sin_a = std::sin(angle);

      QVector3D segment_offset = plate_forward * (greave_offset * cos_a) +
                                 plate_right * (greave_offset * sin_a);

      QVector3D segment_top = greave_top + segment_offset;
      QVector3D segment_bottom = greave_bottom + segment_offset;
      QVector3D segment_center = (segment_top + segment_bottom) * 0.5F;

      QVector3D segment_normal =
          (plate_forward * cos_a + plate_right * sin_a).normalized();
      QVector3D seg_tangent =
          QVector3D::crossProduct(plate_up, segment_normal).normalized();

      QMatrix4x4 seg_transform = ctx.model;
      seg_transform.translate(segment_center);

      QMatrix4x4 orient;
      orient.setColumn(0, QVector4D(seg_tangent, 0.0F));
      orient.setColumn(1, QVector4D(plate_up, 0.0F));
      orient.setColumn(2, QVector4D(segment_normal, 0.0F));
      orient.setColumn(3, QVector4D(0.0F, 0.0F, 0.0F, 1.0F));
      seg_transform = seg_transform * orient;

      seg_transform.scale(segment_width, greave_len * 0.5F, greave_thickness);

      submitter.mesh(getUnitCube(), seg_transform, greaves_color, nullptr, 1.0F,
                     5);
    }
  };

  render_greave(frames.shin_l);
  render_greave(frames.shin_r);
}

} // namespace Render::GL
