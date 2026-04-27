

#pragma once

#include "../render_request.h"
#include "creature_render_state.h"
#include "unit_visual_spec.h"

#include <cstdint>
#include <span>

namespace Render::GL {
class ISubmitter;
}

namespace Render::Creature::Pipeline {

struct SubmitStats {
  std::uint32_t entities_submitted{0};
  std::uint32_t lod_full{0};
  std::uint32_t lod_minimal{0};
  std::uint32_t lod_billboard{0};

  void reset() noexcept { *this = SubmitStats{}; }

  void operator+=(const SubmitStats &other) noexcept {
    entities_submitted += other.entities_submitted;
    lod_full += other.lod_full;
    lod_minimal += other.lod_minimal;
    lod_billboard += other.lod_billboard;
  }
};

class CreaturePipeline {
public:
  CreaturePipeline() = default;

  auto submit_requests(
      std::span<const Render::Creature::CreatureRenderRequest> requests,
      Render::GL::ISubmitter &out) const -> SubmitStats;
};

} // namespace Render::Creature::Pipeline
