#pragma once

#include <algorithm>
#include <cstdint>

#include "../units/spawn_type.h"
#include "animation/death_pose_manifest.h"
#include "component_economy.h"
#include "component_gameplay.h"
#include "entity.h"

namespace Engine::Core {

struct DeathSequenceTiming {
  float state_duration{1.0F};
  float dead_hold_duration{Defaults::k_corpse_hold_duration};
  float sink_duration{Defaults::k_corpse_sink_duration};
  std::uint8_t sequence_variant{0U};
};

template <typename Sequence>
void apply_death_sequence_timing(Sequence& sequence,
                                 const DeathSequenceTiming& timing) noexcept {
  sequence.state = DeathSequenceState::Dying;
  sequence.state_time = 0.0F;
  sequence.state_duration = timing.state_duration;
  sequence.dead_hold_duration = timing.dead_hold_duration;
  sequence.sink_duration = timing.sink_duration;
  sequence.sequence_variant = timing.sequence_variant;
}

template <typename Sequence>
[[nodiscard]] auto advance_death_sequence(Sequence& sequence,
                                          float delta_time) noexcept -> bool {
  sequence.state_time += std::max(0.0F, delta_time);
  if (sequence.state == DeathSequenceState::Dying &&
      sequence.state_time >= sequence.state_duration) {
    sequence.state = DeathSequenceState::DeadHold;
    sequence.state_time = 0.0F;
  }
  if (sequence.state == DeathSequenceState::DeadHold &&
      sequence.state_time >= sequence.dead_hold_duration) {
    sequence.state = DeathSequenceState::Sinking;
    sequence.state_time = 0.0F;
  }
  return sequence.state == DeathSequenceState::Sinking &&
         sequence.state_time >= sequence.sink_duration;
}

template <typename Sequence>
void begin_death_sinking(Sequence& sequence) noexcept {
  if (sequence.state == DeathSequenceState::Sinking) {
    return;
  }
  sequence.state = DeathSequenceState::Sinking;
  sequence.state_time = 0.0F;
}

template <typename Sequence>
[[nodiscard]] auto death_sink_progress(const Sequence& sequence) noexcept -> float {
  if (sequence.state != DeathSequenceState::Sinking) {
    return 0.0F;
  }
  if (sequence.sink_duration <= 0.0F) {
    return 1.0F;
  }
  return std::clamp(sequence.state_time / sequence.sink_duration, 0.0F, 1.0F);
}

template <typename Sequence>
[[nodiscard]] auto
death_sequence_is_settled(const Sequence& sequence) noexcept -> bool {
  return sequence.state == DeathSequenceState::DeadHold;
}

template <typename Sequence>
[[nodiscard]] auto death_sequence_elapsed(const Sequence& sequence) noexcept -> float {
  switch (sequence.state) {
  case DeathSequenceState::Dying:
    return sequence.state_time;
  case DeathSequenceState::DeadHold:
    return sequence.state_duration + sequence.state_time;
  case DeathSequenceState::Sinking:
    return sequence.state_duration + sequence.dead_hold_duration + sequence.state_time;
  }
  return sequence.state_time;
}

[[nodiscard]] inline auto
resolve_death_profile(const UnitComponent* unit,
                      bool wildlife) noexcept -> DeathSequenceProfile {
  using Game::Units::SpawnType;
  if (wildlife) {
    return DeathSequenceProfile::Horse;
  }
  if (unit == nullptr) {
    return DeathSequenceProfile::Infantry;
  }
  if (unit->death_sequence_override != 0xFFU &&
      unit->death_sequence_override <=
          static_cast<std::uint8_t>(DeathSequenceProfile::Elephant)) {
    return static_cast<DeathSequenceProfile>(unit->death_sequence_override);
  }
  if (unit->spawn_type == SpawnType::Elephant) {
    return DeathSequenceProfile::Elephant;
  }
  if (unit->spawn_type == SpawnType::MountedKnight ||
      unit->spawn_type == SpawnType::HorseArcher ||
      unit->spawn_type == SpawnType::HorseSpearman) {
    return DeathSequenceProfile::MountedRider;
  }
  if (Game::Units::is_wildlife_spawn(unit->spawn_type)) {
    return DeathSequenceProfile::Horse;
  }
  return DeathSequenceProfile::Infantry;
}

[[nodiscard]] inline auto
resolve_death_profile(const Entity& entity) noexcept -> DeathSequenceProfile {
  return resolve_death_profile(entity.get_component<UnitComponent>(),
                               entity.has_component<WildlifeComponent>());
}

[[nodiscard]] inline auto
resolve_death_timing(DeathSequenceProfile profile,
                     std::uint8_t variant) noexcept -> DeathSequenceTiming {
  DeathSequenceTiming timing{};
  timing.sequence_variant = variant;
  switch (profile) {
  case DeathSequenceProfile::MountedRider:
    timing.state_duration = Animation::humanoid_death_collapse_duration(
        Animation::HumanoidDeathCollapse::MountedUnseat);
    break;
  case DeathSequenceProfile::Horse:
    timing.state_duration = 1.20F;
    timing.sink_duration = 1.8F;
    timing.sequence_variant = 0U;
    break;
  case DeathSequenceProfile::Elephant:
    timing.state_duration = 1.50F;
    timing.dead_hold_duration = 10.0F;
    timing.sink_duration = 2.4F;
    break;
  case DeathSequenceProfile::Infantry:
  default:
    timing.state_duration = Animation::humanoid_death_collapse_duration(
        Animation::humanoid_infantry_death_collapse(variant));
    break;
  }
  return timing;
}

inline auto begin_death_sequence(Entity& entity, std::uint8_t variant) noexcept
    -> DeathAnimationComponent* {
  auto* death = get_or_add_component<DeathAnimationComponent>(&entity);
  if (death == nullptr) {
    return nullptr;
  }
  if (auto* unit = entity.get_component<UnitComponent>()) {
    unit->health = 0;
  }
  auto const profile = resolve_death_profile(entity);
  death->profile = profile;
  apply_death_sequence_timing(*death, resolve_death_timing(profile, variant));
  return death;
}

} // namespace Engine::Core
