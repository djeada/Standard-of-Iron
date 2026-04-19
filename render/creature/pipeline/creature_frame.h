

#pragma once

#include "../../elephant/elephant_spec.h"
#include "../../equipment/horse/i_horse_equipment_renderer.h"
#include "../../gl/humanoid/humanoid_types.h"
#include "../../horse/horse_spec.h"
#include "../part_graph.h"
#include "unit_visual_spec.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <cstdint>
#include <vector>

namespace Render::GL {
struct DrawContext;
}

namespace Render::Creature::Pipeline {

using EntityId = std::uint32_t;

using BonePaletteHandle = std::uint32_t;
inline constexpr BonePaletteHandle kInvalidPalette =
    static_cast<BonePaletteHandle>(0xFFFFFFFFu);

struct PoseState {
  std::uint32_t clip_id{0};
  float phase{0.0F};
  float speed{0.0F};
  bool is_moving{false};
};

struct CreatureFrame {
  std::vector<EntityId> entity_id;
  std::vector<QMatrix4x4> world_matrix;
  std::vector<SpecId> spec_id;
  std::vector<std::uint32_t> seed;
  std::vector<CreatureLOD> lod;
  std::vector<PoseState> pose;
  std::vector<BonePaletteHandle> bone_palette;

  std::vector<Render::GL::HumanoidPose> humanoid_pose;
  std::vector<Render::GL::HumanoidVariant> humanoid_variant;
  std::vector<Render::GL::HumanoidAnimationContext> humanoid_anim;

  std::vector<Render::Horse::HorseSpecPose> horse_pose;
  std::vector<Render::GL::HorseVariant> horse_variant;
  std::vector<Render::Elephant::ElephantSpecPose> elephant_pose;
  std::vector<Render::GL::ElephantVariant> elephant_variant;
  std::vector<QMatrix4x4> mount_world_matrix;

  std::vector<const Render::GL::DrawContext *> legacy_ctx;

  std::vector<const Render::GL::HorseBodyFrames *> horse_frames;
  std::vector<Render::GL::HorseAnimationContext> horse_anim;

  [[nodiscard]] auto size() const noexcept -> std::size_t {
    return entity_id.size();
  }

  [[nodiscard]] auto empty() const noexcept -> bool {
    return entity_id.empty();
  }

  void clear() noexcept {
    entity_id.clear();
    world_matrix.clear();
    spec_id.clear();
    seed.clear();
    lod.clear();
    pose.clear();
    bone_palette.clear();
    humanoid_pose.clear();
    humanoid_variant.clear();
    humanoid_anim.clear();
    horse_pose.clear();
    horse_variant.clear();
    elephant_pose.clear();
    elephant_variant.clear();
    mount_world_matrix.clear();
    legacy_ctx.clear();
    horse_frames.clear();
    horse_anim.clear();
  }

  void reserve(std::size_t n) {
    entity_id.reserve(n);
    world_matrix.reserve(n);
    spec_id.reserve(n);
    seed.reserve(n);
    lod.reserve(n);
    pose.reserve(n);
    bone_palette.reserve(n);
    humanoid_pose.reserve(n);
    humanoid_variant.reserve(n);
    humanoid_anim.reserve(n);
    horse_pose.reserve(n);
    horse_variant.reserve(n);
    elephant_pose.reserve(n);
    elephant_variant.reserve(n);
    mount_world_matrix.reserve(n);
    legacy_ctx.reserve(n);
    horse_frames.reserve(n);
    horse_anim.reserve(n);
  }

  void push(EntityId id, const QMatrix4x4 &world, SpecId sid, std::uint32_t s,
            CreatureLOD l, const PoseState &p,
            BonePaletteHandle handle = kInvalidPalette) {
    entity_id.push_back(id);
    world_matrix.push_back(world);
    spec_id.push_back(sid);
    seed.push_back(s);
    lod.push_back(l);
    pose.push_back(p);
    bone_palette.push_back(handle);
    humanoid_pose.emplace_back();
    humanoid_variant.emplace_back();
    humanoid_anim.emplace_back();
    horse_pose.emplace_back();
    horse_variant.emplace_back();
    elephant_pose.emplace_back();
    elephant_variant.emplace_back();
    mount_world_matrix.emplace_back();
    legacy_ctx.emplace_back(nullptr);
    horse_frames.emplace_back(nullptr);
    horse_anim.emplace_back();
  }

  void push_humanoid(EntityId id, const QMatrix4x4 &world, SpecId sid,
                     std::uint32_t s, CreatureLOD l,
                     const Render::GL::HumanoidPose &hpose,
                     const Render::GL::HumanoidVariant &hvariant,
                     const Render::GL::HumanoidAnimationContext &anim) {
    entity_id.push_back(id);
    world_matrix.push_back(world);
    spec_id.push_back(sid);
    seed.push_back(s);
    lod.push_back(l);
    pose.push_back(PoseState{});
    bone_palette.push_back(kInvalidPalette);
    humanoid_pose.push_back(hpose);
    humanoid_variant.push_back(hvariant);
    humanoid_anim.push_back(anim);
    horse_pose.emplace_back();
    horse_variant.emplace_back();
    elephant_pose.emplace_back();
    elephant_variant.emplace_back();
    mount_world_matrix.emplace_back();
    legacy_ctx.emplace_back(nullptr);
    horse_frames.emplace_back(nullptr);
    horse_anim.emplace_back();
  }

  void push_horse(EntityId id, const QMatrix4x4 &world, SpecId sid,
                  std::uint32_t s, CreatureLOD l,
                  const Render::Horse::HorseSpecPose &hpose,
                  const Render::GL::HorseVariant &hvariant) {
    entity_id.push_back(id);
    world_matrix.push_back(world);
    spec_id.push_back(sid);
    seed.push_back(s);
    lod.push_back(l);
    pose.push_back(PoseState{});
    bone_palette.push_back(kInvalidPalette);
    humanoid_pose.emplace_back();
    humanoid_variant.emplace_back();
    humanoid_anim.emplace_back();
    horse_pose.push_back(hpose);
    horse_variant.push_back(hvariant);
    elephant_pose.emplace_back();
    elephant_variant.emplace_back();
    mount_world_matrix.emplace_back();
    legacy_ctx.emplace_back(nullptr);
    horse_frames.emplace_back(nullptr);
    horse_anim.emplace_back();
  }

  void push_elephant(EntityId id, const QMatrix4x4 &world, SpecId sid,
                     std::uint32_t s, CreatureLOD l,
                     const Render::Elephant::ElephantSpecPose &epose,
                     const Render::GL::ElephantVariant &evariant) {
    entity_id.push_back(id);
    world_matrix.push_back(world);
    spec_id.push_back(sid);
    seed.push_back(s);
    lod.push_back(l);
    pose.push_back(PoseState{});
    bone_palette.push_back(kInvalidPalette);
    humanoid_pose.emplace_back();
    humanoid_variant.emplace_back();
    humanoid_anim.emplace_back();
    horse_pose.emplace_back();
    horse_variant.emplace_back();
    elephant_pose.push_back(epose);
    elephant_variant.push_back(evariant);
    mount_world_matrix.emplace_back();
    legacy_ctx.emplace_back(nullptr);
    horse_frames.emplace_back(nullptr);
    horse_anim.emplace_back();
  }

  void
  push_mounted_horse(EntityId id, const QMatrix4x4 &mount_world,
                     const QMatrix4x4 &rider_world, SpecId sid, std::uint32_t s,
                     CreatureLOD l, const Render::Horse::HorseSpecPose &hpose,
                     const Render::GL::HorseVariant &hvariant,
                     const Render::GL::HumanoidPose &rider_pose,
                     const Render::GL::HumanoidVariant &rider_variant,
                     const Render::GL::HumanoidAnimationContext &rider_anim) {
    entity_id.push_back(id);
    world_matrix.push_back(rider_world);
    spec_id.push_back(sid);
    seed.push_back(s);
    lod.push_back(l);
    pose.push_back(PoseState{});
    bone_palette.push_back(kInvalidPalette);
    humanoid_pose.push_back(rider_pose);
    humanoid_variant.push_back(rider_variant);
    humanoid_anim.push_back(rider_anim);
    horse_pose.push_back(hpose);
    horse_variant.push_back(hvariant);
    elephant_pose.emplace_back();
    elephant_variant.emplace_back();
    mount_world_matrix.push_back(mount_world);
    legacy_ctx.emplace_back(nullptr);
    horse_frames.emplace_back(nullptr);
    horse_anim.emplace_back();
  }

  void push_mounted_elephant(
      EntityId id, const QMatrix4x4 &mount_world, const QMatrix4x4 &rider_world,
      SpecId sid, std::uint32_t s, CreatureLOD l,
      const Render::Elephant::ElephantSpecPose &epose,
      const Render::GL::ElephantVariant &evariant,
      const Render::GL::HumanoidPose &rider_pose,
      const Render::GL::HumanoidVariant &rider_variant,
      const Render::GL::HumanoidAnimationContext &rider_anim) {
    entity_id.push_back(id);
    world_matrix.push_back(rider_world);
    spec_id.push_back(sid);
    seed.push_back(s);
    lod.push_back(l);
    pose.push_back(PoseState{});
    bone_palette.push_back(kInvalidPalette);
    humanoid_pose.push_back(rider_pose);
    humanoid_variant.push_back(rider_variant);
    humanoid_anim.push_back(rider_anim);
    horse_pose.emplace_back();
    horse_variant.emplace_back();
    elephant_pose.push_back(epose);
    elephant_variant.push_back(evariant);
    mount_world_matrix.push_back(mount_world);
    legacy_ctx.emplace_back(nullptr);
    horse_frames.emplace_back(nullptr);
    horse_anim.emplace_back();
  }
};

[[nodiscard]] inline auto
frame_columns_consistent(const CreatureFrame &f) noexcept -> bool {
  const auto n = f.entity_id.size();
  return f.world_matrix.size() == n && f.spec_id.size() == n &&
         f.seed.size() == n && f.lod.size() == n && f.pose.size() == n &&
         f.bone_palette.size() == n && f.humanoid_pose.size() == n &&
         f.humanoid_variant.size() == n && f.humanoid_anim.size() == n &&
         f.horse_pose.size() == n && f.horse_variant.size() == n &&
         f.elephant_pose.size() == n && f.elephant_variant.size() == n &&
         f.mount_world_matrix.size() == n && f.legacy_ctx.size() == n &&
         f.horse_frames.size() == n && f.horse_anim.size() == n;
}

} // namespace Render::Creature::Pipeline
