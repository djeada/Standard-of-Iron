#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace Animation {

struct ClipMarkers {
  float anticipation_start{-1.0F};
  float weapon_release{-1.0F};
  float contact{-1.0F};
  float recover_unlocked{-1.0F};
  float exit_safe{-1.0F};
};

enum class HumanoidClipProfile : std::uint8_t {
  Default,
  SwordReady,
  SpearReady,
  Skeleton
};

enum class AttackClipFamily : std::uint8_t {
  Sword,
  Spear,
  Bow
};

enum class HumanoidAmbientIdle : std::uint8_t {
  None = 0,
  SitDown,
  ShuffleFeet,
  TapFoot,
  ShiftWeight,
  StepInPlace,
  BendKnee,
  RaiseWeapon,
  Jump,
  PlantFlag
};

enum class HumanoidConstructionRole : std::uint8_t {
  None = 0,
  Hammer,
  Saw,
  Chisel,
  KneelingChisel,
};

struct HumanoidConstructionRoleInputs {
  std::uint32_t seed{0U};
  bool force_single_soldier{false};
  bool variant_table_can_select_roles{false};
  std::uint8_t variant_stride{0U};
  bool variant_is_seed_based{false};
};

enum class StateId : std::uint8_t {
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
  Cast = 11,

  RidingIdle = 12,
  RidingCharge = 13,
  RidingReining = 14,
  RidingBowShot = 15,

  Count
};

[[nodiscard]] constexpr auto state_count() noexcept -> std::size_t {
  return static_cast<std::size_t>(StateId::Count);
}

inline constexpr std::uint16_t k_unmapped_clip = 0xFFFFU;

inline constexpr std::uint16_t k_humanoid_idle_clip = 0U;
inline constexpr std::uint16_t k_humanoid_idle_squat_clip = 1U;
inline constexpr std::uint16_t k_humanoid_idle_jump_clip = 2U;
inline constexpr std::uint16_t k_humanoid_idle_weapon_clip = 3U;
inline constexpr std::uint16_t k_humanoid_idle_weave_clip = 4U;

inline constexpr std::uint16_t k_humanoid_walk_clip = 5U;
inline constexpr std::uint16_t k_humanoid_run_clip = 6U;
inline constexpr std::uint16_t k_humanoid_hold_clip = 7U;
inline constexpr std::uint16_t k_humanoid_hold_bow_clip = 8U;
inline constexpr std::uint16_t k_humanoid_attack_sword_a_clip = 9U;
inline constexpr std::uint16_t k_humanoid_attack_sword_b_clip = 10U;
inline constexpr std::uint16_t k_humanoid_attack_sword_c_clip = 11U;
inline constexpr std::uint16_t k_humanoid_attack_spear_a_clip = 12U;
inline constexpr std::uint16_t k_humanoid_attack_spear_b_clip = 13U;
inline constexpr std::uint16_t k_humanoid_attack_spear_c_clip = 14U;
inline constexpr std::uint16_t k_humanoid_attack_bow_clip = 15U;
inline constexpr std::uint16_t k_humanoid_riding_idle_clip = 16U;
inline constexpr std::uint16_t k_humanoid_riding_charge_clip = 17U;
inline constexpr std::uint16_t k_humanoid_riding_reining_clip = 18U;
inline constexpr std::uint16_t k_humanoid_riding_bow_shot_clip = 19U;
inline constexpr std::uint16_t k_humanoid_riding_sword_strike_clip = 20U;
inline constexpr std::uint16_t k_humanoid_die_infantry_clip = 21U;
inline constexpr std::uint16_t k_humanoid_dead_infantry_clip = 22U;
inline constexpr std::uint16_t k_humanoid_die_mounted_clip = 23U;
inline constexpr std::uint16_t k_humanoid_dead_mounted_clip = 24U;
inline constexpr std::uint16_t k_humanoid_clip_count = 25U;

inline constexpr std::uint8_t k_humanoid_idle_variant_count = 5U;
inline constexpr std::uint8_t k_humanoid_attack_sword_variant_count = 3U;
inline constexpr std::uint8_t k_humanoid_attack_spear_variant_count = 3U;
inline constexpr std::uint8_t k_humanoid_riding_sword_variant_count = 1U;

inline constexpr std::uint16_t k_horse_idle_clip = 0U;
inline constexpr std::uint16_t k_horse_walk_clip = 1U;
inline constexpr std::uint16_t k_horse_run_clip = 4U;
inline constexpr std::uint16_t k_horse_attack_clip = 5U;
inline constexpr std::uint16_t k_horse_die_clip = 6U;
inline constexpr std::uint16_t k_horse_dead_clip = 7U;

inline constexpr std::uint16_t k_elephant_idle_clip = 0U;
inline constexpr std::uint16_t k_elephant_walk_clip = 1U;
inline constexpr std::uint16_t k_elephant_run_clip = 2U;
inline constexpr std::uint16_t k_elephant_attack_clip = 3U;
inline constexpr std::uint16_t k_elephant_die_clip = 4U;
inline constexpr std::uint16_t k_elephant_dead_clip = 5U;

struct ClipManifest {
  std::array<std::uint16_t, state_count()> clips{};
  std::array<std::uint8_t, state_count()> variant_counts{};
  std::array<bool, state_count()> snapshot{};
};

struct HumanoidClipVariantInputs {
  StateId state{StateId::Idle};
  bool is_constructing{false};
  HumanoidConstructionRole construction_role{HumanoidConstructionRole::None};
  float construction_jitter_seed{0.0F};
  std::uint8_t death_variant{0U};
  std::uint8_t attack_variant{0U};
  HumanoidAmbientIdle ambient_idle{HumanoidAmbientIdle::None};
  std::uint8_t available_variant_count{1U};
};

[[nodiscard]] constexpr auto state_index(StateId state) noexcept -> std::size_t {
  return static_cast<std::size_t>(state);
}

[[nodiscard]] constexpr auto
make_unmapped_clip_table() noexcept -> std::array<std::uint16_t, state_count()> {
  std::array<std::uint16_t, state_count()> t{};
  for (std::size_t i = 0; i < state_count(); ++i) {
    t[i] = k_unmapped_clip;
  }
  return t;
}

[[nodiscard]] constexpr auto make_variant_count_table_for_clips(
    const std::array<std::uint16_t, state_count()>& clips) noexcept
    -> std::array<std::uint8_t, state_count()> {
  std::array<std::uint8_t, state_count()> t{};
  for (std::size_t i = 0; i < state_count(); ++i) {
    t[i] = (clips[i] != k_unmapped_clip) ? 1U : 0U;
  }
  return t;
}

[[nodiscard]] constexpr auto make_snapshot_table_for_clips(
    const std::array<std::uint16_t, state_count()>& clips) noexcept
    -> std::array<bool, state_count()> {
  std::array<bool, state_count()> t{};
  for (std::size_t i = 0; i < state_count(); ++i) {
    t[i] = (clips[i] != k_unmapped_clip);
  }
  t[state_index(StateId::Die)] = false;
  return t;
}

[[nodiscard]] constexpr auto
humanoid_clip_table() noexcept -> std::array<std::uint16_t, state_count()> {
  auto t = make_unmapped_clip_table();
  t[state_index(StateId::Idle)] = k_humanoid_idle_clip;
  t[state_index(StateId::Walk)] = k_humanoid_walk_clip;
  t[state_index(StateId::Run)] = k_humanoid_run_clip;
  t[state_index(StateId::Hold)] = k_humanoid_hold_clip;
  t[state_index(StateId::AttackMelee)] = k_humanoid_hold_clip;
  t[state_index(StateId::AttackRanged)] = k_humanoid_hold_clip;
  t[state_index(StateId::Die)] = k_humanoid_die_infantry_clip;
  t[state_index(StateId::Dead)] = k_humanoid_dead_infantry_clip;
  t[state_index(StateId::AttackSword)] = k_humanoid_attack_sword_a_clip;
  t[state_index(StateId::AttackSpear)] = k_humanoid_attack_spear_a_clip;
  t[state_index(StateId::AttackBow)] = k_humanoid_attack_bow_clip;
  t[state_index(StateId::Cast)] = k_humanoid_attack_bow_clip;
  return t;
}

[[nodiscard]] constexpr auto
humanoid_variant_count_table() noexcept -> std::array<std::uint8_t, state_count()> {
  auto t = make_variant_count_table_for_clips(humanoid_clip_table());
  t[state_index(StateId::Idle)] = k_humanoid_idle_variant_count;
  t[state_index(StateId::AttackSword)] = k_humanoid_attack_sword_variant_count;
  t[state_index(StateId::AttackSpear)] = k_humanoid_attack_spear_variant_count;
  return t;
}

[[nodiscard]] constexpr auto
humanoid_snapshot_table() noexcept -> std::array<bool, state_count()> {
  auto t = make_snapshot_table_for_clips(humanoid_clip_table());
  t[state_index(StateId::Walk)] = false;
  t[state_index(StateId::Run)] = false;
  return t;
}

[[nodiscard]] constexpr auto humanoid_clip_manifest() noexcept -> ClipManifest {
  return {
      humanoid_clip_table(), humanoid_variant_count_table(), humanoid_snapshot_table()};
}

[[nodiscard]] constexpr auto
rider_clip_table() noexcept -> std::array<std::uint16_t, state_count()> {
  auto t = make_unmapped_clip_table();
  t[state_index(StateId::Idle)] = k_humanoid_riding_idle_clip;
  t[state_index(StateId::Walk)] = k_humanoid_riding_idle_clip;
  t[state_index(StateId::Run)] = k_humanoid_riding_charge_clip;
  t[state_index(StateId::Hold)] = k_humanoid_riding_idle_clip;
  t[state_index(StateId::AttackMelee)] = k_humanoid_riding_charge_clip;
  t[state_index(StateId::AttackRanged)] = k_humanoid_riding_bow_shot_clip;
  t[state_index(StateId::Die)] = k_humanoid_die_mounted_clip;
  t[state_index(StateId::Dead)] = k_humanoid_dead_mounted_clip;
  t[state_index(StateId::AttackSword)] = k_humanoid_riding_sword_strike_clip;
  t[state_index(StateId::AttackSpear)] = k_humanoid_attack_spear_a_clip;
  t[state_index(StateId::AttackBow)] = k_humanoid_riding_bow_shot_clip;
  t[state_index(StateId::Cast)] = k_humanoid_riding_bow_shot_clip;
  t[state_index(StateId::RidingIdle)] = k_humanoid_riding_idle_clip;
  t[state_index(StateId::RidingCharge)] = k_humanoid_riding_charge_clip;
  t[state_index(StateId::RidingReining)] = k_humanoid_riding_reining_clip;
  t[state_index(StateId::RidingBowShot)] = k_humanoid_riding_bow_shot_clip;
  return t;
}

[[nodiscard]] constexpr auto
rider_variant_count_table() noexcept -> std::array<std::uint8_t, state_count()> {
  auto t = make_variant_count_table_for_clips(rider_clip_table());
  t[state_index(StateId::AttackSword)] = k_humanoid_riding_sword_variant_count;
  t[state_index(StateId::AttackSpear)] = k_humanoid_attack_spear_variant_count;
  return t;
}

[[nodiscard]] constexpr auto rider_clip_manifest() noexcept -> ClipManifest {
  auto const clips = rider_clip_table();
  return {clips, rider_variant_count_table(), make_snapshot_table_for_clips(clips)};
}

[[nodiscard]] constexpr auto
horse_clip_table() noexcept -> std::array<std::uint16_t, state_count()> {
  auto t = make_unmapped_clip_table();
  t[state_index(StateId::Idle)] = k_horse_idle_clip;
  t[state_index(StateId::Walk)] = k_horse_walk_clip;
  t[state_index(StateId::Run)] = k_horse_run_clip;
  t[state_index(StateId::Hold)] = k_horse_idle_clip;
  t[state_index(StateId::AttackMelee)] = k_horse_attack_clip;
  t[state_index(StateId::AttackRanged)] = k_horse_attack_clip;
  t[state_index(StateId::Die)] = k_horse_die_clip;
  t[state_index(StateId::Dead)] = k_horse_dead_clip;
  t[state_index(StateId::AttackSword)] = k_horse_attack_clip;
  t[state_index(StateId::AttackSpear)] = k_horse_attack_clip;
  t[state_index(StateId::AttackBow)] = k_horse_attack_clip;
  t[state_index(StateId::Cast)] = k_horse_attack_clip;
  return t;
}

[[nodiscard]] constexpr auto horse_clip_manifest() noexcept -> ClipManifest {
  auto const clips = horse_clip_table();
  return {clips,
          make_variant_count_table_for_clips(clips),
          make_snapshot_table_for_clips(clips)};
}

[[nodiscard]] constexpr auto
elephant_clip_table() noexcept -> std::array<std::uint16_t, state_count()> {
  auto t = make_unmapped_clip_table();
  t[state_index(StateId::Idle)] = k_elephant_idle_clip;
  t[state_index(StateId::Walk)] = k_elephant_walk_clip;
  t[state_index(StateId::Run)] = k_elephant_run_clip;
  t[state_index(StateId::Hold)] = k_elephant_idle_clip;
  t[state_index(StateId::AttackMelee)] = k_elephant_attack_clip;
  t[state_index(StateId::AttackRanged)] = k_elephant_attack_clip;
  t[state_index(StateId::Die)] = k_elephant_die_clip;
  t[state_index(StateId::Dead)] = k_elephant_dead_clip;
  t[state_index(StateId::AttackSword)] = k_elephant_attack_clip;
  t[state_index(StateId::AttackSpear)] = k_elephant_attack_clip;
  t[state_index(StateId::AttackBow)] = k_elephant_attack_clip;
  t[state_index(StateId::Cast)] = k_elephant_attack_clip;
  return t;
}

[[nodiscard]] constexpr auto elephant_clip_manifest() noexcept -> ClipManifest {
  auto const clips = elephant_clip_table();
  return {clips,
          make_variant_count_table_for_clips(clips),
          make_snapshot_table_for_clips(clips)};
}

[[nodiscard]] auto humanoid_attack_clip(AttackClipFamily family,
                                        bool mounted,
                                        std::uint8_t variant) noexcept -> std::uint16_t;

[[nodiscard]] auto
humanoid_ambient_idle_clip_variant(HumanoidAmbientIdle idle) noexcept -> std::uint8_t;

[[nodiscard]] auto
humanoid_idle_variant_clip_name(std::uint8_t clip_variant) noexcept -> std::string_view;

[[nodiscard]] auto humanoid_construction_role_for_variant_index(
    std::uint8_t variant_index) noexcept -> HumanoidConstructionRole;

[[nodiscard]] auto resolve_humanoid_construction_role(
    const HumanoidConstructionRoleInputs& inputs) noexcept -> HumanoidConstructionRole;

[[nodiscard]] auto requested_humanoid_clip_variant(
    const HumanoidClipVariantInputs& inputs) noexcept -> std::uint8_t;

[[nodiscard]] auto resolve_humanoid_clip_variant(
    const HumanoidClipVariantInputs& inputs) noexcept -> std::uint8_t;

[[nodiscard]] auto authored_humanoid_clip_markers(
    std::uint16_t clip_id,
    HumanoidClipProfile profile = HumanoidClipProfile::Default) noexcept -> ClipMarkers;

[[nodiscard]] auto
authored_generic_clip_markers(std::string_view clip_name) noexcept -> ClipMarkers;

} // namespace Animation
