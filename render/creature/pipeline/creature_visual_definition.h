#pragma once

#include "../spec.h"
#include "unit_visual_spec.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace Render::Creature::Pipeline {

enum class CreatureMeshLod : std::uint8_t {
  Full = 0,
  Reduced = 1,
  Minimal = 2,
};

enum class CreatureLegIndex : std::uint8_t {
  FrontLeft = 0,
  FrontRight = 1,
  RearLeft = 2,
  RearRight = 3,
};

enum class CreatureLocomotionMode : std::uint8_t {
  Idle = 0,
  Walk = 1,
  Trot = 2,
  Canter = 3,
  Gallop = 4,
};

inline constexpr std::size_t kLargeCreatureLegCount = 4;

using CreatureBindPaletteFn = std::span<const QMatrix4x4> (*)() noexcept;
using CreatureSpecResolveFn =
    const Render::Creature::CreatureSpec &(*)() noexcept;
using CreaturePartGraphResolveFn = const Render::Creature::PartGraph
    &(*)(Render::Creature::CreatureLOD) noexcept;

struct CreatureMeshRecipe {
  std::string_view debug_name{};
  std::uint8_t lod_mask{0};
  CreatureSpecResolveFn resolve_spec{nullptr};
  CreaturePartGraphResolveFn resolve_part_graph{nullptr};
  std::array<std::uint16_t, 3> part_counts{};
};

struct CreatureRigDefinition {
  std::string_view debug_name{};
  std::uint8_t bone_count{0};
  CreatureBindPaletteFn bind_palette{nullptr};
  const Render::Creature::SkeletonTopology *topology{nullptr};
  std::array<std::uint8_t, kLargeCreatureLegCount> contact_bones{};
  std::uint8_t seat_anchor_bone{0};
};

struct CreatureMotionInputs {
  float locomotion_speed{0.0F};
  float normalized_speed{0.0F};
  float turn_amount{0.0F};
  float stop_intent{0.0F};
  float combat_intent{0.0F};
  float terrain_response{0.0F};
  float facing_velocity_dot{1.0F};
};

struct CreatureLegContactState {
  CreatureLegIndex leg{CreatureLegIndex::FrontLeft};
  bool planted{false};
  float phase{0.0F};
  QVector3D target{};
};

struct CreatureAttachmentSyncChannel {
  float seat_bounce{0.0F};
  float pitch_transfer{0.0F};
  float roll_transfer{0.0F};
  float rein_response{0.0F};
  float anticipation{0.0F};
};

struct CreatureMotionOutput {
  float phase{0.0F};
  float bob{0.0F};
  CreatureLocomotionMode locomotion_mode{CreatureLocomotionMode::Idle};
  float body_sway{0.0F};
  float body_pitch{0.0F};
  float spine_bend{0.0F};
  std::array<CreatureLegContactState, kLargeCreatureLegCount> contacts{};
  CreatureAttachmentSyncChannel attachment_sync{};
};

struct CreatureMotionState {
  float phase{0.0F};
  CreatureLocomotionMode locomotion_mode{CreatureLocomotionMode::Idle};
  std::array<QVector3D, kLargeCreatureLegCount> planted_targets{};
  std::array<bool, kLargeCreatureLegCount> planted{};
  bool initialized{false};
};

struct CreatureGroundingModel {
  std::string_view debug_name{};
  std::uint8_t leg_count{kLargeCreatureLegCount};
  float stance_duty_factor{0.5F};
  float stride_warp_scale{1.0F};
};

struct CreatureMotionController {
  std::string_view debug_name{};
  CreatureLocomotionMode default_mode{CreatureLocomotionMode::Idle};
};

struct CreatureAttachmentFrameSet {
  QVector3D seat_anchor{};
  QVector3D forward{0.0F, 0.0F, 1.0F};
  QVector3D right{1.0F, 0.0F, 0.0F};
  QVector3D up{0.0F, 1.0F, 0.0F};
  QVector3D tack_anchor{};
  QVector3D ground_offset{};
};

struct CreatureAttachmentFrameExtractor {
  std::string_view debug_name{};
};

struct CreatureLodStrategy {
  std::string_view debug_name{};
  std::uint8_t lod_mask{0};
};

struct CreatureVisualDefinition {
  std::string_view debug_name{};
  CreatureKind species{CreatureKind::Humanoid};
  const CreatureMeshRecipe *mesh_recipe{nullptr};
  const CreatureRigDefinition *rig_definition{nullptr};
  const CreatureMotionController *motion_controller{nullptr};
  const CreatureGroundingModel *grounding_model{nullptr};
  const CreatureAttachmentFrameExtractor *attachment_frame_extractor{nullptr};
  const CreatureLodStrategy *lod_strategy{nullptr};
  std::uint8_t role_color_count{0};
  LegacySlotMask material_slots{LegacySlotMask::None};
};

[[nodiscard]] auto
horse_creature_visual_definition() noexcept -> const CreatureVisualDefinition &;
[[nodiscard]] auto elephant_creature_visual_definition() noexcept
    -> const CreatureVisualDefinition &;
[[nodiscard]] auto resolve_creature_visual_definition(
    const UnitVisualSpec &spec) noexcept -> const CreatureVisualDefinition *;

} // namespace Render::Creature::Pipeline
