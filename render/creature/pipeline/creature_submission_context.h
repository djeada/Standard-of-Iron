#pragma once

namespace Render::GL {
class ISubmitter;
class RiggedMeshCache;
class Shader;
class SnapshotMeshCache;
} // namespace Render::GL

namespace Render::Creature::Pipeline {

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

private:
  Render::GL::RiggedMeshCache* m_rigged_meshes;
  Render::GL::SnapshotMeshCache* m_snapshot_meshes;
  Render::GL::Shader** m_current_shader;
};

[[nodiscard]] auto resolve_creature_submission(Render::GL::ISubmitter& out) noexcept
    -> CreatureSubmissionContext*;

[[nodiscard]] auto detached_rigged_mesh_cache() -> Render::GL::RiggedMeshCache&;

} // namespace Render::Creature::Pipeline
