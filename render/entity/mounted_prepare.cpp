#include "mounted_prepare.h"

namespace Render::GL {

auto prepare_mounted_rows(
    const Render::Creature::Pipeline::MountedSpec &mounted_spec,
    const QMatrix4x4 &mount_world_from_unit,
    const QMatrix4x4 &rider_world_from_unit,
    const Render::Horse::HorseSpecPose &mount_pose,
    const Render::GL::HorseVariant &mount_variant,
    const Render::GL::HumanoidPose &rider_pose,
    const Render::GL::HumanoidVariant &rider_variant,
    const Render::GL::HumanoidAnimationContext &rider_anim, std::uint32_t seed,
    Render::Creature::CreatureLOD lod,
    Render::Creature::Pipeline::RenderPassIntent pass) noexcept
    -> MountedPreparedSet {
  MountedPreparedSet set{};
  (void)mount_pose;
  (void)mount_variant;
  set.mount_row = Render::Creature::Pipeline::make_prepared_creature_row(
      mounted_spec.mount, Render::Creature::Pipeline::CreatureKind::Horse,
      mount_world_from_unit, seed, lod, 0, pass);
  set.rider_row = Render::Creature::Pipeline::make_prepared_humanoid_row(
      mounted_spec.rider, rider_pose, rider_variant, rider_anim,
      rider_world_from_unit, seed, lod, 0, pass);
  return set;
}

} // namespace Render::GL
