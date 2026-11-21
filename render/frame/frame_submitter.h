#pragma once

#include <QMatrix4x4>
#include <QString>
#include <QVector3D>

#include <cstddef>

#include "render/frame/draw_commands.h"
#include "render/frame/mist_volume.h"
#include "render/frame/submission_visibility.h"
#include "render/frame/submitter.h"
#include "render/frame/world_view.h"
#include "scene/environment_lighting.h"

namespace Render::Ground {
struct BuildingGroundSet;
}

namespace Render::GL {

class Backend;
class Buffer;
class Camera;
class Mesh;
class ResourceManager;
class Shader;
class Texture;
struct RainBatchParams;

// The frame a domain renderer draws into. Ground, entity and geometry
// renderers take this rather than the scene renderer, so the domain layers
// depend on what a frame offers and not on how the scene builds one. The scene
// renderer is the implementation.
class FrameSubmitter : public ISubmitter {
public:
  // What is being drawn and from where.
  [[nodiscard]] virtual auto get_animation_time() const -> float = 0;
  [[nodiscard]] virtual auto camera() const -> Camera* = 0;
  [[nodiscard]] virtual auto world_view() const noexcept -> const Render::WorldView& = 0;
  [[nodiscard]] virtual auto
  submission_visibility() const noexcept -> const SubmissionVisibilityPolicy& = 0;
  [[nodiscard]] virtual auto static_world_visibility_filter_enabled() const -> bool = 0;
  [[nodiscard]] virtual auto
  visibility_mask() -> const TerrainSurfaceCmd::VisibilityResources& = 0;
  [[nodiscard]] virtual auto
  order_markers_visible_for_owner(int owner_id) const noexcept -> bool = 0;
  [[nodiscard]] virtual auto
  environment_lighting() const noexcept -> const EnvironmentLightingState& = 0;
  [[nodiscard]] virtual auto
  building_ground() const noexcept -> const Render::Ground::BuildingGroundSet& = 0;

  // Shared GPU resources.
  [[nodiscard]] virtual auto backend() -> Backend* = 0;
  [[nodiscard]] virtual auto resources() const -> ResourceManager* = 0;
  [[nodiscard]] virtual auto get_mesh_cube() const -> Mesh* = 0;
  [[nodiscard]] virtual auto get_white_texture() const -> Texture* = 0;
  [[nodiscard]] virtual auto get_shader(const QString& name) const -> Shader* = 0;
  virtual auto load_shader(const QString& name,
                           const QString& vert_path,
                           const QString& frag_path) -> Shader* = 0;
  virtual void set_current_shader(Shader* shader) = 0;
  [[nodiscard]] virtual auto get_current_shader() const -> Shader* = 0;

  // World surfaces and effects beyond the per-entity commands of ISubmitter.
  virtual void terrain_surface(const TerrainSurfaceCmd& cmd) = 0;
  virtual void terrain_feature(const TerrainFeatureCmd& cmd) = 0;
  virtual void terrain_scatter(const TerrainScatterCmd& cmd) = 0;
  virtual void set_terrain_height_resources(
      const TerrainSurfaceCmd::HeightResources& height) = 0;
  virtual void set_ground_fog(const Render::GroundFogSettings& fog) = 0;
  virtual void fog_batch(const FogInstanceData* instances,
                         std::size_t count,
                         const FogMaskResources& mask = {}) = 0;
  virtual void fog_batch(Buffer* instance_buffer,
                         std::size_t count,
                         const FogMaskResources& mask = {}) = 0;
  virtual void rain_batch(const RainBatchParams& params) = 0;
  virtual void building_flame(const QVector3D& position,
                              const QVector3D& color,
                              float radius,
                              float intensity,
                              float time) = 0;
  virtual void burning_flame(const QVector3D& position,
                             const QVector3D& color,
                             float radius,
                             float intensity,
                             float time) = 0;
  virtual void fireball(const QVector3D& position,
                        const QVector3D& color,
                        float radius,
                        float intensity,
                        float time) = 0;
  virtual void blood_pool(const QVector3D& position,
                          float radius,
                          float alpha_scale,
                          float rotation = 0.0F,
                          float aspect_ratio = 1.0F,
                          float seed = 0.0F) = 0;
  virtual void metal_spark(const QVector3D& position,
                           const QVector3D& color,
                           float radius,
                           float intensity,
                           float time,
                           const QVector3D& direction = {}) = 0;
  virtual void weapon_arc(const QVector3D& position,
                          const QVector3D& color,
                          float radius,
                          float intensity,
                          float time,
                          const QVector3D& direction,
                          float span,
                          float tilt = 0.0F) = 0;
};

} // namespace Render::GL
