#include "preparation_common.h"

#include "../../entity/registry.h"
#include "../../../game/core/component.h"

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

} // namespace Render::Creature::Pipeline
