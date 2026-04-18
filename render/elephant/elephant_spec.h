// Stage 15.5e — elephant CreatureSpec (Minimal + Reduced + Full LODs).
//
// Production body rendering goes through `submit_elephant_lod`; Full
// uses a static baseline silhouette, while Reduced and Minimal are built
// from the per-entity pose.
//
// Skeleton (8 bones):
//   0 Root     — identity basis at barrel centre + motion bob.
//   1 Body     — Minimal LOD only: basis columns pre-scaled to the
//                Minimal ellipsoid.
//   2 FootFL   — world foot position, front-left.
//   3 FootFR
//   4 FootBL
//   5 FootBR
//   6 Head     — Reduced LOD: head centre, identity basis, acts as
//                both the neck cylinder tail and the head ellipsoid
//                anchor and the trunk cone base.
//   7 TrunkTip — Reduced LOD: tip position of the trunk cone.
//
// Color roles (fill_elephant_role_colors):
//   1 Skin                         (variant.skin_color)
//   2 SkinShadow                   (Minimal legs, skin * 0.80)
//   3 SkinReducedLeg               (Reduced legs, skin * 0.88)
//   4 SkinReducedFootPad           (Reduced foot pads, skin * 0.75)
//   5 SkinReducedTrunk             (Reduced trunk, skin * 0.90)

#pragma once

#include "../creature/spec.h"
#include "../gl/humanoid/humanoid_types.h"
#include "rig.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <array>
#include <cstdint>

namespace Render::GL {
class ISubmitter;
} // namespace Render::GL

namespace Render::Elephant {

enum class ElephantBone : std::uint8_t {
  Root = 0,
  Body,
  FootFL,
  FootFR,
  FootBL,
  FootBR,
  Head,
  TrunkTip,
  Count,
};

inline constexpr std::size_t kElephantBoneCount =
    static_cast<std::size_t>(ElephantBone::Count);

using BonePalette = std::array<QMatrix4x4, kElephantBoneCount>;

struct ElephantSpecPose {
  QVector3D barrel_center{};
  QVector3D foot_fl{};
  QVector3D foot_fr{};
  QVector3D foot_bl{};
  QVector3D foot_br{};

  float body_ellipsoid_x{1.0F};
  float body_ellipsoid_y{1.0F};
  float body_ellipsoid_z{1.0F};

  QVector3D shoulder_offset_fl{};
  QVector3D shoulder_offset_fr{};
  QVector3D shoulder_offset_bl{};
  QVector3D shoulder_offset_br{};

  float leg_radius{0.1F};

  // --- Reduced-LOD-only fields (ignored at Minimal LOD). -----------

  // Reduced body ellipsoid half-extents in (right, up, forward).
  // Legacy: scale(body_width, body_height*0.90, body_length*0.75) on
  // a unit sphere, so half-extents = that / 2.
  QVector3D reduced_body_half{};

  // Neck cylinder: Root origin + this offset → Head bone origin.
  QVector3D neck_base_offset{};
  float neck_radius{0.1F};

  // Head bone translation (world-local to the elephant) + ellipsoid
  // half-extents. Legacy head scale = (head_width*0.85,
  // head_height*0.80, head_length*0.70).
  QVector3D head_center{};
  QVector3D head_half{};

  // Trunk tip world position (TrunkTip bone) + trunk base radius.
  QVector3D trunk_end{};
  float trunk_base_radius{0.1F};

  // Per-leg Reduced shoulder offsets (motion-biased). Independent of
  // shoulder_offset_* used by Minimal.
  QVector3D shoulder_offset_reduced_fl{};
  QVector3D shoulder_offset_reduced_fr{};
  QVector3D shoulder_offset_reduced_bl{};
  QVector3D shoulder_offset_reduced_br{};

  // Motion-biased foot overrides applied to foot_* at Reduced LOD.
  QVector3D foot_reduced_fl{};
  QVector3D foot_reduced_fr{};
  QVector3D foot_reduced_bl{};
  QVector3D foot_reduced_br{};

  // Reduced leg cylinder radius (legacy: leg_radius * 0.85).
  float leg_radius_reduced{0.1F};

  // Foot pad ellipsoid anchored at each foot, offset down by
  // pad_offset_y and scaled by pad_half_extents. Legacy:
  // translate(foot + (0, -foot_radius*0.18, 0)),
  // scale(foot_radius, foot_radius*0.65, foot_radius*1.10).
  float foot_pad_offset_y{0.0F};
  QVector3D foot_pad_half{};
};

[[nodiscard]] auto elephant_topology() noexcept
    -> const Render::Creature::SkeletonTopology &;

void evaluate_elephant_skeleton(const ElephantSpecPose &pose,
                                BonePalette &out_palette) noexcept;

void make_elephant_spec_pose(const Render::GL::ElephantDimensions &dims,
                             float bob,
                             ElephantSpecPose &out_pose) noexcept;

// Minimal HorseReducedMotion analog. Encodes just the inputs the
// Reduced-LOD walker needs; lets callers in rig.cpp stay thin.
struct ElephantReducedMotion {
  float phase{0.0F};
  float bob{0.0F};
  bool is_moving{false};
  bool is_fighting{false};
  // Active when is_fighting: raw anim.time seconds; the spec derives
  // the stomp cycle phase internally so rig.cpp doesn't have to
  // duplicate the legacy formula.
  float anim_time{0.0F};
  Render::GL::CombatAnimPhase combat_phase{Render::GL::CombatAnimPhase::Idle};
};

// Build a Reduced-LOD ElephantSpecPose. Populates both Minimal and
// Reduced fields so Minimal submission off the same pose remains
// valid.
void make_elephant_spec_pose_reduced(
    const Render::GL::ElephantDimensions &dims,
    const Render::GL::ElephantGait &gait,
    const ElephantReducedMotion &motion,
    ElephantSpecPose &out_pose) noexcept;

void fill_elephant_role_colors(const Render::GL::ElephantVariant &variant,
                               std::array<QVector3D, 5> &out_roles) noexcept;

// CreatureSpec body and palette helpers for Reduced + Full LODs.

[[nodiscard]] auto elephant_creature_spec() noexcept
    -> const Render::Creature::CreatureSpec &;

auto compute_elephant_bone_palette(const ElephantSpecPose &pose,
                                   std::span<QMatrix4x4> out_bones) noexcept
    -> std::uint32_t;

auto elephant_bind_palette() noexcept -> std::span<const QMatrix4x4>;

void submit_elephant_reduced_rigged(const ElephantSpecPose &pose,
                                    const Render::GL::ElephantVariant &variant,
                                    const QMatrix4x4 &world_from_unit,
                                    Render::GL::ISubmitter &out) noexcept;

// Tusks, ear flaps, eye highlights, full trunk segmentation, tail
// tuft and per-prim skin gradients are NOT part of the bake — those
// remain visual deferrals (tracked by v3-s15-5g-mount-fidelity). The
// howdah and rider continue to render as DrawPartCmd via the existing
// equipment dispatch.
void submit_elephant_full_rigged(const ElephantSpecPose &pose,
                                 const Render::GL::ElephantVariant &variant,
                                 const QMatrix4x4 &world_from_unit,
                                 Render::GL::ISubmitter &out) noexcept;

void submit_elephant_lod(const ElephantSpecPose &pose,
                         const Render::GL::ElephantVariant &variant,
                         Render::Creature::CreatureLOD lod,
                         const QMatrix4x4 &world_from_unit,
                         Render::GL::ISubmitter &out) noexcept;

} // namespace Render::Elephant
