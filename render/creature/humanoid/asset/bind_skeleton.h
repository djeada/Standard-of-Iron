#pragma once

#include <QMatrix4x4>

#include "render/creature/humanoid/humanoid_types.h"
#include "render/creature/humanoid/schema/skeleton_schema.h"

namespace Render::Humanoid {

[[nodiscard]] auto bind_socket_transform(HumanoidSocket socket) noexcept -> QMatrix4x4;

[[nodiscard]] auto bind_socket_attachment_frame(HumanoidSocket socket) noexcept
    -> Render::GL::AttachmentFrame;

} // namespace Render::Humanoid
