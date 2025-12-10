#include "scene_renderer.h"
#include "../game/map/terrain_service.h"
#include "../game/map/visibility_service.h"
#include "../game/systems/nation_registry.h"
#include "../game/systems/troop_profile_service.h"
#include "../game/units/spawn_type.h"
#include "../game/units/troop_config.h"
#include "draw_queue.h"
#include "entity/registry.h"
#include "equipment/equipment_registry.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "gl/backend.h"
#include "gl/buffer.h"
#include "gl/camera.h"
#include "gl/primitives.h"
#include "gl/resources.h"
#include "graphics_settings.h"
#include "ground/firecamp_gpu.h"
#include "ground/grass_gpu.h"
#include "ground/pine_gpu.h"
#include "ground/plant_gpu.h"
#include "ground/stone_gpu.h"
#include "ground/terrain_gpu.h"
#include "horse/rig.h"
#include "humanoid/rig.h"
#include "primitive_batch.h"
#include "submitter.h"
#include <QDebug>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <qvectornd.h>
#include <string>

namespace Render::GL {

namespace {
const QVector3D k_axis_x(1.0F, 0.0F, 0.0F);
const QVector3D k_axis_y(0.0F, 1.0F, 0.0F);
const QVector3D k_axis_z(0.0F, 0.0F, 1.0F);
} // namespace

Renderer::Renderer() { m_activeQueue = &m_queues[m_fillQueueIndex]; }

Renderer::~Renderer() { shutdown(); }

auto Renderer::initialize() -> bool {
  if (!m_backend) {
    m_backend = std::make_shared<Backend>();
  }
  m_backend->initialize();
  m_entityRegistry = std::make_unique<EntityRendererRegistry>();
  registerBuiltInEntityRenderers(*m_entityRegistry);
  registerBuiltInEquipment();
  return true;
}

void Renderer::shutdown() { m_backend.reset(); }

void Renderer::beginFrame() {

  advancePoseCacheFrame();

  resetHumanoidRenderStats();
  resetHorseRenderStats();

  m_activeQueue = &m_queues[m_fillQueueIndex];
  m_activeQueue->clear();

  if (m_camera != nullptr) {
    m_view_proj = m_camera->getProjectionMatrix() * m_camera->getViewMatrix();
  }

  if (m_backend) {
    m_backend->beginFrame();
  }
}

void Renderer::endFrame() {
  if (m_paused.load()) {
    return;
  }
  if (m_backend && (m_camera != nullptr)) {
    std::swap(m_fillQueueIndex, m_render_queueIndex);
    DrawQueue &render_queue = m_queues[m_render_queueIndex];
    render_queue.sortForBatching();
    m_backend->setAnimationTime(m_accumulatedTime);
    m_backend->execute(render_queue, *m_camera);
  }
}

void Renderer::setCamera(Camera *camera) { m_camera = camera; }

void Renderer::setClearColor(float r, float g, float b, float a) {
  if (m_backend) {
    m_backend->setClearColor(r, g, b, a);
  }
}

void Renderer::setViewport(int width, int height) {
  m_viewportWidth = width;
  m_viewportHeight = height;
  if (m_backend) {
    m_backend->setViewport(width, height);
  }
  if ((m_camera != nullptr) && height > 0) {
    float const aspect = float(width) / float(height);
    m_camera->setPerspective(m_camera->getFOV(), aspect, m_camera->getNear(),
                             m_camera->getFar());
  }
}
void Renderer::mesh(Mesh *mesh, const QMatrix4x4 &model, const QVector3D &color,
                    Texture *texture, float alpha, int materialId) {
  if (mesh == nullptr) {
    return;
  }

  if (mesh == getUnitCylinder() && (texture == nullptr) &&
      (m_currentShader == nullptr)) {
    QVector3D start;
    QVector3D end;
    float radius = 0.0F;
    if (detail::decomposeUnitCylinder(model, start, end, radius)) {
      cylinder(start, end, radius, color, alpha);
      return;
    }
  }
  MeshCmd cmd;
  cmd.mesh = mesh;
  cmd.texture = texture;
  cmd.model = model;
  cmd.mvp = m_view_proj * model;
  cmd.color = color;
  cmd.alpha = alpha;
  cmd.materialId = materialId;
  cmd.shader = m_currentShader;
  if (m_activeQueue != nullptr) {
    m_activeQueue->submit(cmd);
  }
}

void Renderer::cylinder(const QVector3D &start, const QVector3D &end,
                        float radius, const QVector3D &color, float alpha) {
  CylinderCmd cmd;
  cmd.start = start;
  cmd.end = end;
  cmd.radius = radius;
  cmd.color = color;
  cmd.alpha = alpha;
  if (m_activeQueue != nullptr) {
    m_activeQueue->submit(cmd);
  }
}

void Renderer::fogBatch(const FogInstanceData *instances, std::size_t count) {
  if ((instances == nullptr) || count == 0 || (m_activeQueue == nullptr)) {
    return;
  }
  FogBatchCmd cmd;
  cmd.instances = instances;
  cmd.count = count;
  m_activeQueue->submit(cmd);
}

void Renderer::grassBatch(Buffer *instanceBuffer, std::size_t instance_count,
                          const GrassBatchParams &params) {
  if ((instanceBuffer == nullptr) || instance_count == 0 ||
      (m_activeQueue == nullptr)) {
    return;
  }
  GrassBatchCmd cmd;
  cmd.instanceBuffer = instanceBuffer;
  cmd.instance_count = instance_count;
  cmd.params = params;
  cmd.params.time = m_accumulatedTime;
  m_activeQueue->submit(cmd);
}

void Renderer::stoneBatch(Buffer *instanceBuffer, std::size_t instance_count,
                          const StoneBatchParams &params) {
  if ((instanceBuffer == nullptr) || instance_count == 0 ||
      (m_activeQueue == nullptr)) {
    return;
  }
  StoneBatchCmd cmd;
  cmd.instanceBuffer = instanceBuffer;
  cmd.instance_count = instance_count;
  cmd.params = params;
  m_activeQueue->submit(cmd);
}

void Renderer::plantBatch(Buffer *instanceBuffer, std::size_t instance_count,
                          const PlantBatchParams &params) {
  if ((instanceBuffer == nullptr) || instance_count == 0 ||
      (m_activeQueue == nullptr)) {
    return;
  }
  PlantBatchCmd cmd;
  cmd.instanceBuffer = instanceBuffer;
  cmd.instance_count = instance_count;
  cmd.params = params;
  cmd.params.time = m_accumulatedTime;
  m_activeQueue->submit(cmd);
}

void Renderer::pineBatch(Buffer *instanceBuffer, std::size_t instance_count,
                         const PineBatchParams &params) {
  if ((instanceBuffer == nullptr) || instance_count == 0 ||
      (m_activeQueue == nullptr)) {
    return;
  }
  PineBatchCmd cmd;
  cmd.instanceBuffer = instanceBuffer;
  cmd.instance_count = instance_count;
  cmd.params = params;
  cmd.params.time = m_accumulatedTime;
  m_activeQueue->submit(cmd);
}

void Renderer::oliveBatch(Buffer *instanceBuffer, std::size_t instance_count,
                          const OliveBatchParams &params) {
  if ((instanceBuffer == nullptr) || instance_count == 0 ||
      (m_activeQueue == nullptr)) {
    return;
  }
  OliveBatchCmd cmd;
  cmd.instanceBuffer = instanceBuffer;
  cmd.instance_count = instance_count;
  cmd.params = params;
  cmd.params.time = m_accumulatedTime;
  m_activeQueue->submit(cmd);
}

void Renderer::firecampBatch(Buffer *instanceBuffer, std::size_t instance_count,
                             const FireCampBatchParams &params) {
  if ((instanceBuffer == nullptr) || instance_count == 0 ||
      (m_activeQueue == nullptr)) {
    return;
  }
  FireCampBatchCmd cmd;
  cmd.instanceBuffer = instanceBuffer;
  cmd.instance_count = instance_count;
  cmd.params = params;
  cmd.params.time = m_accumulatedTime;
  m_activeQueue->submit(cmd);
}

void Renderer::terrainChunk(Mesh *mesh, const QMatrix4x4 &model,
                            const TerrainChunkParams &params,
                            std::uint16_t sortKey, bool depthWrite,
                            float depthBias) {
  if ((mesh == nullptr) || (m_activeQueue == nullptr)) {
    return;
  }
  TerrainChunkCmd cmd;
  cmd.mesh = mesh;
  cmd.model = model;
  cmd.params = params;
  cmd.sortKey = sortKey;
  cmd.depthWrite = depthWrite;
  cmd.depthBias = depthBias;
  m_activeQueue->submit(cmd);
}

void Renderer::selectionRing(const QMatrix4x4 &model, float alphaInner,
                             float alphaOuter, const QVector3D &color) {
  SelectionRingCmd cmd;
  cmd.model = model;
  cmd.mvp = m_view_proj * model;
  cmd.alphaInner = alphaInner;
  cmd.alphaOuter = alphaOuter;
  cmd.color = color;
  if (m_activeQueue != nullptr) {
    m_activeQueue->submit(cmd);
  }
}

void Renderer::grid(const QMatrix4x4 &model, const QVector3D &color,
                    float cellSize, float thickness, float extent) {
  GridCmd cmd;
  cmd.model = model;
  cmd.mvp = m_view_proj * model;
  cmd.color = color;
  cmd.cellSize = cellSize;
  cmd.thickness = thickness;
  cmd.extent = extent;
  if (m_activeQueue != nullptr) {
    m_activeQueue->submit(cmd);
  }
}

void Renderer::selectionSmoke(const QMatrix4x4 &model, const QVector3D &color,
                              float baseAlpha) {
  SelectionSmokeCmd cmd;
  cmd.model = model;
  cmd.mvp = m_view_proj * model;
  cmd.color = color;
  cmd.baseAlpha = baseAlpha;
  if (m_activeQueue != nullptr) {
    m_activeQueue->submit(cmd);
  }
}

void Renderer::enqueueSelectionRing(Engine::Core::Entity *,
                                    Engine::Core::TransformComponent *transform,
                                    Engine::Core::UnitComponent *unit_comp,
                                    bool selected, bool hovered) {
  if ((!selected && !hovered) || (transform == nullptr)) {
    return;
  }

  float ring_size = 0.5F;
  float ring_offset = 0.05F;
  float ground_offset = 0.0F;
  float scale_y = 1.0F;

  if (unit_comp != nullptr) {
    auto troop_type_opt =
        Game::Units::spawn_typeToTroopType(unit_comp->spawn_type);

    if (troop_type_opt) {
      const auto &nation_reg = Game::Systems::NationRegistry::instance();
      const Game::Systems::Nation *nation =
          nation_reg.getNationForPlayer(unit_comp->owner_id);
      Game::Systems::NationID nation_id =
          nation != nullptr ? nation->id : nation_reg.default_nation_id();

      const auto profile =
          Game::Systems::TroopProfileService::instance().get_profile(
              nation_id, *troop_type_opt);

      ring_size = profile.visuals.selection_ring_size;
      ring_offset += profile.visuals.selection_ring_y_offset;
      ground_offset = profile.visuals.selection_ring_ground_offset;
    } else {

      auto &config = Game::Units::TroopConfig::instance();
      ring_size = config.getSelectionRingSize(unit_comp->spawn_type);
      ring_offset += config.getSelectionRingYOffset(unit_comp->spawn_type);
      ground_offset =
          config.getSelectionRingGroundOffset(unit_comp->spawn_type);
    }
  }
  if (transform != nullptr) {
    scale_y = transform->scale.y;
  }

  QVector3D pos(transform->position.x, transform->position.y,
                transform->position.z);
  auto &terrain_service = Game::Map::TerrainService::instance();
  float terrain_y = transform->position.y;
  if (terrain_service.isInitialized()) {
    terrain_y = terrain_service.getTerrainHeight(pos.x(), pos.z());
  } else {
    terrain_y -= ground_offset * scale_y;
  }
  pos.setY(terrain_y);

  QMatrix4x4 ring_model;
  ring_model.translate(pos.x(), pos.y() + ring_offset, pos.z());
  ring_model.scale(ring_size, 1.0F, ring_size);

  if (selected) {
    selectionRing(ring_model, 0.6F, 0.25F, QVector3D(0.2F, 0.4F, 1.0F));
  } else if (hovered) {
    selectionRing(ring_model, 0.35F, 0.15F, QVector3D(0.90F, 0.90F, 0.25F));
  }
}

void Renderer::renderWorld(Engine::Core::World *world) {
  if (m_paused.load()) {
    return;
  }
  if (world == nullptr) {
    return;
  }

  std::lock_guard<std::recursive_mutex> const guard(world->getEntityMutex());

  auto &vis = Game::Map::VisibilityService::instance();
  const bool visibility_enabled = vis.isInitialized();

  auto renderable_entities =
      world->getEntitiesWith<Engine::Core::RenderableComponent>();

  const auto &gfxSettings = Render::GraphicsSettings::instance();
  const auto &batch_config = gfxSettings.batching_config();

  float cameraHeight = 0.0F;
  if (m_camera != nullptr) {
    cameraHeight = m_camera->getPosition().y();
  }

  int visibleUnitCount = 0;
  for (auto *entity : renderable_entities) {
    if (entity->hasComponent<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }
    auto *unit_comp = entity->getComponent<Engine::Core::UnitComponent>();
    if (unit_comp != nullptr && unit_comp->health > 0) {
      auto *transform =
          entity->getComponent<Engine::Core::TransformComponent>();
      if (transform != nullptr && m_camera != nullptr) {
        QVector3D const unit_pos(transform->position.x, transform->position.y,
                                 transform->position.z);
        if (m_camera->isInFrustum(unit_pos, 4.0F)) {
          ++visibleUnitCount;
        }
      }
    }
  }

  float batching_ratio =
      gfxSettings.calculate_batching_ratio(visibleUnitCount, cameraHeight);

  PrimitiveBatcher batcher;
  if (batching_ratio > 0.0F) {
    batcher.reserve(2000, 4000, 500);
  }

  float fullShaderMaxDistance = 30.0F * (1.0F - batching_ratio * 0.7F);
  if (batch_config.force_batching) {
    fullShaderMaxDistance = 0.0F;
  }

  BatchingSubmitter batchSubmitter(this, &batcher);

  for (auto *entity : renderable_entities) {

    if (entity->hasComponent<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto *renderable =
        entity->getComponent<Engine::Core::RenderableComponent>();
    auto *transform = entity->getComponent<Engine::Core::TransformComponent>();

    if (!renderable->visible || (transform == nullptr)) {
      continue;
    }

    auto *unit_comp = entity->getComponent<Engine::Core::UnitComponent>();
    if ((unit_comp != nullptr) && unit_comp->health <= 0) {
      continue;
    }

    float distanceToCamera = 0.0F;
    if ((m_camera != nullptr) && (unit_comp != nullptr)) {

      float cull_radius = 3.0F;

      if (unit_comp->spawn_type == Game::Units::SpawnType::MountedKnight) {
        cull_radius = 4.0F;
      } else if (unit_comp->spawn_type == Game::Units::SpawnType::Spearman ||
                 unit_comp->spawn_type == Game::Units::SpawnType::Archer ||
                 unit_comp->spawn_type == Game::Units::SpawnType::Knight) {
        cull_radius = 2.5F;
      }

      QVector3D const unit_pos(transform->position.x, transform->position.y,
                               transform->position.z);
      if (!m_camera->isInFrustum(unit_pos, cull_radius)) {
        continue;
      }

      QVector3D camPos = m_camera->getPosition();
      float dx = unit_pos.x() - camPos.x();
      float dz = unit_pos.z() - camPos.z();
      distanceToCamera = std::sqrt(dx * dx + dz * dz);
    }

    if ((unit_comp != nullptr) && unit_comp->owner_id != m_localOwnerId) {
      if (visibility_enabled) {
        if (!vis.isVisibleWorld(transform->position.x, transform->position.z)) {
          continue;
        }
      }
    }

    bool const is_selected =
        (m_selectedIds.find(entity->getId()) != m_selectedIds.end());
    bool const is_hovered = (entity->getId() == m_hoveredEntityId);

    QMatrix4x4 model_matrix;
    model_matrix.translate(transform->position.x, transform->position.y,
                           transform->position.z);
    model_matrix.rotate(transform->rotation.x, k_axis_x);
    model_matrix.rotate(transform->rotation.y, k_axis_y);
    model_matrix.rotate(transform->rotation.z, k_axis_z);
    model_matrix.scale(transform->scale.x, transform->scale.y,
                       transform->scale.z);

    bool drawn_by_registry = false;
    if (m_entityRegistry) {
      std::string renderer_key;
      if (!renderable->renderer_id.empty()) {
        renderer_key = renderable->renderer_id;
      } else if (unit_comp != nullptr) {
        renderer_key = Game::Units::spawn_typeToString(unit_comp->spawn_type);
      }
      auto fn = m_entityRegistry->get(renderer_key);
      if (fn) {
        DrawContext ctx{resources(), entity, world, model_matrix};

        ctx.selected = is_selected;
        ctx.hovered = is_hovered;
        ctx.animationTime = m_accumulatedTime;
        ctx.renderer_id = renderer_key;
        ctx.backend = m_backend.get();
        ctx.camera = m_camera;

        bool useBatching = (batching_ratio > 0.0F) &&
                           (distanceToCamera > fullShaderMaxDistance) &&
                           !is_selected && !is_hovered &&
                           !batch_config.never_batch;

        if (useBatching) {
          fn(ctx, batchSubmitter);
        } else {
          fn(ctx, *this);
        }

        enqueueSelectionRing(entity, transform, unit_comp, is_selected,
                             is_hovered);
        drawn_by_registry = true;
      }
    }
    if (drawn_by_registry) {
      continue;
    }

    Mesh *mesh_to_draw = nullptr;
    ResourceManager *res = resources();
    switch (renderable->mesh) {
    case Engine::Core::RenderableComponent::MeshKind::Quad:
      mesh_to_draw = (res != nullptr) ? res->quad() : nullptr;
      break;
    case Engine::Core::RenderableComponent::MeshKind::Plane:
      mesh_to_draw = (res != nullptr) ? res->ground() : nullptr;
      break;
    case Engine::Core::RenderableComponent::MeshKind::Cube:
      mesh_to_draw = (res != nullptr) ? res->unit() : nullptr;
      break;
    case Engine::Core::RenderableComponent::MeshKind::Capsule:
      mesh_to_draw = nullptr;
      break;
    case Engine::Core::RenderableComponent::MeshKind::Ring:
      mesh_to_draw = nullptr;
      break;
    case Engine::Core::RenderableComponent::MeshKind::None:
    default:
      break;
    }
    if ((mesh_to_draw == nullptr) && (res != nullptr)) {
      mesh_to_draw = res->unit();
    }
    if ((mesh_to_draw == nullptr) && (res != nullptr)) {
      mesh_to_draw = res->quad();
    }
    QVector3D const color = QVector3D(
        renderable->color[0], renderable->color[1], renderable->color[2]);

    if (res != nullptr) {
      Mesh *contact_quad = res->quad();
      Texture *white = res->white();
      if ((contact_quad != nullptr) && (white != nullptr)) {
        QMatrix4x4 contact_base;
        contact_base.translate(transform->position.x,
                               transform->position.y + 0.03F,
                               transform->position.z);
        constexpr float k_contact_shadow_rotation = -90.0F;
        contact_base.rotate(k_contact_shadow_rotation, 1.0F, 0.0F, 0.0F);
        float const footprint =
            std::max({transform->scale.x, transform->scale.z, 0.6F});

        float size_ratio = 1.0F;
        if (auto *unit = entity->getComponent<Engine::Core::UnitComponent>()) {
          int const mh = std::max(1, unit->max_health);
          size_ratio = std::clamp(unit->health / float(mh), 0.0F, 1.0F);
        }
        float const eased = 0.25F + 0.75F * size_ratio;

        float const base_scale_x = footprint * 0.55F * eased;
        float const base_scale_y = footprint * 0.35F * eased;

        QVector3D const col(0.03F, 0.03F, 0.03F);
        float const center_alpha = 0.32F * eased;
        float const mid_alpha = 0.16F * eased;
        float const outer_alpha = 0.07F * eased;

        QMatrix4x4 c0 = contact_base;
        c0.scale(base_scale_x * 0.60F, base_scale_y * 0.60F, 1.0F);
        mesh(contact_quad, c0, col, white, center_alpha);

        QMatrix4x4 c1 = contact_base;
        c1.scale(base_scale_x * 0.95F, base_scale_y * 0.95F, 1.0F);
        mesh(contact_quad, c1, col, white, mid_alpha);

        QMatrix4x4 c2 = contact_base;
        c2.scale(base_scale_x * 1.35F, base_scale_y * 1.35F, 1.0F);
        mesh(contact_quad, c2, col, white, outer_alpha);
      }
    }
    enqueueSelectionRing(entity, transform, unit_comp, is_selected, is_hovered);
    mesh(mesh_to_draw, model_matrix, color,
         (res != nullptr) ? res->white() : nullptr, 1.0F);
  }

  if ((m_activeQueue != nullptr) && batcher.totalCount() > 0) {
    PrimitiveBatchParams params;
    params.viewProj = m_view_proj;

    if (batcher.sphereCount() > 0) {
      PrimitiveBatchCmd cmd;
      cmd.type = PrimitiveType::Sphere;
      cmd.instances = batcher.sphereData();
      cmd.params = params;
      m_activeQueue->submit(cmd);
    }

    if (batcher.cylinderCount() > 0) {
      PrimitiveBatchCmd cmd;
      cmd.type = PrimitiveType::Cylinder;
      cmd.instances = batcher.cylinderData();
      cmd.params = params;
      m_activeQueue->submit(cmd);
    }

    if (batcher.coneCount() > 0) {
      PrimitiveBatchCmd cmd;
      cmd.type = PrimitiveType::Cone;
      cmd.instances = batcher.coneData();
      cmd.params = params;
      m_activeQueue->submit(cmd);
    }
  }
}

} // namespace Render::GL
