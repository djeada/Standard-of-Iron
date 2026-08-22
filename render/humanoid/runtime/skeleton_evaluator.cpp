#include "render/humanoid/runtime/skeleton_evaluator.h"

#include <QVector3D>

#include <cmath>
#include <span>

#include "animation/rig/side.h"
#include "render/creature/skeleton.h"

namespace Render::Humanoid {

namespace {

namespace Creature = Render::Creature;

struct HumanoidProviderContext {
  const Render::GL::HumanoidPose* pose{};
  QVector3D pelvis;
  QVector3D neck_base;
  QVector3D head;
  QVector3D spine_tail;
  QVector3D hip_l;
  QVector3D hip_r;
  QVector3D body_up;
};

auto resolved_hand_axis(const Render::GL::HumanoidPose& pose,
                        Render::GL::Side side,
                        const QVector3D& body_up) noexcept -> QVector3D {
  Render::GL::AttachmentFrame const& hand_frame = (side == Render::GL::Side::Left)
                                                      ? pose.body_frames.hand_l
                                                      : pose.body_frames.hand_r;
  QVector3D axis{};
  if (hand_frame.radius > 0.0F && hand_frame.up.lengthSquared() >= 1e-6F) {
    axis = hand_frame.up;
  } else {
    axis = body_up;
  }
  if (axis.lengthSquared() < 1e-6F) {
    axis = QVector3D(0.0F, 1.0F, 0.0F);
  } else {
    axis.normalize();
  }
  return axis;
}

auto foot_up_axis(float pitch) noexcept -> QVector3D {

  return {0.0F, std::cos(pitch), -std::sin(pitch)};
}

auto humanoid_provider(void* user,
                       Creature::BoneIndex bone) noexcept -> Creature::BoneResolution {
  auto const* ctx = static_cast<const HumanoidProviderContext*>(user);
  auto const* p = ctx->pose;
  auto const b = static_cast<HumanoidBone>(bone);

  Creature::BoneResolution r;
  switch (b) {
  case HumanoidBone::Root:
    r.kind = Creature::BoneBasisKind::FromRootUp;
    r.head = ctx->pelvis;
    break;
  case HumanoidBone::Pelvis:
    r.kind = Creature::BoneBasisKind::FromParent;
    r.head = ctx->pelvis;
    break;
  case HumanoidBone::Spine:
    r.kind = Creature::BoneBasisKind::FromHeadTail;
    r.head = ctx->pelvis;
    r.tail = ctx->spine_tail;
    break;
  case HumanoidBone::Chest:
    r.kind = Creature::BoneBasisKind::FromHeadTail;
    r.head = ctx->spine_tail;
    r.tail = ctx->neck_base;
    break;
  case HumanoidBone::Neck:
    r.kind = Creature::BoneBasisKind::FromHeadTail;
    r.head = ctx->neck_base;
    r.tail = ctx->head;
    break;
  case HumanoidBone::Head:
    r.kind = Creature::BoneBasisKind::FromParent;
    r.head = ctx->head;
    break;
  case HumanoidBone::ShoulderL:
    r.kind = Creature::BoneBasisKind::FromParent;
    r.head = p->shoulder_l;
    break;
  case HumanoidBone::UpperArmL:
    r.kind = Creature::BoneBasisKind::FromHeadTail;
    r.head = p->shoulder_l;
    r.tail = p->elbow_l;
    break;
  case HumanoidBone::ForearmL:
    r.kind = Creature::BoneBasisKind::FromHeadTail;
    r.head = p->elbow_l;
    r.tail = p->hand_l;
    break;
  case HumanoidBone::HandL:
    r.kind = Creature::BoneBasisKind::FromHeadTail;
    r.head = p->hand_l;
    r.tail = p->hand_l +
             resolved_hand_axis(*p, Render::GL::Side::Left, ctx->body_up) * 0.10F;
    break;
  case HumanoidBone::ShoulderR:
    r.kind = Creature::BoneBasisKind::FromParent;
    r.head = p->shoulder_r;
    break;
  case HumanoidBone::UpperArmR:
    r.kind = Creature::BoneBasisKind::FromHeadTail;
    r.head = p->shoulder_r;
    r.tail = p->elbow_r;
    break;
  case HumanoidBone::ForearmR:
    r.kind = Creature::BoneBasisKind::FromHeadTail;
    r.head = p->elbow_r;
    r.tail = p->hand_r;
    break;
  case HumanoidBone::HandR:
    r.kind = Creature::BoneBasisKind::FromHeadTail;
    r.head = p->hand_r;
    r.tail = p->hand_r +
             resolved_hand_axis(*p, Render::GL::Side::Right, ctx->body_up) * 0.10F;
    break;
  case HumanoidBone::HipL:
    r.kind = Creature::BoneBasisKind::FromHeadTail;
    r.head = ctx->hip_l;
    r.tail = p->knee_l;
    break;
  case HumanoidBone::KneeL:
    r.kind = Creature::BoneBasisKind::FromHeadTail;
    r.head = p->knee_l;
    r.tail = p->foot_l;
    break;
  case HumanoidBone::FootL:
    r.kind = Creature::BoneBasisKind::FromHeadTail;
    r.head = p->foot_l;
    r.tail = p->foot_l + foot_up_axis(p->foot_pitch_l) * 0.10F;
    break;
  case HumanoidBone::HipR:
    r.kind = Creature::BoneBasisKind::FromHeadTail;
    r.head = ctx->hip_r;
    r.tail = p->knee_r;
    break;
  case HumanoidBone::KneeR:
    r.kind = Creature::BoneBasisKind::FromHeadTail;
    r.head = p->knee_r;
    r.tail = p->foot_r;
    break;
  case HumanoidBone::FootR:
    r.kind = Creature::BoneBasisKind::FromHeadTail;
    r.head = p->foot_r;
    r.tail = p->foot_r + foot_up_axis(p->foot_pitch_r) * 0.10F;
    break;
  case HumanoidBone::Count:
    break;
  }
  return r;
}

} // namespace

void evaluate_skeleton(const Render::GL::HumanoidPose& pose,
                       const QVector3D& right_axis,
                       BonePalette& out_palette) noexcept {
  HumanoidProviderContext ctx;
  ctx.pose = &pose;
  ctx.pelvis = pose.pelvis_pos;
  ctx.neck_base = pose.neck_base;
  ctx.head = pose.head_pos;
  ctx.spine_tail = ctx.pelvis + (ctx.neck_base - ctx.pelvis) * (1.0F / 3.0F);
  ctx.body_up = ctx.neck_base - ctx.pelvis;
  if (ctx.body_up.lengthSquared() < 1.0e-8F) {
    ctx.body_up = QVector3D(0.0F, 1.0F, 0.0F);
  } else {
    ctx.body_up.normalize();
  }

  QVector3D const to_knee_l = pose.knee_l - ctx.pelvis;
  QVector3D const to_knee_r = pose.knee_r - ctx.pelvis;
  ctx.hip_l = ctx.pelvis + QVector3D(to_knee_l.x(), 0.0F, to_knee_l.z()) * 0.3F;
  ctx.hip_r = ctx.pelvis + QVector3D(to_knee_r.x(), 0.0F, to_knee_r.z()) * 0.3F;

  Creature::evaluate_skeleton(
      humanoid_topology(),
      &humanoid_provider,
      &ctx,
      right_axis,
      std::span<QMatrix4x4>(out_palette.data(), out_palette.size()));
}

auto socket_transform(const BonePalette& palette,
                      HumanoidSocket socket) noexcept -> QMatrix4x4 {
  return Creature::socket_transform(
      humanoid_topology(),
      std::span<const QMatrix4x4>(palette.data(), palette.size()),
      static_cast<Creature::SocketIndex>(socket));
}

auto socket_transform(const Render::GL::AttachmentFrame& bone_frame,
                      HumanoidSocket socket) noexcept -> QMatrix4x4 {
  Render::GL::AttachmentFrame const socket_frame =
      Creature::socket_attachment_frame(bone_frame, socket_def(socket));
  QMatrix4x4 m;
  m.setColumn(0, QVector4D(socket_frame.right, 0.0F));
  m.setColumn(1, QVector4D(socket_frame.up, 0.0F));
  m.setColumn(2, QVector4D(socket_frame.forward, 0.0F));
  m.setColumn(3, QVector4D(socket_frame.origin, 1.0F));
  return m;
}

auto socket_position(const BonePalette& palette,
                     HumanoidSocket socket) noexcept -> QVector3D {
  return socket_transform(palette, socket).column(3).toVector3D();
}

auto socket_attachment_frame(const BonePalette& palette, HumanoidSocket socket) noexcept
    -> Render::GL::AttachmentFrame {
  return Creature::socket_attachment_frame(
      humanoid_topology(),
      std::span<const QMatrix4x4>(palette.data(), palette.size()),
      static_cast<Creature::SocketIndex>(socket));
}

auto socket_attachment_frame(const Render::GL::AttachmentFrame& bone_frame,
                             HumanoidSocket socket) noexcept
    -> Render::GL::AttachmentFrame {
  return Creature::socket_attachment_frame(bone_frame, socket_def(socket));
}

} // namespace Render::Humanoid
