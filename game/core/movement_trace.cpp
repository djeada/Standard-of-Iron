#include "movement_trace.h"

#include <array>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <locale>
#include <sstream>
#include <string_view>

namespace Engine::Core {

namespace {

class FlatJsonWriter {
public:
  FlatJsonWriter() { m_out.imbue(std::locale::classic()); }

  void field(std::string_view key, float value) {
    separate();
    m_out << '"' << key << "\":";
    append_float(value);
  }

  void field(std::string_view key, double value) {
    field(key, static_cast<float>(value));
  }

  void field(std::string_view key, std::uint64_t value) {
    separate();
    m_out << '"' << key << "\":" << value;
  }

  void field(std::string_view key, std::uint32_t value) {
    field(key, static_cast<std::uint64_t>(value));
  }

  void field(std::string_view key, int value) {
    separate();
    m_out << '"' << key << "\":" << value;
  }

  void field(std::string_view key, bool value) {
    separate();
    m_out << '"' << key << "\":" << (value ? 1 : 0);
  }

  void field(std::string_view key, std::string_view value) {
    separate();
    m_out << '"' << key << "\":\"";
    for (char const character : value) {
      if (character == '"' || character == '\\') {
        m_out << '\\';
      }
      if (static_cast<unsigned char>(character) < 0x20U) {
        m_out << ' ';
        continue;
      }
      m_out << character;
    }
    m_out << '"';
  }

  [[nodiscard]] auto str() const -> std::string { return "{" + m_out.str() + "}"; }

private:
  void separate() {
    if (m_first) {
      m_first = false;
      return;
    }
    m_out << ',';
  }

  void append_float(float value) {
    if (value != value || value > 3.0e38F || value < -3.0e38F) {
      m_out << '0';
      return;
    }
    std::array<char, 40> buffer{};
    auto const result =
        std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    if (result.ec != std::errc{}) {
      m_out << '0';
      return;
    }
    m_out << std::string_view(buffer.data(),
                              static_cast<std::size_t>(result.ptr - buffer.data()));
  }

  std::ostringstream m_out;
  bool m_first{true};
};

auto find_value(const std::string& line, std::string_view key) -> std::string_view {
  std::string const needle = "\"" + std::string(key) + "\":";
  std::size_t const at = line.find(needle);
  if (at == std::string::npos) {
    return {};
  }
  std::size_t start = at + needle.size();
  std::size_t end = start;
  while (end < line.size() && line[end] != ',' && line[end] != '}') {
    ++end;
  }
  return std::string_view(line).substr(start, end - start);
}

auto read_float(const std::string& line, std::string_view key, float& out) -> bool {
  auto const text = find_value(line, key);
  if (text.empty()) {
    return false;
  }

  std::istringstream stream{std::string(text)};
  stream.imbue(std::locale::classic());
  float parsed = 0.0F;
  stream >> parsed;
  if (stream.fail() || !(stream >> std::ws).eof()) {
    return false;
  }
  out = parsed;
  return true;
}

auto read_u64(const std::string& line,
              std::string_view key,
              std::uint64_t& out) -> bool {
  auto const text = find_value(line, key);
  if (text.empty()) {
    return false;
  }
  std::uint64_t parsed = 0;
  auto const result = std::from_chars(text.data(), text.data() + text.size(), parsed);
  if (result.ec != std::errc{}) {
    return false;
  }
  out = parsed;
  return true;
}

template <typename T>
auto read_small(const std::string& line, std::string_view key, T& out) -> bool {
  std::uint64_t parsed = 0;
  if (!read_u64(line, key, parsed)) {
    return false;
  }
  out = static_cast<T>(parsed);
  return true;
}

auto read_int(const std::string& line, std::string_view key, int& out) -> bool {
  auto const text = find_value(line, key);
  if (text.empty()) {
    return false;
  }
  int parsed = 0;
  auto const result = std::from_chars(text.data(), text.data() + text.size(), parsed);
  if (result.ec != std::errc{}) {
    return false;
  }
  out = parsed;
  return true;
}

auto read_bool(const std::string& line, std::string_view key, bool& out) -> bool {
  std::uint64_t parsed = 0;
  if (!read_u64(line, key, parsed)) {
    return false;
  }
  out = parsed != 0U;
  return true;
}

constexpr std::size_t k_default_troop_limit = 4000000U;
constexpr std::size_t k_default_soldier_limit = 4000000U;

} // namespace

auto to_json(const MovementTroopSample& s) -> std::string {
  FlatJsonWriter w;
  w.field("session", s.session_id);
  w.field("tick", s.tick);
  w.field("entity", static_cast<std::uint64_t>(s.entity_id));
  w.field("owner", s.owner_id);
  w.field("troop_type", static_cast<std::uint32_t>(s.troop_type));
  w.field("command_seq", s.command_sequence);
  w.field("order_kind", static_cast<std::uint32_t>(s.order_kind));
  w.field("state", static_cast<std::uint32_t>(s.state));
  w.field("root_x", s.root_x);
  w.field("root_z", s.root_z);
  w.field("root_yaw", s.root_yaw);
  w.field("prev_root_x", s.previous_root_x);
  w.field("prev_root_z", s.previous_root_z);
  w.field("prev_root_yaw", s.previous_root_yaw);
  w.field("req_goal_x", s.requested_goal_x);
  w.field("req_goal_z", s.requested_goal_z);
  w.field("goal_x", s.resolved_goal_x);
  w.field("goal_z", s.resolved_goal_z);
  w.field("route_id", s.route_id);
  w.field("route_rev", s.route_revision);
  w.field("topo_rev", s.topology_revision);
  w.field("lane_offset", s.lane_offset);
  w.field("lane_scale", s.lane_scale);
  w.field("cohesion_pace", s.cohesion_pace);
  w.field("wp_index", s.waypoint_index);
  w.field("wp_count", s.waypoint_count);
  w.field("wp_x", s.waypoint_x);
  w.field("wp_z", s.waypoint_z);
  w.field("look_x", s.lookahead_x);
  w.field("look_z", s.lookahead_z);
  w.field("tan_x", s.tangent_x);
  w.field("tan_z", s.tangent_z);
  w.field("remaining", s.remaining_arclength);
  w.field("des_vx", s.desired_vx);
  w.field("des_vz", s.desired_vz);
  w.field("avoid_dx", s.avoidance_dx);
  w.field("avoid_dz", s.avoidance_dz);
  w.field("steer_vx", s.steered_vx);
  w.field("steer_vz", s.steered_vz);
  w.field("contact", s.has_contact);
  w.field("contact_nx", s.contact_nx);
  w.field("contact_nz", s.contact_nz);
  w.field("acc_dx", s.accepted_dx);
  w.field("acc_dz", s.accepted_dz);
  w.field("acc_vx", s.accepted_vx);
  w.field("acc_vz", s.accepted_vz);
  w.field("rej_dx", s.rejected_dx);
  w.field("rej_dz", s.rejected_dz);
  w.field("penetration", s.penetration_depth);
  w.field("advance", s.route_advance);
  w.field("lat_err", s.lateral_route_error);
  w.field("no_progress", s.no_progress_seconds);
  w.field("order_seconds", s.order_seconds);
  w.field("blocked_steps", s.blocked_steps);
  w.field("repaths", s.repath_count);
  w.field("repath_reason", static_cast<std::uint32_t>(s.repath_reason));
  w.field("queue_owner", static_cast<std::uint64_t>(s.queue_owner));
  w.field("neighbors", s.neighbor_count);
  w.field("ttc", s.nearest_time_to_collision);
  w.field("side", static_cast<int>(s.passing_side));
  w.field("solver", static_cast<std::uint32_t>(s.solver_result));
  w.field("envelope", s.envelope_radius);
  w.field("body_radius", s.soldier_body_radius);
  w.field("corridor_hw", s.corridor_half_width);
  w.field("portal", s.portal_id);
  w.field("mode", static_cast<std::uint32_t>(s.traversal_mode));
  w.field("files", s.current_files);
  w.field("target_files", s.target_files);
  w.field("transition", s.transition_progress);
  w.field("dwell", s.mode_dwell_seconds);
  w.field("pres_valid", s.presentation_valid);
  w.field("pres_state", static_cast<std::uint32_t>(s.presentation_state));
  w.field("pres_speed", s.presentation_speed);
  w.field("pres_dx", s.presentation_dir_x);
  w.field("pres_dz", s.presentation_dir_z);
  w.field("dir_src", static_cast<std::uint32_t>(s.direction_source));
  w.field("gait", static_cast<std::uint32_t>(s.gait_state));
  w.field("phase", s.locomotion_phase);
  return w.str();
}

auto to_json(const MovementSoldierSample& s) -> std::string {
  FlatJsonWriter w;
  w.field("session", s.session_id);
  w.field("frame", s.frame);
  w.field("prev_tick", s.previous_tick);
  w.field("tick", s.current_tick);
  w.field("alpha", s.interpolation_alpha);
  w.field("troop", static_cast<std::uint64_t>(s.troop_id));
  w.field("slot", s.stable_slot);
  w.field("alive", s.alive);
  w.field("culled", s.culled);
  w.field("has_anchor", s.has_final_anchor);
  w.field("lod", static_cast<std::uint32_t>(s.lod));
  w.field("mode", static_cast<std::uint32_t>(s.traversal_mode));
  w.field("transition", s.transition_progress);
  w.field("prev_ax", s.previous_anchor_x);
  w.field("prev_az", s.previous_anchor_z);
  w.field("cur_ax", s.current_anchor_x);
  w.field("cur_az", s.current_anchor_z);
  w.field("lerp_ax", s.interpolated_anchor_x);
  w.field("lerp_az", s.interpolated_anchor_z);
  w.field("yaw", s.facing_yaw);
  w.field("combat_dx", s.combat_offset_x);
  w.field("combat_dz", s.combat_offset_z);
  w.field("body_x", s.body_root_x);
  w.field("body_z", s.body_root_z);
  w.field("shadow_x", s.shadow_root_x);
  w.field("shadow_z", s.shadow_root_z);
  w.field("ring_x", s.ring_root_x);
  w.field("ring_z", s.ring_root_z);
  w.field("pick_x", s.picking_root_x);
  w.field("pick_z", s.picking_root_z);
  w.field("gait_speed", s.gait_speed);
  w.field("gait_dx", s.gait_dir_x);
  w.field("gait_dz", s.gait_dir_z);
  w.field("phase", s.locomotion_phase);
  w.field("clip", static_cast<std::uint32_t>(s.clip_id));
  w.field("blend", s.clip_blend);
  w.field("reloc_gait", s.relocation_drives_locomotion);
  return w.str();
}

auto to_json(const MovementTraceManifest& m) -> std::string {
  FlatJsonWriter w;
  w.field("seed", m.seed);
  w.field("command_stream", std::string_view(m.command_stream));
  w.field("map_id", std::string_view(m.map_id));
  w.field("topology_revision", m.topology_revision);
  w.field("fixed_step_seconds", m.fixed_step_seconds);
  w.field("presentation_cap_hz", m.presentation_cap_hz);
  w.field("build_type", std::string_view(m.build_type));
  w.field("commit", std::string_view(m.commit));
  w.field("graphics_preset", std::string_view(m.graphics_preset));
  w.field("unit_composition", std::string_view(m.unit_composition));
  w.field("scenario", std::string_view(m.scenario));
  for (auto const& [key, value] : m.extra) {
    w.field(std::string_view(key), std::string_view(value));
  }
  return w.str();
}

auto parse_troop_sample(const std::string& line, MovementTroopSample& out) -> bool {
  read_u64(line, "session", out.session_id);
  if (!read_u64(line, "tick", out.tick)) {
    return false;
  }
  std::uint64_t entity = 0;
  if (!read_u64(line, "entity", entity)) {
    return false;
  }
  out.entity_id = static_cast<EntityID>(entity);
  read_int(line, "owner", out.owner_id);
  read_small(line, "troop_type", out.troop_type);
  read_u64(line, "command_seq", out.command_sequence);
  read_small(line, "order_kind", out.order_kind);
  read_small(line, "state", out.state);
  read_float(line, "root_x", out.root_x);
  read_float(line, "root_z", out.root_z);
  read_float(line, "root_yaw", out.root_yaw);
  read_float(line, "prev_root_x", out.previous_root_x);
  read_float(line, "prev_root_z", out.previous_root_z);
  read_float(line, "prev_root_yaw", out.previous_root_yaw);
  read_float(line, "req_goal_x", out.requested_goal_x);
  read_float(line, "req_goal_z", out.requested_goal_z);
  read_float(line, "goal_x", out.resolved_goal_x);
  read_float(line, "goal_z", out.resolved_goal_z);
  read_u64(line, "route_id", out.route_id);
  read_u64(line, "route_rev", out.route_revision);
  read_u64(line, "topo_rev", out.topology_revision);
  read_float(line, "lane_offset", out.lane_offset);
  read_float(line, "lane_scale", out.lane_scale);
  read_float(line, "cohesion_pace", out.cohesion_pace);
  read_small(line, "wp_index", out.waypoint_index);
  read_small(line, "wp_count", out.waypoint_count);
  read_float(line, "wp_x", out.waypoint_x);
  read_float(line, "wp_z", out.waypoint_z);
  read_float(line, "look_x", out.lookahead_x);
  read_float(line, "look_z", out.lookahead_z);
  read_float(line, "tan_x", out.tangent_x);
  read_float(line, "tan_z", out.tangent_z);
  read_float(line, "remaining", out.remaining_arclength);
  read_float(line, "des_vx", out.desired_vx);
  read_float(line, "des_vz", out.desired_vz);
  read_float(line, "avoid_dx", out.avoidance_dx);
  read_float(line, "avoid_dz", out.avoidance_dz);
  read_float(line, "steer_vx", out.steered_vx);
  read_float(line, "steer_vz", out.steered_vz);
  read_bool(line, "contact", out.has_contact);
  read_float(line, "contact_nx", out.contact_nx);
  read_float(line, "contact_nz", out.contact_nz);
  read_float(line, "acc_dx", out.accepted_dx);
  read_float(line, "acc_dz", out.accepted_dz);
  read_float(line, "acc_vx", out.accepted_vx);
  read_float(line, "acc_vz", out.accepted_vz);
  read_float(line, "rej_dx", out.rejected_dx);
  read_float(line, "rej_dz", out.rejected_dz);
  read_float(line, "penetration", out.penetration_depth);
  read_float(line, "advance", out.route_advance);
  read_float(line, "lat_err", out.lateral_route_error);
  read_float(line, "no_progress", out.no_progress_seconds);
  read_float(line, "order_seconds", out.order_seconds);
  read_small(line, "blocked_steps", out.blocked_steps);
  read_small(line, "repaths", out.repath_count);
  read_small(line, "repath_reason", out.repath_reason);
  std::uint64_t queue_owner = 0;
  read_u64(line, "queue_owner", queue_owner);
  out.queue_owner = static_cast<EntityID>(queue_owner);
  read_small(line, "neighbors", out.neighbor_count);
  read_float(line, "ttc", out.nearest_time_to_collision);
  int side = 0;
  read_int(line, "side", side);
  out.passing_side = static_cast<std::int8_t>(side);
  read_small(line, "solver", out.solver_result);
  read_float(line, "envelope", out.envelope_radius);
  read_float(line, "body_radius", out.soldier_body_radius);
  read_float(line, "corridor_hw", out.corridor_half_width);
  read_small(line, "portal", out.portal_id);
  read_small(line, "mode", out.traversal_mode);
  read_small(line, "files", out.current_files);
  read_small(line, "target_files", out.target_files);
  read_float(line, "transition", out.transition_progress);
  read_float(line, "dwell", out.mode_dwell_seconds);
  read_bool(line, "pres_valid", out.presentation_valid);
  read_small(line, "pres_state", out.presentation_state);
  read_float(line, "pres_speed", out.presentation_speed);
  read_float(line, "pres_dx", out.presentation_dir_x);
  read_float(line, "pres_dz", out.presentation_dir_z);
  read_small(line, "dir_src", out.direction_source);
  read_small(line, "gait", out.gait_state);
  read_float(line, "phase", out.locomotion_phase);
  return true;
}

auto parse_soldier_sample(const std::string& line, MovementSoldierSample& out) -> bool {
  read_u64(line, "session", out.session_id);
  if (!read_u64(line, "frame", out.frame)) {
    return false;
  }
  read_u64(line, "prev_tick", out.previous_tick);
  read_u64(line, "tick", out.current_tick);
  read_float(line, "alpha", out.interpolation_alpha);
  std::uint64_t troop = 0;
  read_u64(line, "troop", troop);
  out.troop_id = static_cast<EntityID>(troop);
  read_small(line, "slot", out.stable_slot);
  read_bool(line, "alive", out.alive);
  read_bool(line, "culled", out.culled);
  read_bool(line, "has_anchor", out.has_final_anchor);
  read_small(line, "lod", out.lod);
  read_small(line, "mode", out.traversal_mode);
  read_float(line, "transition", out.transition_progress);
  read_float(line, "prev_ax", out.previous_anchor_x);
  read_float(line, "prev_az", out.previous_anchor_z);
  read_float(line, "cur_ax", out.current_anchor_x);
  read_float(line, "cur_az", out.current_anchor_z);
  read_float(line, "lerp_ax", out.interpolated_anchor_x);
  read_float(line, "lerp_az", out.interpolated_anchor_z);
  read_float(line, "yaw", out.facing_yaw);
  read_float(line, "combat_dx", out.combat_offset_x);
  read_float(line, "combat_dz", out.combat_offset_z);
  read_float(line, "body_x", out.body_root_x);
  read_float(line, "body_z", out.body_root_z);
  read_float(line, "shadow_x", out.shadow_root_x);
  read_float(line, "shadow_z", out.shadow_root_z);
  read_float(line, "ring_x", out.ring_root_x);
  read_float(line, "ring_z", out.ring_root_z);
  read_float(line, "pick_x", out.picking_root_x);
  read_float(line, "pick_z", out.picking_root_z);
  read_float(line, "gait_speed", out.gait_speed);
  read_float(line, "gait_dx", out.gait_dir_x);
  read_float(line, "gait_dz", out.gait_dir_z);
  read_float(line, "phase", out.locomotion_phase);
  read_small(line, "clip", out.clip_id);
  read_float(line, "blend", out.clip_blend);
  read_bool(line, "reloc_gait", out.relocation_drives_locomotion);
  return true;
}

struct MovementTrace::Session {
  MovementTraceManifest manifest;
  std::vector<MovementTroopSample> troops;
  std::vector<MovementSoldierSample> soldiers;
  std::ofstream troop_stream;
  std::ofstream soldier_stream;
  bool to_file{false};
  std::size_t troop_limit{k_default_troop_limit};
  std::size_t soldier_limit{k_default_soldier_limit};
  std::size_t troop_written{0};
  std::size_t soldier_written{0};
};

auto MovementTrace::instance() -> MovementTrace& {
  static MovementTrace singleton;
  return singleton;
}

void MovementTrace::configure_from_environment() {
  if (m_environment_checked) {
    return;
  }
  m_environment_checked = true;
  const char* const directory = std::getenv("SOI_MOVEMENT_TRACE_DIR");
  if (directory == nullptr || *directory == '\0') {
    return;
  }

  MovementTraceManifest manifest;
  manifest.scenario = "environment";
  manifest.fixed_step_seconds = 1.0F / 60.0F;
  if (const char* const scenario = std::getenv("SOI_MOVEMENT_TRACE_SCENARIO")) {
    manifest.scenario = scenario;
  }
  if (const char* const commit = std::getenv("SOI_MOVEMENT_TRACE_COMMIT")) {
    manifest.commit = commit;
  }
  begin_file_session(directory, manifest);
}

auto MovementTrace::begin_file_session(const std::string& directory,
                                       const MovementTraceManifest& manifest) -> bool {
  end_session();
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  if (error) {
    return false;
  }

  auto session = std::make_unique<Session>();
  session->manifest = manifest;
  session->to_file = true;
  session->troop_stream.open(directory + "/troops.jsonl", std::ios::trunc);
  session->soldier_stream.open(directory + "/soldiers.jsonl", std::ios::trunc);
  if (!session->troop_stream.is_open() || !session->soldier_stream.is_open()) {
    return false;
  }

  std::ofstream manifest_stream(directory + "/manifest.json", std::ios::trunc);
  if (!manifest_stream.is_open()) {
    return false;
  }
  manifest_stream << to_json(manifest) << '\n';

  std::lock_guard<std::mutex> const lock(m_mutex);
  m_session = std::move(session);
  m_enabled = true;
  return true;
}

void MovementTrace::begin_memory_session(const MovementTraceManifest& manifest) {
  end_session();
  auto session = std::make_unique<Session>();
  session->manifest = manifest;
  session->to_file = false;
  std::lock_guard<std::mutex> const lock(m_mutex);
  m_session = std::move(session);
  m_enabled = true;
}

void MovementTrace::end_session() {
  std::unique_ptr<Session> finished;
  {
    std::lock_guard<std::mutex> const lock(m_mutex);
    m_enabled = false;
    finished = std::move(m_session);
  }
  if (finished && finished->to_file) {
    finished->troop_stream.flush();
    finished->soldier_stream.flush();
  }
}

void MovementTrace::record(const MovementTroopSample& sample) {
  if (!m_enabled) {
    return;
  }
  std::lock_guard<std::mutex> const lock(m_mutex);
  if (!m_session) {
    return;
  }
  if (m_session->to_file) {
    m_session->troop_stream << to_json(sample) << '\n';
    ++m_session->troop_written;
    return;
  }
  if (m_session->troops.size() >= m_session->troop_limit) {
    return;
  }
  m_session->troops.push_back(sample);
  ++m_session->troop_written;
}

void MovementTrace::record(const MovementSoldierSample& sample) {
  if (!m_enabled) {
    return;
  }
  std::lock_guard<std::mutex> const lock(m_mutex);
  if (!m_session) {
    return;
  }
  if (m_session->to_file) {
    m_session->soldier_stream << to_json(sample) << '\n';
    ++m_session->soldier_written;
    return;
  }
  if (m_session->soldiers.size() >= m_session->soldier_limit) {
    return;
  }
  m_session->soldiers.push_back(sample);
  ++m_session->soldier_written;
}

auto MovementTrace::manifest() const -> const MovementTraceManifest& {
  static const MovementTraceManifest empty;
  std::lock_guard<std::mutex> const lock(m_mutex);
  return m_session ? m_session->manifest : empty;
}

auto MovementTrace::troop_samples() const -> const std::vector<MovementTroopSample>& {
  static const std::vector<MovementTroopSample> empty;
  std::lock_guard<std::mutex> const lock(m_mutex);
  return m_session ? m_session->troops : empty;
}

auto MovementTrace::soldier_samples() const
    -> const std::vector<MovementSoldierSample>& {
  static const std::vector<MovementSoldierSample> empty;
  std::lock_guard<std::mutex> const lock(m_mutex);
  return m_session ? m_session->soldiers : empty;
}

auto MovementTrace::troop_sample_count() const -> std::size_t {
  std::lock_guard<std::mutex> const lock(m_mutex);
  return m_session ? m_session->troop_written : 0U;
}

auto MovementTrace::soldier_sample_count() const -> std::size_t {
  std::lock_guard<std::mutex> const lock(m_mutex);
  return m_session ? m_session->soldier_written : 0U;
}

void MovementTrace::set_memory_sample_limit(std::size_t troop_limit,
                                            std::size_t soldier_limit) {
  std::lock_guard<std::mutex> const lock(m_mutex);
  if (!m_session) {
    return;
  }
  m_session->troop_limit = troop_limit;
  m_session->soldier_limit = soldier_limit;
}

ScopedMovementTrace::ScopedMovementTrace(const MovementTraceManifest& manifest) {
  MovementTrace::instance().begin_memory_session(manifest);
}

ScopedMovementTrace::~ScopedMovementTrace() {
  MovementTrace::instance().end_session();
}

} // namespace Engine::Core
