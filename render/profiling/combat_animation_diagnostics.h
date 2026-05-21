#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "../creature/movement_state.h"
#include "../creature/pose_intent.h"
#include "../gl/humanoid/humanoid_types.h"

namespace Render::Profiling {

struct CombatPhaseWindow {
  float start{0.0F};
  float end{0.0F};
  float offset_weight{0.0F};
};

[[nodiscard]] auto
attack_phase_window(Render::GL::CombatAnimPhase phase,
                    bool amplified_attack,
                    bool finisher_attack) noexcept -> CombatPhaseWindow;

struct ScrubbedCombatPhase {
  Render::GL::CombatAnimPhase phase{Render::GL::CombatAnimPhase::Idle};
  float progress{0.0F};
};

[[nodiscard]] auto scrubbed_combat_phase_from_attack_phase(
    float attack_phase,
    bool amplified_attack,
    bool finisher_attack) noexcept -> ScrubbedCombatPhase;

[[nodiscard]] auto
combat_phase_name(Render::GL::CombatAnimPhase phase) noexcept -> const char*;
[[nodiscard]] auto
locomotion_state_name(Render::Creature::MovementAnimationState state) noexcept -> const
    char*;
[[nodiscard]] auto
animation_state_name(Render::Creature::AnimationStateId state) noexcept -> const char*;

enum class SoldierCullReason : std::uint8_t {
  None = 0,
  Frustum,
  Billboard,
  Temporal,
};

[[nodiscard]] auto soldier_cull_reason_name(SoldierCullReason reason) noexcept -> const
    char*;

enum class SoldierVisualState : std::uint8_t {
  Idle = 0,
  Walk,
  Run,
  Attack,
  Hold,
  HitReaction,
  Dying,
  Dead,
};

[[nodiscard]] auto soldier_visual_state_name(SoldierVisualState state) noexcept -> const
    char*;

struct UnitAnimationDebugSample {
  std::uint32_t entity_id{0U};
  float sample_time{0.0F};
  Render::GL::CombatAnimPhase combat_phase{Render::GL::CombatAnimPhase::Idle};
  float combat_phase_progress{0.0F};
  float attack_phase{0.0F};
  std::uint8_t attack_variant{0U};
  bool is_attacking{false};
  bool is_in_melee_lock{false};
  Render::Creature::MovementAnimationState locomotion_state{
      Render::Creature::MovementAnimationState::Idle};
  std::uint32_t attack_target_id{0U};
  bool attack_from_combat_state{false};
  bool attack_from_melee_lock{false};
  bool elephant_attack_override{false};
};

struct SoldierAnimationDebugSample {
  int soldier_index{0};
  float sample_time{0.0F};
  Render::GL::CombatAnimPhase combat_phase{Render::GL::CombatAnimPhase::Idle};
  float combat_phase_progress{0.0F};
  float attack_phase{0.0F};
  std::uint8_t attack_variant{0U};
  bool is_attacking{false};
  bool is_in_melee_lock{false};
  bool transient_recovery_override{false};
  Render::Creature::MovementAnimationState locomotion_state{
      Render::Creature::MovementAnimationState::Idle};
  Render::Creature::AnimationStateId animation_state{
      Render::Creature::AnimationStateId::Idle};
  std::uint8_t lod{0U};
  SoldierCullReason cull_reason{SoldierCullReason::None};
  SoldierVisualState visual_state{SoldierVisualState::Idle};
  bool visual_state_changed{false};
  bool movement_state_changed{false};
  bool variant_changed{false};
  bool lod_changed{false};
  bool attack_phase_reset{false};
  bool churn_flagged{false};
  std::uint32_t transitions_last_second{0U};
};

struct CombatAnimationDebugUnit {
  UnitAnimationDebugSample unit{};
  std::vector<SoldierAnimationDebugSample> soldiers;
  std::uint32_t flagged_soldiers{0U};
};

class CombatAnimationDiagnostics {
public:
  [[nodiscard]] static auto instance() -> CombatAnimationDiagnostics&;

  void set_enabled(bool enabled);
  [[nodiscard]] auto enabled() const noexcept -> bool { return m_enabled; }

  void set_logging_enabled(bool enabled);
  [[nodiscard]] auto logging_enabled() const noexcept -> bool {
    return m_logging_enabled;
  }

  void begin_frame(std::uint64_t frame_index);

  void record_unit_sample(const UnitAnimationDebugSample& sample);
  void mark_elephant_override(std::uint32_t entity_id);
  void record_soldier_sample(std::uint32_t entity_id,
                             const SoldierAnimationDebugSample& sample);

  [[nodiscard]] auto
  find_unit(std::uint32_t entity_id) const -> const CombatAnimationDebugUnit*;
  [[nodiscard]] auto flagged_unit_count() const noexcept -> std::size_t;

private:
  struct SoldierTracker {
    float last_time{0.0F};
    float last_attack_phase{0.0F};
    std::uint8_t last_attack_variant{0U};
    std::uint8_t last_lod{0U};
    Render::Creature::MovementAnimationState last_locomotion{
        Render::Creature::MovementAnimationState::Idle};
    SoldierVisualState last_visual_state{SoldierVisualState::Idle};
    bool has_previous{false};
    std::vector<float> transition_times{};
    std::uint64_t last_logged_frame{0U};
  };

  struct SoldierKey {
    std::uint32_t entity_id{0U};
    std::uint16_t soldier_index{0U};

    auto operator==(const SoldierKey& other) const noexcept -> bool {
      return entity_id == other.entity_id && soldier_index == other.soldier_index;
    }
  };

  struct SoldierKeyHash {
    auto operator()(const SoldierKey& key) const noexcept -> std::size_t {
      return (static_cast<std::size_t>(key.entity_id) << 16U) ^
             static_cast<std::size_t>(key.soldier_index);
    }
  };

  CombatAnimationDiagnostics() = default;

  [[nodiscard]] auto active() const noexcept -> bool {
    return m_enabled || m_logging_enabled;
  }

  bool m_enabled{false};
  bool m_logging_enabled{false};
  std::uint64_t m_frame_index{0U};
  std::unordered_map<std::uint32_t, CombatAnimationDebugUnit> m_units{};
  std::unordered_map<SoldierKey, SoldierTracker, SoldierKeyHash> m_trackers{};
};

} // namespace Render::Profiling
