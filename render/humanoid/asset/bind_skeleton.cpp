#include "render/humanoid/asset/bind_skeleton.h"

#include "render/creature/skeleton.h"
#include "render/humanoid/asset/humanoid_spec.h"

namespace Render::Humanoid {

namespace {
namespace Creature = Render::Creature;
}

auto bind_socket_transform(HumanoidSocket socket) noexcept -> QMatrix4x4 {
  return Creature::socket_transform(humanoid_topology(),
                                    humanoid_bind_palette(),
                                    static_cast<Creature::SocketIndex>(socket));
}

auto bind_socket_attachment_frame(HumanoidSocket socket) noexcept
    -> Render::GL::AttachmentFrame {
  Render::GL::AttachmentFrame frame =
      Creature::socket_attachment_frame(humanoid_topology(),
                                        humanoid_bind_palette(),
                                        static_cast<Creature::SocketIndex>(socket));
  const auto& bind_frames = humanoid_bind_body_frames();
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
