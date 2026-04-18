// Stage 15 — humanoid clip registry + per-state overlay channels.
//
// The registry binds discrete HumanoidStates to authored Clips that drive
// overlay channels on top of the procedurally-computed base pose. This is
// the end-user consumer of Stage 12's Clip/StateMachine/ChannelEvaluator
// that was previously scaffolded with no live user.
//
// Channels currently driven:
//   * torso_sway_x  — left/right lean in metres, added to chest/neck
//                     origin at submit time.
//   * breathing     — vertical bob of the chest+head, metres.
//   * weapon_jitter — small random offset for idle hand-weapon micro
//                     movements (dampened when running/attacking).
//
// Each channel has one Clip per HumanoidState; the registry builds a
// ChannelEvaluator that samples the right clip automatically. Adding a
// new channel is: (1) new field on `HumanoidOverlays`, (2) author clips
// per state, (3) construct a ChannelEvaluator, (4) sample in
// `HumanoidClipDriver::sample()`. That's it — no changes to the existing
// 2858-line rig path.

#pragma once

#include "../animation/channel_evaluator.h"
#include "../animation/clip.h"
#include "../animation/state_machine.h"
#include "humanoid_state_machine.h"

#include <array>
#include <memory>

namespace Render::Humanoid {

struct HumanoidOverlays {
  // Lateral torso sway (metres). Applied to chest, neck, head, shoulders
  // so the upper body leans as one rigid segment without affecting legs.
  float torso_sway_x{0.0F};
  // Vertical torso bob (metres). Adds to chest+head only, not shoulders,
  // to keep arm anchors stable.
  float breathing_y{0.0F};
  // Hand micro-jitter magnitude (metres, applied per-frame with random
  // direction at draw time). Zero for running / attacking; small for idle.
  float hand_jitter_amp{0.0F};
};

// Registry is build-once, sample-many. The clips are all authored in the
// implementation file with short keyframe lists — adding a new state
// means adding a new per-state vector entry.
class HumanoidClipRegistry {
public:
  HumanoidClipRegistry();

  [[nodiscard]] auto sway_clips() const noexcept
      -> const std::vector<Render::Animation::Clip<float>> & {
    return m_sway_clips;
  }
  [[nodiscard]] auto breathing_clips() const noexcept
      -> const std::vector<Render::Animation::Clip<float>> & {
    return m_breathing_clips;
  }
  [[nodiscard]] auto jitter_clips() const noexcept
      -> const std::vector<Render::Animation::Clip<float>> & {
    return m_jitter_clips;
  }

  // Global singleton: clips are immutable after construction, shared
  // across every humanoid unit in the scene. Allocated on first use.
  static auto instance() -> const HumanoidClipRegistry &;

private:
  // One clip per HumanoidState (index = static_cast<uint8_t>(state)).
  std::vector<Render::Animation::Clip<float>> m_sway_clips;
  std::vector<Render::Animation::Clip<float>> m_breathing_clips;
  std::vector<Render::Animation::Clip<float>> m_jitter_clips;
};

// Per-unit driver: owns the state machine and samples overlays using
// the shared registry. One ClipDriver per rendered unit instance (cheap
// — the state machine is two uint8s + two floats and the evaluators are
// non-owning views).
class HumanoidClipDriver {
public:
  explicit HumanoidClipDriver(const HumanoidClipRegistry &registry =
                                  HumanoidClipRegistry::instance());

  // Advance the machine using gameplay inputs. Safe to call every frame.
  void tick(float dt, const Render::GL::AnimationInputs &inputs,
            bool dead_flag = false);

  // Snap without blending — useful for spawn / test setup.
  void snap_to(HumanoidState state) { m_state.snap_to(state); }

  // Sample overlays for the given absolute clip time. Wraps clip time
  // per-channel so each channel can have its own loop length.
  [[nodiscard]] auto sample(float time) const -> HumanoidOverlays;

  [[nodiscard]] auto state() const noexcept -> HumanoidState {
    return m_state.current();
  }
  [[nodiscard]] auto state_machine() const noexcept
      -> const HumanoidStateMachine & {
    return m_state;
  }

private:
  const HumanoidClipRegistry *m_registry;
  HumanoidStateMachine m_state;
};

} // namespace Render::Humanoid
