#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include "render/gl/humanoid/humanoid_types.h"
#include "render/humanoid/schema/skeleton_schema.h"

namespace Render::Humanoid {

void evaluate_skeleton(const Render::GL::HumanoidPose& pose,
                       const QVector3D& right_axis,
                       BonePalette& out_palette) noexcept;

[[nodiscard]] auto socket_transform(const BonePalette& palette,
                                    HumanoidSocket socket) noexcept -> QMatrix4x4;

[[nodiscard]] auto socket_transform(const Render::GL::AttachmentFrame& bone_frame,
                                    HumanoidSocket socket) noexcept -> QMatrix4x4;

[[nodiscard]] auto socket_position(const BonePalette& palette,
                                   HumanoidSocket socket) noexcept -> QVector3D;

[[nodiscard]] auto
socket_attachment_frame(const BonePalette& palette,
                        HumanoidSocket socket) noexcept -> Render::GL::AttachmentFrame;

[[nodiscard]] auto
socket_attachment_frame(const Render::GL::AttachmentFrame& bone_frame,
                        HumanoidSocket socket) noexcept -> Render::GL::AttachmentFrame;

} // namespace Render::Humanoid
