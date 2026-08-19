#pragma once

#include <QImage>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "bone_palette_arena.h"
#include "draw_queue.h"
#include "entity/registry.h"
#include "frame_budget.h"
#include "game/systems/unit_activity.h"
#include "gl/backend.h"
#include "gl/mesh.h"
#include "gl/resources.h"
#include "gl/texture.h"
#include "i_render_backend.h"
#include "prepare_worker_pool.h"
#include "render/async_template_prewarm.h"
#include "render/creature/pipeline/creature_render_graph.h"
#include "render/render_view_state.h"
#include "render/world_render_mode.h"
#include "render/world_view.h"
#include "rigged_mesh_cache.h"
#include "scene/camera.h"
#include "scene/environment_lighting.h"
#include "snapshot_mesh_cache.h"
#include "submission_visibility.h"
#include "submitter.h"
#include "template_prewarm_catalog.h"
#include "unit_render_cache.h"

namespace Engine::Core {
class World;
class Entity;
class TransformComponent;
class UnitComponent;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace Game::Map {
class VisibilityService;
}

namespace Render::Profiling {
struct FrameProfile;
}

namespace Render::Creature::Pipeline {
struct CreaturePreparationResult;
}

namespace Render::GL {
class EntityRendererRegistry;
}

namespace Render::Ground {
class VisibilityTextureHelper;
}

namespace Game::Systems {
class ArrowSystem;
}

namespace Render::GL {

class Backend;
class EffectsSubmitter;
struct UnitRenderEntry;
struct RenderEntry;
struct UnitSubmitContext;
struct UnitDrawPlan;

class Renderer : public ISubmitter {
public:
  using WorldRenderMode = Render::GL::WorldRenderMode;

  explicit Renderer(ShaderQuality quality = ShaderQuality::Full);
  ~Renderer() override;

  auto initialize() -> bool;
  void shutdown();

  void begin_frame();
  void end_frame();
  void set_viewport(int width, int height);

  void set_world_view(const Render::WorldView& view) { m_world_view = view; }
  [[nodiscard]] auto world_view() const noexcept -> const Render::WorldView& {
    return m_world_view;
  }

  void set_camera(Camera* camera);
  void set_clear_color(float r, float g, float b, float a = 1.0F);
  auto camera() const -> Camera* { return m_camera; }
  auto backend() -> Backend* { return m_gl_backend; }

  void update_animation_time(float delta_time) { m_accumulated_time += delta_time; }
  auto get_animation_time() const -> float { return m_accumulated_time; }

  auto resources() const -> ResourceManager* {
    return m_backend ? m_backend->resources() : nullptr;
  }

  [[nodiscard]] auto view() noexcept -> RenderViewState& { return m_view; }
  [[nodiscard]] auto view() const noexcept -> const RenderViewState& { return m_view; }

  void set_hovered_entity_id(Engine::Core::EntityID id) {
    m_view.set_hovered_entity_id(id);
  }
  void set_local_owner_id(int owner_id) { m_view.set_local_owner_id(owner_id); }
  void set_order_marker_spectator_mode(bool enabled) {
    m_view.set_order_marker_spectator_mode(enabled);
  }
  [[nodiscard]] auto
  order_markers_visible_for_owner(int owner_id) const noexcept -> bool {
    return m_view.order_markers_visible_for_owner(owner_id);
  }
  void set_force_full_creature_lod(bool enabled) {
    m_view.set_force_full_creature_lod(enabled);
  }

  void set_cinematic_mode(bool enabled) { m_view.set_cinematic_mode(enabled); }
  [[nodiscard]] auto cinematic_mode() const noexcept -> bool {
    return m_view.cinematic_mode();
  }
  void set_world_render_mode(WorldRenderMode mode);
  [[nodiscard]] auto world_render_mode() const -> WorldRenderMode {
    return m_view.world_render_mode();
  }

  void set_rpg_camera_focus(Engine::Core::EntityID entity_id) {
    m_view.set_rpg_camera_focus(entity_id);
  }
  [[nodiscard]] auto rpg_camera_focus() const -> Engine::Core::EntityID {
    return m_view.rpg_camera_focus();
  }
  [[nodiscard]] auto non_local_unit_visibility_filter_enabled() const -> bool;
  [[nodiscard]] auto static_world_visibility_filter_enabled() const -> bool;
  [[nodiscard]] auto
  submission_visibility() const noexcept -> const SubmissionVisibilityPolicy& {
    return m_submission_visibility;
  }

  [[nodiscard]] auto visibility_mask() -> const TerrainSurfaceCmd::VisibilityResources&;

  void set_frame_budget(const FrameBudgetConfig& config) {
    if (m_backend) {
      m_backend->set_frame_budget(config);
    }
  }
  [[nodiscard]] auto frame_tracker() const -> const FrameTimeTracker* {
    return m_backend ? m_backend->frame_tracker() : nullptr;
  }
  [[nodiscard]] auto last_draw_command_count() const -> std::size_t {
    return m_queues[m_render_queue_index].size();
  }
  [[nodiscard]] auto last_playback_stats() const noexcept -> Backend::PlaybackStats;

  void set_selected_entities(const std::vector<Engine::Core::EntityID>& ids) {
    m_selected_ids.clear();
    m_selected_ids.insert(ids.begin(), ids.end());
  }

  auto get_mesh_quad() const -> Mesh* {
    return m_backend && (m_backend->resources() != nullptr)
               ? m_backend->resources()->quad()
               : nullptr;
  }
  auto get_mesh_plane() const -> Mesh* {
    return m_backend && (m_backend->resources() != nullptr)
               ? m_backend->resources()->ground()
               : nullptr;
  }
  auto get_mesh_cube() const -> Mesh* {
    return m_backend && (m_backend->resources() != nullptr)
               ? m_backend->resources()->unit()
               : nullptr;
  }

  auto get_white_texture() const -> Texture* {
    return m_backend && (m_backend->resources() != nullptr)
               ? m_backend->resources()->white()
               : nullptr;
  }

  auto get_shader(const QString& name) const -> Shader* {
    return m_backend ? m_backend->shader(name) : nullptr;
  }
  auto load_shader(const QString& name,
                   const QString& vert_path,
                   const QString& frag_path) -> Shader* {
    return m_gl_backend ? m_gl_backend->get_or_load_shader(name, vert_path, frag_path)
                        : nullptr;
  }

  void set_current_shader(Shader* shader) { m_current_shader = shader; }
  auto get_current_shader() const -> Shader* { return m_current_shader; }

  struct GridParams {
    float cell_size = 1.0F;
    float thickness = 0.06F;
    QVector3D grid_color{0.15F, 0.18F, 0.15F};
    float extent = 50.0F;
  };
  void set_grid_params(const GridParams& gp) { m_grid_params = gp; }
  auto grid_params() const -> const GridParams& { return m_grid_params; }

  void set_environment_lighting(const EnvironmentLightingState& lighting) {
    m_environment_lighting = lighting.sanitized();
    m_light_dir = m_environment_lighting.primary_direction;
    m_ambient_strength = m_environment_lighting.ambient_intensity;
    if (m_gl_backend) {
      m_gl_backend->set_environment_lighting(m_environment_lighting);
      const float haze =
          std::clamp(m_environment_lighting.fog_density * 28.0F, 0.0F, 0.72F);
      const QVector3D sky =
          (m_environment_lighting.sky_color +
           (m_environment_lighting.fog_color - m_environment_lighting.sky_color) *
               haze) *
          m_environment_lighting.exposure;
      m_gl_backend->set_clear_color(sky.x(), sky.y(), sky.z(), 1.0F);
    }
  }
  [[nodiscard]] auto
  environment_lighting() const noexcept -> const EnvironmentLightingState& {
    return m_environment_lighting;
  }

  void set_lighting(const QVector3D& light_dir, float ambient_strength) {
    EnvironmentLightingState lighting = m_environment_lighting;
    lighting.primary_direction = light_dir;
    lighting.ambient_intensity = ambient_strength;
    set_environment_lighting(lighting);
  }

  void pause() { m_paused = true; }
  void resume() { m_paused = false; }
  auto is_paused() const -> bool { return m_paused; }

  void clear_entity_render_caches() {
    m_unit_render_cache.clear();
    m_model_matrix_cache.clear();
    m_rigged_mesh_cache.clear();
    m_snapshot_mesh_cache.clear();
    m_animation_time_cache.clear();
    if (m_gl_backend != nullptr) {
      m_gl_backend->reset_local_lights();
    }
  }

  void set_mist_volumes(const std::vector<Render::MistVolume>& volumes) {
    if (m_gl_backend != nullptr) {
      m_gl_backend->set_mist_volumes(volumes);
    }
  }
  void set_ground_fog(const Render::GroundFogSettings& fog) {
    if (m_gl_backend != nullptr) {
      m_gl_backend->set_ground_fog(fog);
    }
  }

  [[nodiscard]] auto render_software_preview(int width, int height) -> QImage;

  void mesh(Mesh* mesh,
            const QMatrix4x4& model,
            const QVector3D& color,
            Texture* texture = nullptr,
            float alpha = 1.0F,
            int material_id = 0) override;
  void banner(Mesh* mesh,
              const QMatrix4x4& model,
              const QVector3D& color,
              const QVector3D& trim_color,
              Texture* texture = nullptr,
              float alpha = 1.0F,
              int material_id = 0) override;
  void part(Mesh* mesh,
            Material* material,
            const QMatrix4x4& model,
            const QVector3D& color,
            Texture* texture = nullptr,
            float alpha = 1.0F,
            int material_id = 0) override;
  void cylinder(const QVector3D& start,
                const QVector3D& end,
                float radius,
                const QVector3D& color,
                float alpha = 1.0F) override;
  void ground_marker(const GroundMarkerCmd& marker) override;

  void set_terrain_height_resources(const TerrainSurfaceCmd::HeightResources& height) {
    m_terrain_height_resources = height;
  }

  void grid(const QMatrix4x4& model,
            const QVector3D& color,
            float cell_size,
            float thickness,
            float extent) override;

  void selection_smoke(const QMatrix4x4& model,
                       const QVector3D& color,
                       float base_alpha = 0.15F) override;
  void healing_beam(const QVector3D& start,
                    const QVector3D& end,
                    const QVector3D& color,
                    float progress,
                    float beam_width,
                    float intensity,
                    float time) override;
  void healer_aura(const QVector3D& position,
                   const QVector3D& color,
                   float radius,
                   float intensity,
                   float time) override;
  void combat_dust(const QVector3D& position,
                   const QVector3D& color,
                   float radius,
                   float intensity,
                   float time) override;
  void building_flame(const QVector3D& position,
                      const QVector3D& color,
                      float radius,
                      float intensity,
                      float time);
  void burning_flame(const QVector3D& position,
                     const QVector3D& color,
                     float radius,
                     float intensity,
                     float time);
  void fireball(const QVector3D& position,
                const QVector3D& color,
                float radius,
                float intensity,
                float time);
  void blood_pool(const QVector3D& position,
                  float radius,
                  float alpha_scale,
                  float rotation = 0.0F,
                  float aspect_ratio = 1.0F,
                  float seed = 0.0F);
  void stone_impact(const QVector3D& position,
                    const QVector3D& color,
                    float radius,
                    float intensity,
                    float time) override;

  void metal_spark(const QVector3D& position,
                   const QVector3D& color,
                   float radius,
                   float intensity,
                   float time,
                   const QVector3D& direction = {});
  void mode_indicator(const QMatrix4x4& model,
                      int mode_type,
                      const QVector3D& color,
                      float alpha = 1.0F) override;
  void rigged(const RiggedCreatureCmd& cmd) override;
  void rigged(RiggedCreatureCmd&& cmd) override;
  void terrain_surface(const TerrainSurfaceCmd& cmd);
  void terrain_feature(const TerrainFeatureCmd& cmd);
  void terrain_scatter(const TerrainScatterCmd& cmd);

  void local_light(const Render::LocalLight& light);

  struct TemplatePrewarmProgress {
    enum class Phase {
      CollectingProfiles,
      BuildingCoreTemplates,
      QueueingExtendedTemplates,
      Completed,
      Cancelled
    };

    Phase phase{Phase::CollectingProfiles};
    std::size_t completed{0};
    std::size_t total{0};
  };

  using TemplatePrewarmProgressCallback =
      std::function<bool(const TemplatePrewarmProgress&)>;

  void render_world(Engine::Core::World* world);
  void prewarm_unit_templates(Engine::Core::World* world = nullptr,
                              TemplatePrewarmProgressCallback progress_callback = {});

  void lock_world_for_modification() { m_world_mutex.lock(); }
  void unlock_world_for_modification() { m_world_mutex.unlock(); }

  void fog_batch(const FogInstanceData* instances,
                 std::size_t count,
                 const FogMaskResources& mask = {});
  void fog_batch(Buffer* instance_buffer,
                 std::size_t count,
                 const FogMaskResources& mask = {});
  void rain_batch(const RainBatchParams& params);

  void render_construction_previews_public(Engine::Core::World* world,
                                           const Game::Map::VisibilityService* vis,
                                           bool visibility_enabled) {
    render_construction_previews(world, vis, visibility_enabled && vis != nullptr);
  }

  auto rigged_mesh_cache() noexcept -> RiggedMeshCache& { return m_rigged_mesh_cache; }

  auto snapshot_mesh_cache() noexcept -> SnapshotMeshCache& {
    return m_snapshot_mesh_cache;
  }

private:
  struct VisibilityModeConfig {
    bool filter_non_local_units = true;
    bool filter_static_world = true;
  };

  [[nodiscard]] auto visibility_mode_config() const -> VisibilityModeConfig;

  void render_construction_previews(Engine::Core::World* world,
                                    const Game::Map::VisibilityService* vis,
                                    bool visibility_enabled);

  [[nodiscard]] auto
  compute_rpg_lens_gap(Engine::Core::World& world) const -> LensGapExclusion;

  void collect_unit_entries(Engine::Core::World& world,
                            std::span<const Engine::Core::EntityID> entity_ids,
                            bool visibility_enabled,
                            std::vector<UnitRenderEntry>& out,
                            int& visible_unit_count);

  void collect_non_unit_entries(Engine::Core::World& world,
                                std::span<const Engine::Core::EntityID> entity_ids,
                                float cull_radius,
                                bool visibility_enabled,
                                std::vector<RenderEntry>& out);

  void submit_non_unit_entry(const RenderEntry& entry,
                             Engine::Core::World* world,
                             ResourceManager* res);

  auto plan_unit_entry(UnitRenderEntry& entry,
                       const UnitSubmitContext& ctx) -> UnitDrawPlan;
  void prepare_unit_plans(std::vector<UnitRenderEntry>& entries,
                          std::vector<UnitDrawPlan>& plans,
                          Render::Profiling::FrameProfile& frame_profile);
  void
  submit_unit_entry(UnitRenderEntry& entry,
                    UnitDrawPlan& plan,
                    const UnitSubmitContext& ctx,
                    Render::Creature::Pipeline::CreaturePreparationResult* prepared);

  void enqueue_selection_ring(Engine::Core::Entity* entity,
                              Engine::Core::TransformComponent* transform,
                              Engine::Core::UnitComponent* unit_comp,
                              bool selected,
                              bool hovered);
  void enqueue_activity_indicator(Engine::Core::EntityID entity_id,
                                  Engine::Core::TransformComponent* transform,
                                  const Game::Systems::UnitActivity& activity,
                                  int owner_id,
                                  float anchor_height,
                                  float distance_sq);

  void refresh_billboard_basis();

  struct AnimationTimeCacheEntry {
    float time = 0.0F;
    uint32_t last_frame = 0;
  };

  auto resolve_animation_time(Engine::Core::EntityID entity_id,
                              bool update,
                              float current_time,
                              uint32_t frame) -> float;
  void prune_animation_time_cache(uint32_t frame);
  void process_async_template_prewarm();
  void cancel_async_template_prewarm();

  void run_template_prewarm_item(const PrewarmProfile& profile,
                                 const PrewarmWorkItem& item);

  Camera* m_camera = nullptr;
  std::unique_ptr<IRenderBackend> m_backend;

  Shader* m_contact_shadow_shader = nullptr;
  Backend* m_gl_backend = nullptr;
  ShaderQuality m_shader_quality{ShaderQuality::Full};
  DrawQueue m_queues[2];
  DrawQueue* m_active_queue = nullptr;
  int m_fill_queue_index = 0;
  int m_render_queue_index = 1;

  std::unique_ptr<EntityRendererRegistry> m_entity_registry;
  std::unique_ptr<EffectsSubmitter> m_effects_submitter;
  TerrainSurfaceCmd::HeightResources m_terrain_height_resources{};
  std::unordered_set<Engine::Core::EntityID> m_selected_ids;

  int m_viewport_width = 0;
  int m_viewport_height = 0;
  GridParams m_grid_params;
  float m_accumulated_time = 0.0F;
  std::atomic<bool> m_paused{false};
  RenderViewState m_view;
  Render::WorldView m_world_view;
  float m_alpha_override = 1.0F;

  Mesh* m_unit_cylinder_mesh = nullptr;
  std::shared_ptr<Engine::Core::World> m_render_world_snapshot;

  std::mutex m_world_mutex;

  QMatrix4x4 m_view_proj;
  QVector3D m_billboard_right{1.0F, 0.0F, 0.0F};
  QVector3D m_billboard_up{0.0F, 1.0F, 0.0F};
  QVector3D m_billboard_forward{0.0F, 0.0F, 1.0F};
  Game::Map::VisibilityService::SnapshotPtr m_frame_visibility_snapshot;
  std::unique_ptr<Ground::VisibilityTextureHelper> m_visibility_mask_helper;
  TerrainSurfaceCmd::VisibilityResources m_visibility_mask_resources{};
  SubmissionVisibilityPolicy m_submission_visibility;
  Shader* m_current_shader = nullptr;
  QVector3D m_light_dir{0.65F, 0.50F, 0.40F};
  float m_ambient_strength{0.30F};
  EnvironmentLightingState m_environment_lighting{};

  std::unordered_map<Engine::Core::EntityID, AnimationTimeCacheEntry>
      m_animation_time_cache;
  UnitRenderCache m_unit_render_cache;
  Render::PrepareWorkerPool m_prepare_pool;
  std::vector<Render::Creature::Pipeline::CreaturePreparationResult>
      m_unit_preparations;
  std::vector<std::uint8_t> m_prepare_warmed_handles;
  std::vector<std::size_t> m_parallel_prepare_jobs;
  ModelMatrixCache m_model_matrix_cache;
  RiggedMeshCache m_rigged_mesh_cache;
  SnapshotMeshCache m_snapshot_mesh_cache;
  std::uint32_t m_frame_counter{0};

  Engine::Core::World* m_cached_world{nullptr};

  AsyncTemplatePrewarm m_async_prewarm;
};

struct FrameScope {
  Renderer& r;
  FrameScope(Renderer& renderer)
      : r(renderer) {
    r.begin_frame();
  }
  ~FrameScope() { r.end_frame(); }
};

} // namespace Render::GL
