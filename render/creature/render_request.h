

#pragma once

#include "../creature/spec.h"
#include "pipeline/bpat_playback.h"
#include "pipeline/render_pass_intent.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <array>
#include <cstdint>
#include <span>

namespace Render::Creature {

enum class AnimationStateId : std::uint8_t {
  Idle = 0,
  Walk = 1,
  Run = 2,
  Hold = 3,
  AttackMelee = 4,
  AttackRanged = 5,
  Die = 6,
  Dead = 7,
  AttackSword = 8,
  AttackSpear = 9,
  AttackBow = 10,

  RidingIdle = 11,
  RidingCharge = 12,
  RidingReining = 13,
  RidingBowShot = 14,

  Count
};

[[nodiscard]] constexpr auto animation_state_count() noexcept -> std::size_t {
  return static_cast<std::size_t>(AnimationStateId::Count);
}

using ArchetypeId = std::uint16_t;
inline constexpr ArchetypeId kInvalidArchetype =
    static_cast<ArchetypeId>(0xFFFFu);

using VariantId = std::uint16_t;
inline constexpr VariantId kCanonicalVariant = static_cast<VariantId>(0u);
using WorldKey = std::uint64_t;

struct CreatureRenderRequest {
  ArchetypeId archetype{kInvalidArchetype};
  VariantId variant{kCanonicalVariant};
  AnimationStateId state{AnimationStateId::Idle};

  float phase{0.0F};

  QMatrix4x4 world{};

  std::uint32_t entity_id{0};
  std::uint32_t seed{0};
  std::uint16_t creature_asset_id{0xFFFFu};
  WorldKey world_key{0};

  Render::Creature::CreatureLOD lod{Render::Creature::CreatureLOD::Full};
  Render::Creature::Pipeline::RenderPassIntent pass{
      Render::Creature::Pipeline::RenderPassIntent::Main};
  bool world_already_grounded{false};

  static constexpr std::size_t kRoleColorCapacity = 32;
  std::array<QVector3D, kRoleColorCapacity> role_colors{};
  std::uint8_t clip_variant{0};
  std::uint8_t role_color_count{0};
  QVector3D base_color{0.5F, 0.5F, 0.5F};

  [[nodiscard]] auto
  role_colors_view() const noexcept -> std::span<const QVector3D> {
    return {role_colors.data(), static_cast<std::size_t>(role_color_count)};
  }
};

} // namespace Render::Creature
