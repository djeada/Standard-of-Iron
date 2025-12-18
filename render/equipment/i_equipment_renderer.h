#pragma once

#include "../humanoid/rig.h"
#include "../palette.h"
#include "../submitter.h"
#include <atomic>
#include <cstdint>

namespace Render::GL {

struct DrawContext;

class IEquipmentRenderer {
public:
  virtual ~IEquipmentRenderer() = default;

  virtual void render(const DrawContext &ctx, const BodyFrames &frames,
                      const HumanoidPalette &palette,
                      const HumanoidAnimationContext &anim,
                      ISubmitter &submitter) = 0;

  static auto next_render_id() -> uint64_t {
    static std::atomic<uint64_t> counter{0};
    return ++counter;
  }
};

} 
