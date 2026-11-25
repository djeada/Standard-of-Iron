#include "app/commander/commander_control_controller.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <utility>

#include "game/audio/audio_cues.h"
#include "game/command/command_queue.h"
#include "game/commander/commander_entity_access.h"
#include "game/commander/commander_heading.h"
#include "game/commander/commander_runtime.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "scene/camera.h"

namespace {

constexpr float k_fov_hip = 68.0F;

} // namespace

void CommanderControlController::set_latency_probe(
    Game::Commander::CommanderLatencyProbe* probe) {
  m_latency_probe = probe;
  m_input_port.set_latency_probe(probe);
  m_look.set_latency_probe(probe);
  m_runtime->set_latency_probe(probe);
}

CommanderControlController::CommanderControlController()
    : m_preview_runtime(std::make_unique<Game::Commander::CommanderRuntime>())
    , m_runtime(m_preview_runtime.get()) {
}

CommanderControlController::~CommanderControlController() {
  bind_runtime(nullptr);
}

auto CommanderControlController::last_refusal() const
    -> const Game::Commander::CommanderRefusal& {
  return m_runtime->last_refusal();
}

auto CommanderControlController::is_dodge_rolling() const -> bool {
  return m_runtime->locomotion().dodge_state() == DodgeState::Rolling;
}

void CommanderControlController::set_presentation_trace_enabled(bool enabled) {
  m_trace_enabled = enabled;
  m_runtime->set_trace_enabled(enabled);
  if (!enabled) {
    m_trace = {};
  }
}

void CommanderControlController::snap_presentation_pose() {
  m_presentation.snap();
}

void CommanderControlController::release_all_input() {
  m_input_port.release_all();
  if (!m_match_runtime) {
    m_runtime->release_all_input();
  }
  m_look.suspend_mouse();
}

void CommanderControlController::release_pointer() {
  m_look.release_mouse();
}

auto CommanderControlController::pointer_captured() const -> bool {
  return m_look.mouse_captured();
}

void CommanderControlController::reset() {
  release_all_input();
  snap_presentation_pose();
  m_look.reset();
  m_camera_rig.reset();
  if (!m_match_runtime) {
    m_runtime->reset();
  }
}

void CommanderControlController::release_for_rally() {
  m_input_port.release_all();
  m_look.reset();
  if (!m_match_runtime) {
    m_runtime->release_for_rally();
  }
}

void CommanderControlController::set_view_yaw(float yaw) {
  m_look.set_yaw(yaw);
}

void CommanderControlController::set_view_pitch(float pitch) {
  m_look.set_pitch(pitch);
}

auto CommanderControlController::view_yaw() const -> float {
  return m_look.yaw();
}

auto CommanderControlController::view_pitch() const -> float {
  return m_look.pitch();
}

auto CommanderControlController::input() -> InputState& {
  return m_input_port.held();
}

auto CommanderControlController::input() const -> const InputState& {
  return m_input_port.held();
}

void CommanderControlController::key_down(int key) {
  m_input_port.key_down(key);
}

void CommanderControlController::key_up(int key) {
  m_input_port.key_up(key);
}

void CommanderControlController::primary_action_down() {
  m_input_port.primary_action_down();
}

void CommanderControlController::primary_action_up() {
  m_input_port.primary_action_up();
}

void CommanderControlController::request_heavy_action() {
  m_input_port.request_heavy_action();
}

void CommanderControlController::secondary_action_down() {
  m_input_port.secondary_action_down();
}

void CommanderControlController::secondary_action_up() {
  m_input_port.secondary_action_up();
}

void CommanderControlController::request_dodge() {
  m_input_port.request_dodge();
}

void CommanderControlController::request_dodge(const QVector3D& world_direction) {
  m_input_port.request_dodge(world_direction);
}

void CommanderControlController::request_jump() {
  m_input_port.request_jump();
}

void CommanderControlController::special_action() {
  m_input_port.special_action();
}

void CommanderControlController::request_vanguard_rush() {
  m_input_port.request_vanguard_rush();
}

void CommanderControlController::request_second_wind() {
  m_input_port.request_second_wind();
}

auto CommanderControlController::look_sensitivity_scale() const -> float {
  constexpr float k_half_degrees_to_radians = 0.008726646259971648F;
  const float hip = std::tan(k_fov_hip * k_half_degrees_to_radians);
  const float current = std::tan(std::clamp(m_camera_rig.fov(), 20.0F, 110.0F) *
                                 k_half_degrees_to_radians);
  const float zoom = std::clamp(current / std::max(hip, 1.0e-4F), 0.35F, 1.0F);

  constexpr float k_aim_steadiness = 0.20F;
  return zoom *
         (1.0F - (k_aim_steadiness * std::clamp(m_camera_rig.aim_blend(), 0.0F, 1.0F)));
}

void CommanderControlController::mouse_move(qreal dx, qreal dy) {
  m_look.apply_mouse_delta(dx, dy, look_sensitivity_scale());
}

void CommanderControlController::mouse_look_at(
    qreal sx, qreal sy, qreal center_sx, qreal center_sy, QQuickWindow* window) {
  m_look.mouse_look_at(sx, sy, center_sx, center_sy, window, look_sensitivity_scale());
}

void CommanderControlController::center_mouse(qreal center_sx,
                                              qreal center_sy,
                                              QQuickWindow* window) {
  m_look.center_mouse(center_sx, center_sy, window);
}

void CommanderControlController::poll_mouse_look(QQuickWindow* window) {
  m_look.poll_mouse(window, look_sensitivity_scale());
}

auto CommanderControlController::sample_frame_intent(QQuickWindow* window)
    -> CommanderFrameIntent {
  poll_mouse_look(window);
  return m_look.sample_frame_intent(m_input_port.locked_copy());
}

auto CommanderControlController::queued_intent_count(
    Engine::Core::World& world,
    Engine::Core::EntityID commander_id,
    int local_owner_id) const -> int {
  auto const* commander = controlled_commander(world, commander_id, local_owner_id);
  auto const* intents =
      commander != nullptr
          ? commander->get_component<Engine::Core::CombatIntentQueueComponent>()
          : nullptr;
  return intents != nullptr ? static_cast<int>(intents->count) : 0;
}

auto CommanderControlController::update(Engine::Core::World& world,
                                        Engine::Core::EntityID commander_id,
                                        int local_owner_id,
                                        Render::GL::Camera& camera,
                                        float dt) -> bool {
  return update_impl(world, commander_id, local_owner_id, &camera, dt);
}

auto CommanderControlController::update_simulation(Engine::Core::World& world,
                                                   Engine::Core::EntityID commander_id,
                                                   int local_owner_id,
                                                   float dt) -> bool {
  return update_impl(world, commander_id, local_owner_id, nullptr, dt);
}

void CommanderControlController::update_camera_presentation(
    Engine::Core::World& world,
    Engine::Core::EntityID commander_id,
    Render::GL::Camera& camera,
    float dt) {
  auto* commander = world.get_entity(commander_id);
  if (commander == nullptr) {
    return;
  }
  update_camera(world, *commander, camera, dt);
}

void CommanderControlController::update_camera(Engine::Core::World& world,
                                               Engine::Core::Entity& commander,
                                               Render::GL::Camera& camera,
                                               float dt) {
  auto* transform = commander.get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return;
  }

  App::Core::CameraFeed feed;
  feed.dt = dt;
  feed.view_yaw = m_look.yaw();
  feed.view_pitch = m_look.pitch();
  feed.move_speed = m_runtime->locomotion().move_speed();
  feed.move_right_axis = m_runtime->locomotion().move_right_axis();
  feed.move_running = m_runtime->locomotion().move_running();
  feed.dodge_fov_kick = m_runtime->locomotion().dodge_fov_kick();
  feed.dodge_rolling = m_runtime->locomotion().dodge_state() == DodgeState::Rolling;
  feed.dodge_timer = m_runtime->locomotion().dodge_timer();
  feed.dodge_direction = m_runtime->locomotion().dodge_direction();
  feed.lock_target_position = m_runtime->targeting().locked_position(world);

  auto const pose = m_presentation.advance_pose(commander, *transform, dt);
  auto const inputs = App::Core::build_camera_inputs(world, commander, feed, pose);

  float const previous_bob_phase = m_camera_rig.update(camera, inputs);
  if (m_trace_enabled) {
    m_trace.camera = m_camera_rig.trace();
    m_trace.motor.presented_position =
        QVector3D(pose.position.x, pose.position.y, pose.position.z);
    m_trace.motor.presented_yaw = pose.yaw;
    m_trace.motor.presentation_alpha = pose.alpha;
    m_trace.motor.presentation_extrapolated = pose.extrapolated;
  }
  if (m_latency_probe != nullptr) {
    m_latency_probe->note_camera_response();
  }
  App::Core::play_footstep_if_stride_landed(
      m_camera_rig, world, commander.get_id(), feed.move_running, previous_bob_phase);
}

void CommanderControlController::set_feedback_bus(Game::Core::PlayerFeedbackBus* bus) {
  m_feedback = bus;
  m_runtime->set_feedback_bus(bus);
}

void CommanderControlController::bind_runtime(
    std::shared_ptr<Game::Commander::CommanderRuntime> runtime) {
  if (m_match_runtime) {
    m_match_runtime->set_feedback_bus(nullptr);
    m_match_runtime->set_latency_probe(nullptr);
  }
  m_match_runtime = std::move(runtime);
  m_runtime = m_match_runtime ? m_match_runtime.get() : m_preview_runtime.get();
  m_runtime->set_latency_probe(m_latency_probe);
  m_runtime->set_feedback_bus(m_feedback);
  m_runtime->set_trace_enabled(m_trace_enabled);
}

auto CommanderControlController::input_edges() const
    -> Game::Commander::CommanderInputTrace {
  auto edges = m_runtime->input_edges();
  const auto& physical = m_input_port.edges();
  edges.primary_press_sequence = physical.primary_press_sequence;
  edges.primary_release_sequence = physical.primary_release_sequence;
  edges.guard_press_sequence = physical.guard_press_sequence;
  edges.guard_release_sequence = physical.guard_release_sequence;
  edges.dodge_request_sequence = physical.dodge_request_sequence;
  edges.jump_request_sequence = physical.jump_request_sequence;
  edges.primary_dropped_sequence += physical.primary_dropped_sequence;
  edges.dodge_refused_sequence += physical.dodge_refused_sequence;
  edges.jump_refused_sequence += physical.jump_refused_sequence;
  return edges;
}

auto CommanderControlController::capture_input(Engine::Core::EntityID commander_id)
    -> Game::Command::CommanderInputFrame {
  std::optional<std::pair<QVector3D, QVector3D>> ray;
  if (m_camera_rig.eye_valid() && m_camera_rig.forward_valid()) {
    ray = std::make_pair(m_camera_rig.eye(), m_camera_rig.forward());
  }
  auto frame = m_input_port.take_snapshot().to_record(
      commander_id, view_yaw(), view_pitch(), ray);
  frame.camera_fov = m_camera_rig.fov();
  return frame;
}

auto CommanderControlController::update_impl(Engine::Core::World& world,
                                             Engine::Core::EntityID commander_id,
                                             int owner,
                                             Render::GL::Camera* camera,
                                             float dt) -> bool {
  if (controlled_commander(world, commander_id, owner) == nullptr) {
    return false;
  }
  if (m_match_runtime) {
    auto& session = Game::Session::session_for(world);
    const auto tick = session.clock().tick();
    if (!session.log_follower() && tick != 0 && dt > 0.0F) {
      session.commands().log().append(
          tick, Game::Command::Source::LocalPlayer, owner, capture_input(commander_id));
    }
    return true;
  }
  if (!m_runtime->advance(
          world, commander_id, owner, capture_input(commander_id), dt)) {
    return false;
  }
  publish_runtime_tick(world, commander_id, dt);
  if (camera != nullptr) {
    update_camera_presentation(world, commander_id, *camera, dt);
  }
  return true;
}

void CommanderControlController::publish_runtime_tick(
    Engine::Core::World& world, Engine::Core::EntityID commander_id, float dt) {
  auto* commander = world.get_entity(commander_id);
  auto* transform = world.try_get<Engine::Core::TransformComponent>(commander_id);
  if (commander == nullptr || transform == nullptr) {
    return;
  }
  set_view_yaw(m_runtime->view_yaw());
  set_view_pitch(m_runtime->view_pitch());
  if (const auto kick = m_runtime->take_impact_kick(); kick != 0.0F) {
    m_camera_rig.add_impact_kick(kick);
  }
  m_presentation.publish_sample(*commander, *transform, dt);
  if (m_trace_enabled) {
    m_trace = m_runtime->trace();
    m_trace.input.frame_index = m_look.frame_intent().frame_index;
    const auto edges = input_edges();
    m_trace.input.primary_press_sequence = edges.primary_press_sequence;
    m_trace.input.primary_release_sequence = edges.primary_release_sequence;
    m_trace.input.guard_press_sequence = edges.guard_press_sequence;
    m_trace.input.guard_release_sequence = edges.guard_release_sequence;
    m_trace.input.dodge_request_sequence = edges.dodge_request_sequence;
    m_trace.input.jump_request_sequence = edges.jump_request_sequence;
  }
}

auto CommanderControlController::controlled_commander(
    Engine::Core::World& world,
    Engine::Core::EntityID commander_id,
    int owner) const -> Engine::Core::Entity* {
  return Game::Commander::CommanderRuntime::controlled_commander(
      world, commander_id, owner);
}

auto CommanderControlController::find_primary_target(
    Engine::Core::World& world,
    Engine::Core::EntityID commander_id,
    int owner,
    float extra_reach) -> Engine::Core::EntityID {
  if (m_match_runtime) {
    return m_runtime->targeting().soft_id();
  }
  m_runtime->set_view(view_yaw(), view_pitch());
  return m_runtime->find_primary_target(world, commander_id, owner, extra_reach);
}

auto CommanderControlController::locked_target_id() const -> Engine::Core::EntityID {
  return m_runtime->targeting().locked_id();
}

auto CommanderControlController::focus_target_id() const -> Engine::Core::EntityID {
  return locked_target_id();
}

void CommanderControlController::note_refusal(const char* action) {
  m_runtime->note_refusal(action);
}

void CommanderControlController::toggle_close_camera_mode(
    Engine::Core::World& world,
    Engine::Core::EntityID commander_id,
    int local_owner_id) const {
  if (!m_match_runtime) {
    m_runtime->toggle_close_camera_mode(world, commander_id, local_owner_id);
  }
}

void CommanderControlController::toggle_weapon_stance(
    Engine::Core::World& world,
    Engine::Core::EntityID commander_id,
    int local_owner_id) {
  if (!m_match_runtime) {
    m_runtime->toggle_weapon_stance(world, commander_id, local_owner_id);
  }
}

void CommanderControlController::cycle_lock_on_target(
    Engine::Core::World& world,
    Engine::Core::EntityID commander_id,
    int local_owner_id) {
  if (!m_match_runtime) {
    m_runtime->set_view(view_yaw(), view_pitch());
    m_runtime->cycle_lock_on_target(world, commander_id, local_owner_id);
  }
}

void CommanderControlController::toggle_lock_on_target(
    Engine::Core::World& world,
    Engine::Core::EntityID commander_id,
    int local_owner_id) {
  if (!m_match_runtime) {
    m_runtime->set_view(view_yaw(), view_pitch());
    m_runtime->toggle_lock_on_target(world, commander_id, local_owner_id);
  }
}

void CommanderControlController::switch_lock_on_target(
    Engine::Core::World& world,
    Engine::Core::EntityID commander_id,
    int local_owner_id,
    int direction) {
  if (!m_match_runtime) {
    m_runtime->switch_lock_on_target(world, commander_id, local_owner_id, direction);
  }
}
