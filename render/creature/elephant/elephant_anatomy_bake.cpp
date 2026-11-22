#include "render/creature/elephant/elephant_anatomy_bake.h"

#include "render/creature/elephant/dimensions.h"
#include "render/creature/elephant/elephant_motion.h"

namespace Render::Creature {

namespace {

inline auto elephant_inputs_match(const ElephantAnatomyComponent& c,
                                  std::uint32_t seed) -> bool {
  return c.seed == seed;
}

void bake_elephant(ElephantAnatomyComponent& c,
                   std::uint32_t seed,
                   const QVector3D& fabric,
                   const QVector3D& metal) {
  c.seed = seed;
  c.profile = Render::GL::make_elephant_profile(seed, fabric, metal);
  c.howdah_frame = Render::GL::compute_howdah_frame(c.profile);
  c.baked = true;
}

} // namespace

auto get_or_bake_elephant_anatomy(Engine::Core::Entity* entity,
                                  std::uint32_t seed,
                                  const QVector3D& fabric_base,
                                  const QVector3D& metal_base)
    -> ElephantAnatomyComponent& {
  if (entity == nullptr) {
    thread_local ElephantAnatomyComponent scratch;
    if (!scratch.baked || !elephant_inputs_match(scratch, seed)) {
      bake_elephant(scratch, seed, fabric_base, metal_base);
    }
    return scratch;
  }
  ElephantAnatomyComponent& c =
      *Engine::Core::get_or_add_component<ElephantAnatomyComponent>(entity);
  if (!c.baked || !elephant_inputs_match(c, seed)) {
    bake_elephant(c, seed, fabric_base, metal_base);
  }
  return c;
}

} // namespace Render::Creature
