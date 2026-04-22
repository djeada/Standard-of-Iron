#include "preparation_common.h"

#include "../../../game/core/component.h"
#include "../../../game/map/terrain_service.h"
#include "../../entity/registry.h"

#include <QVector4D>
#include <cstdint>

namespace Render::Creature::Pipeline {

auto pass_intent_from_ctx(const Render::GL::DrawContext &ctx) noexcept
    -> RenderPassIntent {
  return ctx.template_prewarm ? RenderPassIntent::Shadow
                              : RenderPassIntent::Main;
}

auto derive_unit_seed(const Render::GL::DrawContext &ctx,
                      const Engine::Core::UnitComponent *unit) noexcept
    -> std::uint32_t {
  if (ctx.has_seed_override) {
    return ctx.seed_override;
  }
  std::uint32_t seed = 0U;
  if (unit != nullptr) {
    seed ^= static_cast<std::uint32_t>(unit->owner_id * 2654435761U);
  }
  if (ctx.entity != nullptr) {
    seed ^= static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(ctx.entity) & 0xFFFFFFFFU);
  }
  return seed;
}

auto sample_terrain_height_or_fallback(float world_x, float world_z,
                                       float fallback_y) noexcept -> float {
  auto &terrain_service = Game::Map::TerrainService::instance();
  if (!terrain_service.is_initialized()) {
    return fallback_y;
  }
  return terrain_service.get_terrain_height(world_x, world_z);
}

auto model_world_origin(const QMatrix4x4 &model) noexcept -> QVector3D {
  return model.column(3).toVector3D();
}

void set_model_world_y(QMatrix4x4 &model, float world_y) noexcept {
  QVector3D origin = model_world_origin(model);
  origin.setY(world_y);
  model.setColumn(3, QVector4D(origin, 1.0F));
}

} // namespace Render::Creature::Pipeline
