#pragma once

#include <QVector3D>

#include <cstdint>
#include <memory>

namespace Render::GL {
class Mesh;
}

namespace Render::Humanoid {

inline constexpr float k_face_feature_uv = -3.0F;

enum class FaceFeatureTone : std::uint8_t {
  EyeWhite = 0,
  Iris,
  Pupil,
  Catchlight,
  Mouth,
};

[[nodiscard]] auto build_humanoid_face_mesh(const QVector3D& cranium_centre,
                                            const QVector3D& cranium_radii)
    -> std::unique_ptr<Render::GL::Mesh>;

} // namespace Render::Humanoid
