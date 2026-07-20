#pragma once

#include <QString>
#include <QVector3D>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "game/systems/nation_id.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"

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
  MovementIsContinuous,
  FormationOrderPreserved,
  FormationEngagementIsStable,
  CombatIndicatorIsContinuous,
  AllLivingSoldiersFight,
  MovementAnimationObserved,
  AttackAnimationObserved,
  HoldPoseMaintained,
  RepeatedAttackAnimationObserved,
  AttackHasVisibleContact,
  AttackRecoveryObserved,
  HitReactionObserved,
  DeathAnimationObserved,
  ChargeImpactPrecedesMeleeLock,
  TargetRetakenAfterDeath,
  BotIssuesUsefulCommand,
  FrameBudget,
  GroupIsRendered,
  GroupExists,
  OwnerCompletesConstruction,
  OwnerHarvestsResource,
};

struct ArenaExpectation {
  ArenaExpectationKind kind{ArenaExpectationKind::MovementIsContinuous};
  QString group;
  QString target_group;
  float start_seconds{0.0F};
  float end_seconds{0.0F};
  float threshold{0.0F};
  float distance{0.0F};
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

struct ArenaScenarioHost {
  std::function<Engine::Core::EntityID(const ArenaScenarioGroup&, const QVector3D&)>
      spawn_unit;
  std::function<Game::Units::Unit*(Engine::Core::EntityID)> find_unit;
  std::function<void(const std::vector<Engine::Core::EntityID>&,
                     const ArenaCameraView&)>
      set_camera;
  std::function<void(bool)> set_force_full_creature_lod;
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
  void set_duration_limit(float duration_seconds);

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
