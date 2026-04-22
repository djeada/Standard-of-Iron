

#pragma once

#include "../../elephant/elephant_spec.h"
#include "../../equipment/horse/i_horse_equipment_renderer.h"
#include "../../gl/humanoid/humanoid_types.h"
#include "../../horse/horse_spec.h"
#include "../../humanoid/humanoid_spec.h"
#include "../part_graph.h"
#include "unit_visual_spec.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <array>
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

inline constexpr std::size_t kCreatureRolePaletteSize = 16;
using RoleColorArray = std::array<QVector3D, kCreatureRolePaletteSize>;

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
  std::vector<CreatureKind> render_kind;
  std::vector<CreatureAssetId> creature_asset_id;
  std::vector<PaletteId> palette_id;
  std::vector<std::uint32_t> seed;
  std::vector<std::uint16_t> variant_bucket;
  std::vector<CreatureLOD> lod;
  std::vector<PoseState> pose;
  std::vector<BonePaletteHandle> bone_palette;
  std::vector<std::uint32_t> role_color_count;
  std::vector<RoleColorArray> role_colors;
  std::vector<QVector3D> base_color;

  std::vector<Render::GL::HumanoidPose> humanoid_pose;
  std::vector<Render::GL::HumanoidVariant> humanoid_variant;
  std::vector<Render::GL::HumanoidAnimationContext> humanoid_anim;

  std::vector<Render::Horse::HorseSpecPose> horse_pose;
  std::vector<Render::GL::HorseVariant> horse_variant;
  std::vector<Render::Elephant::ElephantSpecPose> elephant_pose;
  std::vector<Render::GL::ElephantVariant> elephant_variant;

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
    render_kind.clear();
    creature_asset_id.clear();
    palette_id.clear();
    seed.clear();
    variant_bucket.clear();
    lod.clear();
    pose.clear();
    bone_palette.clear();
    role_color_count.clear();
    role_colors.clear();
    base_color.clear();
    humanoid_pose.clear();
    humanoid_variant.clear();
    humanoid_anim.clear();
    horse_pose.clear();
    horse_variant.clear();
    elephant_pose.clear();
    elephant_variant.clear();
    legacy_ctx.clear();
    horse_frames.clear();
    horse_anim.clear();
  }

  void reserve(std::size_t n) {
    entity_id.reserve(n);
    world_matrix.reserve(n);
    spec_id.reserve(n);
    render_kind.reserve(n);
    creature_asset_id.reserve(n);
    palette_id.reserve(n);
    seed.reserve(n);
    variant_bucket.reserve(n);
    lod.reserve(n);
    pose.reserve(n);
    bone_palette.reserve(n);
    role_color_count.reserve(n);
    role_colors.reserve(n);
    base_color.reserve(n);
    humanoid_pose.reserve(n);
    humanoid_variant.reserve(n);
    humanoid_anim.reserve(n);
    horse_pose.reserve(n);
    horse_variant.reserve(n);
    elephant_pose.reserve(n);
    elephant_variant.reserve(n);
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
    render_kind.push_back(CreatureKind::Humanoid);
    creature_asset_id.push_back(kInvalidCreatureAsset);
    palette_id.push_back(kDefaultPalette);
    seed.push_back(s);
    variant_bucket.push_back(0);
    lod.push_back(l);
    pose.push_back(p);
    bone_palette.push_back(handle);
    role_color_count.push_back(0);
    role_colors.emplace_back();
    base_color.emplace_back(0.5F, 0.5F, 0.5F);
    humanoid_pose.emplace_back();
    humanoid_variant.emplace_back();
    humanoid_anim.emplace_back();
    horse_pose.emplace_back();
    horse_variant.emplace_back();
    elephant_pose.emplace_back();
    elephant_variant.emplace_back();
    legacy_ctx.emplace_back(nullptr);
    horse_frames.emplace_back(nullptr);
    horse_anim.emplace_back();
  }

  void push_humanoid(EntityId id, const QMatrix4x4 &world, SpecId sid,
                     std::uint32_t s, CreatureLOD l,
                     const Render::GL::HumanoidPose &hpose,
                     const Render::GL::HumanoidVariant &hvariant,
                     const Render::GL::HumanoidAnimationContext &anim,
                     PaletteId pid = kDefaultPalette,
                     CreatureAssetId asset_id = kInvalidCreatureAsset,
                     std::uint16_t bucket = 0) {
    RoleColorArray roles{};
    std::array<QVector3D, Render::Humanoid::kHumanoidRoleCount> filled{};
    Render::Humanoid::fill_humanoid_role_colors(hvariant, filled);
    for (std::size_t i = 0; i < filled.size(); ++i) {
      roles[i] = filled[i];
    }

    entity_id.push_back(id);
    world_matrix.push_back(world);
    spec_id.push_back(sid);
    render_kind.push_back(CreatureKind::Humanoid);
    creature_asset_id.push_back(asset_id);
    palette_id.push_back(pid);
    seed.push_back(s);
    variant_bucket.push_back(bucket);
    lod.push_back(l);
    pose.push_back(PoseState{});
    bone_palette.push_back(kInvalidPalette);
    role_color_count.push_back(
        static_cast<std::uint32_t>(Render::Humanoid::kHumanoidRoleCount));
    role_colors.push_back(roles);
    base_color.push_back(hvariant.palette.cloth);
    humanoid_pose.push_back(hpose);
    humanoid_variant.push_back(hvariant);
    humanoid_anim.push_back(anim);
    horse_pose.emplace_back();
    horse_variant.emplace_back();
    elephant_pose.emplace_back();
    elephant_variant.emplace_back();
    legacy_ctx.emplace_back(nullptr);
    horse_frames.emplace_back(nullptr);
    horse_anim.emplace_back();
  }

  void push_horse(EntityId id, const QMatrix4x4 &world, SpecId sid,
                  std::uint32_t s, CreatureLOD l,
                  const Render::Horse::HorseSpecPose &hpose,
                  const Render::GL::HorseVariant &hvariant,
                  PaletteId pid = kDefaultPalette,
                  CreatureAssetId asset_id = kInvalidCreatureAsset,
                  std::uint16_t bucket = 0) {
    RoleColorArray roles{};
    std::array<QVector3D, 8> filled{};
    Render::Horse::fill_horse_role_colors(hvariant, filled);
    for (std::size_t i = 0; i < filled.size(); ++i) {
      roles[i] = filled[i];
    }

    entity_id.push_back(id);
    world_matrix.push_back(world);
    spec_id.push_back(sid);
    render_kind.push_back(CreatureKind::Horse);
    creature_asset_id.push_back(asset_id);
    palette_id.push_back(pid);
    seed.push_back(s);
    variant_bucket.push_back(bucket);
    lod.push_back(l);
    pose.push_back(PoseState{});
    bone_palette.push_back(kInvalidPalette);
    role_color_count.push_back(static_cast<std::uint32_t>(filled.size()));
    role_colors.push_back(roles);
    base_color.push_back(hvariant.coat_color);
    humanoid_pose.emplace_back();
    humanoid_variant.emplace_back();
    humanoid_anim.emplace_back();
    horse_pose.push_back(hpose);
    horse_variant.push_back(hvariant);
    elephant_pose.emplace_back();
    elephant_variant.emplace_back();
    legacy_ctx.emplace_back(nullptr);
    horse_frames.emplace_back(nullptr);
    horse_anim.emplace_back();
  }

  void push_elephant(EntityId id, const QMatrix4x4 &world, SpecId sid,
                     std::uint32_t s, CreatureLOD l,
                     const Render::Elephant::ElephantSpecPose &epose,
                     const Render::GL::ElephantVariant &evariant,
                     PaletteId pid = kDefaultPalette,
                     CreatureAssetId asset_id = kInvalidCreatureAsset,
                     std::uint16_t bucket = 0) {
    RoleColorArray roles{};
    std::array<QVector3D, Render::Elephant::kElephantRoleCount> filled{};
    Render::Elephant::fill_elephant_role_colors(evariant, filled);
    for (std::size_t i = 0; i < filled.size(); ++i) {
      roles[i] = filled[i];
    }

    entity_id.push_back(id);
    world_matrix.push_back(world);
    spec_id.push_back(sid);
    render_kind.push_back(CreatureKind::Elephant);
    creature_asset_id.push_back(asset_id);
    palette_id.push_back(pid);
    seed.push_back(s);
    variant_bucket.push_back(bucket);
    lod.push_back(l);
    pose.push_back(PoseState{});
    bone_palette.push_back(kInvalidPalette);
    role_color_count.push_back(static_cast<std::uint32_t>(filled.size()));
    role_colors.push_back(roles);
    base_color.push_back(evariant.skin_color);
    humanoid_pose.emplace_back();
    humanoid_variant.emplace_back();
    humanoid_anim.emplace_back();
    horse_pose.emplace_back();
    horse_variant.emplace_back();
    elephant_pose.push_back(epose);
    elephant_variant.push_back(evariant);
    legacy_ctx.emplace_back(nullptr);
    horse_frames.emplace_back(nullptr);
    horse_anim.emplace_back();
  }

  /// Generic push that dispatches to the species-specific push method
  /// based on \p kind.  Accepts all species-specific data as defaulted
  /// parameters so callers only fill in what their species needs.
  void push_creature(
      CreatureKind kind, EntityId id, const QMatrix4x4 &world, SpecId sid,
      std::uint32_t s, CreatureLOD l,
      const Render::GL::HumanoidPose &hpose = {},
      const Render::GL::HumanoidVariant &hvariant = {},
      const Render::GL::HumanoidAnimationContext &hanim = {},
      const Render::Horse::HorseSpecPose &hrpose = {},
      const Render::GL::HorseVariant &hrvariant = {},
      const Render::Elephant::ElephantSpecPose &epose = {},
      const Render::GL::ElephantVariant &evariant = {},
      PaletteId pid = kDefaultPalette,
      CreatureAssetId asset_id = kInvalidCreatureAsset,
      std::uint16_t bucket = 0) {
    switch (kind) {
    case CreatureKind::Humanoid:
      push_humanoid(id, world, sid, s, l, hpose, hvariant, hanim, pid,
                    asset_id, bucket);
      break;
    case CreatureKind::Horse:
      push_horse(id, world, sid, s, l, hrpose, hrvariant, pid, asset_id,
                 bucket);
      break;
    case CreatureKind::Elephant:
      push_elephant(id, world, sid, s, l, epose, evariant, pid, asset_id,
                    bucket);
      break;
    case CreatureKind::Mounted:
      break;
    }
  }
};

[[nodiscard]] inline auto
frame_columns_consistent(const CreatureFrame &f) noexcept -> bool {
  const auto n = f.entity_id.size();
  return f.world_matrix.size() == n && f.spec_id.size() == n &&
         f.render_kind.size() == n && f.creature_asset_id.size() == n &&
         f.palette_id.size() == n && f.seed.size() == n &&
         f.variant_bucket.size() == n && f.lod.size() == n &&
         f.pose.size() == n && f.bone_palette.size() == n &&
         f.role_color_count.size() == n && f.role_colors.size() == n &&
         f.base_color.size() == n &&
         f.humanoid_pose.size() == n && f.humanoid_variant.size() == n &&
         f.humanoid_anim.size() == n && f.horse_pose.size() == n &&
         f.horse_variant.size() == n && f.elephant_pose.size() == n &&
         f.elephant_variant.size() == n && f.legacy_ctx.size() == n &&
         f.horse_frames.size() == n && f.horse_anim.size() == n;
}

} // namespace Render::Creature::Pipeline
