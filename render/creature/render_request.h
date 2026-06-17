

#pragma once

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>

#include <array>
#include <cstdint>
#include <span>

#include "../creature/spec.h"
#include "animation/clip_manifest.h"
#include "pipeline/bpat_playback.h"
#include "pipeline/render_pass_intent.h"

namespace Render::Creature {

using AnimationStateId = Animation::StateId;

[[nodiscard]] constexpr auto animation_state_count() noexcept -> std::size_t {
  return Animation::state_count();
}

using ArchetypeId = std::uint16_t;
inline constexpr ArchetypeId k_invalid_archetype = static_cast<ArchetypeId>(0xFFFFu);

using VariantId = std::uint16_t;
inline constexpr VariantId k_canonical_variant = static_cast<VariantId>(0u);
using WorldKey = std::uint64_t;
using CreatureRenderAssetHandleId = std::uint16_t;
inline constexpr CreatureRenderAssetHandleId k_invalid_creature_render_asset_handle =
    static_cast<CreatureRenderAssetHandleId>(0xFFFFu);

enum class PlaybackLayerMode : std::uint8_t {
  None = 0,
  FullBodyBlend,
  UpperBodyOverlay,
};

struct PlaybackLayerRequest {
  ArchetypeId archetype{k_invalid_archetype};
  AnimationStateId state{AnimationStateId::Idle};
  float phase{0.0F};
  float weight{0.0F};
  std::uint8_t clip_variant{0U};
  PlaybackLayerMode mode{PlaybackLayerMode::None};

  [[nodiscard]] auto active() const noexcept -> bool {
    return mode != PlaybackLayerMode::None && archetype != k_invalid_archetype &&
           weight > 0.0F;
  }
};

struct CreatureRenderRequest {
  ArchetypeId archetype{k_invalid_archetype};
  VariantId variant{k_canonical_variant};
  AnimationStateId state{AnimationStateId::Idle};

  float phase{0.0F};

  QMatrix4x4 world{};

  std::uint32_t entity_id{0};
  std::uint32_t seed{0};
  std::uint16_t creature_asset_id{0xFFFFu};
  CreatureRenderAssetHandleId render_asset_handle{
      k_invalid_creature_render_asset_handle};
  WorldKey world_key{0};

  Render::Creature::CreatureLOD lod{Render::Creature::CreatureLOD::Full};
  Render::Creature::Pipeline::RenderPassIntent pass{
      Render::Creature::Pipeline::RenderPassIntent::Main};
  bool world_already_grounded{false};

  static constexpr std::size_t k_role_color_capacity = 32;
  std::array<QVector3D, k_role_color_capacity> role_colors{};
  std::uint8_t clip_variant{0};
  std::uint8_t role_color_count{0};
  QVector3D base_color{0.5F, 0.5F, 0.5F};
  QVector4D wear_params{0.0F, 0.0F, 0.0F, 0.0F};
  PlaybackLayerRequest full_body_blend{};
  PlaybackLayerRequest upper_body_overlay{};

  [[nodiscard]] auto role_colors_view() const noexcept -> std::span<const QVector3D> {
    return {role_colors.data(), static_cast<std::size_t>(role_color_count)};
  }
};

} // namespace Render::Creature
