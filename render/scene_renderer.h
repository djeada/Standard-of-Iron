#pragma once

#include "draw_queue.h"
#include "gl/backend.h"
#include "gl/camera.h"
#include "gl/mesh.h"
#include "gl/resources.h"
#include "gl/texture.h"
#include "submitter.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
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

  void render_world(Engine::Core::World *world);

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
  void enqueue_mode_indicator(Engine::Core::Entity *entity,
                              Engine::Core::TransformComponent *transform,
                              Engine::Core::UnitComponent *unit_comp);

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
};

struct FrameScope {
  Renderer &r;
  FrameScope(Renderer &renderer) : r(renderer) { r.begin_frame(); }
  ~FrameScope() { r.end_frame(); }
};

} // namespace Render::GL
