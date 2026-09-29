#include "app/commander/commander_presentation.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>

#include "game/audio/audio_cues.h"
#include "game/core/world.h"
#include "game/session/session_context.h"
#include "game/systems/combat_actions/commander_defense_timeline.h"
#include "game/systems/rpg_combat_system/rpg_targeting.h"

namespace App::Core {

namespace {

constexpr float k_footstep_min_bob_amplitude = 0.25F;
constexpr float k_footstep_bob_offset = 1.5F * std::numbers::pi_v<float>;

struct EngagementFraming {
  Engine::Core::FightContext fight_context{Engine::Core::FightContext::None};
  float threat_side_bias{0.0F};
  Engine::Core::EntityID nearest_pressing_id{0};
};

auto read_engagement(const Engine::Core::Entity& commander) -> EngagementFraming {
  EngagementFraming framing;
  auto const* engagement =
      commander.get_component<Engine::Core::RpgEngagementComponent>();
  if (engagement == nullptr) {
    return framing;
  }
  framing.fight_context = engagement->fight_context;

  float pressure_bias = 0.0F;
  for (auto const& slot : engagement->engagement_slots) {
    if (!slot.pressing) {
      continue;
    }
    if (framing.nearest_pressing_id == 0) {
      framing.nearest_pressing_id = slot.entity_id;
    }
    pressure_bias += slot.signed_angle_degrees < 0.0F ? 1.0F : -1.0F;
  }
  if (framing.fight_context == Engine::Core::FightContext::Skirmish) {
    framing.threat_side_bias = std::clamp(pressure_bias, -1.0F, 1.0F);
  }
  return framing;
}

auto is_aiming_bow(const Engine::Core::Entity& commander) -> bool {
  auto const* aim_state =
      commander.get_component<Engine::Core::RpgCommanderAimComponent>();
  auto const* bow_action =
      commander.get_component<Engine::Core::RpgCommanderActionComponent>();
  bool const bow_stance =
      aim_state != nullptr && aim_state->stance == Engine::Core::FpvWeaponStance::Bow;
  return aim_state != nullptr &&
         (aim_state->is_drawing() ||
          (bow_stance && bow_action != nullptr && bow_action->action_running));
}

auto soft_focus_position(Engine::Core::World& world,
                         Engine::Core::EntityID nearest_pressing_id)
    -> std::optional<QVector3D> {
  auto* front =
      nearest_pressing_id != 0 ? world.get_entity(nearest_pressing_id) : nullptr;
  if (front == nullptr) {
    return std::nullopt;
  }
  if (auto const sample = Game::Systems::RpgCombat::resolve_soldier_target(
          *front, Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot);
      sample.has_value()) {
    return sample->position;
  }
  return std::nullopt;
}

} // namespace

void CommanderPresentation::publish_sample(
    Engine::Core::Entity& commander,
    const Engine::Core::TransformComponent& transform,
    float dt) {
  auto* sample = Engine::Core::get_or_add_component<
      Engine::Core::CommanderPresentationSampleComponent>(&commander);
  if (sample == nullptr) {
    return;
  }

  auto const previous = sample->position;
  float const step_x = transform.position.x - previous.x;
  float const step_z = transform.position.z - previous.z;
  bool const teleported =
      !sample->valid || ((step_x * step_x) + (step_z * step_z)) >
                            (Engine::Core::k_presentation_teleport_threshold *
                             Engine::Core::k_presentation_teleport_threshold);

  sample->previous_position = sample->valid ? previous : transform.position;
  sample->previous_yaw = sample->valid ? sample->yaw : transform.rotation.y;
  sample->position = transform.position;
  sample->yaw = transform.rotation.y;
  sample->tick_seconds = dt;
  sample->snap = teleported || m_snap_requested;
  sample->valid = true;
  sample->presented_valid = false;
  ++sample->tick_sequence;
  m_snap_requested = false;
}

auto CommanderPresentation::advance_pose(
    Engine::Core::Entity& commander,
    const Engine::Core::TransformComponent& transform,
    float dt) -> Engine::Core::PresentationPose {
  auto* sample =
      commander.get_component<Engine::Core::CommanderPresentationSampleComponent>();
  if (sample == nullptr || !sample->valid) {
    m_pose.position = transform.position;
    m_pose.yaw = transform.rotation.y;
    m_pose.alpha = 1.0F;
    m_pose.extrapolated = false;
    return m_pose;
  }

  float const age = m_clock.advance(*sample, dt);
  m_pose = Engine::Core::resolve_presentation_pose(*sample, age);
  sample->presented_position = m_pose.position;
  sample->presented_yaw = m_pose.yaw;
  sample->presented_valid = true;
  return m_pose;
}

void CommanderPresentation::snap() {
  m_snap_requested = true;
  m_clock.reset();
}

auto build_camera_inputs(Engine::Core::World& world,
                         Engine::Core::Entity& commander,
                         const CameraFeed& feed,
                         const Engine::Core::PresentationPose& pose)
    -> CommanderCameraInputs {
  auto const framing = read_engagement(commander);

  CommanderCameraInputs inputs;
  inputs.dt = feed.dt;
  inputs.view_yaw_degrees = feed.view_yaw;
  inputs.view_pitch_degrees = feed.view_pitch;
  inputs.move_speed = feed.move_speed;
  inputs.move_right_axis = feed.move_right_axis;
  inputs.move_running = feed.move_running;
  inputs.aiming_bow = is_aiming_bow(commander);
  if (auto const* cmd = commander.get_component<Engine::Core::CommanderComponent>()) {
    inputs.jump_height_offset = cmd->jump_height_offset;
    inputs.close_camera_mode = cmd->close_camera_mode;
  }
  inputs.lock_target_active = feed.lock_target_position.has_value();
  inputs.dodge_fov_kick = feed.dodge_fov_kick;
  inputs.dodge_rolling = feed.dodge_rolling;
  inputs.dodge_tilt_progress =
      1.0F -
      std::clamp(
          feed.dodge_timer /
              Game::Systems::CombatActions::k_commander_dodge_timeline.roll_seconds,
          0.0F,
          1.0F);
  inputs.dodge_direction = feed.dodge_direction;
  inputs.commander_position =
      QVector3D(pose.position.x, pose.position.y, pose.position.z);
  inputs.lock_target_position = feed.lock_target_position;
  if (!feed.lock_target_position.has_value()) {
    inputs.soft_focus_position =
        soft_focus_position(world, framing.nearest_pressing_id);
  }
  inputs.fight_context = framing.fight_context;
  inputs.threat_side_bias = framing.threat_side_bias;
  auto& camera_session = Game::Session::session_for(world);
  inputs.terrain = &camera_session.terrain();
  inputs.buildings = &camera_session.building_collision();
  return inputs;
}

void play_footstep_if_stride_landed(const CommanderCameraRig& rig,
                                    const Engine::Core::Entity& commander,
                                    bool running,
                                    float previous_bob_phase) {
  if (rig.bob_amplitude() < k_footstep_min_bob_amplitude) {
    return;
  }

  const auto stride_index = [](float phase) {
    return static_cast<long long>(std::floor((phase - k_footstep_bob_offset) /
                                             (2.0F * std::numbers::pi_v<float>)));
  };
  if (stride_index(rig.bob_phase()) == stride_index(previous_bob_phase)) {
    return;
  }

  if (running) {
    Game::Audio::play_cue(Game::Audio::Cue::k_move_footstep_run);
    return;
  }

  const auto* terrain =
      commander.get_component<Engine::Core::TerrainContextComponent>();
  const bool hard_ground = terrain != nullptr && terrain->is_on_bridge;
  Game::Audio::play_cue(hard_ground ? Game::Audio::Cue::k_move_footstep_hard
                                    : Game::Audio::Cue::k_move_footstep);
}

} // namespace App::Core
