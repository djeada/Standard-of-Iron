#pragma once

#include <cstddef>

namespace Engine::Core {
class World;
}

namespace Render::GL {

class ISubmitter;
class Camera;
class SubmissionVisibilityPolicy;

struct CarriedLoadSubmitStats {
  std::size_t haulers{0U};
  std::size_t loads{0U};
};

auto submit_carried_loads(Engine::Core::World* world,
                          ISubmitter& out,
                          const SubmissionVisibilityPolicy* visibility,
                          const Camera* camera) -> CarriedLoadSubmitStats;

} // namespace Render::GL
