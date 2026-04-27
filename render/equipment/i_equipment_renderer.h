#pragma once

#include "../humanoid/humanoid_renderer_base.h"
#include "../palette.h"
#include <atomic>
#include <cstdint>

namespace Render::GL {

struct DrawContext;
struct EquipmentBatch;

class IEquipmentRenderer {
public:
  virtual ~IEquipmentRenderer() = default;

  virtual void render(const DrawContext &ctx, const BodyFrames &frames,
                      const HumanoidPalette &palette,
                      const HumanoidAnimationContext &anim,
                      EquipmentBatch &batch) = 0;

  static auto next_render_id() -> uint64_t {
    static std::atomic<uint64_t> counter{0};
    return ++counter;
  }
};

} // namespace Render::GL
