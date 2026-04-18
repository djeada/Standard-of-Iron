// Stage 16.1 — species-agnostic creature skeleton.
//
// Generalises the Stage 15 humanoid skeleton to any biological topology:
// humanoids, horses, elephants, future quadrupeds/dragons/camels, etc.
//
// Design contract
// ---------------
//   * A `SkeletonTopology` is a pure-data description of a creature's
//     articulation tree: a contiguous, topologically-sorted list of
//     `BoneDef`s with parent indices and a table of named equipment
//     `SocketDef`s. Topologies are constexpr / static data owned by the
//     species module (e.g. `render/humanoid/humanoid_topology.cpp`).
//
//   * A `JointProvider` callback supplies per-bone `BoneResolution`
//     samples at evaluation time. This is the single escape hatch where
//     species-specific joint math (HumanoidPose field reads, horse
//     BodyFrames lookups, IK solver outputs) enters the evaluator.
//     Anything beyond the provider's contract — basis orthogonalisation,
//     degenerate-bone handling, socket composition — lives here.
//
//   * Output is a `std::span<QMatrix4x4>` (the "bone palette") sized to
//     `topology.bones.size()`. Species facades own the storage, be it
//     `std::array<QMatrix4x4, kHumanoidBoneCount>` or
//     `std::vector<QMatrix4x4>`; this module never allocates.
//
// Invariants (checked by evaluator in debug builds)
// -------------------------------------------------
//   * `bones[i].parent` is either `kInvalidBone` (for the single root)
//     or strictly less than `i` (topological order).
//   * Socket bone indices are < `bones.size()`.
//   * Every bone's palette entry is an orthonormal rotation with a
//     well-defined translation, even for degenerate zero-length bones.

#pragma once

#include "../gl/humanoid/humanoid_types.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace Render::Creature {

using BoneIndex = std::uint16_t;
using SocketIndex = std::uint16_t;

inline constexpr BoneIndex kInvalidBone = static_cast<BoneIndex>(0xFFFFu);
inline constexpr SocketIndex kInvalidSocket =
    static_cast<SocketIndex>(0xFFFFu);

// How the evaluator should derive a given bone's basis from the joint
// samples provided by the species. `FromHeadTail` is the common case: the
// bone's long axis is the head→tail direction, the short X axis is
// orthogonalised against the unit's right hint. `FromParent` keeps the
// parent's basis and only moves the origin (used for zero-length tip
// bones like HandL/HandR, FootL/FootR, or socket-only bones). `FromRootUp`
// forces world-up as the Y axis (used for the Root bone itself).
enum class BoneBasisKind : std::uint8_t {
  FromHeadTail = 0,
  FromParent = 1,
  FromRootUp = 2,
};

// Per-bone sample returned by a `JointProvider`. All positions are in the
// creature's unit-local space (the same space the palette is written in).
struct BoneResolution {
  BoneBasisKind kind{BoneBasisKind::FromParent};
  QVector3D head{};
  QVector3D tail{};
};

// Pure function that, given species-specific input state, returns the
// resolution for a single bone. Must be deterministic and may be called
// exactly once per bone during a single evaluate_skeleton() pass. The
// `user` pointer is forwarded verbatim from the evaluator call; the
// provider owns its meaning (species module casts it back to its own
// context type).
using JointProviderFn = BoneResolution (*)(void *user, BoneIndex bone);

struct BoneDef {
  std::string_view name;
  BoneIndex parent{kInvalidBone};
};

struct SocketDef {
  std::string_view name;
  BoneIndex bone{kInvalidBone};
  // Offset from the bone origin in bone-local axes (X=right, Y=long,
  // Z=forward). Rotations applied to the bone's basis are NOT modified;
  // the socket inherits the bone's orientation verbatim so equipment
  // modelled in +Y=up local space slots into place directly.
  QVector3D local_offset{};
};

struct SkeletonTopology {
  std::span<const BoneDef> bones;
  std::span<const SocketDef> sockets;
};

// Evaluate world-space bone transforms from `provider` samples into
// `out_palette`. Preconditions:
//   * `out_palette.size() >= topo.bones.size()`.
//   * `right_axis` is a non-zero hint for the creature's local +X axis;
//     pass the facing-normalised unit-right vector. If the hint is zero
//     or colinear with a bone's long axis, a deterministic fallback is
//     used (world +X, then world +Z).
//
// Degenerate bones (head ≈ tail) fall back to inheriting the parent's
// basis at their own origin. A missing provider-supplied orientation
// never produces a non-orthonormal matrix or NaN components.
void evaluate_skeleton(const SkeletonTopology &topo,
                       JointProviderFn provider, void *user,
                       const QVector3D &right_axis,
                       std::span<QMatrix4x4> out_palette) noexcept;

// Socket transform = bone's world transform with its origin translated
// by `local_offset` expressed in the bone's basis. Orientation is
// unchanged.
[[nodiscard]] auto socket_transform(const SkeletonTopology &topo,
                                    std::span<const QMatrix4x4> palette,
                                    SocketIndex socket) noexcept
    -> QMatrix4x4;

[[nodiscard]] auto socket_position(const SkeletonTopology &topo,
                                   std::span<const QMatrix4x4> palette,
                                   SocketIndex socket) noexcept -> QVector3D;

// Bridge to the existing equipment-renderer attachment-frame format.
[[nodiscard]] auto
socket_attachment_frame(const SkeletonTopology &topo,
                        std::span<const QMatrix4x4> palette,
                        SocketIndex socket) noexcept
    -> Render::GL::AttachmentFrame;

// Name-based lookups. Linear scan; intended for config/loading paths and
// tests, not per-frame hot loops. Returns `kInvalidBone` / `kInvalidSocket`
// if the name is not present.
[[nodiscard]] auto find_bone(const SkeletonTopology &topo,
                             std::string_view name) noexcept -> BoneIndex;
[[nodiscard]] auto find_socket(const SkeletonTopology &topo,
                               std::string_view name) noexcept -> SocketIndex;

// Primitive helpers exposed for reuse by species modules (e.g. the
// humanoid adapter). They are the same math used internally by
// `evaluate_skeleton` and are guaranteed to produce orthonormal results.
[[nodiscard]] auto make_bone_basis(const QVector3D &head,
                                   const QVector3D &tail,
                                   const QVector3D &right_hint) noexcept
    -> QMatrix4x4;

[[nodiscard]] auto basis_from_parent(const QMatrix4x4 &parent,
                                     const QVector3D &origin) noexcept
    -> QMatrix4x4;

[[nodiscard]] auto basis_from_root_up(const QVector3D &origin,
                                      const QVector3D &right_hint) noexcept
    -> QMatrix4x4;

// Debug-only: verifies topology invariants (parent ordering, valid
// socket bones, no duplicate bone names). Returns true if well-formed.
// Cost is linear; only call in tests or at startup.
[[nodiscard]] auto validate_topology(const SkeletonTopology &topo) noexcept
    -> bool;

} // namespace Render::Creature
