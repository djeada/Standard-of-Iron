#pragma once

#include <QDebug>
#include <QMatrix4x4>
#include <QOpenGLFunctions_3_3_Core>
#include <QVector2D>
#include <QVector3D>

#include <array>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "backend/sky_box_transition.h"
#include "directional_shadow_block.h"
#include "persistent_buffer.h"
#include "render/decoration_gpu.h"
#include "render/draw_queue.h"
#include "render/frame_budget.h"
#include "render/gl/frame_environment.h"
#include "render/graphics_settings.h"
#include "render/i_render_backend.h"
#include "render/local_lighting.h"
#include "render/mist_volume.h"
#include "render/world_chunk.h"
#include "render/world_render_mode.h"
#include "resources.h"
#include "scene/camera.h"
#include "scene/environment_lighting.h"
#include "shader.h"
#include "shader_cache.h"

namespace Render::GL::BackendPipelines {
class CylinderPipeline;
class VegetationPipeline;
class TerrainPipeline;
class RiggedCharacterPipeline;
class RiggedCullPipeline;
class WaterPipeline;
class EffectsPipeline;
class PrimitiveBatchPipeline;
class BannerPipeline;
class HealingBeamPipeline;
class HealerAuraPipeline;
class CombatDustPipeline;
class RainPipeline;
class ModeIndicatorPipeline;
class GroundMarkerPipeline;
class PostProcessPipeline;
class MeshInstancingPipeline;
class SkyBoxPipeline;
} // namespace Render::GL::BackendPipelines

namespace Render::GL {

class ShaderUniformCache;

class Backend : public IRenderBackend,
                public IFrameEnvironment,
                protected QOpenGLFunctions_3_3_Core {
public:
  struct PlaybackStats {
    std::size_t submitted_commands{0};
    std::size_t prepared_batches{0};
    std::size_t rigged_commands{0};
    std::size_t rigged_prepared_batches{0};
    std::size_t rigged_instanced_draws{0};
    std::size_t rigged_instanced_instances{0};
    std::size_t rigged_single_draws{0};
    std::size_t shadow_rigged_instanced_draws{0};
    std::size_t shadow_rigged_instanced_instances{0};
    std::size_t shadow_rigged_single_draws{0};
    std::size_t shadow_static_single_draws{0};
    std::size_t shadow_static_instanced_draws{0};
    std::size_t shadow_static_instanced_instances{0};
    double gpu_shadow_ms{0.0};
    double gpu_color_ms{0.0};
    double gpu_wait_ms{0.0};
  };

  Backend();
  explicit Backend(ShaderQuality quality);
  ~Backend() override;

  Backend(const Backend&) = delete;
  auto operator=(const Backend&) -> Backend& = delete;
  Backend(Backend&&) = delete;
  auto operator=(Backend&&) -> Backend& = delete;

  [[nodiscard]] auto initialize() -> bool override;
  void begin_frame() override;
  void set_viewport(int w, int h) override;
  void set_clear_color(float r, float g, float b, float a) override;
  void set_animation_time(float time) noexcept override { m_animation_time = time; }
  void execute(const DrawQueue& queue, const Camera& cam) override;
  void execute_scene(const DrawQueue& queue, const Camera& cam);

  void set_world_render_mode(WorldRenderMode mode) noexcept {
    m_sky_box_transition.set_target(mode == WorldRenderMode::Rpg);
  }

  void set_environment_lighting(const EnvironmentLightingState& lighting) noexcept {
    m_environment_lighting = lighting.sanitized();
    m_light_dir = m_environment_lighting.primary_direction;
    m_ambient_strength = m_environment_lighting.ambient_intensity;
  }
  [[nodiscard]] auto
  environment_lighting() const noexcept -> const EnvironmentLightingState& override {
    return m_environment_lighting;
  }
  void reset_local_lights() noexcept { m_local_light_fader.reset(); }
  void set_mist_volumes(const std::vector<Render::MistVolume>& volumes) {
    m_mist_volumes = volumes;
    m_mist_volumes_dirty = true;
  }
  void set_ground_fog(const Render::GroundFogSettings& fog) noexcept {
    m_ground_fog = fog;
  }
  [[nodiscard]] auto light_direction() const noexcept -> const QVector3D& override {
    return m_light_dir;
  }
  [[nodiscard]] auto ambient_strength() const noexcept -> float override {
    return m_ambient_strength;
  }
  [[nodiscard]] auto viewport_height() const noexcept -> int override {
    return m_viewport_height;
  }
  [[nodiscard]] auto last_playback_stats() const noexcept -> const PlaybackStats& {
    return m_last_playback_stats;
  }

  [[nodiscard]] auto resources() const -> ResourceManager* override {
    return m_resources.get();
  }

  [[nodiscard]] auto shader(const QString& name) const -> Shader* override {
    return m_shader_cache ? m_shader_cache->get(name) : nullptr;
  }

  [[nodiscard]] auto supports_shaders() const -> bool override { return true; }
  [[nodiscard]] auto shader_quality() const -> ShaderQuality override {
    return m_shader_quality;
  }
  void set_shader_quality(ShaderQuality q) noexcept { m_shader_quality = q; }
  auto get_or_load_shader(const QString& name,
                          const QString& vert_path,
                          const QString& frag_path) -> Shader* {
    if (!m_shader_cache) {
      return nullptr;
    }
    return m_shader_cache->load(name, vert_path, frag_path);
  }

  [[nodiscard]] auto banner_mesh() const -> Mesh*;

  [[nodiscard]] auto banner_shader() const -> Shader*;

  [[nodiscard]] auto troop_shadow_shader() const noexcept -> Shader* {
    return m_shadow_shader;
  }

  void enable_depth_test(bool enable) {
    if (enable) {
      glEnable(GL_DEPTH_TEST);
    } else {
      glDisable(GL_DEPTH_TEST);
    }
  }
  void set_depth_func(GLenum func) { glDepthFunc(func); }
  void set_depth_mask(bool write) { glDepthMask(write ? GL_TRUE : GL_FALSE); }

  void enable_blend(bool enable) {
    if (enable) {
      glEnable(GL_BLEND);
    } else {
      glDisable(GL_BLEND);
    }
  }
  void set_blend_func(GLenum src, GLenum dst) { glBlendFunc(src, dst); }

  void enable_polygon_offset(bool enable) {
    if (enable) {
      glEnable(GL_POLYGON_OFFSET_FILL);
    } else {
      glDisable(GL_POLYGON_OFFSET_FILL);
    }
  }
  void set_polygon_offset(float factor, float units) { glPolygonOffset(factor, units); }

  void set_frame_budget(const Render::FrameBudgetConfig& config) override {
    m_frame_budget_config = config;
  }
  [[nodiscard]] auto frame_tracker() const -> const Render::FrameTimeTracker* override {
    return &m_frame_tracker;
  }

private:
  struct CommandExecutionContext {
    const DrawQueue& queue;
    const Camera& cam;
    const QMatrix4x4& view;
    const QMatrix4x4& projection;
    const QMatrix4x4& view_proj;
    float banner_wind_strength;
    bool& polygon_offset_enabled;
  };

  void upload_frame_uniform_buffers(const QMatrix4x4& view_proj,
                                    const DrawQueue& queue,
                                    const Camera& cam);

  void set_ground_plane_uniforms(Shader& shader,
                                 const TerrainSurfaceCmd& single,
                                 const QMatrix4x4& mvp,
                                 const QVector3D& camera_position);
  void set_terrain_chunk_uniforms(Shader& shader,
                                  const TerrainSurfaceCmd& single,
                                  const QMatrix4x4& mvp,
                                  const QVector3D& camera_position);

  void execute_cylinder_commands(const PreparedBatch& prepared,
                                 CommandExecutionContext& context);
  void execute_scatter_commands(const PreparedBatch& prepared,
                                CommandExecutionContext& context);
  void execute_terrain_commands(const PreparedBatch& prepared,
                                CommandExecutionContext& context);
  void execute_water_linear_commands(const PreparedBatch& prepared,
                                     CommandExecutionContext& context);
  void execute_mesh_commands(const PreparedBatch& prepared,
                             CommandExecutionContext& context);
  void execute_effects_commands(const PreparedBatch& prepared,
                                CommandExecutionContext& context);
  void execute_rigged_commands(const PreparedBatch& prepared,
                               CommandExecutionContext& context);

  void sync_graphics_profile();
  void update_contact_shadow_uniforms();
  void apply_graphics_profile(const Render::GraphicsProfile& profile, bool initial);

  void render_directional_shadows(const DrawQueue& queue, const Camera& cam);
  void ensure_directional_shadow_resources(int resolution, int cascades);
  void release_directional_shadow_resources();

  template <typename Visitor>
  void for_each_pipeline_slot(Visitor&& visit) {
    visit(m_cylinder_pipeline);
    visit(m_vegetation_pipeline);
    visit(m_terrain_pipeline);
    visit(m_rigged_character_pipeline);
    visit(m_rigged_cull_pipeline);
    visit(m_water_pipeline);
    visit(m_effects_pipeline);
    visit(m_primitive_batch_pipeline);
    visit(m_banner_pipeline);
    visit(m_healing_beam_pipeline);
    visit(m_healer_aura_pipeline);
    visit(m_combat_dust_pipeline);
    visit(m_rain_pipeline);
    visit(m_mode_indicator_pipeline);
    visit(m_ground_marker_pipeline);
    visit(m_mesh_instancing_pipeline);
    visit(m_post_process_pipeline);
    visit(m_sky_box_pipeline);
  }

  template <typename Subsystem, typename... Args>
  auto create_subsystem(std::unique_ptr<Subsystem>& slot,
                        const char* name,
                        Args&&... args) -> bool {
    qInfo() << "Backend: Creating" << name << "...";
    slot = std::make_unique<Subsystem>(std::forward<Args>(args)...);
    if (!slot->initialize()) {
      qCritical() << "Backend::initialize() FAILED:" << name;
      return false;
    }
    qInfo() << "Backend:" << name << "initialized";
    return true;
  }

  int m_viewport_width{0};
  int m_viewport_height{0};

  std::array<float, 4> m_clear_color{0.055F, 0.065F, 0.05F, 1.0F};
  std::unique_ptr<ShaderCache> m_shader_cache;
  std::unique_ptr<ResourceManager> m_resources;
  std::unique_ptr<BackendPipelines::CylinderPipeline> m_cylinder_pipeline;
  std::unique_ptr<BackendPipelines::VegetationPipeline> m_vegetation_pipeline;
  std::unique_ptr<BackendPipelines::TerrainPipeline> m_terrain_pipeline;
  std::unique_ptr<ShaderUniformCache> m_shader_uniform_cache;
  std::unique_ptr<BackendPipelines::RiggedCharacterPipeline>
      m_rigged_character_pipeline;
  std::unique_ptr<BackendPipelines::RiggedCullPipeline> m_rigged_cull_pipeline;
  std::unique_ptr<BackendPipelines::WaterPipeline> m_water_pipeline;
  std::unique_ptr<BackendPipelines::EffectsPipeline> m_effects_pipeline;
  std::unique_ptr<BackendPipelines::PrimitiveBatchPipeline> m_primitive_batch_pipeline;
  std::unique_ptr<BackendPipelines::BannerPipeline> m_banner_pipeline;
  std::unique_ptr<BackendPipelines::HealingBeamPipeline> m_healing_beam_pipeline;
  std::unique_ptr<BackendPipelines::HealerAuraPipeline> m_healer_aura_pipeline;
  std::unique_ptr<BackendPipelines::CombatDustPipeline> m_combat_dust_pipeline;
  std::unique_ptr<BackendPipelines::RainPipeline> m_rain_pipeline;
  std::unique_ptr<BackendPipelines::ModeIndicatorPipeline> m_mode_indicator_pipeline;
  std::unique_ptr<BackendPipelines::GroundMarkerPipeline> m_ground_marker_pipeline;
  std::unique_ptr<BackendPipelines::MeshInstancingPipeline> m_mesh_instancing_pipeline;
  std::unique_ptr<BackendPipelines::PostProcessPipeline> m_post_process_pipeline;
  std::unique_ptr<BackendPipelines::SkyBoxPipeline> m_sky_box_pipeline;

  Shader* m_basic_shader = nullptr;
  Shader* m_grid_shader = nullptr;
  Shader* m_shadow_shader = nullptr;
  Shader* m_shadow_instanced_shader = nullptr;

  Shader* m_last_bound_shader = nullptr;
  Texture* m_last_bound_texture = nullptr;
  bool m_depth_testEnabled = true;
  bool m_blend_enabled = false;
  float m_animation_time = 0.0F;
  float m_sky_box_blend_time = 0.0F;
  BackendPipelines::SkyBoxTransition m_sky_box_transition;
  GLuint m_frame_ubo{0};
  GLuint m_environment_lighting_ubo{0};
  GLuint m_local_lighting_ubo{0};
  Render::LocalLightFader m_local_light_fader{};
  std::vector<Render::LocalLight> m_active_local_lights{};
  GLuint m_directional_shadow_ubo{0};
  GLuint m_directional_shadow_fbo{0};
  std::uint32_t m_graphics_generation{0U};
  bool m_graphics_profile_applied{false};
  Render::ShaderTier m_shader_tier{Render::ShaderTier::High};
  Render::DirectionalShadowSettings m_shadow_settings{};
  GLuint m_directional_shadow_texture{0};
  GLuint m_directional_shadow_far_texture{0};
  GLuint m_directional_shadow_compare_sampler{0};
  GLuint m_directional_shadow_depth_sampler{0};
  int m_directional_shadow_resolution{0};
  int m_directional_shadow_far_resolution{0};
  int m_directional_shadow_cascades{0};
  int m_directional_shadow_near_cascades{0};

  struct ShadowStaticCaster {
    Mesh* mesh = nullptr;
    const QMatrix4x4* model = nullptr;
    QVector3D world_center;
    float world_radius = 0.0F;
  };
  std::vector<ShadowStaticCaster> m_shadow_static_casters;
  std::vector<const ShadowStaticCaster*> m_shadow_cascade_casters;

  std::unordered_map<const Mesh*, std::pair<float, float>>
      m_shadow_terrain_height_cache;

  std::size_t m_rigged_drawn_this_frame = 0;
  PlaybackStats m_last_playback_stats{};

  std::array<QMatrix4x4, k_max_shadow_cascades> m_directional_shadow_matrices{};
  std::array<float, k_max_shadow_cascades> m_directional_shadow_splits{};
  Shader* m_directional_shadow_depth_shader = nullptr;
  Shader* m_directional_shadow_depth_instanced_shader = nullptr;
  Shader* m_directional_shadow_rigged_shader = nullptr;
  Shader::UniformHandle m_shadow_depth_light_vp = Shader::InvalidUniform;
  Shader::UniformHandle m_shadow_depth_model = Shader::InvalidUniform;
  Shader::UniformHandle m_shadow_depth_instanced_light_vp = Shader::InvalidUniform;

  QVector3D m_light_dir{0.65F, 0.50F, 0.40F};
  float m_ambient_strength{0.30F};
  EnvironmentLightingState m_environment_lighting{};
  std::vector<Render::MistVolume> m_mist_volumes;
  Render::GroundFogSettings m_ground_fog{};
  bool m_mist_volumes_dirty{false};

  Render::FrameBudgetConfig m_frame_budget_config;
  Render::FrameTimeTracker m_frame_tracker;

  static constexpr std::size_t k_frames_in_flight = 2;
  static constexpr std::size_t k_gpu_breakdown_marks = 48;
  struct FrameGpuTiming {
    GLsync fence{nullptr};
    std::array<GLuint, 4> timestamps{0U, 0U, 0U, 0U};
    std::array<GLuint, k_gpu_breakdown_marks> marks{};
    std::array<std::uint8_t, k_gpu_breakdown_marks> mark_types{};
    std::size_t mark_count{0};
    bool pending{false};
  };
  QVector2D m_contact_shadow_ground{0.0F, 1.0F};
  float m_contact_shadow_cast_weight{-1.0F};
  bool m_contact_shadow_uniforms_valid{false};

  bool m_gpu_breakdown_enabled{false};

  bool m_frame_timing_active{false};
  std::array<double, 32> m_gpu_breakdown_ms{};
  double m_gpu_breakdown_post_ms{0.0};
  std::size_t m_gpu_breakdown_frames{0};
  void gpu_breakdown_mark(FrameGpuTiming& slot, std::uint8_t type);
  std::array<FrameGpuTiming, k_frames_in_flight> m_frame_timings{};
  std::size_t m_frame_timing_slot{0};
  FrameGpuTiming* m_active_frame_timing{nullptr};
  void wait_for_frame_slot(FrameGpuTiming& slot);
  ShaderQuality m_shader_quality{ShaderQuality::Full};
};

} // namespace Render::GL
