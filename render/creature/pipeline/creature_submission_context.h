#pragma once

#include <QMatrix4x4>
#include <QVector3D>

namespace Render::GL {
class ISubmitter;
class RiggedMeshCache;
class Shader;
class SnapshotMeshCache;
} // namespace Render::GL

namespace Render::Creature::Pipeline {

inline constexpr float k_reduced_geometry_max_height_px = 60.0F;

class ReducedGeometryRule {
public:
  ReducedGeometryRule() = default;
  ReducedGeometryRule(const QVector3D& camera_position, float focal_length_px) noexcept
      : m_camera(camera_position)
      , m_focal_sq_over_max_height_sq(
            focal_length_px * focal_length_px /
            (k_reduced_geometry_max_height_px * k_reduced_geometry_max_height_px))
      , m_enabled(focal_length_px > 0.0F) {}

  [[nodiscard]] auto small_on_screen(const QMatrix4x4& world_from_unit,
                                     float mesh_height) const noexcept -> bool {
    if (!m_enabled || !(mesh_height > 0.0F)) {
      return false;
    }
    const float dx = world_from_unit(0, 3) - m_camera.x();
    const float dy = world_from_unit(1, 3) - m_camera.y();
    const float dz = world_from_unit(2, 3) - m_camera.z();
    const float distance_sq = (dx * dx) + (dy * dy) + (dz * dz);
    const float up_scale_sq = (world_from_unit(0, 1) * world_from_unit(0, 1)) +
                              (world_from_unit(1, 1) * world_from_unit(1, 1)) +
                              (world_from_unit(2, 1) * world_from_unit(2, 1));
    const float world_height_sq = mesh_height * mesh_height * up_scale_sq;
    return world_height_sq * m_focal_sq_over_max_height_sq < distance_sq;
  }

private:
  QVector3D m_camera;
  float m_focal_sq_over_max_height_sq{0.0F};
  bool m_enabled{false};
};

class CreatureSubmissionContext {
public:
  CreatureSubmissionContext(Render::GL::RiggedMeshCache& rigged_meshes,
                            Render::GL::SnapshotMeshCache& snapshot_meshes,
                            Render::GL::Shader*& current_shader) noexcept
      : m_rigged_meshes(&rigged_meshes)
      , m_snapshot_meshes(&snapshot_meshes)
      , m_current_shader(&current_shader) {}

  CreatureSubmissionContext(const CreatureSubmissionContext&) = delete;
  CreatureSubmissionContext(CreatureSubmissionContext&&) = delete;
  auto
  operator=(const CreatureSubmissionContext&) -> CreatureSubmissionContext& = delete;
  auto operator=(CreatureSubmissionContext&&) -> CreatureSubmissionContext& = delete;
  ~CreatureSubmissionContext() = default;

  [[nodiscard]] auto rigged_meshes() const noexcept -> Render::GL::RiggedMeshCache& {
    return *m_rigged_meshes;
  }
  [[nodiscard]] auto
  snapshot_meshes() const noexcept -> Render::GL::SnapshotMeshCache& {
    return *m_snapshot_meshes;
  }
  [[nodiscard]] auto current_shader() const noexcept -> Render::GL::Shader* {
    return *m_current_shader;
  }
  void set_current_shader(Render::GL::Shader* shader) const noexcept {
    *m_current_shader = shader;
  }

  void set_reduced_geometry(const ReducedGeometryRule& rule) noexcept {
    m_reduced_geometry = rule;
  }
  [[nodiscard]] auto reduced_geometry() const noexcept -> const ReducedGeometryRule& {
    return m_reduced_geometry;
  }

private:
  Render::GL::RiggedMeshCache* m_rigged_meshes;
  Render::GL::SnapshotMeshCache* m_snapshot_meshes;
  Render::GL::Shader** m_current_shader;
  ReducedGeometryRule m_reduced_geometry;
};

[[nodiscard]] auto resolve_creature_submission(Render::GL::ISubmitter& out) noexcept
    -> CreatureSubmissionContext*;

[[nodiscard]] auto detached_rigged_mesh_cache() -> Render::GL::RiggedMeshCache&;

} // namespace Render::Creature::Pipeline
