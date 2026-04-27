#include "horse_attachment_archetype.h"

#include "../../horse/dimensions.h"
#include "../../horse/horse_motion.h"

#include <QVector3D>

namespace Render::GL {

namespace {

auto make_baseline_dims() noexcept -> HorseDimensions {
  HorseProfile const profile = make_horse_profile(0U, QVector3D(), QVector3D());
  return profile.dims;
}

auto make_identity_frame(const QVector3D &origin) noexcept
    -> HorseAttachmentFrame {
  HorseAttachmentFrame f;
  f.origin = origin;
  f.right = QVector3D(1.0F, 0.0F, 0.0F);
  f.up = QVector3D(0.0F, 1.0F, 0.0F);
  f.forward = QVector3D(0.0F, 0.0F, 1.0F);
  return f;
}

} // namespace

auto horse_baseline_back_center_frame() noexcept -> HorseAttachmentFrame {

  HorseProfile const profile = make_horse_profile(0U, QVector3D(), QVector3D());
  MountedAttachmentFrame const mount = compute_mount_frame(profile);

  HorseAttachmentFrame frame;
  frame.origin = mount.saddle_center;
  frame.right = QVector3D(1.0F, 0.0F, 0.0F);
  frame.up = QVector3D(0.0F, 1.0F, 0.0F);
  frame.forward = QVector3D(0.0F, 0.0F, 1.0F);
  return frame;
}

auto horse_baseline_barrel_frame() noexcept -> HorseAttachmentFrame {
  const auto d = make_baseline_dims();

  return make_identity_frame(QVector3D(0.0F, d.barrel_center_y, 0.0F));
}

auto horse_baseline_chest_frame() noexcept -> HorseAttachmentFrame {
  const auto d = make_baseline_dims();

  const QVector3D barrel(0.0F, d.barrel_center_y, 0.0F);
  const QVector3D chest =
      barrel + QVector3D(0.0F, d.body_height * 0.12F, d.body_length * 0.34F);
  return make_identity_frame(chest);
}

auto horse_baseline_rump_frame() noexcept -> HorseAttachmentFrame {
  const auto d = make_baseline_dims();

  const QVector3D barrel(0.0F, d.barrel_center_y, 0.0F);
  const QVector3D rump =
      barrel + QVector3D(0.0F, d.body_height * 0.08F, -d.body_length * 0.36F);
  return make_identity_frame(rump);
}

auto horse_baseline_neck_base_frame() noexcept -> HorseAttachmentFrame {
  const auto d = make_baseline_dims();

  const QVector3D barrel(0.0F, d.barrel_center_y, 0.0F);
  const QVector3D chest =
      barrel + QVector3D(0.0F, d.body_height * 0.12F, d.body_length * 0.34F);
  const QVector3D neck_base =
      chest + QVector3D(0.0F, d.body_height * 0.42F, d.body_length * 0.08F);
  return make_identity_frame(neck_base);
}

auto horse_baseline_muzzle_frame() noexcept -> HorseAttachmentFrame {
  const auto d = make_baseline_dims();

  const QVector3D barrel(0.0F, d.barrel_center_y, 0.0F);
  const QVector3D chest =
      barrel + QVector3D(0.0F, d.body_height * 0.12F, d.body_length * 0.34F);
  const QVector3D neck_base =
      chest + QVector3D(0.0F, d.body_height * 0.42F, d.body_length * 0.08F);
  const QVector3D neck_top =
      neck_base + QVector3D(0.0F, d.neck_rise, d.neck_length);
  const QVector3D head_center =
      neck_top + QVector3D(0.0F, d.head_height * 0.10F, d.head_length * 0.40F);
  const QVector3D muzzle = head_center + QVector3D(0.0F, -d.head_height * 0.18F,
                                                   d.head_length * 0.58F);
  return make_identity_frame(muzzle);
}

} // namespace Render::GL
