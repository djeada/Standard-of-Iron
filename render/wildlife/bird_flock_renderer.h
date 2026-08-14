#pragma once

#include <cstddef>

#include "render/world_view.h"

namespace Render::GL {

class ISubmitter;
class Camera;
class SubmissionVisibilityPolicy;

} // namespace Render::GL

namespace Render::GL::Wildlife {

struct BirdFlockSubmitStats {
  std::size_t considered{0U};
  std::size_t submitted{0U};
};

auto submit_bird_flocks(const Render::WorldView& world,
                        ISubmitter& out,
                        const SubmissionVisibilityPolicy* visibility,
                        const Camera* camera) -> BirdFlockSubmitStats;

} // namespace Render::GL::Wildlife
