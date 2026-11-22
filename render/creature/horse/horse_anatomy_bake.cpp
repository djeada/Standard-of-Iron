#include "render/creature/horse/horse_anatomy_bake.h"

#include "render/creature/horse/dimensions.h"
#include "render/creature/horse/horse_motion.h"

namespace Render::Creature {

namespace {

constexpr float k_color_eq_tol = 1e-4F;

inline auto color_close(const QVector3D& a, const QVector3D& b) -> bool {
  return (a - b).lengthSquared() < k_color_eq_tol;
}

inline auto horse_inputs_match(const HorseAnatomyComponent& c,
                               std::uint32_t seed,
                               const QVector3D& leather,
                               const QVector3D& cloth) -> bool {
  return c.seed == seed && color_close(c.leather_base, leather) &&
         color_close(c.cloth_base, cloth);
}

void bake_horse(HorseAnatomyComponent& c,
                std::uint32_t seed,
                const QVector3D& leather,
                const QVector3D& cloth,
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

} // namespace

auto get_or_bake_horse_anatomy(Engine::Core::Entity* entity,
                               std::uint32_t seed,
                               const QVector3D& leather_base,
                               const QVector3D& cloth_base,
                               float mount_scale) -> HorseAnatomyComponent& {
  if (entity == nullptr) {
    thread_local HorseAnatomyComponent scratch;
    if (!scratch.baked ||
        !horse_inputs_match(scratch, seed, leather_base, cloth_base)) {
      bake_horse(scratch, seed, leather_base, cloth_base, mount_scale);
    }
    return scratch;
  }
  HorseAnatomyComponent& c =
      *Engine::Core::get_or_add_component<HorseAnatomyComponent>(entity);
  if (!c.baked || !horse_inputs_match(c, seed, leather_base, cloth_base)) {
    bake_horse(c, seed, leather_base, cloth_base, mount_scale);
  }
  return c;
}

} // namespace Render::Creature
