

#include "skeleton.h"
#include "../creature/skeleton.h"
#include "humanoid_spec.h"

#include <QVector3D>
#include <array>
#include <cmath>

namespace Render::Humanoid {

namespace {

namespace Creature = Render::Creature;

constexpr std::array<std::string_view, kBoneCount> k_bone_names = {
    "Root",      "Pelvis",    "Spine",     "Chest",    "Neck",
    "Head",      "ShoulderL", "UpperArmL", "ForearmL", "HandL",
    "ShoulderR", "UpperArmR", "ForearmR",  "HandR",    "HipL",
    "KneeL",     "FootL",     "HipR",      "KneeR",    "FootR",
};

const std::array<SocketDef, kSocketCount> k_socket_defs = {{
    {"Head", static_cast<Creature::BoneIndex>(HumanoidBone::Head),
     QVector3D(0.0F, 0.00F, 0.0F)},
    {"HandR", static_cast<Creature::BoneIndex>(HumanoidBone::HandR),
     QVector3D(0.0F, 0.00F, 0.0F)},
    {"HandL", static_cast<Creature::BoneIndex>(HumanoidBone::HandL),
     QVector3D(0.0F, 0.00F, 0.0F)},
    {"GripR", static_cast<Creature::BoneIndex>(HumanoidBone::HandR),
     QVector3D(0.0F, 0.00F, 0.0F), QVector3D(1.0F, 0.0F, 0.0F),
     QVector3D(0.0F, 1.0F, 0.0F), QVector3D(0.0F, 0.0F, 1.0F)},
    {"GripL", static_cast<Creature::BoneIndex>(HumanoidBone::HandL),
     QVector3D(0.0F, 0.00F, 0.0F), QVector3D(-1.0F, 0.0F, 0.0F),
     QVector3D(0.0F, 1.0F, 0.0F), QVector3D(0.0F, 0.0F, -1.0F)},
    {"Back", static_cast<Creature::BoneIndex>(HumanoidBone::Chest),
     QVector3D(0.0F, 0.10F, -0.12F)},
    {"HipL", static_cast<Creature::BoneIndex>(HumanoidBone::HipL),
     QVector3D(0.0F, -0.02F, 0.0F)},
    {"HipR", static_cast<Creature::BoneIndex>(HumanoidBone::HipR),
     QVector3D(0.0F, -0.02F, 0.0F)},
    {"ChestFront", static_cast<Creature::BoneIndex>(HumanoidBone::Chest),
     QVector3D(0.0F, 0.05F, 0.15F)},
    {"ChestBack", static_cast<Creature::BoneIndex>(HumanoidBone::Chest),
     QVector3D(0.0F, 0.05F, -0.15F)},
    {"FootL", static_cast<Creature::BoneIndex>(HumanoidBone::FootL),
     QVector3D(0.0F, 0.0F, 0.0F)},
    {"FootR", static_cast<Creature::BoneIndex>(HumanoidBone::FootR),
     QVector3D(0.0F, 0.0F, 0.0F)},
}};

} // namespace

auto humanoid_topology() noexcept -> const Creature::SkeletonTopology & {
  static const std::array<Creature::BoneDef, kBoneCount> bones = [] {
    std::array<Creature::BoneDef, kBoneCount> out{};
    for (std::size_t i = 0; i < kBoneCount; ++i) {
      out[i].name = k_bone_names[i];
      out[i].parent = (kBoneParents[i] == kInvalidBone)
                          ? Creature::kInvalidBone
                          : static_cast<Creature::BoneIndex>(kBoneParents[i]);
    }
    return out;
  }();

  static const Creature::SkeletonTopology topo{
      std::span<const Creature::BoneDef>(bones.data(), bones.size()),
      std::span<const Creature::SocketDef>(k_socket_defs.data(),
                                           k_socket_defs.size()),
  };
  return topo;
}

namespace {

namespace Creature = Render::Creature;

struct HumanoidProviderContext {
  const Render::GL::HumanoidPose *pose;
  QVector3D pelvis;
  QVector3D neck_base;
  QVector3D head;
  QVector3D spine_tail;
  QVector3D hip_l;
  QVector3D hip_r;
  QVector3D body_up;
};

auto humanoid_provider(void *user, Creature::BoneIndex bone) noexcept
    -> Creature::BoneResolution {
  auto const *ctx = static_cast<const HumanoidProviderContext *>(user);
  auto const *p = ctx->pose;
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
    r.tail = p->hand_l + ctx->body_up * 0.10F;
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
    r.tail = p->hand_r + ctx->body_up * 0.10F;
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
    r.kind = Creature::BoneBasisKind::FromRootUp;
    r.head = p->foot_l;
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
    r.kind = Creature::BoneBasisKind::FromRootUp;
    r.head = p->foot_r;
    break;
  case HumanoidBone::Count:
    break;
  }
  return r;
}

} // namespace

auto bone_name(HumanoidBone bone) noexcept -> std::string_view {
  auto const i = static_cast<std::size_t>(bone);
  return i < kBoneCount ? k_bone_names[i] : std::string_view{"<invalid>"};
}

auto parent_of(HumanoidBone bone) noexcept -> std::uint8_t {
  auto const i = static_cast<std::size_t>(bone);
  return i < kBoneCount ? kBoneParents[i] : kInvalidBone;
}

auto socket_def(HumanoidSocket socket) noexcept -> const SocketDef & {
  auto const i = static_cast<std::size_t>(socket);
  static const SocketDef k_default{
      "<invalid>", static_cast<Creature::BoneIndex>(HumanoidBone::Root),
      QVector3D()};
  if (i >= kSocketCount) {
    return k_default;
  }
  return k_socket_defs[i];
}

auto socket_bone(HumanoidSocket socket) noexcept -> HumanoidBone {
  return static_cast<HumanoidBone>(socket_def(socket).bone);
}

void evaluate_skeleton(const Render::GL::HumanoidPose &pose,
                       const QVector3D &right_axis,
                       BonePalette &out_palette) noexcept {
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
      humanoid_topology(), &humanoid_provider, &ctx, right_axis,
      std::span<QMatrix4x4>(out_palette.data(), out_palette.size()));
}

auto socket_transform(const BonePalette &palette,
                      HumanoidSocket socket) noexcept -> QMatrix4x4 {
  return Creature::socket_transform(
      humanoid_topology(),
      std::span<const QMatrix4x4>(palette.data(), palette.size()),
      static_cast<Creature::SocketIndex>(socket));
}

auto socket_transform(const Render::GL::AttachmentFrame &bone_frame,
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

auto socket_position(const BonePalette &palette,
                     HumanoidSocket socket) noexcept -> QVector3D {
  return socket_transform(palette, socket).column(3).toVector3D();
}

auto socket_attachment_frame(const BonePalette &palette,
                             HumanoidSocket socket) noexcept
    -> Render::GL::AttachmentFrame {
  return Creature::socket_attachment_frame(
      humanoid_topology(),
      std::span<const QMatrix4x4>(palette.data(), palette.size()),
      static_cast<Creature::SocketIndex>(socket));
}

auto socket_attachment_frame(const Render::GL::AttachmentFrame &bone_frame,
                             HumanoidSocket socket) noexcept
    -> Render::GL::AttachmentFrame {
  return Creature::socket_attachment_frame(bone_frame, socket_def(socket));
}

auto bind_socket_transform(HumanoidSocket socket) noexcept -> QMatrix4x4 {
  return Creature::socket_transform(humanoid_topology(),
                                    humanoid_bind_palette(),
                                    static_cast<Creature::SocketIndex>(socket));
}

auto bind_socket_attachment_frame(HumanoidSocket socket) noexcept
    -> Render::GL::AttachmentFrame {
  Render::GL::AttachmentFrame frame = Creature::socket_attachment_frame(
      humanoid_topology(), humanoid_bind_palette(),
      static_cast<Creature::SocketIndex>(socket));
  const auto &bind_frames = humanoid_bind_body_frames();
  switch (socket) {
  case HumanoidSocket::Head:
    frame.radius = bind_frames.head.radius;
    frame.depth = bind_frames.head.depth;
    break;
  case HumanoidSocket::HandR:
  case HumanoidSocket::GripR:
    frame.radius = bind_frames.hand_r.radius;
    frame.depth = bind_frames.hand_r.depth;
    break;
  case HumanoidSocket::HandL:
  case HumanoidSocket::GripL:
    frame.radius = bind_frames.hand_l.radius;
    frame.depth = bind_frames.hand_l.depth;
    break;
  case HumanoidSocket::Back:
  case HumanoidSocket::ChestFront:
  case HumanoidSocket::ChestBack:
    frame.radius = bind_frames.torso.radius;
    frame.depth = bind_frames.torso.depth;
    break;
  case HumanoidSocket::HipL:
  case HumanoidSocket::HipR:
    frame.radius = bind_frames.waist.radius;
    frame.depth = bind_frames.waist.depth;
    break;
  case HumanoidSocket::FootL:
    frame.radius = bind_frames.foot_l.radius;
    frame.depth = bind_frames.foot_l.depth;
    break;
  case HumanoidSocket::FootR:
    frame.radius = bind_frames.foot_r.radius;
    frame.depth = bind_frames.foot_r.depth;
    break;
  case HumanoidSocket::Count:
    break;
  }
  return frame;
}

} // namespace Render::Humanoid
