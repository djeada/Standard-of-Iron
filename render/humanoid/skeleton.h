// Stage 15 — declarative humanoid skeleton.
//
// The skeleton is the single, canonical source of truth for "where is every
// articulation point on a humanoid body". It gives us:
//
//   1. A typed bone id enum so equipment, AI, and animation code never
//      index by magic numbers or string lookups.
//   2. A parent-index table so a single tree walk evaluates world
//      transforms for every bone.
//   3. Named sockets that attach equipment (helmet, weapons, shield,
//      back, hips) to specific bones with a local offset and orientation.
//   4. A BonePalette (array<QMatrix4x4, kBoneCount>) that is the handoff
//      format to the renderer. Stage 7 GPU skinning consumes this
//      directly; the current CPU pipeline samples sockets out of it.
//
// Stage 15 deliberately does NOT demand that the existing 2858-line
// rig.cpp be rewritten overnight. Instead the skeleton evaluator consumes
// the *existing* HumanoidPose (joint positions computed by
// pose_controller) and synthesises bone-space transforms from joint
// pairs. This lets clip-driven animation, bone sockets, and legacy
// procedural rendering coexist — the new system is live end-to-end
// while the legacy pose math remains the source of joint positions
// until a later stage replaces it with clip sampling wholesale.
//
// Invariants:
//   * Bone ids are contiguous, 0..kBoneCount-1. Parent index is always
//     < child index (topologically sorted), so a single forward sweep
//     of the array computes world transforms.
//   * HumanoidBone::Root has parent = kInvalidBone and is the only bone
//     with no parent. All other bones have a valid parent.
//   * BonePalette[bone] is the bone's *world-space* transform (origin
//     at the bone's head joint, Y axis along the bone's long axis,
//     X axis matching the unit's right-hand direction where meaningful).

#pragma once

#include "../creature/skeleton.h"
#include "../gl/humanoid/humanoid_types.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace Render::Humanoid {

enum class HumanoidBone : std::uint8_t {
  Root = 0,
  Pelvis,
  Spine,
  Chest,
  Neck,
  Head,
  ShoulderL,
  UpperArmL,
  ForearmL,
  HandL,
  ShoulderR,
  UpperArmR,
  ForearmR,
  HandR,
  HipL,
  KneeL,
  FootL,
  HipR,
  KneeR,
  FootR,
  Count
};

inline constexpr std::size_t kBoneCount =
    static_cast<std::size_t>(HumanoidBone::Count);

inline constexpr std::uint8_t kInvalidBone = 0xFF;

// Parent index for each bone. The Root has parent = kInvalidBone. Every
// other entry is strictly less than its own index so a forward sweep
// of the palette computes world transforms correctly.
inline constexpr std::array<std::uint8_t, kBoneCount> kBoneParents = {
    kInvalidBone,                                // Root
    static_cast<std::uint8_t>(HumanoidBone::Root),      // Pelvis
    static_cast<std::uint8_t>(HumanoidBone::Pelvis),    // Spine
    static_cast<std::uint8_t>(HumanoidBone::Spine),     // Chest
    static_cast<std::uint8_t>(HumanoidBone::Chest),     // Neck
    static_cast<std::uint8_t>(HumanoidBone::Neck),      // Head
    static_cast<std::uint8_t>(HumanoidBone::Chest),     // ShoulderL
    static_cast<std::uint8_t>(HumanoidBone::ShoulderL), // UpperArmL
    static_cast<std::uint8_t>(HumanoidBone::UpperArmL), // ForearmL
    static_cast<std::uint8_t>(HumanoidBone::ForearmL),  // HandL
    static_cast<std::uint8_t>(HumanoidBone::Chest),     // ShoulderR
    static_cast<std::uint8_t>(HumanoidBone::ShoulderR), // UpperArmR
    static_cast<std::uint8_t>(HumanoidBone::UpperArmR), // ForearmR
    static_cast<std::uint8_t>(HumanoidBone::ForearmR),  // HandR
    static_cast<std::uint8_t>(HumanoidBone::Pelvis),    // HipL
    static_cast<std::uint8_t>(HumanoidBone::HipL),      // KneeL
    static_cast<std::uint8_t>(HumanoidBone::KneeL),     // FootL
    static_cast<std::uint8_t>(HumanoidBone::Pelvis),    // HipR
    static_cast<std::uint8_t>(HumanoidBone::HipR),      // KneeR
    static_cast<std::uint8_t>(HumanoidBone::KneeR),     // FootR
};

[[nodiscard]] auto bone_name(HumanoidBone bone) noexcept -> std::string_view;
[[nodiscard]] auto parent_of(HumanoidBone bone) noexcept -> std::uint8_t;

// Returns the humanoid's SkeletonTopology in the generic creature
// namespace. Safe to cache the returned reference.
[[nodiscard]] auto humanoid_topology() noexcept
    -> const Render::Creature::SkeletonTopology &;

// Named equipment attachment points. A socket binds a logical equipment
// slot to a specific bone plus a local offset — adding a new slot means
// adding one enum value, not adding new fields to HumanoidPose.
enum class HumanoidSocket : std::uint8_t {
  Head = 0,   // Helmet, crown, hood.
  HandR,      // Sword, spear shaft grip, bow riser.
  HandL,      // Shield, off-hand weapon, bow string hand, torch.
  Back,       // Quiver, scabbard, cape anchor.
  HipL,       // Sheathed dagger, pouch.
  HipR,       // Sheathed sword, water skin.
  ChestFront, // Armor plate, medal, sash clasp.
  ChestBack,  // Rear armor plate, banner anchor.
  FootL,      // Boot/greave anchor.
  FootR,
  Count
};

inline constexpr std::size_t kSocketCount =
    static_cast<std::size_t>(HumanoidSocket::Count);

struct SocketDef {
  HumanoidBone bone;
  // Local offset from bone origin expressed in bone-local axes (metres).
  QVector3D local_offset;
};

[[nodiscard]] auto socket_def(HumanoidSocket socket) noexcept
    -> const SocketDef &;

// BonePalette is the output of skeleton evaluation and the input to
// rendering + equipment placement. Each entry is a world-space
// transform whose:
//   * translation = the bone's head joint position
//   * Y axis      = from head joint towards tail joint (bone's long axis)
//   * X axis      = the unit's local "right" direction (for most bones)
//   * Z axis      = right × Y, completing a right-handed basis
//
// A degenerate (zero-length) bone falls back to the parent's basis.
using BonePalette = std::array<QMatrix4x4, kBoneCount>;

// Evaluate a BonePalette from an already-built HumanoidPose. `right_axis`
// is the unit's facing-independent right vector (usually world +X after
// yaw rotation is applied to the whole unit at submit time — we stay in
// unit-local space here so equipment renderers can compose with the same
// model matrix that drives the body mesh).
//
// `pose` is treated as read-only; mesh positions stored on the pose
// (head_pos, shoulder_l, hand_r, etc.) are interpreted as unit-local
// coordinates.
void evaluate_skeleton(const Render::GL::HumanoidPose &pose,
                       const QVector3D &right_axis,
                       BonePalette &out_palette) noexcept;

// World transform for an equipment socket. `palette` must have been
// populated by `evaluate_skeleton`. Returns a transform whose origin is
// the socket's world position and whose basis matches the underlying
// bone's basis (so a helmet modelled in +Y = up local space slots into
// place with no additional rotation).
[[nodiscard]] auto socket_transform(const BonePalette &palette,
                                    HumanoidSocket socket) noexcept
    -> QMatrix4x4;

// Convenience: the world position of a socket origin.
[[nodiscard]] auto socket_position(const BonePalette &palette,
                                   HumanoidSocket socket) noexcept
    -> QVector3D;

// Bridge to the existing AttachmentFrame system used by every current
// equipment renderer. Returns an AttachmentFrame whose origin/right/up/
// forward match the socket's bone-local basis, with the bone's world
// axes mapped so that:
//   * frame.right   = bone X axis
//   * frame.up      = bone Y axis
//   * frame.forward = bone Z axis
// `radius` and `depth` are left at their defaults (0); callers set them
// per-equipment to describe the grip / slot width.
[[nodiscard]] auto socket_attachment_frame(const BonePalette &palette,
                                           HumanoidSocket socket) noexcept
    -> Render::GL::AttachmentFrame;

} // namespace Render::Humanoid
