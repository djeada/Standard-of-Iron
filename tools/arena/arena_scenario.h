#pragma once

#include <QString>
#include <QVector3D>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "game/map/map_definition.h"
#include "game/map/terrain.h"
#include "game/systems/nation_id.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"
#include "render/graphics_settings.h"

namespace Engine::Core {
class World;
using EntityID = unsigned int;
} // namespace Engine::Core

namespace Game::Units {
class Unit;
}

namespace Arena {

enum class ScenarioTriggerKind : std::uint8_t {
  AtTime,
  GroupDestroyed,
  FirstContact,
  GroupsWithinDistance,
  GroupEnteredArea,
  PreviousStepComplete,
};

struct ScenarioTrigger {
  ScenarioTriggerKind kind{ScenarioTriggerKind::AtTime};
  float time_seconds{0.0F};
  QString group;
  QString target_group;
  float distance{0.0F};
  QVector3D position;
};

enum class ScenarioCommandKind : std::uint8_t {
  Stand,
  Move,
  FormationMove,
  Charge,
  Attack,
  AttackMove,
  Hold,
  Guard,
  Stop,
  SpawnAmbush,
  ReleaseReserve,
  SetCamera,
  SetHealth,
  ApplyDamage,
  MeleeLock,
  SetFullCreatureLod,
  TriggerCommanderAura,
};

struct ArenaScenarioGroup {
  QString name;
  Game::Units::TroopType troop_type{Game::Units::TroopType::Swordsman};
  Game::Systems::NationID nation_id{Game::Systems::NationID::RomanRepublic};
  int owner_id{1};
  int count{1};

  int individuals_per_unit{0};
  QVector3D origin;
  QVector3D spacing{2.4F, 0.0F, 0.0F};
  float facing_degrees{0.0F};
  bool ai_controlled{false};
  bool spawn_at_start{true};
  bool render_rider{true};
  int health_override{0};
  int max_health_override{0};

  std::optional<Game::Units::SpawnType> spawn_type;
};

struct ArenaScenarioResourcePatch {
  QString prop_type;
  int count{1};
  QVector3D origin;
  QVector3D spacing{2.5F, 0.0F, 0.0F};
  float scale{1.0F};
};

struct ArenaScenarioElevationPatch {
  QVector3D center;
  float radius{8.0F};
  float height{3.0F};
};

struct ArenaScenarioStep {
  QString name;
  ScenarioTrigger trigger;
  ScenarioCommandKind command{ScenarioCommandKind::Stand};
  QString group;
  QString target_group;
  QVector3D destination;
  bool chase{true};
  bool enabled{true};
  int value{0};
  float camera_distance{14.0F};
  float camera_angle{45.0F};
  float camera_yaw{30.0F};
};

enum class ArenaExpectationKind : std::uint8_t {
  AllGroupsRespondWithin,
  NoEligibleTroopIdleDuringCombat,
  NoPoseOscillation,
  NoRootTeleport,
  NoUnexpectedFallPose,
  NoLimbOverextension,
  NoRenderVisibilityChurn,
  FullCreatureDetailOnly,
  NoFullscreenFlash,
  MovementIsContinuous,
  FormationOrderPreserved,
  FormationEngagementIsStable,
  FormationBodyOverlapObserved,
  CombatIndicatorIsContinuous,
  AllLivingSoldiersFight,
  MovementAnimationObserved,
  AttackAnimationObserved,
  HoldPoseMaintained,
  RepeatedAttackAnimationObserved,
  AttackHasVisibleContact,
  AttackRecoveryObserved,
  NoActiveCombatAtEnd,
  HitReactionObserved,
  DeathAnimationObserved,
  LaunchedCasualtyObserved,
  NoLaunchedCasualtyObserved,
  ChargeImpactPrecedesMeleeLock,
  TargetRetakenAfterDeath,
  BotIssuesUsefulCommand,
  FrameBudget,
  GroupIsRendered,
  GroupExists,
  GroupDestroyed,
  GroupReachedDestination,
  BridgeTraversalObserved,
  BridgeCenterlineAligned,
  ElevationGainObserved,
  OwnerCompletesConstruction,
  OwnerHarvestsResource,
  CommanderAuraActivated,
  CommanderAuraBuffObserved,
  CommanderAuraExpired,
  NoCommanderAuraBuffObserved,
  UndeadZoneDormantBefore,
  UndeadZoneAwakened,
  UndeadZoneCleared,
};

struct ArenaExpectation {
  ArenaExpectationKind kind{ArenaExpectationKind::MovementIsContinuous};
  QString group;
  QString target_group;
  QString zone_id;
  float start_seconds{0.0F};
  float end_seconds{0.0F};
  float threshold{0.0F};
  float distance{0.0F};
  QVector3D position;
};

struct ArenaCameraView {
  float distance{14.0F};
  float angle{45.0F};
  float yaw{30.0F};
};

struct ArenaScenarioDefinition {
  QString id;
  QString label;
  QString description;
  float duration_seconds{12.0F};
  ArenaCameraView camera;
  std::optional<QVector3D> camera_focus;
  bool suppress_terrain_scatter{false};
  bool select_spawned_units{true};
  bool suppress_spawn_anchor{false};
  bool suppress_ui_overlays{false};
  bool force_full_creature_lod{true};
  bool collect_animation_diagnostics{true};
  Render::GraphicsQuality graphics_quality{Render::GraphicsQuality::High};
  Game::Map::EnvironmentDefinition environment{};
  Game::Map::WeatherLightingInput weather{};
  std::vector<Game::Map::RiverSegment> rivers;
  std::vector<Game::Map::Lake> lakes;
  std::vector<Game::Map::Bridge> bridges;
  std::vector<ArenaScenarioElevationPatch> elevation_patches;
  std::vector<Game::Map::UndeadZone> undead_zones;
  std::vector<ArenaScenarioGroup> groups;
  std::vector<ArenaScenarioResourcePatch> resource_patches;
  std::vector<ArenaScenarioStep> steps;
  std::vector<ArenaExpectation> expectations;
};

struct ArenaScenarioValidationError {
  QString field;
  QString message;
};

[[nodiscard]] auto validate_scenario(const ArenaScenarioDefinition& definition)
    -> std::vector<ArenaScenarioValidationError>;

struct ArenaScenarioIssue {
  QString code;
  QString message;
  float time_seconds{0.0F};
  Engine::Core::EntityID entity_id{0};
  int soldier_index{-1};
};

struct ArenaScenarioReport {
  QString scenario_id;
  float elapsed_seconds{0.0F};
  std::uint64_t rendered_frames{0};
  std::uint64_t rendered_soldier_samples{0};
  std::vector<ArenaScenarioIssue> issues;

  [[nodiscard]] auto passed() const noexcept -> bool { return issues.empty(); }
  [[nodiscard]] auto summary() const -> QString;
};

// Lighting and shadow settings in force while a scenario ran.  Recorded in the
// report so a regression capture can be attributed to a known environment
// rather than guessed at from the image.
struct ArenaEnvironmentSnapshot {
  bool valid{false};
  float hour{0.0F};
  QString time_of_day;
  QString time_mode;
  QString lighting_profile;
  QString shadow_quality;
  bool directional_shadows_enabled{false};
  int shadow_resolution{0};
  int shadow_cascades{0};
  float shadow_distance{0.0F};
  int contact_shadow_casters{0};
  QVector3D primary_direction;
  QVector3D primary_color;
  QVector3D sky_color;
  float primary_intensity{0.0F};
  float ambient_intensity{0.0F};
  float exposure{1.0F};
  float fog_density{0.0F};
  float cloud_cover{0.0F};
  float wetness{0.0F};
};

struct ArenaScenarioHost {
  std::function<Engine::Core::EntityID(const ArenaScenarioGroup&, const QVector3D&)>
      spawn_unit;
  std::function<Game::Units::Unit*(Engine::Core::EntityID)> find_unit;
  std::function<void(const std::vector<Engine::Core::EntityID>&,
                     const ArenaCameraView&)>
      set_camera;
  std::function<void(bool)> set_force_full_creature_lod;
};

struct ArenaRenderedFrameTimings {
  double total_ms{0.0};
  double simulation_ms{0.0};
  double terrain_submit_ms{0.0};
  double world_submit_ms{0.0};
  double effects_submit_ms{0.0};
  double playback_ms{0.0};
  double overlays_ms{0.0};
  double humanoid_preparation_ms{0.0};
  double animation_sampling_ms{0.0};
  double bpat_playback_ms{0.0};
  double layout_generation_ms{0.0};
  std::uint64_t visible_soldiers{0};
  std::uint64_t draw_calls{0};
};

class ArenaScenarioRunner {
public:
  ArenaScenarioRunner(Engine::Core::World& world,
                      ArenaScenarioHost host,
                      const ArenaScenarioDefinition& definition,
                      QVector3D world_origin = {});
  ~ArenaScenarioRunner();

  ArenaScenarioRunner(const ArenaScenarioRunner&) = delete;
  auto operator=(const ArenaScenarioRunner&) -> ArenaScenarioRunner& = delete;

  [[nodiscard]] auto start() -> bool;
  void update(float simulation_dt);
  void observe_rendered_frame(double frame_time_ms);
  void observe_rendered_frame(const ArenaRenderedFrameTimings& timings);
  void report_external_issue(QString code, QString message);
  void set_duration_limit(float duration_seconds);

  void set_environment_snapshot(const ArenaEnvironmentSnapshot& snapshot);

  [[nodiscard]] auto definition() const noexcept -> const ArenaScenarioDefinition&;
  [[nodiscard]] auto elapsed_seconds() const noexcept -> float;
  [[nodiscard]] auto finished() const noexcept -> bool;
  [[nodiscard]] auto report() const noexcept -> const ArenaScenarioReport&;
  [[nodiscard]] auto group_entities(const QString& group) const
      -> const std::vector<Engine::Core::EntityID>&;
  [[nodiscard]] auto all_entities() const -> std::vector<Engine::Core::EntityID>;
  [[nodiscard]] auto issue_revision() const noexcept -> std::size_t;

  [[nodiscard]] auto write_artifacts(const QString& directory,
                                     QString* error = nullptr) const -> bool;

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

namespace Scenarios {

[[nodiscard]] auto definitions() -> const std::vector<ArenaScenarioDefinition>&;
[[nodiscard]] auto
find_definition(const QString& scenario_id) -> const ArenaScenarioDefinition*;

} // namespace Scenarios

} // namespace Arena
