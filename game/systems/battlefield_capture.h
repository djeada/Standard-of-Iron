#pragma once

#include <cstdint>
#include <functional>
#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../units/factory.h"
#include "../units/unit.h"

namespace Engine::Core {
class World;
}

namespace Game::BattlefieldCapture {

enum class ScenarioId {
  InfantryApproach20v20,
  ArchersVsInfantry,
  MixedFormation,
  CasualtyReflow,
  CavalryCharge,
  NarrowPassage,
  CommanderInLine,
  BotSkirmish,
};

struct ScenarioMatch {
  Engine::Core::World* world = nullptr;
  std::unique_ptr<Engine::Core::World> owned_world;
  Game::Units::UnitFactoryRegistry factories;
  std::vector<std::unique_ptr<Game::Units::Unit>> unit_handles;
  std::vector<std::uint64_t> player_units;
  std::vector<std::uint64_t> enemy_units;
  std::unordered_map<std::uint64_t, std::pair<std::uint64_t, int>> slot_assignments;
};

auto build_scenario(ScenarioId id,
                    Engine::Core::World& into) -> std::unique_ptr<ScenarioMatch>;

struct RunnerConfig {
  ScenarioId scenario{ScenarioId::InfantryApproach20v20};
  std::uint64_t seed{1};
  double fixed_step_seconds{1.0 / 30.0};
  double duration_seconds{60.0};
  int capture_render_fps{30};

  std::optional<std::uint64_t> snapshot_tick;
};

struct TickRecord {
  std::uint64_t tick{};
  std::uint64_t entity_id{};
  std::uint64_t group_id{};
  int slot_id{-1};
  std::uint64_t target_id{};
  std::uint64_t action_id{};
  double x{};
  double z{};
  double yaw{};
  double preferred_vx{};
  double preferred_vz{};
  double actual_vx{};
  double actual_vz{};
  double health{};
  std::string action_event;
  std::string animation_state;
  bool dead{};
};

struct DebugOverlayRecord {
  std::uint64_t tick{};
  std::string kind;
  std::uint64_t owner_id{};
  std::vector<double> geometry;
  std::string label;
};

struct PerformanceCounters {
  std::uint64_t ticks{};
  std::uint64_t entity_updates{};
  std::uint64_t action_events{};
  std::uint64_t damage_events{};
  std::uint64_t deaths{};
  std::uint64_t max_live_entities{};
  double simulated_seconds{};
  double wall_seconds{};
  double slowest_tick_ms{};
  std::uint64_t ai_decisions{};
  std::uint64_t ai_commands{};
};

struct QualityMetrics {
  double first_response_seconds{-1.0};
  double first_combat_seconds{-1.0};
  double max_position_step{};
  double max_yaw_step_degrees{};
  std::uint64_t locomotion_flickers{};
  std::uint64_t non_finite_samples{};
  std::uint64_t excessive_position_steps{};
  std::uint64_t damage_without_visible_action{};
  std::uint64_t rear_rank_bypasses{};
  std::uint64_t friendly_order_inversions{};
};

struct VerificationIssue {
  std::string code;
  std::string message;
};

struct VerificationReport {
  bool passed{false};
  QualityMetrics metrics;
  std::vector<VerificationIssue> issues;
};

struct CaptureResult {
  RunnerConfig config;
  std::vector<TickRecord> ticks;
  std::vector<DebugOverlayRecord> overlays;
  PerformanceCounters performance;
  QualityMetrics quality;
  std::uint64_t deterministic_digest{};

  std::vector<std::uint64_t> tick_digests;
  std::string world_snapshot;
};

struct DeterminismReport {
  std::optional<std::uint64_t> divergent_tick;
  int divergent_run{-1};
  std::string first_state;
  std::string other_state;
  int runs{0};
  [[nodiscard]] auto deterministic() const -> bool {
    return !divergent_tick.has_value();
  }
};
auto check_determinism(const RunnerConfig& config, int runs) -> DeterminismReport;

using TickObserver = std::function<void(const TickRecord&)>;

auto scenario_name(ScenarioId id) -> const char*;
auto parse_scenario(const std::string& name, ScenarioId& result) -> bool;
auto all_scenarios() -> std::vector<ScenarioId>;
auto run(const RunnerConfig& config,
         const TickObserver& observer = {}) -> CaptureResult;
auto verify(const CaptureResult& result) -> VerificationReport;
void write_json_lines(const CaptureResult& result, std::ostream& output);
void write_acceptance_manifest(const CaptureResult& result, std::ostream& output);
void write_verification_report(const VerificationReport& report, std::ostream& output);

} // namespace Game::BattlefieldCapture
