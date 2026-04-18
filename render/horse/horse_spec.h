// Stage 15.5e — horse CreatureSpec (Minimal + Reduced + Full LODs).
//
// Wraps the horse as a CreatureSpec. Production body rendering goes
// through `submit_horse_lod`; Full uses a static baseline silhouette,
// while Reduced and Minimal are built from the per-entity pose.
//
// Skeleton (8 bones):
//   0 Root     — identity basis, origin = barrel centre + motion bob.
//                Minimal body anchor (with basis-scaled ellipsoid) and
//                Reduced body anchor (with per-primitive half_extents).
//                Also the anchor for leg tops via head_offset.
//   1 Body     — Minimal LOD only: basis columns pre-scaled to the
//                Minimal ellipsoid; half_extents=(1,1,1) at submit.
//   2 FootFL   — world leg-tip position of the front-left hoof.
//   3 FootFR
//   4 FootBL
//   5 FootBR
//   6 NeckTop  — Reduced LOD: tip of the neck cylinder, also the
//                base for the head sphere's anchor offset.
//   7 Head     — Reduced LOD: head centre, identity basis, used as
//                the anchor for the head ellipsoid.
//
// Color roles (see fill_horse_role_colors):
//   1 Coat, 2 CoatDark (= coat * 0.75  for Minimal legs),
//   3 CoatReducedLeg   (= coat * 0.85  for Reduced legs),
//   4 Hoof             (variant hoof_color).

#pragma once

#include "../creature/spec.h"
#include "rig.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <array>
#include <cstdint>
#include <span>

namespace Render::GL {
class ISubmitter;
} // namespace Render::GL

namespace Render::Horse {

enum class HorseBone : std::uint8_t {
  Root = 0,
  Body,
  FootFL,
  FootFR,
  FootBL,
  FootBR,
  NeckTop,
  Head,
  Count,
};

inline constexpr std::size_t kHorseBoneCount =
    static_cast<std::size_t>(HorseBone::Count);

using BonePalette = std::array<QMatrix4x4, kHorseBoneCount>;

// Per-frame pose input. All positions are in the horse's local frame
// (i.e. before the outer unit model matrix `world_from_unit` is
// applied by the walker).
struct HorseSpecPose {
  QVector3D barrel_center{};

  // Bottom (hoof) positions of each leg in horse-local space.
  QVector3D foot_fl{};
  QVector3D foot_fr{};
  QVector3D foot_bl{};
  QVector3D foot_br{};

  // Per-variant dimensions baked into the Body bone's basis (Minimal).
  float body_ellipsoid_x{1.0F};
  float body_ellipsoid_y{1.0F};
  float body_ellipsoid_z{1.0F};

  // Shoulder/hip offsets relative to Root origin, used for leg
  // cylinder tops. Fractional values are already pre-multiplied by
  // the corresponding per-variant dimension by the caller.
  QVector3D shoulder_offset_fl{};
  QVector3D shoulder_offset_fr{};
  QVector3D shoulder_offset_bl{};
  QVector3D shoulder_offset_br{};

  // Per-variant leg radius (legacy: body_width * 0.15 for Minimal).
  float leg_radius{0.08F};

  // --- Reduced-LOD-only fields (ignored at Minimal LOD). -----------

  // Reduced body ellipsoid half-extents along (X=right, Y=up, Z=forward).
  // Legacy values: (body_width/2, body_height*0.425, body_length*0.40).
  QVector3D reduced_body_half{};

  // Neck cylinder endpoints in horse-local space. Used as bone origins
  // so the walker can build the cylinder between them.
  QVector3D neck_base{};
  QVector3D neck_top{};
  float neck_radius{0.1F};

  // Head ellipsoid centre + per-axis half-extents. Legacy scale = 
  // (head_width*0.90, head_height*0.85, head_length*0.75) applied to
  // a unit sphere, so half-extents = that / 2.
  QVector3D head_center{};
  QVector3D head_half{};

  // Per-leg Reduced shoulder offsets (motion-biased) relative to
  // Root origin. Independent from shoulder_offset_* (Minimal static).
  QVector3D shoulder_offset_reduced_fl{};
  QVector3D shoulder_offset_reduced_fr{};
  QVector3D shoulder_offset_reduced_bl{};
  QVector3D shoulder_offset_reduced_br{};

  // Reduced leg radius (legacy: body_width * 0.22).
  float leg_radius_reduced{0.11F};

  // Hoof cylinder scale factors applied to a unit cylinder along
  // the foot bone's identity basis. Legacy scale:
  // (body_width*0.28, hoof_height, body_width*0.30).
  QVector3D hoof_scale{};
};

[[nodiscard]] auto horse_topology() noexcept
    -> const Render::Creature::SkeletonTopology &;

void evaluate_horse_skeleton(const HorseSpecPose &pose,
                             BonePalette &out_palette) noexcept;

// Build a Minimal-LOD HorseSpecPose from dimensions + motion bob.
void make_horse_spec_pose(const Render::GL::HorseDimensions &dims, float bob,
                          HorseSpecPose &out_pose) noexcept;

// Build a Reduced-LOD HorseSpecPose. Populates both the Minimal
// fields (for the Body bone's basis-scaling path) and the Reduced
// fields (neck, head, per-leg motion-biased shoulders, feet).
struct HorseReducedMotion {
  float phase{0.0F};
  float bob{0.0F};
  bool is_moving{false};
};
void make_horse_spec_pose_reduced(const Render::GL::HorseDimensions &dims,
                                  const Render::GL::HorseGait &gait,
                                  HorseReducedMotion motion,
                                  HorseSpecPose &out_pose) noexcept;

// Populate a caller-owned role-colour array from a HorseVariant.
// Indices match the color_role values in horse_spec.cpp:
//   roles[0] = Coat            (role=1)
//   roles[1] = CoatDark        (role=2) — Minimal legs
//   roles[2] = CoatReducedLeg  (role=3) — Reduced legs (coat*0.85)
//   roles[3] = Hoof            (role=4) — variant.hoof_color
void fill_horse_role_colors(const Render::GL::HorseVariant &variant,
                            std::array<QVector3D, 4> &out_roles) noexcept;

// Full-LOD body shell helper.
//
// Submits the horse Full-LOD body silhouette (chest/withers/belly/
// rump/spine/sternum + neck + head + 4 legs + hooves) through the
// shared CreatureSpec walker. Equipment (saddle, bridle, reins) and
// procedural fluff (mane, tail, eyes, blaze, fetlock joints) continue
// to flow through their respective DrawPartCmd helpers, anchored on
// per-entity bone-derived frames.

[[nodiscard]] auto horse_creature_spec() noexcept
    -> const Render::Creature::CreatureSpec &;

// Evaluate the per-entity horse skeleton at `pose` and write
// unit-local bone world matrices into `out_bones`. Returns the number
// of bones written (== kHorseBoneCount).
auto compute_horse_bone_palette(const HorseSpecPose &pose,
                                std::span<QMatrix4x4> out_bones) noexcept
    -> std::uint32_t;

// Resting BonePalette used as the bake's bind pose for horse rigged
// meshes. Evaluated once from a baseline horse pose at bob=0 and
// cached for the process lifetime.
auto horse_bind_palette() noexcept -> std::span<const QMatrix4x4>;

// Stage 15.5e — route the horse Reduced LOD body through the baked
// rigged mesh. Resolves the Renderer from `out`, bakes or reuses the
// RiggedMesh, and emits one RiggedCreatureCmd. Equipment continues on
// its existing path.
void submit_horse_reduced_rigged(const HorseSpecPose &pose,
                                 const Render::GL::HorseVariant &variant,
                                 const QMatrix4x4 &world_from_unit,
                                 Render::GL::ISubmitter &out) noexcept;

// Stage 15.5e — route the horse Full LOD body through a baked rigged
// mesh. Same contract as the Reduced helper but keyed into the Full
// PartGraph (~16 primitives — see `k_horse_full_parts` in the .cpp).
// Per-entity coat gradients, mane / forelock / tail strands, eye
// highlights, blaze markings and bridle/reins are NOT part of the
// bake — saddle / armor / rider continue to emit as DrawPartCmds via
// the existing equipment dispatch.
void submit_horse_full_rigged(const HorseSpecPose &pose,
                              const Render::GL::HorseVariant &variant,
                              const QMatrix4x4 &world_from_unit,
                              Render::GL::ISubmitter &out) noexcept;

void submit_horse_lod(const HorseSpecPose &pose,
                      const Render::GL::HorseVariant &variant,
                      Render::Creature::CreatureLOD lod,
                      const QMatrix4x4 &world_from_unit,
                      Render::GL::ISubmitter &out) noexcept;

} // namespace Render::Horse
