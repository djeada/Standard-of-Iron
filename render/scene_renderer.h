#pragma once

#include "draw_queue.h"
#include "entity/registry.h"
#include "gl/backend.h"
#include "gl/camera.h"
#include "gl/mesh.h"
#include "gl/resources.h"
#include "gl/texture.h"
#include "submitter.h"
#include "unit_render_cache.h"
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Engine::Core {
class World;
class Entity;
class TransformComponent;
class UnitComponent;
} // namespace Engine::Core

namespace Game::Map {
class VisibilityService;
}

namespace Render::GL {
class EntityRendererRegistry;
}

namespace Game::Systems {
class ArrowSystem;
}

namespace Render::GL {

class Backend;

class Renderer : public ISubmitter {
public:
  Renderer();
  ~Renderer() override;

  auto initialize() -> bool;
  void shutdown();

  void begin_frame();
  void end_frame();
  void set_viewport(int width, int height);

  void set_camera(Camera *camera);
  void set_clear_color(float r, float g, float b, float a = 1.0F);
  auto camera() const -> Camera * { return m_camera; }
  auto backend() -> Backend * { return m_backend.get(); }

  void update_animation_time(float delta_time) {
    m_accumulated_time += delta_time;
  }
  auto get_animation_time() const -> float { return m_accumulated_time; }

  auto resources() const -> ResourceManager * {
    return m_backend ? m_backend->resources() : nullptr;
  }
  void set_hovered_entity_id(unsigned int id) { m_hovered_entity_id = id; }
  void set_local_owner_id(int owner_id) { m_local_owner_id = owner_id; }

  void set_selected_entities(const std::vector<unsigned int> &ids) {
    m_selected_ids.clear();
    m_selected_ids.insert(ids.begin(), ids.end());
  }

  auto get_mesh_quad() const -> Mesh * {
    return m_backend && (m_backend->resources() != nullptr)
               ? m_backend->resources()->quad()
               : nullptr;
  }
  auto get_mesh_plane() const -> Mesh * {
    return m_backend && (m_backend->resources() != nullptr)
               ? m_backend->resources()->ground()
               : nullptr;
  }
  auto get_mesh_cube() const -> Mesh * {
    return m_backend && (m_backend->resources() != nullptr)
               ? m_backend->resources()->unit()
               : nullptr;
  }

  auto get_white_texture() const -> Texture * {
    return m_backend && (m_backend->resources() != nullptr)
               ? m_backend->resources()->white()
               : nullptr;
  }

  auto get_shader(const QString &name) const -> Shader * {
    return m_backend ? m_backend->shader(name) : nullptr;
  }
  auto load_shader(const QString &name, const QString &vert_path,
                   const QString &frag_path) -> Shader * {
    return m_backend ? m_backend->get_or_load_shader(name, vert_path, frag_path)
                     : nullptr;
  }

  void set_current_shader(Shader *shader) { m_current_shader = shader; }
  auto get_current_shader() const -> Shader * { return m_current_shader; }

  struct GridParams {
    float cell_size = 1.0F;
    float thickness = 0.06F;
    QVector3D grid_color{0.15F, 0.18F, 0.15F};
    float extent = 50.0F;
  };
  void set_grid_params(const GridParams &gp) { m_grid_params = gp; }
  auto grid_params() const -> const GridParams & { return m_grid_params; }

  void pause() { m_paused = true; }
  void resume() { m_paused = false; }
  auto is_paused() const -> bool { return m_paused; }

  void mesh(Mesh *mesh, const QMatrix4x4 &model, const QVector3D &color,
            Texture *texture = nullptr, float alpha = 1.0F,
            int material_id = 0) override;
  void cylinder(const QVector3D &start, const QVector3D &end, float radius,
                const QVector3D &color, float alpha = 1.0F) override;
  void selection_ring(const QMatrix4x4 &model, float alpha_inner,
                      float alpha_outer, const QVector3D &color) override;

  void grid(const QMatrix4x4 &model, const QVector3D &color, float cell_size,
            float thickness, float extent) override;

  void selection_smoke(const QMatrix4x4 &model, const QVector3D &color,
                       float base_alpha = 0.15F) override;
  void healing_beam(const QVector3D &start, const QVector3D &end,
                    const QVector3D &color, float progress, float beam_width,
                    float intensity, float time) override;
  void healer_aura(const QVector3D &position, const QVector3D &color,
                   float radius, float intensity, float time) override;
  void combat_dust(const QVector3D &position, const QVector3D &color,
                   float radius, float intensity, float time) override;
  void building_flame(const QVector3D &position, const QVector3D &color,
                      float radius, float intensity, float time);
  void stone_impact(const QVector3D &position, const QVector3D &color,
                    float radius, float intensity, float time) override;
  void mode_indicator(const QMatrix4x4 &model, int mode_type,
                      const QVector3D &color, float alpha = 1.0F) override;
  void terrain_chunk(Mesh *mesh, const QMatrix4x4 &model,
                     const TerrainChunkParams &params,
                     std::uint16_t sort_key = 0x8000U, bool depth_write = true,
                     float depth_bias = 0.0F);

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
      std::function<bool(const TemplatePrewarmProgress &)>;

  void render_world(Engine::Core::World *world);
  void prewarm_unit_templates(
      Engine::Core::World *world = nullptr,
      TemplatePrewarmProgressCallback progress_callback = {});

  void lock_world_for_modification() { m_world_mutex.lock(); }
  void unlock_world_for_modification() { m_world_mutex.unlock(); }

  void fog_batch(const FogInstanceData *instances, std::size_t count);
  void grass_batch(Buffer *instance_buffer, std::size_t instance_count,
                   const GrassBatchParams &params);
  void stone_batch(Buffer *instance_buffer, std::size_t instance_count,
                   const StoneBatchParams &params);
  void plant_batch(Buffer *instance_buffer, std::size_t instance_count,
                   const PlantBatchParams &params);
  void pine_batch(Buffer *instance_buffer, std::size_t instance_count,
                  const PineBatchParams &params);
  void olive_batch(Buffer *instance_buffer, std::size_t instance_count,
                   const OliveBatchParams &params);
  void firecamp_batch(Buffer *instance_buffer, std::size_t instance_count,
                      const FireCampBatchParams &params);
  void rain_batch(Buffer *instance_buffer, std::size_t instance_count,
                  const RainBatchParams &params);

private:
  void render_construction_previews(Engine::Core::World *world,
                                    const Game::Map::VisibilityService &vis,
                                    bool visibility_enabled);

  void enqueue_selection_ring(Engine::Core::Entity *entity,
                              Engine::Core::TransformComponent *transform,
                              Engine::Core::UnitComponent *unit_comp,
                              bool selected, bool hovered);
  void enqueue_mode_indicator(Engine::Core::TransformComponent *transform,
                              Engine::Core::UnitComponent *unit_comp,
                              bool has_attack, bool has_guard_mode,
                              bool has_hold_mode, bool has_patrol);

  struct AnimationTimeCacheEntry {
    float time = 0.0F;
    uint32_t last_frame = 0;
  };

  auto resolve_animation_time(uint32_t entity_id, bool update,
                              float current_time, uint32_t frame) -> float;
  void prune_animation_time_cache(uint32_t frame);
  void process_async_template_prewarm();
  void cancel_async_template_prewarm();

  struct AsyncPrewarmProfile {
    std::string renderer_id;
    int spawn_type{0};
    int nation_id{0};
    int max_health{1};
    bool is_mounted{false};
    bool is_elephant{false};
    RenderFunc fn;
  };

  struct AsyncPrewarmWorkItem {
    std::size_t profile_index{0};
    int owner_id{0};
    std::uint8_t lod{0};
    std::uint8_t variant{0};
    std::uint8_t anim_state{0};
    std::uint8_t combat_phase{0};
    std::uint8_t frame{0};
    std::uint8_t attack_variant{0};
  };

  struct AsyncTemplatePrewarmState {
    std::vector<AsyncPrewarmProfile> profiles;
    std::vector<AsyncPrewarmWorkItem> work_items;
    std::atomic<std::size_t> next_index{0};
    std::atomic<bool> cancel_requested{false};
  };

  void run_template_prewarm_item(const AsyncPrewarmProfile &profile,
                                 const AsyncPrewarmWorkItem &item);

  Camera *m_camera = nullptr;
  std::shared_ptr<Backend> m_backend;
  DrawQueue m_queues[2];
  DrawQueue *m_active_queue = nullptr;
  int m_fill_queue_index = 0;
  int m_render_queue_index = 1;

  std::unique_ptr<EntityRendererRegistry> m_entity_registry;
  unsigned int m_hovered_entity_id = 0;
  std::unordered_set<unsigned int> m_selected_ids;

  int m_viewport_width = 0;
  int m_viewport_height = 0;
  GridParams m_grid_params;
  float m_accumulated_time = 0.0F;
  std::atomic<bool> m_paused{false};
  float m_alpha_override = 1.0F;

  std::mutex m_world_mutex;
  int m_local_owner_id = 1;

  QMatrix4x4 m_view_proj;
  Shader *m_current_shader = nullptr;

  std::unordered_map<uint32_t, AnimationTimeCacheEntry> m_animation_time_cache;
  UnitRenderCache m_unit_render_cache;
  ModelMatrixCache m_model_matrix_cache;
  std::uint32_t m_frame_counter{0};

  std::mutex m_async_prewarm_mutex;
  std::shared_ptr<AsyncTemplatePrewarmState> m_async_prewarm_state;
};

struct FrameScope {
  Renderer &r;
  FrameScope(Renderer &renderer) : r(renderer) { r.begin_frame(); }
  ~FrameScope() { r.end_frame(); }
};

} // namespace Render::GL
