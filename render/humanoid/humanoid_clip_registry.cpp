#include "humanoid_clip_registry.h"

#include "../animation/channel_evaluator.h"
#include "../animation/clip.h"

namespace Render::Humanoid {

namespace {

using Clip = Render::Animation::Clip<float>;
using KF = Render::Animation::Keyframe<float>;

auto build_sway_clips() -> std::vector<Clip> {
  std::vector<Clip> clips(kStateCount);
  clips[static_cast<std::size_t>(HumanoidState::Idle)] =
      Clip("humanoid_idle_sway",
           {KF{0.00F, 0.00F}, KF{0.60F, 0.018F}, KF{1.20F, 0.00F},
            KF{1.80F, -0.018F}, KF{2.40F, 0.00F}});
  clips[static_cast<std::size_t>(HumanoidState::Hold)] =
      Clip("humanoid_hold_sway",
           {KF{0.00F, 0.00F}, KF{1.00F, 0.008F}, KF{2.00F, 0.00F},
            KF{3.00F, -0.008F}, KF{4.00F, 0.00F}});
  clips[static_cast<std::size_t>(HumanoidState::Walk)] =
      Clip("humanoid_walk_sway",
           {KF{0.00F, 0.00F}, KF{0.25F, 0.05F}, KF{0.50F, 0.00F},
            KF{0.75F, -0.05F}, KF{1.00F, 0.00F}});
  clips[static_cast<std::size_t>(HumanoidState::Run)] =
      Clip("humanoid_run_sway",
           {KF{0.00F, 0.00F}, KF{0.15F, 0.08F}, KF{0.30F, 0.00F},
            KF{0.45F, -0.08F}, KF{0.60F, 0.00F}});

  clips[static_cast<std::size_t>(HumanoidState::AttackMelee)] =
      Clip("humanoid_melee_sway", {KF{0.00F, 0.00F}, KF{0.30F, 0.02F},
                                   KF{0.60F, -0.02F}, KF{0.90F, 0.00F}});
  clips[static_cast<std::size_t>(HumanoidState::AttackRanged)] =
      Clip("humanoid_ranged_sway", {KF{0.00F, 0.00F}, KF{0.50F, 0.01F},
                                    KF{1.00F, -0.01F}, KF{1.50F, 0.00F}});
  clips[static_cast<std::size_t>(HumanoidState::Construct)] =
      Clip("humanoid_construct_sway",
           {KF{0.00F, 0.00F}, KF{0.40F, 0.03F}, KF{0.80F, 0.00F},
            KF{1.20F, -0.03F}, KF{1.60F, 0.00F}});

  return clips;
}

auto build_breathing_clips() -> std::vector<Clip> {
  std::vector<Clip> clips(kStateCount);
  clips[static_cast<std::size_t>(HumanoidState::Idle)] =
      Clip("humanoid_idle_breathe",
           {KF{0.00F, 0.000F}, KF{1.50F, 0.010F}, KF{3.00F, 0.000F}});
  clips[static_cast<std::size_t>(HumanoidState::Hold)] =
      Clip("humanoid_hold_breathe",
           {KF{0.00F, 0.000F}, KF{2.00F, 0.006F}, KF{4.00F, 0.000F}});
  clips[static_cast<std::size_t>(HumanoidState::Walk)] =
      Clip("humanoid_walk_breathe",
           {KF{0.00F, 0.000F}, KF{0.50F, 0.006F}, KF{1.00F, 0.000F}});
  clips[static_cast<std::size_t>(HumanoidState::Run)] =
      Clip("humanoid_run_breathe",
           {KF{0.00F, 0.000F}, KF{0.25F, 0.012F}, KF{0.50F, 0.000F}});
  clips[static_cast<std::size_t>(HumanoidState::HitReaction)] =
      Clip("humanoid_hit_breathe",
           {KF{0.00F, -0.015F}, KF{0.20F, 0.005F}, KF{0.40F, 0.000F}});
  return clips;
}

auto build_jitter_clips() -> std::vector<Clip> {
  std::vector<Clip> clips(kStateCount);

  clips[static_cast<std::size_t>(HumanoidState::Idle)] =
      Clip("humanoid_idle_jitter",
           {KF{0.00F, 0.004F}, KF{1.00F, 0.006F}, KF{2.00F, 0.004F}});

  clips[static_cast<std::size_t>(HumanoidState::Hold)] =
      Clip("humanoid_hold_jitter", {KF{0.00F, 0.002F}, KF{2.00F, 0.002F}});

  return clips;
}

} // namespace

HumanoidClipRegistry::HumanoidClipRegistry()
    : m_sway_clips(build_sway_clips()),
      m_breathing_clips(build_breathing_clips()),
      m_jitter_clips(build_jitter_clips()) {}

auto HumanoidClipRegistry::instance() -> const HumanoidClipRegistry & {
  static const HumanoidClipRegistry k_instance;
  return k_instance;
}

HumanoidClipDriver::HumanoidClipDriver(const HumanoidClipRegistry &registry)
    : m_registry(&registry) {}

void HumanoidClipDriver::tick(float dt,
                              const Render::GL::AnimationInputs &inputs,
                              bool dead_flag) {
  m_state.tick(dt, inputs, dead_flag);
}

auto HumanoidClipDriver::sample(float time) const -> HumanoidOverlays {

  Render::Animation::ChannelEvaluator<float> sway_eval(m_registry->sway_clips(),
                                                       &m_state.raw());
  Render::Animation::ChannelEvaluator<float> breathing_eval(
      m_registry->breathing_clips(), &m_state.raw());
  Render::Animation::ChannelEvaluator<float> jitter_eval(
      m_registry->jitter_clips(), &m_state.raw());

  HumanoidOverlays out;
  out.torso_sway_x = sway_eval.sample(time, Render::Animation::WrapMode::Loop);
  out.breathing_y =
      breathing_eval.sample(time, Render::Animation::WrapMode::Loop);
  out.hand_jitter_amp =
      jitter_eval.sample(time, Render::Animation::WrapMode::Loop);
  return out;
}

} // namespace Render::Humanoid
