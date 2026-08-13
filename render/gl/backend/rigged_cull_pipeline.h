#pragma once

#include <QMatrix4x4>
#include <QOpenGLExtraFunctions>
#include <QVector2D>
#include <QVector3D>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "render/gl/shader.h"

namespace Render::GL {
class ShaderCache;
struct RiggedCreatureCmd;
} // namespace Render::GL

namespace Render::GL::BackendPipelines {

class RiggedCullPipeline : protected QOpenGLExtraFunctions {
public:
  struct Stats {
    std::uint32_t dispatched_instances{0};
    std::uint32_t submitted_triangles{0};
    std::uint32_t candidate_triangles{0};
    std::uint32_t resident_instances{0};
    std::uint32_t draw_calls{0};
    bool overflowed{false};
  };

  RiggedCullPipeline() = default;
  ~RiggedCullPipeline();
  RiggedCullPipeline(const RiggedCullPipeline&) = delete;
  auto operator=(const RiggedCullPipeline&) -> RiggedCullPipeline& = delete;

  void set_shader_cache(ShaderCache* cache) { m_shader_cache = cache; }

  auto initialize() -> bool;
  void shutdown();

  [[nodiscard]] auto is_available() const -> bool { return m_available; }

  [[nodiscard]] static auto minimum_instances() -> std::size_t;

  auto draw(const RiggedCreatureCmd* const* cmds,
            std::size_t count,
            const QMatrix4x4& view_proj,
            const QVector3D& camera_position,
            const QVector2D& viewport) -> bool;

  auto draw_shadow(const RiggedCreatureCmd* const* cmds,
                   std::size_t count,
                   const QMatrix4x4& light_view_proj,
                   const QVector2D& shadow_extent) -> bool;

  auto draw_full_mesh(const RiggedCreatureCmd* const* cmds,
                      std::size_t count,
                      const QMatrix4x4& view_proj,
                      const QVector3D& camera_position) -> bool;

  auto draw_full_mesh_shadow(const RiggedCreatureCmd* const* cmds,
                             std::size_t count,
                             const QMatrix4x4& light_view_proj) -> bool;

  [[nodiscard]] auto has_shadow_path() const -> bool {
    return m_available && m_shadow_shader != nullptr;
  }

  [[nodiscard]] auto last_stats() const -> const Stats& { return m_stats; }

  [[nodiscard]] auto shader() -> Shader* { return m_draw_shader; }
  [[nodiscard]] auto full_mesh_shader() -> Shader* { return m_full_mesh_shader; }

private:
  enum class Pass : std::uint8_t {
    Color,
    Depth
  };

  auto dispatch(const RiggedCreatureCmd* const* cmds,
                std::size_t count,
                const QMatrix4x4& view_proj,
                const QVector3D& camera_position,
                const QVector2D& viewport,
                Pass pass) -> bool;
  auto ensure_buffers(std::size_t instance_count,
                      std::size_t bone_count,
                      std::size_t candidate_triangles) -> bool;
  auto ensure_instance_buffers(std::size_t instance_count,
                               std::size_t bone_count) -> bool;
  auto draw_full_mesh_pass(const RiggedCreatureCmd* const* cmds,
                           std::size_t count,
                           const QMatrix4x4& view_proj,
                           const QVector3D& camera_position,
                           Pass pass) -> bool;
  auto upload_instances(const RiggedCreatureCmd* const* cmds,
                        std::size_t count,
                        std::size_t bone_count) -> bool;
  void bind_role_color_texture(const RiggedCreatureCmd* const* cmds, std::size_t count);

  ShaderCache* m_shader_cache{nullptr};
  bool m_available{false};

  std::unique_ptr<Shader> m_cull_shader_storage;
  std::unique_ptr<Shader> m_finalize_shader_storage;
  std::unique_ptr<Shader> m_draw_shader_storage;
  std::unique_ptr<Shader> m_shadow_shader_storage;
  std::unique_ptr<Shader> m_full_mesh_shader_storage;
  std::unique_ptr<Shader> m_full_mesh_shadow_shader_storage;
  Shader* m_draw_shader{nullptr};
  Shader* m_shadow_shader{nullptr};
  Shader* m_full_mesh_shader{nullptr};
  Shader* m_full_mesh_shadow_shader{nullptr};

  GLuint m_palette_ssbo{0};
  GLuint m_instance_ssbo{0};
  GLuint m_out_index_buffer{0};
  GLuint m_command_buffer{0};
  GLuint m_vao{0};
  GLuint m_role_color_buffer{0};
  GLuint m_role_color_texture{0};

  std::size_t m_palette_capacity_bytes{0};
  std::size_t m_instance_capacity_bytes{0};
  std::size_t m_out_capacity_triangles{0};
  std::size_t m_role_color_capacity_bytes{0};

  std::vector<float> m_palette_scratch;
  std::vector<float> m_instance_scratch;
  std::vector<float> m_role_color_scratch;

  Stats m_stats{};
  std::uint32_t m_readback_counter{0};
};

} // namespace Render::GL::BackendPipelines
