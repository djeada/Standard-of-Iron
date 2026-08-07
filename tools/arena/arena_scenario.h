#pragma once

#include <QString>
#include <QStringList>
#include <QVector3D>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "game/formation/army_formation_types.h"
#include "game/map/map_definition.h"
#include "game/map/terrain.h"
#include "game/systems/nation_id.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"
#include "game/wildlife/wildlife_config.h"
#include "render/graphics_settings.h"

namespace Engine::Core {
class World;
using EntityID = std::uint64_t;
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
  Run,
  FormArmy,
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
  RpgPrimaryAttack,
  RpgAttackHold,
  RpgAim,
  RpgGuard,
  RpgDodge,
  RpgMove,

  RepairStructure,
  DeliverToStructure,
  HarvestResource,
  AbandonWork,

  ReloadUndeadZoneState,
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
  bool settlement_resident{false};
  float settlement_roam_radius{16.0F};
  int health_override{0};
  int max_health_override{0};

  float attack_range_override{0.0F};
  float attack_min_range_override{0.0F};

  std::optional<Game::Units::SpawnType> spawn_type;

  QString renderer_override;
  QStringList showcase_routine;
  float showcase_start_delay{0.0F};
  bool showcase_loop{true};
  std::optional<QVector3D> showcase_throw_target;
  float render_scale_override{0.0F};
  QString showcase_released_renderer;
};

struct ArenaScenarioResourcePatch {
  QString prop_type;
  int count{1};
  QVector3D origin;
  QVector3D spacing{2.5F, 0.0F, 0.0F};
  float scale{1.0F};
};

struct ArenaScenarioOwnerTeam {
  int owner_id{0};
  int team_id{0};
};

struct ArenaScenarioElevationPatch {
  QVector3D center;
  float radius{8.0F};
  float height{3.0F};
};

struct ArenaScenarioFormationOrder {

  QStringList groups;
  Game::Formation::ArmyFormationIntent intent{
      Game::Formation::ArmyFormationIntent::FactionDefault};

  QString doctrine;
  Game::Formation::ArmyFormationOptions options;

  QVector3D anchor;
  float facing_degrees{0.0F};
  float frontage{0.0F};
  float spacing{0.0F};
};

struct ArenaScenarioStep {
  QString name;
  ScenarioTrigger trigger;
  ScenarioCommandKind command{ScenarioCommandKind::Stand};
  QString group;
  QString target_group;

  QString zone_id;
  QVector3D destination;
  bool chase{true};
  bool enabled{true};
  int value{0};

  QString resource_kind;
  float camera_distance{14.0F};
  float camera_angle{45.0F};
  float camera_yaw{30.0F};

  std::optional<float> rpg_view_yaw_degrees;
  std::optional<float> rpg_view_pitch_degrees;
  ArenaScenarioFormationOrder formation;
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
  ProjectileFlightObserved,
  ProjectileImpactObserved,
  ProjectileImpactSynchronized,
  GroupHealthUnchanged,
  GroupHealthReduced,
  StructureDamageCueObserved,
  StructureFacadeContactObserved,
  StructureFireObserved,
  NoStructureFireObserved,
  FlamingProjectileObserved,
  NoFlamingProjectileObserved,
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
  GroupHeldOutsideDestination,
  GateOpenedObserved,
  GateRemainedClosed,
  BridgeTraversalObserved,
  BridgeCenterlineAligned,
  ElevationGainObserved,
  OwnerCompletesConstruction,
  OwnerHarvestsResource,
  CommanderAuraActivated,
  CommanderAuraBuffObserved,
  CommanderAuraExpired,
  NoCommanderAuraBuffObserved,
  ExactRpgTargetObserved,
  RpgDamageContactObserved,
  RpgBlockContactObserved,
  RpgDodgeContactObserved,
  RpgDodgeWindowObserved,
  RpgHealthReduced,
  RpgHealthUnchanged,
  RpgWalkObserved,
  RpgRunObserved,
  RpgLocomotionAnimationMatched,
  RpgStrikeAnimationMatched,
  RpgSwingCadenceWithin,
  RpgTravelObserved,
  RpgFormationSurvivesLensGap,
  RpgApproachWithin,
  UndeadZoneDormantBefore,
  UndeadZoneAwakened,
  UndeadZoneCleared,
  UndeadZoneShrineStands,
  UndeadZoneShrineDestroyed,
  WildlifeGrazingObserved,
  WildlifeFleeObserved,
  WildlifeHuntObserved,
  WildlifeBirdsScattered,
  WildlifeBirdFlyoverObserved,
  WildlifePopulationHeld,
  WildlifeCasualtyObserved,
  RangeIndicatorObserved,
  RangeIndicatorCountAtMost,
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

  float arena_floor_half_extent{18.0F};

  float terrain_height_scale_override{0.0F};

  bool suppress_boundary_mountains{false};

  bool suppress_combat_dust{false};
  ArenaCameraView camera;
  std::optional<QVector3D> camera_focus;
  bool suppress_terrain_scatter{false};
  bool suppress_terrain_features{false};
  bool select_spawned_units{true};
  bool suppress_spawn_anchor{false};
  bool suppress_ui_overlays{false};

  bool capture_ui_overlays{false};
  bool force_full_creature_lod{true};
  bool require_rigged_instancing{false};
  bool collect_animation_diagnostics{true};
  bool rpg_mode{false};
  QString rpg_commander_group;
  Render::GraphicsQuality graphics_quality{Render::GraphicsQuality::High};
  Game::Map::EnvironmentDefinition environment{};
  Game::Map::WeatherLightingInput weather{};
  Game::Map::RainSettings precipitation{};
  Game::Wildlife::WildlifeSettings wildlife{};
  std::vector<Game::Map::RiverSegment> rivers;
  std::vector<Game::Map::Lake> lakes;
  std::vector<Game::Map::Bridge> bridges;
  std::vector<Game::Map::RoadSegment> roads;
  std::vector<ArenaScenarioElevationPatch> elevation_patches;
  std::vector<Game::Map::UndeadZone> undead_zones;
  std::vector<ArenaScenarioOwnerTeam> owner_teams;
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
  std::uint64_t frame_time_samples{0};
  double frame_budget_ms{0.0};
  double frame_time_p50_ms{0.0};
  double frame_time_p95_ms{0.0};
  double frame_time_max_ms{0.0};
  std::uint64_t peak_visible_soldiers{0};
  std::uint64_t peak_draw_commands{0};
  std::uint64_t peak_rigged_commands{0};
  std::uint64_t peak_rigged_instanced_instances{0};
  std::uint64_t peak_rigged_single_draws{0};
  std::uint64_t peak_shadow_rigged_instanced_instances{0};
  std::uint64_t peak_shadow_rigged_single_draws{0};
  std::vector<ArenaScenarioIssue> issues;

  [[nodiscard]] auto passed() const noexcept -> bool { return issues.empty(); }
  [[nodiscard]] auto summary() const -> QString;
};

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
  std::function<void(Engine::Core::EntityID)> configure_rpg_commander;
  std::function<bool(Engine::Core::EntityID)> rpg_primary_attack;
  std::function<void(Engine::Core::EntityID, bool)> set_rpg_attack_held;
  std::function<void(Engine::Core::EntityID, bool)> set_rpg_guard;
  std::function<void(Engine::Core::EntityID, const QVector3D&)> request_rpg_dodge;

  std::function<void(Engine::Core::EntityID, const QVector3D&, bool)>
      set_rpg_move_input;
  std::function<void(Engine::Core::EntityID, float)> set_rpg_view_yaw;
  std::function<void(Engine::Core::EntityID, float)> set_rpg_view_pitch;

  std::function<void(Engine::Core::EntityID, const QVector3D&)> aim_rpg_view_at;
};

struct ArenaRenderedFrameTimings {
  double total_ms{0.0};
  double simulation_ms{0.0};
  double terrain_submit_ms{0.0};
  double world_submit_ms{0.0};
  double effects_submit_ms{0.0};
  double render_execute_ms{0.0};
  double overlays_ms{0.0};
  double humanoid_preparation_ms{0.0};
  double animation_sampling_ms{0.0};
  double bpat_playback_ms{0.0};
  double layout_generation_ms{0.0};
  std::uint64_t visible_soldiers{0};
  std::uint64_t draw_calls{0};
  std::uint64_t rigged_commands{0};
  std::uint64_t rigged_instanced_instances{0};
  std::uint64_t rigged_single_draws{0};
  std::uint64_t shadow_rigged_instanced_instances{0};
  std::uint64_t shadow_rigged_single_draws{0};
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
