// Stage 16.3 — humanoid CreatureSpec.
//
// Wraps the humanoid SkeletonTopology as a full CreatureSpec and
// provides a single entry point (`submit_humanoid_lod`) that the rig
// uses to render body LODs through the CreatureSpec walker.
//
// Consumers call `humanoid_creature_spec()` for structural introspection
// and `submit_humanoid_lod()` for body draw submission.

#pragma once

#include "../creature/part_graph.h"
#include "humanoid_full_builder.h"
#include "skeleton.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <span>

namespace Render::GL {
struct HumanoidVariant;
struct HumanoidPose;
class Mesh;
class ISubmitter;
} // namespace Render::GL

namespace Render::Creature {
struct CreatureSpec;
} // namespace Render::Creature

namespace Render::Humanoid {

// Returns a reference to the shared humanoid CreatureSpec. Safe to call
// from any thread after first construction.
[[nodiscard]] auto humanoid_creature_spec() noexcept
    -> const Render::Creature::CreatureSpec &;

// Walker-based body path used by production for Minimal, Reduced and Full.
void submit_humanoid_lod(const Render::GL::HumanoidPose &pose,
                         const Render::GL::HumanoidVariant &variant,
                         Render::Creature::CreatureLOD lod,
                         const QMatrix4x4 &world_from_unit,
                         Render::GL::ISubmitter &out) noexcept;

// Stage 15.5c — helper that evaluates the humanoid skeleton at `pose`
// and writes unit-local bone world matrices into `out_bones`. Returns
// the number of bones written (== Render::Humanoid::kBoneCount). The
// destination span must have room for at least kBoneCount matrices;
// extra slots are left untouched.
auto compute_bone_palette(const Render::GL::HumanoidPose &pose,
                          std::span<QMatrix4x4> out_bones) noexcept
    -> std::uint32_t;

// Stage 15.5c — returns the resting BonePalette used as the bake's
// bind pose for humanoid rigged meshes. Evaluated once from an idle
// locomotion pose at t=0 and cached for the process lifetime.
auto humanoid_bind_palette() noexcept
    -> std::span<const QMatrix4x4>;

// Stage 15.5c — route the humanoid Reduced LOD body through the
// baked rigged mesh instead of the per-primitive walker. Computes the
// current bone palette, composes it with the cached inverse-bind to
// produce correct skinning matrices, allocates a palette slot in the
// renderer's per-frame arena, and emits a single `RiggedCreatureCmd`
// via `out.rigged()`. Equipment remains on its legacy path.
//
// `world_from_unit` is the per-soldier world matrix.
// `color_tint` is applied as the command's uniform tint (typically the
// Cloth role colour for the variant).
void submit_humanoid_reduced_rigged(const Render::GL::HumanoidPose &pose,
                                    const Render::GL::HumanoidVariant &variant,
                                    const QMatrix4x4 &world_from_unit,
                                    Render::GL::ISubmitter &out) noexcept;

// Stage 15.5d — route the humanoid Full LOD body through a baked
// rigged mesh. Same contract as the Reduced helper but keyed into the
// Full PartGraph (19 primitives — see `k_full_parts` in the .cpp).
// Torso oriented shape, sheared feet, RigDSL head ornamentation,
// facial hair and per-entity proportion scaling are NOT part of the
// bake — armor/helmet/attachments continue to emit as DrawPartCmds on
// bone sockets from their respective helpers.
void submit_humanoid_full_rigged(const Render::GL::HumanoidPose &pose,
                                 const Render::GL::HumanoidVariant &variant,
                                 const QMatrix4x4 &world_from_unit,
                                 Render::GL::ISubmitter &out) noexcept;

// Generic primitive carrier for the remaining humanoid body/face draws
// that live outside the CreatureSpec PartGraph (facial hair strands,
// ground shadow quad). Each prim is fully baked — `model` is the final
// per-draw transform — so the submitter just iterates and dispatches.
struct HumanoidFullPrim {
  Render::GL::Mesh *mesh{nullptr};
  QMatrix4x4 model{};
  QVector3D color{};
  float alpha{1.0F};
  int material_id{0};
};

void submit_humanoid_full_prims(std::span<const HumanoidFullPrim> prims,
                                Render::GL::ISubmitter &out) noexcept;

} // namespace Render::Humanoid
