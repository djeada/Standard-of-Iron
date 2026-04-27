#include "anatomy_bake.h"

#include "../elephant/dimensions.h"
#include "../elephant/elephant_motion.h"
#include "../horse/dimensions.h"
#include "../horse/horse_motion.h"

namespace Render::Creature {

namespace {

constexpr float k_color_eq_tol = 1e-4F;

inline auto color_close(const QVector3D &a, const QVector3D &b) -> bool {
  return (a - b).lengthSquared() < k_color_eq_tol;
}

inline auto horse_inputs_match(const HorseAnatomyComponent &c,
                               std::uint32_t seed, const QVector3D &leather,
                               const QVector3D &cloth) -> bool {
  return c.seed == seed && color_close(c.leather_base, leather) &&
         color_close(c.cloth_base, cloth);
}

inline auto elephant_inputs_match(const ElephantAnatomyComponent &c,
                                  std::uint32_t seed) -> bool {
  return c.seed == seed;
}

void bake_horse(HorseAnatomyComponent &c, std::uint32_t seed,
                const QVector3D &leather, const QVector3D &cloth,
                float mount_scale) {
  c.seed = seed;
  c.leather_base = leather;
  c.cloth_base = cloth;
  c.profile = Render::GL::make_horse_profile(seed, leather, cloth);
  if (mount_scale != 1.0F) {
    Render::GL::scale_horse_dimensions(c.profile.dims, mount_scale);
  }
  c.mount_frame = Render::GL::compute_mount_frame(c.profile);
  c.baked = true;
}

void bake_elephant(ElephantAnatomyComponent &c, std::uint32_t seed,
                   const QVector3D &fabric, const QVector3D &metal) {
  c.seed = seed;
  c.profile = Render::GL::make_elephant_profile(seed, fabric, metal);
  c.howdah_frame = Render::GL::compute_howdah_frame(c.profile);
  c.baked = true;
}

} // namespace

auto get_or_bake_horse_anatomy(Engine::Core::Entity *entity, std::uint32_t seed,
                               const QVector3D &leather_base,
                               const QVector3D &cloth_base,
                               float mount_scale) -> HorseAnatomyComponent & {
  if (entity == nullptr) {
    thread_local HorseAnatomyComponent scratch;
    if (!scratch.baked ||
        !horse_inputs_match(scratch, seed, leather_base, cloth_base)) {
      bake_horse(scratch, seed, leather_base, cloth_base, mount_scale);
    }
    return scratch;
  }
  HorseAnatomyComponent &c =
      *Engine::Core::get_or_add_component<HorseAnatomyComponent>(entity);
  if (!c.baked || !horse_inputs_match(c, seed, leather_base, cloth_base)) {
    bake_horse(c, seed, leather_base, cloth_base, mount_scale);
  }
  return c;
}

auto get_or_bake_elephant_anatomy(
    Engine::Core::Entity *entity, std::uint32_t seed,
    const QVector3D &fabric_base,
    const QVector3D &metal_base) -> ElephantAnatomyComponent & {
  if (entity == nullptr) {
    thread_local ElephantAnatomyComponent scratch;
    if (!scratch.baked || !elephant_inputs_match(scratch, seed)) {
      bake_elephant(scratch, seed, fabric_base, metal_base);
    }
    return scratch;
  }
  ElephantAnatomyComponent &c =
      *Engine::Core::get_or_add_component<ElephantAnatomyComponent>(entity);
  if (!c.baked || !elephant_inputs_match(c, seed)) {
    bake_elephant(c, seed, fabric_base, metal_base);
  }
  return c;
}

} // namespace Render::Creature
