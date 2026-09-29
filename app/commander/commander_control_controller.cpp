#include "app/commander/commander_control_controller.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <utility>

#include "app/commander/commander_entity_access.h"
#include "app/commander/commander_heading.h"
#include "app/commander/commander_tick_trace.h"
#include "game/audio/audio_cues.h"
#include "game/core/component.h"
#include "game/core/simulation_timing.h"
#include "game/core/world.h"
#include "game/systems/combat_actions/combat_action_definition.h"
#include "game/systems/combat_actions/melee_intent_solver.h"
#include "game/systems/rpg_combat_system/rpg_bow_aim.h"
#include "game/systems/rpg_combat_system/rpg_commander_damage.h"
#include "scene/camera.h"

namespace {

constexpr float k_fov_hip = 68.0F;
constexpr float k_turn_speed_degrees = 105.0F;

} // namespace

void CommanderControlController::set_latency_probe(
    App::Core::CommanderLatencyProbe* probe) {
  m_latency_probe = probe;
  m_input_port.set_latency_probe(probe);
  m_look.set_latency_probe(probe);
}

void CommanderControlController::snap_presentation_pose() {
  m_presentation.snap();
}

void CommanderControlController::release_all_input() {
  m_input_port.release_all();
  m_locomotion.release_input();
  m_strike.release_input();
  m_look.release_mouse();
}

void CommanderControlController::reset() {
  release_all_input();
  snap_presentation_pose();
  m_body_facing.reset();
  m_look.reset();
  m_camera_rig.reset();
  m_locomotion.reset();
  m_targeting.reset();
  m_defence.reset();
  m_strike.reset();
  m_abilities.reset();
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

void CommanderControlController::toggle_close_camera_mode(
    Engine::Core::World& world,
    Engine::Core::EntityID commander_id,
    int local_owner_id) const {
  auto* commander = controlled_commander(world, commander_id, local_owner_id);
  if (commander == nullptr) {
    return;
  }
  if (auto* cmd = commander->get_component<Engine::Core::CommanderComponent>()) {
    cmd->close_camera_mode = !cmd->close_camera_mode;
  }
}

void CommanderControlController::toggle_weapon_stance(
    Engine::Core::World& world,
    Engine::Core::EntityID commander_id,
    int local_owner_id) {
  auto* commander = controlled_commander(world, commander_id, local_owner_id);
  if (commander == nullptr) {
    return;
  }
  if (Game::Systems::RpgCombat::toggle_weapon_stance(*commander)) {
    if (auto* intents = Engine::Core::get_or_add_component<
            Engine::Core::CombatIntentQueueComponent>(commander)) {
      Engine::Core::CombatActionIntent transition;
      transition.type = Engine::Core::CommanderCombatIntentType::WeaponSwitch;
      transition.pressed_at = intents->clock;
      intents->push(transition);
    }
    Game::Audio::play_cue(Game::Audio::Cue::k_combat_guard_raise);
  } else {
    Game::Audio::play_cue(Game::Audio::Cue::k_combat_ability_refused);
  }
}

auto CommanderControlController::locked_target_id() const -> Engine::Core::EntityID {
  return m_targeting.locked_id();
}

auto CommanderControlController::focus_target_id() const -> Engine::Core::EntityID {
  return m_targeting.locked_id();
}

void CommanderControlController::cycle_lock_on_target(
    Engine::Core::World& world,
    Engine::Core::EntityID commander_id,
    int local_owner_id) {
  m_targeting.cycle_lock(world, commander_id, local_owner_id, m_look.yaw());
}

auto CommanderControlController::controlled_commander(
    Engine::Core::World& world,
    Engine::Core::EntityID commander_id,
    int local_owner_id) const -> Engine::Core::Entity* {
  return App::Core::controlled_commander(world, commander_id, local_owner_id);
}

auto CommanderControlController::find_primary_target(
    Engine::Core::World& world,
    Engine::Core::EntityID commander_id,
    int local_owner_id,
    float extra_reach) -> Engine::Core::EntityID {
  return m_targeting.find_primary_target(
      world, commander_id, local_owner_id, m_look.yaw(), extra_reach);
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

void CommanderControlController::capture_input(Engine::Core::World& world,
                                               Engine::Core::EntityID commander_id) {
  m_input_port.capture_tick();
  if (auto const replayed_yaw =
          m_input_port.exchange_recorded(world, commander_id, m_look.yaw());
      replayed_yaw.has_value()) {
    m_look.set_yaw(*replayed_yaw);
  }
}

auto CommanderControlController::hold_for_rally(Engine::Core::World& world,
                                                const App::Core::CommanderHandles& body,
                                                Render::GL::Camera* camera,
                                                float dt) -> bool {
  m_abilities.advance_cooldowns(body.commander_data, dt);
  body.commander_data->fpv_motion_vx = 0.0F;
  body.commander_data->fpv_motion_vz = 0.0F;
  body.commander_data->fpv_motion_requested = false;

  m_input_port.release_for_rally();
  m_locomotion.hold_for_rally();
  m_look.set_yaw(App::Core::wrap_angle_degrees(body.transform.rotation.y));

  if (body.movement != nullptr) {
    body.movement->set_manual_velocity(0.0F, 0.0F);
  }
  m_defence.hold_for_rally(body.entity, dt);

  m_presentation.publish_sample(body.entity, body.transform, dt);
  if (camera != nullptr) {
    update_camera(world, body.entity, *camera, dt);
  }
  return true;
}

void CommanderControlController::steer_view(Engine::Core::World& world,
                                            Engine::Core::Entity& commander,
                                            float dt) {
  auto const& tick = m_input_port.tick();
  m_look.set_yaw(
      m_targeting.steer_view_toward_lock(world,
                                         commander,
                                         {.view_yaw = m_look.yaw(),
                                          .run = tick.run,
                                          .backward = tick.backward,
                                          .dodge_pressed = tick.dodge_pressed,
                                          .dt = dt}));

  if (m_targeting.locked_id() == 0) {
    if (tick.turn_left) {
      m_look.set_yaw(m_look.yaw() - k_turn_speed_degrees * dt);
    }
    if (tick.turn_right) {
      m_look.set_yaw(m_look.yaw() + k_turn_speed_degrees * dt);
    }
  }
  m_look.set_yaw(App::Core::wrap_angle_degrees(m_look.yaw()));
}

auto CommanderControlController::advance_jump_stage(
    const App::Core::CommanderHandles& body, float dt) -> bool {
  auto const& tick = m_input_port.tick();
  auto const jump = m_locomotion.advance_jump(
      body,
      {.jump_pressed = tick.jump_pressed,
       .primary_held = tick.primary_held,
       .guard_held = tick.guard_held,
       .ability_requested = tick.shield_bash_pressed || tick.vanguard_rush_pressed ||
                            tick.second_wind_pressed,
       .dt = dt});
  m_input_port.record_jump(jump.edge);
  m_input_port.clear_jump_edge();
  return jump.active;
}

auto CommanderControlController::start_dodge_stage(
    Engine::Core::World& world,
    const App::Core::CommanderHandles& body,
    const MotionOutcome& motion) -> App::Core::MoveBasis {
  auto const& tick = m_input_port.tick();
  auto const basis =
      App::Core::CommanderLocomotion::move_basis(m_look.yaw(),
                                                 motion.forward_axis,
                                                 motion.right_axis,
                                                 body.transform,
                                                 m_targeting.locked_position(world));
  m_input_port.record_dodge(
      m_locomotion.try_start_dodge(body,
                                   {.pressed = tick.dodge_pressed,
                                    .has_direction = tick.has_dodge_direction,
                                    .direction = tick.dodge_direction,
                                    .move = basis.move,
                                    .forward = basis.forward},
                                   m_latency_probe));
  m_input_port.clear_dodge_edge();
  return basis;
}

auto CommanderControlController::advance_motion(Engine::Core::World& world,
                                                const App::Core::CommanderHandles& body,
                                                const TickFacts& facts,
                                                float dt) -> MotionOutcome {
  auto const& tick = m_input_port.tick();
  MotionOutcome out;
  out.forward_axis = tick.forward_axis();
  out.right_axis = tick.right_axis();
  out.previous_position = QVector3D(
      body.transform.position.x, body.transform.position.y, body.transform.position.z);

  m_locomotion.decay_dodge_kick(dt);
  out.jump_active = advance_jump_stage(body, dt);
  m_defence.advance_upkeep(body.entity, body.commander_data, tick.guard_held, dt);
  auto const basis = start_dodge_stage(world, body, out);

  out.report = m_locomotion.advance_motion(body,
                                           m_motor,
                                           {.move = basis.move,
                                            .forward_axis = out.forward_axis,
                                            .right_axis = out.right_axis,
                                            .run_held = tick.run,
                                            .drawing_bow = facts.drawing_bow,
                                            .jump_active = out.jump_active,
                                            .dt = dt});
  m_locomotion.recover_from_fall(body, m_motor, out.jump_active, out.report);

  out.report.lunge_distance =
      m_lunge.apply(world,
                    body.entity,
                    body.transform,
                    m_motor,
                    m_locomotion.dodge_state() != DodgeState::None,
                    body.commander_data != nullptr && body.commander_data->jump_active,
                    dt);
  if (out.report.lunge_distance > 1.0e-5F) {
    out.report.source = App::Core::CommanderDisplacementSource::StrikeLunge;
  }
  m_locomotion.commit_motion(out.report, out.forward_axis, out.right_axis, dt);
  return out;
}

void CommanderControlController::face_body(const App::Core::CommanderHandles& body,
                                           const TickFacts& facts,
                                           const MotionOutcome& motion,
                                           float dt) {
  auto const& tick = m_input_port.tick();
  float action_redirect_authority = 1.0F;
  if (facts.attack_animation_active) {
    if (auto const* definition =
            Game::Systems::CombatActions::find_combat_action_definition(
                static_cast<Game::Systems::CombatActions::CombatActionId>(
                    facts.active_action->combat_action_id))) {
      action_redirect_authority =
          Game::Systems::CombatActions::melee_interruption_at(
              *definition, facts.active_action->normalized_action_time)
              .redirect_authority;
    }
  }

  bool const must_face_view = tick.primary_held || tick.guard_held ||
                              m_locomotion.dodge_state() != DodgeState::None ||
                              motion.jump_active || facts.drawing_bow ||
                              m_targeting.locked_id() != 0 ||
                              facts.attack_animation_active;

  float const body_yaw =
      m_body_facing.advance({.view_yaw = m_look.yaw(),
                             .attack_animation_active = facts.attack_animation_active,
                             .action_redirect_authority = action_redirect_authority,
                             .must_face_view = must_face_view,
                             .follows_travel = m_locomotion.follows_travel(),
                             .dt = dt});
  body.transform.rotation.y = body_yaw;
  body.transform.desired_yaw = body_yaw;
  body.transform.has_desired_yaw = true;
}

auto CommanderControlController::sync_direct_control(
    const App::Core::CommanderHandles& body,
    const MotionOutcome& motion) -> DirectControlSync {
  DirectControlSync sync;
  sync.aim = Game::Systems::RpgCombat::sync_commander_aim(
      body.entity,
      {.view_yaw_degrees = m_look.yaw(),
       .view_pitch_degrees = m_look.pitch(),
       .move_speed = m_locomotion.move_speed(),
       .running = m_locomotion.move_running(),
       .primary_held = m_input_port.tick().primary_held,
       .camera_origin = m_camera_rig.eye(),
       .camera_origin_valid = m_camera_rig.eye_valid(),
       .camera_forward = m_camera_rig.forward(),
       .camera_forward_valid = m_camera_rig.forward_valid(),
       .camera_fov_degrees = m_camera_rig.fov()});
  m_locomotion.publish_fpv_motion(body, motion.report.requested_speed);
  sync.traced_stamina = m_locomotion.engage_manual_motion(body, m_latency_probe);
  return sync;
}

void CommanderControlController::activate_abilities(
    Engine::Core::World& world,
    const App::Core::CommanderHandles& body,
    Engine::Core::EntityID commander_id,
    int local_owner_id,
    float dt) {
  auto const& tick = m_input_port.tick();
  App::Core::CommanderAbilityRequest request;
  request.shield_bash = tick.shield_bash_pressed;
  request.vanguard_rush = tick.vanguard_rush_pressed;
  request.second_wind = tick.second_wind_pressed;
  m_input_port.clear_ability_edges();

  m_abilities.advance_cooldowns(body.commander_data, dt);
  if (!request.any()) {
    return;
  }
  App::Core::CommanderAbilityContext context;
  context.world = &world;
  context.commander = &body.entity;
  context.commander_id = commander_id;
  context.local_owner_id = local_owner_id;
  context.view_yaw = m_look.yaw();
  context.dodging = m_locomotion.dodge_state() != DodgeState::None;
  context.airborne = m_locomotion.jump_airborne();
  context.locked_target_id = m_targeting.locked_id();
  context.soft_target_id = m_targeting.soft_id();
  context.motor = &m_motor;
  static_cast<void>(m_abilities.activate(request, context));
}

void CommanderControlController::queue_pressed_intents(
    const App::Core::CommanderHandles& body,
    Engine::Core::CombatIntentQueueComponent& intents,
    const Engine::Core::RpgCommanderAimComponent* aim) {
  auto const& tick = m_input_port.tick();
  auto const* guard =
      body.entity.get_component<Engine::Core::CommanderGuardComponent>();
  bool const primary_was_pressed = tick.primary_pressed;
  bool const queued = m_strike.queue_intents(
      body.entity,
      intents,
      tick,
      {.dodge_clear = m_locomotion.dodge_state() == DodgeState::None,
       .guarding = guard != nullptr && guard->active,
       .bow_stance =
           aim != nullptr && aim->stance == Engine::Core::FpvWeaponStance::Bow,
       .jump_followup_pending = m_locomotion.jump_followup_pending()});
  if (queued) {
    m_input_port.consume_press_edges(primary_was_pressed);
    m_locomotion.clear_jump_followup();
  }
}

auto CommanderControlController::dispatch_front_intent(
    Engine::Core::World& world,
    const App::Core::CommanderHandles& body,
    Engine::Core::CombatIntentQueueComponent& intents,
    Engine::Core::EntityID commander_id,
    int local_owner_id) -> bool {
  return m_strike.dispatch_pending(
      {.world = world,
       .commander_id = commander_id,
       .local_owner_id = local_owner_id,
       .view_yaw = m_look.yaw(),
       .move_right_axis = m_locomotion.move_right_axis(),
       .move_forward_axis = m_locomotion.move_forward_axis(),
       .commander_position = QVector3D(body.transform.position.x,
                                       body.transform.position.y,
                                       body.transform.position.z)},
      m_targeting,
      body.entity,
      intents,
      {.feedback = m_feedback, .probe = m_latency_probe});
}

void CommanderControlController::advance_swing(const App::Core::CommanderHandles& body,
                                               float dt) {
  auto const& tick = m_input_port.tick();
  m_strike.advance_melee(body.entity,
                         {.aim_delta_x = App::Core::signed_angle_delta(
                              m_look.yaw(), m_look.previous_yaw()),
                          .aim_delta_y = m_look.pitch() - m_look.previous_pitch(),
                          .view_pitch = m_look.pitch(),
                          .move_right_axis = m_locomotion.move_right_axis(),
                          .move_forward_axis = m_locomotion.move_forward_axis(),
                          .primary_held = tick.primary_held,
                          .guard_held = tick.guard_held,
                          .dt = dt});
  m_look.remember_previous_view();

  if (auto const kick = m_strike.observe_hits(body.entity); kick.has_value()) {
    m_camera_rig.add_impact_kick(*kick);
  }
}

auto CommanderControlController::advance_strike(
    Engine::Core::World& world,
    const App::Core::CommanderHandles& body,
    const TickFacts& facts,
    Engine::Core::RpgCommanderAimComponent* aim,
    Engine::Core::EntityID commander_id,
    int local_owner_id,
    float dt) -> std::optional<QueueOutcome> {
  m_strike.advance_hold_timers(body.commander_data,
                               m_input_port.tick().primary_held,
                               facts.attack_animation_active,
                               dt);

  QueueOutcome queue;
  queue.intents = App::Core::CommanderStrike::open_queue(body.entity, dt);
  queue.dodge_grace_remaining =
      App::Core::CommanderDefence::advance_dodge_grace(body.entity, dt);

  bool const waiting_for_release = aim != nullptr && aim->relaxed_from_overhold;
  bool const body_can_take_input =
      m_locomotion.dodge_state() != DodgeState::Rolling && !waiting_for_release;

  if (queue.intents != nullptr && body_can_take_input) {
    queue_pressed_intents(body, *queue.intents, aim);
  }
  m_input_port.settle_primary_press();

  if (queue.intents != nullptr && !queue.intents->empty() && body_can_take_input &&
      !dispatch_front_intent(
          world, body, *queue.intents, commander_id, local_owner_id)) {
    return std::nullopt;
  }

  advance_swing(body, dt);
  return queue;
}

void CommanderControlController::advance_targeting(
    Engine::Core::World& world,
    const App::Core::CommanderHandles& body,
    Engine::Core::RpgCommanderAimComponent* aim,
    Engine::Core::EntityID commander_id,
    int local_owner_id,
    float dt) {
  auto const aim_candidate_id = m_targeting.resolve_aim_candidate(
      world, body.entity, commander_id, local_owner_id, aim, m_look.yaw());

  m_defence.publish_resolved_feedback(
      body.entity, commander_id, body.transform, m_feedback);

  if (auto const kick = m_targeting.publish_targets(body.entity, aim_candidate_id, dt);
      kick.has_value()) {
    m_camera_rig.add_impact_kick(*kick);
  }
}

void CommanderControlController::record_trace(const App::Core::CommanderHandles& body,
                                              const TickFacts& facts,
                                              const MotionOutcome& motion,
                                              const QueueOutcome& queue,
                                              float traced_stamina,
                                              float dt) {
  auto const& tick = m_input_port.tick();
  App::Core::record_tick_trace(
      m_trace,
      body.entity,
      dt,
      {.edges = m_input_port.edges(),
       .frame_index = m_look.frame_intent().frame_index,
       .forward_axis = motion.forward_axis,
       .right_axis = motion.right_axis,
       .run_held = tick.run,
       .primary_held = tick.primary_held,
       .guard_held = tick.guard_held,
       .primary_held_duration = m_strike.primary_held_duration(),
       .view_yaw = m_look.yaw(),
       .view_pitch = m_look.pitch(),
       .previous_view_yaw = m_look.previous_yaw(),
       .previous_view_pitch = m_look.previous_pitch()},
      {.transform = body.transform,
       .direct_control =
           body.commander_data != nullptr && body.commander_data->fpv_controlled,
       .report = motion.report,
       .previous_position = motion.previous_position,
       .smoothed_speed = m_locomotion.planar_speed(),
       .grounded = !motion.jump_active,
       .dt = dt,
       .pose = m_presentation.pose(),
       .camera = m_camera_rig.trace()},
      {.action = facts.active_action,
       .intents = queue.intents,
       .dodge_state = m_locomotion.dodge_state(),
       .dodge_timer = m_locomotion.dodge_timer(),
       .dodge_grace_remaining = queue.dodge_grace_remaining,
       .locked_target_id = m_targeting.locked_id(),
       .locked_target_slot = m_targeting.locked_slot(),
       .soft_target_id = m_targeting.soft_id(),
       .soft_target_slot = m_targeting.soft_slot(),
       .hit_confirm_sequence = m_targeting.observed_hit_confirm_sequence(),
       .stamina = traced_stamina,
       .health = body.unit.health});
}

auto CommanderControlController::resolve_handles(
    Engine::Core::Entity& commander,
    Engine::Core::TransformComponent& transform,
    Engine::Core::UnitComponent& unit) -> App::Core::CommanderHandles {
  auto* movement = commander.get_component<Engine::Core::MovementComponent>();
  if (movement == nullptr) {
    movement = commander.add_component<Engine::Core::MovementComponent>();
  }
  return {commander,
          transform,
          unit,
          commander.get_component<Engine::Core::CommanderComponent>(),
          movement};
}

auto CommanderControlController::read_tick_facts(const Engine::Core::Entity& commander)
    -> TickFacts {
  auto const* aim_state =
      commander.get_component<Engine::Core::RpgCommanderAimComponent>();
  auto const* active_action =
      commander.get_component<Engine::Core::RpgCommanderActionComponent>();
  return {.active_action = active_action,
          .attack_animation_active =
              active_action != nullptr && active_action->action_running,
          .drawing_bow = aim_state != nullptr && aim_state->is_drawing()};
}

void CommanderControlController::present_tick(Engine::Core::World& world,
                                              const App::Core::CommanderHandles& body,
                                              Render::GL::Camera* camera,
                                              float dt) {
  m_presentation.publish_sample(body.entity, body.transform, dt);
  m_look.set_pitch(m_look.pitch());
  if (camera != nullptr) {
    update_camera(world, body.entity, *camera, dt);
  }
}

auto CommanderControlController::advance_combat(Engine::Core::World& world,
                                                const App::Core::CommanderHandles& body,
                                                const TickFacts& facts,
                                                const MotionOutcome& motion,
                                                Engine::Core::EntityID commander_id,
                                                int local_owner_id,
                                                float dt) -> bool {
  auto const sync = sync_direct_control(body, motion);
  m_defence.apply_guard(body.entity,
                        {.guard_held = m_input_port.tick().guard_held,
                         .dodging = m_locomotion.dodge_state() != DodgeState::None,
                         .airborne = motion.jump_active,
                         .dt = dt},
                        m_latency_probe);
  activate_abilities(world, body, commander_id, local_owner_id, dt);

  auto const queue =
      advance_strike(world, body, facts, sync.aim, commander_id, local_owner_id, dt);
  if (!queue.has_value()) {
    return false;
  }
  advance_targeting(world, body, sync.aim, commander_id, local_owner_id, dt);
  if (m_latency_probe != nullptr) {
    m_latency_probe->note_simulation_response();
  }
  if (m_trace_enabled) {
    record_trace(body, facts, motion, *queue, sync.traced_stamina, dt);
  }
  return true;
}

auto CommanderControlController::update_impl(Engine::Core::World& world,
                                             Engine::Core::EntityID commander_id,
                                             int local_owner_id,
                                             Render::GL::Camera* camera,
                                             float dt) -> bool {
  auto* commander =
      App::Core::controlled_commander(world, commander_id, local_owner_id);
  if (commander == nullptr) {
    return false;
  }
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  auto* unit = commander->get_component<Engine::Core::UnitComponent>();
  if (transform == nullptr || unit == nullptr) {
    return false;
  }

  capture_input(world, commander_id);
  auto const body = resolve_handles(*commander, *transform, *unit);
  auto const facts = read_tick_facts(*commander);

  if (body.commander_data != nullptr && body.commander_data->flag_rally_in_progress &&
      !body.commander_data->fpv_controlled) {
    return hold_for_rally(world, body, camera, dt);
  }

  if (body.movement != nullptr) {
    body.movement->stop();
    body.movement->set_rest_position(transform->position.x, transform->position.z);
  }

  steer_view(world, *commander, dt);
  auto const motion = advance_motion(world, body, facts, dt);
  face_body(body, facts, motion, dt);
  present_tick(world, body, camera, dt);
  return advance_combat(world, body, facts, motion, commander_id, local_owner_id, dt);
}

void CommanderControlController::update_camera(Engine::Core::World& world,
                                               Engine::Core::Entity& commander,
                                               Render::GL::Camera& camera,
                                               float dt) {
  auto* transform = commander.get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return;
  }

  m_look.set_pitch(m_look.pitch());

  App::Core::CameraFeed feed;
  feed.dt = dt;
  feed.view_yaw = m_look.yaw();
  feed.view_pitch = m_look.pitch();
  feed.move_speed = m_locomotion.move_speed();
  feed.move_right_axis = m_locomotion.move_right_axis();
  feed.move_running = m_locomotion.move_running();
  feed.dodge_fov_kick = m_locomotion.dodge_fov_kick();
  feed.dodge_rolling = m_locomotion.dodge_state() == DodgeState::Rolling;
  feed.dodge_timer = m_locomotion.dodge_timer();
  feed.dodge_direction = m_locomotion.dodge_direction();
  feed.lock_target_position = m_targeting.locked_position(world);

  auto const pose = m_presentation.advance_pose(commander, *transform, dt);
  auto const inputs = App::Core::build_camera_inputs(world, commander, feed, pose);

  float const previous_bob_phase = m_camera_rig.update(camera, inputs);
  if (m_latency_probe != nullptr) {
    m_latency_probe->note_camera_response();
  }
  App::Core::play_footstep_if_stride_landed(
      m_camera_rig, commander, m_locomotion.move_running(), previous_bob_phase);
}
