#include "arena_scenario_internal.h"

namespace Arena {

using namespace scenario_internal;

namespace {

auto json_vector(const QVector3D& value) -> QJsonArray {
  return {value.x(), value.y(), value.z()};
}

auto framing_state_name(App::Core::CommanderFramingState state) -> QString {
  switch (state) {
  case App::Core::CommanderFramingState::Explore:
    return QStringLiteral("explore");
  case App::Core::CommanderFramingState::Melee:
    return QStringLiteral("melee");
  case App::Core::CommanderFramingState::DuelLock:
    return QStringLiteral("duel_lock");
  case App::Core::CommanderFramingState::BowAim:
    return QStringLiteral("bow_aim");
  }
  return QStringLiteral("unknown");
}

auto commander_trace_json(const App::Core::CommanderPresentationTrace& trace)
    -> QJsonObject {
  auto const& input = trace.input;
  auto const& motor = trace.motor;
  auto const& camera = trace.camera;
  auto const& combat = trace.combat;

  QJsonObject input_json{
      {QStringLiteral("frame_index"), static_cast<qint64>(input.frame_index)},
      {QStringLiteral("primary_press_sequence"),
       static_cast<qint64>(input.primary_press_sequence)},
      {QStringLiteral("primary_release_sequence"),
       static_cast<qint64>(input.primary_release_sequence)},
      {QStringLiteral("primary_consumed_sequence"),
       static_cast<qint64>(input.primary_consumed_sequence)},
      {QStringLiteral("primary_dropped_sequence"),
       static_cast<qint64>(input.primary_dropped_sequence)},
      {QStringLiteral("guard_press_sequence"),
       static_cast<qint64>(input.guard_press_sequence)},
      {QStringLiteral("guard_release_sequence"),
       static_cast<qint64>(input.guard_release_sequence)},
      {QStringLiteral("dodge_request_sequence"),
       static_cast<qint64>(input.dodge_request_sequence)},
      {QStringLiteral("dodge_consumed_sequence"),
       static_cast<qint64>(input.dodge_consumed_sequence)},
      {QStringLiteral("dodge_refused_sequence"),
       static_cast<qint64>(input.dodge_refused_sequence)},
      {QStringLiteral("jump_request_sequence"),
       static_cast<qint64>(input.jump_request_sequence)},
      {QStringLiteral("jump_consumed_sequence"),
       static_cast<qint64>(input.jump_consumed_sequence)},
      {QStringLiteral("jump_refused_sequence"),
       static_cast<qint64>(input.jump_refused_sequence)},
      {QStringLiteral("move_axes"),
       QJsonArray{input.move_forward_axis, input.move_right_axis}},
      {QStringLiteral("run_held"), input.run_held},
      {QStringLiteral("primary_held"), input.primary_held},
      {QStringLiteral("guard_held"), input.guard_held},
      {QStringLiteral("primary_held_duration"), input.primary_held_duration},
      {QStringLiteral("look_delta"),
       QJsonArray{input.look_delta_yaw, input.look_delta_pitch}},
      {QStringLiteral("view_yaw"), input.view_yaw},
      {QStringLiteral("view_pitch"), input.view_pitch}};

  QJsonObject motor_json{
      {QStringLiteral("previous_position"), json_vector(motor.previous_position)},
      {QStringLiteral("position"), json_vector(motor.position)},
      {QStringLiteral("desired_velocity"), json_vector(motor.desired_velocity)},
      {QStringLiteral("actual_velocity"), json_vector(motor.actual_velocity)},
      {QStringLiteral("requested_speed"), motor.requested_speed},
      {QStringLiteral("smoothed_speed"), motor.smoothed_speed},
      {QStringLiteral("speed_error"), motor.speed_error},
      {QStringLiteral("grounded"), motor.grounded},
      {QStringLiteral("blocked"), motor.blocked},
      {QStringLiteral("slid"), motor.slid},
      {QStringLiteral("separation_push"), motor.separation_push},
      {QStringLiteral("movement_mode"),
       QString::fromLatin1(App::Core::movement_mode_name(motor.movement_mode))},
      {QStringLiteral("steering_source"), QString::fromLatin1(motor.steering_source)},
      {QStringLiteral("static_walkable"), motor.static_walkable},
      {QStringLiteral("dynamic_push"), json_vector(motor.dynamic_push)},
      {QStringLiteral("dynamic_neighbors"),
       static_cast<qint64>(motor.dynamic_neighbors)},
      {QStringLiteral("dynamic_overlap"), motor.dynamic_overlap},
      {QStringLiteral("accepted_displacement"), motor.accepted_displacement},
      {QStringLiteral("lunge_distance"), motor.lunge_distance},
      {QStringLiteral("snap_back_distance"), motor.snap_back_distance},
      {QStringLiteral("displacement_source"),
       QString::fromLatin1(
           App::Core::displacement_source_name(motor.displacement_source))},
      {QStringLiteral("dt"), motor.dt},
      {QStringLiteral("presented_position"), json_vector(motor.presented_position)},
      {QStringLiteral("presented_yaw"), motor.presented_yaw},
      {QStringLiteral("presentation_alpha"), motor.presentation_alpha},
      {QStringLiteral("presentation_extrapolated"), motor.presentation_extrapolated}};

  QJsonObject camera_json{
      {QStringLiteral("valid"), camera.valid},
      {QStringLiteral("commander_position"), json_vector(camera.commander_position)},
      {QStringLiteral("visual_anchor"), json_vector(camera.visual_anchor)},
      {QStringLiteral("anchor_lag"), camera.anchor_lag},
      {QStringLiteral("pivot"), json_vector(camera.pivot)},
      {QStringLiteral("eye_unconstrained"), json_vector(camera.eye_unconstrained)},
      {QStringLiteral("target_unconstrained"),
       json_vector(camera.target_unconstrained)},
      {QStringLiteral("eye_resolved"), json_vector(camera.eye_resolved)},
      {QStringLiteral("target_resolved"), json_vector(camera.target_resolved)},
      {QStringLiteral("boom_unconstrained"), camera.boom_unconstrained},
      {QStringLiteral("boom_resolved"), camera.boom_resolved},
      {QStringLiteral("boom_clear_fraction"), camera.boom_clear_fraction},
      {QStringLiteral("terrain_clear_fraction"), camera.terrain_clear_fraction},
      {QStringLiteral("sight_line_clear"), camera.sight_line_clear},
      {QStringLiteral("occlusion_fraction"), camera.occlusion_fraction},
      {QStringLiteral("terrain_lift"), camera.terrain_lift},
      {QStringLiteral("eye_clearance"), camera.eye_clearance},
      {QStringLiteral("fov"), camera.fov},
      {QStringLiteral("yaw"), camera.yaw},
      {QStringLiteral("pitch"), camera.pitch},
      {QStringLiteral("yaw_velocity"), camera.yaw_velocity},
      {QStringLiteral("pitch_velocity"), camera.pitch_velocity},
      {QStringLiteral("ground_y"), camera.ground_y},
      {QStringLiteral("framing_state"), framing_state_name(camera.framing_state)},
      {QStringLiteral("framing_changed"), camera.framing_changed},
      {QStringLiteral("dt"), camera.dt}};

  QJsonObject combat_json{
      {QStringLiteral("action_id"), combat.action_id},
      {QStringLiteral("action_phase"), combat.action_phase},
      {QStringLiteral("action_normalized_time"), combat.action_normalized_time},
      {QStringLiteral("action_running"), combat.action_running},
      {QStringLiteral("queued_intents"), combat.queued_intents},
      {QStringLiteral("guard_active"), combat.guard_active},
      {QStringLiteral("perfect_guard_remaining"), combat.perfect_guard_remaining},
      {QStringLiteral("dodge_state"), combat.dodge_state},
      {QStringLiteral("dodge_timer"), combat.dodge_timer},
      {QStringLiteral("dodge_grace_remaining"), combat.dodge_grace_remaining},
      {QStringLiteral("locked_target_id"),
       static_cast<qint64>(combat.locked_target_id)},
      {QStringLiteral("locked_target_slot"), combat.locked_target_slot},
      {QStringLiteral("soft_target_id"), static_cast<qint64>(combat.soft_target_id)},
      {QStringLiteral("soft_target_slot"), combat.soft_target_slot},
      {QStringLiteral("hit_confirm_sequence"),
       static_cast<qint64>(combat.hit_confirm_sequence)},
      {QStringLiteral("action_hit_count"), combat.action_hit_count},
      {QStringLiteral("health"), combat.health},
      {QStringLiteral("stamina"), combat.stamina},
      {QStringLiteral("queue_outcome"),
       QString::fromLatin1(
           App::Core::combat_intent_outcome_name(combat.queue_outcome))},
      {QStringLiteral("queue_outcome_age"), combat.queue_outcome_age},
      {QStringLiteral("queue_accepted"), static_cast<qint64>(combat.queue_accepted)},
      {QStringLiteral("queue_buffered"), static_cast<qint64>(combat.queue_buffered)},
      {QStringLiteral("queue_refused"), static_cast<qint64>(combat.queue_refused)},
      {QStringLiteral("queue_expired"), static_cast<qint64>(combat.queue_expired)},
      {QStringLiteral("queue_overflow"), static_cast<qint64>(combat.queue_overflow)},
      {QStringLiteral("action_window_start"), combat.action_window_start},
      {QStringLiteral("action_window_end"), combat.action_window_end},
      {QStringLiteral("blocked_contacts"),
       static_cast<qint64>(combat.blocked_contacts)},
      {QStringLiteral("perfect_guard_contacts"),
       static_cast<qint64>(combat.perfect_guard_contacts)},
      {QStringLiteral("dodged_contacts"), static_cast<qint64>(combat.dodged_contacts)},
      {QStringLiteral("damaging_contacts"),
       static_cast<qint64>(combat.damaging_contacts)},
      {QStringLiteral("guard_broken_contacts"),
       static_cast<qint64>(combat.guard_broken_contacts)}};

  QJsonObject const costs_json{
      {QStringLiteral("motor_ms"), trace.costs.motor_ms},
      {QStringLiteral("targeting_ms"), trace.costs.targeting_ms},
      {QStringLiteral("weapon_trace_ms"), trace.costs.weapon_trace_ms},
      {QStringLiteral("engagement_ms"), trace.costs.engagement_ms},
      {QStringLiteral("camera_ms"), trace.costs.camera_ms},
      {QStringLiteral("total_ms"), trace.costs.total_ms()}};

  return QJsonObject{{QStringLiteral("sequence"), static_cast<qint64>(trace.sequence)},
                     {QStringLiteral("time_seconds"), trace.time_seconds},
                     {QStringLiteral("input"), input_json},
                     {QStringLiteral("motor"), motor_json},
                     {QStringLiteral("camera"), camera_json},
                     {QStringLiteral("combat"), combat_json},
                     {QStringLiteral("costs"), costs_json}};
}

auto battle_summary(const ArenaBattleOutcome& battle) -> QString {
  if (!battle.tracked || battle.sides.empty()) {
    return {};
  }
  QStringList parts;
  for (auto const& side : battle.sides) {
    parts.push_back(
        QStringLiteral("%1[%2] %3u/%4b peak %5 adv %6 atk %7s "
                       "built %8 home %9 fwd %10%11")
            .arg(side.label,
                 side.strategy.isEmpty()
                     ? QStringLiteral("?")
                     : side.strategy + QStringLiteral("/") + side.posture)
            .arg(side.living_units)
            .arg(side.living_buildings)
            .arg(side.peak_units)
            .arg(side.peak_advance, 0, 'f', 2)
            .arg(side.seconds_attacking, 0, 'f', 0)
            .arg(side.buildings_constructed)
            .arg(side.peak_home_units)
            .arg(side.peak_forward_units)
            .arg(side.eliminated_at >= 0.0F
                     ? QStringLiteral(" dead@%1s").arg(side.eliminated_at, 0, 'f', 1)
                     : QString()));
  }
  QString const verdict = battle.decided
                              ? QStringLiteral("victor %1 at %2 s")
                                    .arg(battle.victor_label)
                                    .arg(battle.decided_at_seconds, 0, 'f', 1)
                              : QStringLiteral("undecided");
  return QStringLiteral(" | battle: %1 [%2]")
      .arg(verdict, parts.join(QStringLiteral("; ")));
}

auto movement_summary(const std::vector<ArenaGroupMovementDiagnostics>& rows)
    -> QString {
  if (rows.empty()) {
    return {};
  }
  QStringList parts;
  parts.reserve(static_cast<int>(rows.size()));
  for (auto const& row : rows) {
    QString const objective = row.has_objective ? QStringLiteral("(%1, %2)")
                                                      .arg(row.objective_x, 0, 'f', 1)
                                                      .arg(row.objective_z, 0, 'f', 1)
                                                : QStringLiteral("none");
    parts.push_back(
        QStringLiteral("%1 stall %2 s@%3 s (%4) for %5, repaths %6, "
                       "recoveries %7, abandoned %8, still wedged %9")
            .arg(row.group)
            .arg(row.worst_stalled_seconds, 0, 'f', 1)
            .arg(row.worst_stalled_at, 0, 'f', 1)
            .arg(row.worst_state.isEmpty() ? QStringLiteral("-") : row.worst_state,
                 objective)
            .arg(row.repaths)
            .arg(row.recovery_attempts)
            .arg(row.abandons)
            .arg(row.units_holding_a_stalled_objective));
  }
  return QStringLiteral(" | movement: ") + parts.join(QStringLiteral("; "));
}

} // namespace

auto narrow_layout_summary(const std::vector<ArenaNarrowLayoutOutcome>& outcomes)
    -> QString {
  if (outcomes.empty()) {
    return {};
  }
  QStringList parts;
  for (auto const& outcome : outcomes) {
    parts.push_back(
        QStringLiteral("%1 frontage %2m corridor %3m %4 %5/%6 files at %7m, "
                       "narrowest %8m over %9m deep, reform %10m")
            .arg(outcome.group)
            .arg(outcome.formation_half_width * 2.0F, 0, 'f', 2)
            .arg(outcome.narrowest_corridor_half_width * 2.0F, 0, 'f', 2)
            .arg(outcome.engaged ? outcome.narrowest_mode
                                 : QStringLiteral("Normal(idle)"))
            .arg(outcome.narrowest_files)
            .arg(outcome.normal_files)
            .arg(outcome.tightest_file_spacing, 0, 'f', 2)
            .arg(outcome.narrowest_frontage, 0, 'f', 2)
            .arg(outcome.deepest_column, 0, 'f', 2)
            .arg(outcome.worst_reform_error, 0, 'f', 2));
  }
  return QStringLiteral(" | narrow layout: ") + parts.join(QStringLiteral("; "));
}

auto ArenaScenarioReport::summary() const -> QString {
  if (passed()) {
    QString result = QStringLiteral("PASS %1: %2 frames, %3 s")
                         .arg(scenario_id)
                         .arg(rendered_frames)
                         .arg(elapsed_seconds, 0, 'f', 2);
    if (frame_time_samples > 0U && frame_time_p95_ms > 0.0) {
      result += QStringLiteral(", peak %1 visible soldiers, frame p50/p95/max "
                               "%2/%3/%4 ms (%5 FPS at p95), peak rigged "
                               "%6 commands/%7 instanced instances")
                    .arg(peak_visible_soldiers)
                    .arg(frame_time_p50_ms, 0, 'f', 2)
                    .arg(frame_time_p95_ms, 0, 'f', 2)
                    .arg(frame_time_max_ms, 0, 'f', 2)
                    .arg(1000.0 / frame_time_p95_ms, 0, 'f', 1)
                    .arg(peak_rigged_commands)
                    .arg(peak_rigged_instanced_instances);
    }
    result += battle_summary(battle);
    result += movement_summary(movement);
    result += narrow_layout_summary(narrow_layout);
    return result;
  }
  return QStringLiteral("FAIL %1: %2 issue(s); first: %3%4%5")
      .arg(scenario_id)
      .arg(issues.size())
      .arg(issues.front().message,
           battle_summary(battle),
           movement_summary(movement) + narrow_layout_summary(narrow_layout));
}

auto ArenaScenarioRunner::write_artifacts(const QString& directory,
                                          QString* error) const -> bool {
  QDir const dir;
  if (!dir.mkpath(directory)) {
    if (error != nullptr) {
      *error = QStringLiteral("failed to create artifact directory %1").arg(directory);
    }
    return false;
  }

  QJsonObject report_object;
  report_object.insert(QStringLiteral("scenario"), m_impl->report.scenario_id);
  report_object.insert(QStringLiteral("completed"), m_impl->complete);
  report_object.insert(QStringLiteral("passed"),
                       m_impl->complete && m_impl->report.passed());
  report_object.insert(QStringLiteral("elapsed_seconds"),
                       m_impl->report.elapsed_seconds);
  report_object.insert(QStringLiteral("rendered_frames"),
                       static_cast<qint64>(m_impl->report.rendered_frames));
  report_object.insert(QStringLiteral("rendered_soldier_samples"),
                       static_cast<qint64>(m_impl->report.rendered_soldier_samples));
  if (m_impl->report.battle.tracked) {
    QJsonArray sides;
    for (auto const& side : m_impl->report.battle.sides) {
      sides.append(QJsonObject{
          {QStringLiteral("owner_id"), side.owner_id},
          {QStringLiteral("label"), side.label},
          {QStringLiteral("living_units"), side.living_units},
          {QStringLiteral("living_buildings"), side.living_buildings},
          {QStringLiteral("peak_units"), side.peak_units},
          {QStringLiteral("units_produced"), side.units_produced},
          {QStringLiteral("peak_advance"), side.peak_advance},
          {QStringLiteral("final_advance"), side.final_advance},
          {QStringLiteral("eliminated_at"), side.eliminated_at},
          {QStringLiteral("strategy"), side.strategy},
          {QStringLiteral("posture"), side.posture},
          {QStringLiteral("seconds_attacking"), side.seconds_attacking},
          {QStringLiteral("seconds_observed"), side.seconds_observed},
          {QStringLiteral("buildings_constructed"), side.buildings_constructed},
          {QStringLiteral("peak_buildings"), side.peak_buildings},
          {QStringLiteral("building_census"), side.building_census},
          {QStringLiteral("peak_home_units"), side.peak_home_units},
          {QStringLiteral("peak_forward_units"), side.peak_forward_units},
          {QStringLiteral("mean_home_share"), side.mean_home_share}});
    }
    report_object.insert(
        QStringLiteral("battle"),
        QJsonObject{
            {QStringLiteral("decided"), m_impl->report.battle.decided},
            {QStringLiteral("victor_owner_id"), m_impl->report.battle.victor_owner_id},
            {QStringLiteral("victor"), m_impl->report.battle.victor_label},
            {QStringLiteral("decided_at_seconds"),
             m_impl->report.battle.decided_at_seconds},
            {QStringLiteral("sides"), sides}});
  }
  if (m_impl->report.frame_time_samples > 0U) {
    double const p95_fps = m_impl->report.frame_time_p95_ms > 0.0
                               ? 1000.0 / m_impl->report.frame_time_p95_ms
                               : 0.0;
    report_object.insert(
        QStringLiteral("performance"),
        QJsonObject{
            {QStringLiteral("sample_count"),
             static_cast<qint64>(m_impl->report.frame_time_samples)},
            {QStringLiteral("budget_ms"), m_impl->report.frame_budget_ms},
            {QStringLiteral("p50_ms"), m_impl->report.frame_time_p50_ms},
            {QStringLiteral("p95_ms"), m_impl->report.frame_time_p95_ms},
            {QStringLiteral("p99_ms"), m_impl->report.frame_time_p99_ms},
            {QStringLiteral("max_ms"), m_impl->report.frame_time_max_ms},
            {QStringLiteral("p95_fps"), p95_fps},
            {QStringLiteral("prewarm_seconds"), m_impl->report.prewarm_seconds},
            {QStringLiteral("prewarm_frames"),
             static_cast<qint64>(m_impl->report.prewarm_frames)},
            {QStringLiteral("prewarm_max_ms"), m_impl->report.prewarm_max_ms},
            {QStringLiteral("gpu_timed_frames"),
             static_cast<qint64>(m_impl->report.gpu_timed_frames)},
            {QStringLiteral("rpg_cost_p95_ms"),
             QJsonObject{
                 {QStringLiteral("motor"), m_impl->report.rpg_cost_p95_motor_ms},
                 {QStringLiteral("targeting"),
                  m_impl->report.rpg_cost_p95_targeting_ms},
                 {QStringLiteral("weapon_trace"),
                  m_impl->report.rpg_cost_p95_weapon_trace_ms},
                 {QStringLiteral("engagement"),
                  m_impl->report.rpg_cost_p95_engagement_ms},
                 {QStringLiteral("camera"), m_impl->report.rpg_cost_p95_camera_ms},
                 {QStringLiteral("total"), m_impl->report.rpg_cost_p95_total_ms}}},
            {QStringLiteral("simulation_p95_ms"), m_impl->report.simulation_p95_ms},
            {QStringLiteral("peak_visible_soldiers"),
             static_cast<qint64>(m_impl->report.peak_visible_soldiers)},
            {QStringLiteral("peak_draw_commands"),
             static_cast<qint64>(m_impl->report.peak_draw_commands)},
            {QStringLiteral("peak_rigged_commands"),
             static_cast<qint64>(m_impl->report.peak_rigged_commands)},
            {QStringLiteral("peak_rigged_instanced_instances"),
             static_cast<qint64>(m_impl->report.peak_rigged_instanced_instances)},
            {QStringLiteral("peak_rigged_single_draws"),
             static_cast<qint64>(m_impl->report.peak_rigged_single_draws)},
            {QStringLiteral("peak_shadow_rigged_instanced_instances"),
             static_cast<qint64>(
                 m_impl->report.peak_shadow_rigged_instanced_instances)},
            {QStringLiteral("peak_shadow_rigged_single_draws"),
             static_cast<qint64>(m_impl->report.peak_shadow_rigged_single_draws)}});
  }
  QJsonArray issues;
  for (auto const& issue : m_impl->report.issues) {
    issues.append(
        QJsonObject{{QStringLiteral("code"), issue.code},
                    {QStringLiteral("message"), issue.message},
                    {QStringLiteral("time_seconds"), issue.time_seconds},
                    {QStringLiteral("entity_id"), static_cast<qint64>(issue.entity_id)},
                    {QStringLiteral("soldier_index"), issue.soldier_index}});
  }
  report_object.insert(QStringLiteral("issues"), issues);

  report_object.insert(QStringLiteral("asset_counters"),
                       Render::Profiling::asset_counters_json());
  report_object.insert(QStringLiteral("navigation"),
                       Render::Profiling::navigation_counters_json());
  report_object.insert(
      QStringLiteral("simulation_systems"),
      Render::Profiling::system_profiler_json(m_impl->world.system_profiler()));
  if (m_impl->report.frame_time_samples > 0U) {
    Render::Profiling::PerformanceMeasurement measurement;
    measurement.frames = m_impl->report.frame_time_samples;
    measurement.frame_p50_ms = m_impl->report.frame_time_p50_ms;
    measurement.frame_p95_ms = m_impl->report.frame_time_p95_ms;
    measurement.frame_p99_ms = m_impl->report.frame_time_p99_ms;
    measurement.frame_max_ms = m_impl->report.frame_time_max_ms;
    measurement.update_p95_ms = m_impl->report.simulation_p95_ms;
    measurement.update_average_ms = m_impl->report.simulation_p95_ms;
    measurement.gpu_timed = m_impl->report.gpu_timed_frames > 0U;
    const auto& graphics = Render::GraphicsSettings::instance();
    measurement.ultra_preset = graphics.quality() == Render::GraphicsQuality::Ultra;
    measurement.full_creature_lod = !graphics.creature_lod_enabled();
    report_object.insert(QStringLiteral("budget"),
                         Render::Profiling::budget_verdict_json(
                             Render::Profiling::PerformanceBudget::scale_gate(
                                 m_impl->report.frame_budget_ms),
                             measurement));
  }

  if (const auto& env = m_impl->environment_snapshot; env.valid) {
    const auto vec3 = [](const QVector3D& value) {
      return QJsonArray{value.x(), value.y(), value.z()};
    };
    report_object.insert(
        QStringLiteral("environment"),
        QJsonObject{{QStringLiteral("hour"), env.hour},
                    {QStringLiteral("time_of_day"), env.time_of_day},
                    {QStringLiteral("time_mode"), env.time_mode},
                    {QStringLiteral("lighting_profile"), env.lighting_profile},
                    {QStringLiteral("primary_direction"), vec3(env.primary_direction)},
                    {QStringLiteral("primary_color"), vec3(env.primary_color)},
                    {QStringLiteral("sky_color"), vec3(env.sky_color)},
                    {QStringLiteral("primary_intensity"), env.primary_intensity},
                    {QStringLiteral("ambient_intensity"), env.ambient_intensity},
                    {QStringLiteral("exposure"), env.exposure},
                    {QStringLiteral("fog_density"), env.fog_density},
                    {QStringLiteral("cloud_cover"), env.cloud_cover},
                    {QStringLiteral("wetness"), env.wetness}});
    report_object.insert(
        QStringLiteral("shadows"),
        QJsonObject{
            {QStringLiteral("quality"), env.shadow_quality},
            {QStringLiteral("directional_enabled"), env.directional_shadows_enabled},
            {QStringLiteral("resolution"), env.shadow_resolution},
            {QStringLiteral("cascades"), env.shadow_cascades},
            {QStringLiteral("distance"), env.shadow_distance},
            {QStringLiteral("contact_shadow_casters"), env.contact_shadow_casters}});
  }

  if (!m_impl->scenario.undead_zones.empty()) {
    QJsonArray zones;
    for (auto const& zone : m_impl->scenario.undead_zones) {
      auto const state = m_impl->undead_zone_state(zone.id);
      zones.append(
          QJsonObject{{QStringLiteral("id"), zone.id},
                      {QStringLiteral("owner_id"), zone.owner_id},
                      {QStringLiteral("spawned_total"), state.spawned_total},
                      {QStringLiteral("peak_alive"), state.peak_alive},
                      {QStringLiteral("alive"), state.alive},
                      {QStringLiteral("first_spawn_seconds"), state.first_spawn_at},
                      {QStringLiteral("shrine_seen"), state.shrine_seen},
                      {QStringLiteral("shrine_standing"), state.shrine_standing},
                      {QStringLiteral("shrine_destroyed"), state.shrine_destroyed}});
    }
    report_object.insert(QStringLiteral("undead_zones"), zones);
  }

  QFile report_file(QDir(directory).filePath(QStringLiteral("report.json")));
  if (!report_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    if (error != nullptr) {
      *error = report_file.errorString();
    }
    return false;
  }
  report_file.write(QJsonDocument(report_object).toJson(QJsonDocument::Indented));
  report_file.close();

  QFile trace_file(QDir(directory).filePath(QStringLiteral("trace.jsonl")));
  if (!trace_file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
    if (error != nullptr) {
      *error = trace_file.errorString();
    }
    return false;
  }
  for (auto const& frame : m_impl->trace) {
    QJsonArray units;
    for (auto const& unit : frame.units) {
      QJsonArray engaged_soldiers;
      for (auto index : unit.engaged_soldiers) {
        engaged_soldiers.append(static_cast<int>(index));
      }
      QJsonArray engagement_pairs;
      for (auto const& pair : unit.engagement_pairs) {
        engagement_pairs.append(
            QJsonObject{{QStringLiteral("attacker_slot"), pair.attacker_slot},
                        {QStringLiteral("target_slot"), pair.target_slot},
                        {QStringLiteral("root_distance"), pair.root_distance},
                        {QStringLiteral("surface_gap"), pair.surface_gap}});
      }
      units.append(QJsonObject{
          {QStringLiteral("entity_id"), static_cast<qint64>(unit.entity_id)},
          {QStringLiteral("group"), unit.group},
          {QStringLiteral("position"), json_vector(unit.position)},
          {QStringLiteral("health"), unit.health},
          {QStringLiteral("target_id"), static_cast<qint64>(unit.target_id)},
          {QStringLiteral("motion"), unit.motion},
          {QStringLiteral("combat_mode"), unit.combat_mode},
          {QStringLiteral("mounted_charge_state"), unit.mounted_charge_state},
          {QStringLiteral("mounted_charge_cancel_reason"),
           unit.mounted_charge_cancel_reason},
          {QStringLiteral("combat_action_id"), unit.combat_action_id},
          {QStringLiteral("melee_lock"), unit.melee_lock},
          {QStringLiteral("melee_lock_target_id"),
           static_cast<qint64>(unit.melee_lock_target_id)},
          {QStringLiteral("combat_indicator_submitted"),
           unit.combat_indicator_submitted},
          {QStringLiteral("yaw"), unit.yaw},
          {QStringLiteral("movement_target"), unit.movement_target},
          {QStringLiteral("movement_velocity"),
           QJsonArray{unit.movement_vx, unit.movement_vz}},
          {QStringLiteral("movement_goal"),
           QJsonArray{unit.movement_goal_x, unit.movement_goal_z}},
          {QStringLiteral("formation_contact"), unit.formation_contact},
          {QStringLiteral("formation_surface_gap"), unit.formation_surface_gap},
          {QStringLiteral("engaged_soldiers"), engaged_soldiers},
          {QStringLiteral("engagement_pairs"), engagement_pairs},
          {QStringLiteral("construction_type"), unit.construction_type},
          {QStringLiteral("construction_site"), unit.construction_site},
          {QStringLiteral("construction_in_progress"), unit.construction_in_progress},
          {QStringLiteral("construction_time_remaining"),
           unit.construction_time_remaining},
          {QStringLiteral("commander_aura_active"), unit.commander_aura_active},
          {QStringLiteral("commander_aura_buffed"), unit.commander_aura_buffed},
          {QStringLiteral("rpg_health"), unit.rpg_health},
          {QStringLiteral("rpg_guard_active"), unit.rpg_guard_active},
          {QStringLiteral("rpg_dodge_grace"), unit.rpg_dodge_grace},
          {QStringLiteral("rpg_aim_target_id"),
           static_cast<qint64>(unit.rpg_aim_target_id)},
          {QStringLiteral("rpg_aim_soldier_slot"), unit.rpg_aim_soldier_slot},
          {QStringLiteral("rpg_action_phase"), unit.rpg_action_phase},
          {QStringLiteral("rpg_action_time"), unit.rpg_action_normalized_time},
          {QStringLiteral("engagement_candidate_id"),
           static_cast<qint64>(unit.engagement_candidate_id)},
          {QStringLiteral("engagement_target_id"),
           static_cast<qint64>(unit.engagement_target_id)},
          {QStringLiteral("engagement_range"), unit.engagement_range},
          {QStringLiteral("engagement_reason"), unit.engagement_reason},
          {QStringLiteral("command_source"), unit.command_source}});
    }
    QJsonArray animals;
    for (auto const& animal : frame.animals) {
      animals.append(QJsonObject{
          {QStringLiteral("entity_id"), static_cast<qint64>(animal.entity_id)},
          {QStringLiteral("species"), animal.species},
          {QStringLiteral("position"), json_vector(animal.position)},
          {QStringLiteral("health"), animal.health},
          {QStringLiteral("behavior"), animal.behavior},
          {QStringLiteral("focus_id"), static_cast<qint64>(animal.focus_id)},
          {QStringLiteral("yaw"), animal.yaw},
          {QStringLiteral("desired_yaw"), animal.desired_yaw},
          {QStringLiteral("has_desired_yaw"), animal.has_desired_yaw},
          {QStringLiteral("velocity"), QJsonArray{animal.vx, animal.vz}},
          {QStringLiteral("biting"), animal.biting},
          {QStringLiteral("bite_phase"), animal.bite_phase},
          {QStringLiteral("flinch_phase"), animal.flinch_phase},
          {QStringLiteral("bite_target_id"),
           static_cast<qint64>(animal.bite_target_id)},
          {QStringLiteral("impact_pending"), animal.impact_pending},
          {QStringLiteral("dying"), animal.dying},
          {QStringLiteral("melee_lock"), animal.melee_lock},
          {QStringLiteral("has_move_target"), animal.has_move_target},
          {QStringLiteral("movement_goal"), QJsonArray{animal.goal_x, animal.goal_z}},
          {QStringLiteral("state_timer"), animal.state_timer},
          {QStringLiteral("stall_timer"), animal.stall_timer},
          {QStringLiteral("staggered"), animal.staggered}});
    }

    QJsonArray soldiers;
    for (auto const& soldier : frame.soldiers) {
      soldiers.append(QJsonObject{
          {QStringLiteral("entity_id"), static_cast<qint64>(soldier.entity_id)},
          {QStringLiteral("soldier_index"), soldier.soldier_index},
          {QStringLiteral("root_position"), json_vector(soldier.root_position)},
          {QStringLiteral("root_yaw_degrees"), soldier.root_yaw_degrees},
          {QStringLiteral("root_up_y"), soldier.root_up_y},
          {QStringLiteral("submitted_body_up_y"), soldier.submitted_body_up_y},
          {QStringLiteral("submitted_max_arm_reach"), soldier.submitted_max_arm_reach},
          {QStringLiteral("submitted_body_pose_valid"),
           soldier.submitted_body_pose_valid},
          {QStringLiteral("foot_l_world"), json_vector(soldier.foot_l_world)},
          {QStringLiteral("foot_r_world"), json_vector(soldier.foot_r_world)},
          {QStringLiteral("hand_l_world"), json_vector(soldier.hand_l_world)},
          {QStringLiteral("hand_r_world"), json_vector(soldier.hand_r_world)},
          {QStringLiteral("locomotion_blend"), soldier.locomotion_blend},
          {QStringLiteral("travel_alignment"), soldier.travel_alignment},
          {QStringLiteral("travel_lateral_share"), soldier.travel_lateral_share},
          {QStringLiteral("action_link_weight"), soldier.action_link_weight},
          {QStringLiteral("locomotion_presence"), soldier.locomotion_presence},
          {QStringLiteral("cycle_phase"), soldier.cycle_phase},
          {QStringLiteral("persistent_valid"), soldier.persistent_valid},
          {QStringLiteral("sample_time"), soldier.sample_time},
          {QStringLiteral("previous_locomotion_presence"),
           soldier.persistent_last_sample_time},
          {QStringLiteral("declared_action"), soldier.declared_action},
          {QStringLiteral("declared_target_slot"), soldier.declared_target_slot},
          {QStringLiteral("declared_surface_gap"), soldier.declared_surface_gap},
          {QStringLiteral("animation"), soldier.animation},
          {QStringLiteral("visual"), soldier.visual},
          {QStringLiteral("swing_recoil"), soldier.swing_recoil},
          {QStringLiteral("hit_reaction_tilt_degrees"),
           soldier.hit_reaction_tilt_degrees},
          {QStringLiteral("attack_phase"), soldier.attack_phase},
          {QStringLiteral("attack_is_melee"), soldier.attack_is_melee},
          {QStringLiteral("transitions_last_second"),
           static_cast<qint64>(soldier.transitions)},
          {QStringLiteral("culled"), soldier.culled},
          {QStringLiteral("cull_reason"), soldier.cull_reason},
          {QStringLiteral("lod"), soldier.lod},
          {QStringLiteral("pelvis_yaw_degrees"), soldier.pelvis_yaw_degrees},
          {QStringLiteral("torso_yaw_degrees"), soldier.torso_yaw_degrees}});
    }
    QJsonObject line{
        {QStringLiteral("time_seconds"), frame.time_seconds},
        {QStringLiteral("frame_time_ms"), frame.frame_time_ms},
        {QStringLiteral("frame_breakdown_ms"),
         QJsonObject{
             {QStringLiteral("simulation"), frame.timings.simulation_ms},
             {QStringLiteral("terrain_submit"), frame.timings.terrain_submit_ms},
             {QStringLiteral("world_submit"), frame.timings.world_submit_ms},
             {QStringLiteral("effects_submit"), frame.timings.effects_submit_ms},
             {QStringLiteral("render_execute"), frame.timings.render_execute_ms},
             {QStringLiteral("overlays"), frame.timings.overlays_ms},
             {QStringLiteral("humanoid_preparation"),
              frame.timings.humanoid_preparation_ms},
             {QStringLiteral("animation_sampling"),
              frame.timings.animation_sampling_ms},
             {QStringLiteral("bpat_playback"), frame.timings.bpat_playback_ms},
             {QStringLiteral("layout_generation"),
              frame.timings.layout_generation_ms}}},
        {QStringLiteral("gpu_ms"),
         QJsonObject{{QStringLiteral("shadow"), frame.timings.gpu_shadow_ms},
                     {QStringLiteral("color"), frame.timings.gpu_color_ms},
                     {QStringLiteral("wait"), frame.timings.gpu_wait_ms}}},
        {QStringLiteral("visible_soldiers"),
         static_cast<qint64>(frame.timings.visible_soldiers)},
        {QStringLiteral("draw_calls"), static_cast<qint64>(frame.timings.draw_calls)},
        {QStringLiteral("prepared_batches"),
         static_cast<qint64>(frame.timings.prepared_batches)},
        {QStringLiteral("rigged_playback"),
         QJsonObject{
             {QStringLiteral("commands"),
              static_cast<qint64>(frame.timings.rigged_commands)},
             {QStringLiteral("instanced_draws"),
              static_cast<qint64>(frame.timings.rigged_instanced_draws)},
             {QStringLiteral("instanced_instances"),
              static_cast<qint64>(frame.timings.rigged_instanced_instances)},
             {QStringLiteral("single_draws"),
              static_cast<qint64>(frame.timings.rigged_single_draws)},
             {QStringLiteral("shadow_instanced_instances"),
              static_cast<qint64>(frame.timings.shadow_rigged_instanced_instances)},
             {QStringLiteral("shadow_instanced_draws"),
              static_cast<qint64>(frame.timings.shadow_rigged_instanced_draws)},
             {QStringLiteral("shadow_single_draws"),
              static_cast<qint64>(frame.timings.shadow_rigged_single_draws)}}},
        {QStringLiteral("animation_time"), frame.animation_time},
        {QStringLiteral("units"), units},
        {QStringLiteral("animals"), animals},
        {QStringLiteral("soldiers"), soldiers}};
    if (frame.commander.valid) {
      line.insert(QStringLiteral("commander"), commander_trace_json(frame.commander));
    }
    trace_file.write(QJsonDocument(line).toJson(QJsonDocument::Compact));
    trace_file.write("\n");
  }
  return true;
}

} // namespace Arena
