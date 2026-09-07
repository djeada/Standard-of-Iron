#include "arena_engagement_scenarios.h"

#include <QVector3D>

#include <utility>
#include <vector>

#include "arena_scenarios.h"
#include "game/systems/nation_registry.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"
#include "game/wildlife/wildlife_config.h"

namespace Arena::Scenarios {
namespace {

using Command = ScenarioCommandKind;
using Expect = ArenaExpectationKind;
using Nation = Game::Systems::NationID;
using Spawn = Game::Units::SpawnType;
using Trigger = ScenarioTriggerKind;
using Troop = Game::Units::TroopType;

constexpr int k_player_owner = 1;
constexpr int k_enemy_owner = 2;

auto definition(const char* id,
                const char* label,
                const char* description,
                float duration) -> ArenaScenarioDefinition {
  ArenaScenarioDefinition result;
  result.id = QString::fromLatin1(id);
  result.label = QString::fromLatin1(label);
  result.description = QString::fromLatin1(description);
  result.duration_seconds = duration;
  result.camera = {34.0F, 48.0F, 28.0F};
  result.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
  result.terrain_grid_extent = 72;
  result.arena_floor_half_extent = 34.0F;
  result.suppress_terrain_scatter = true;
  result.suppress_terrain_features = true;
  result.suppress_spawn_anchor = true;
  result.suppress_boundary_mountains = true;
  result.select_spawned_units = false;
  result.owner_teams = {{.owner_id = k_player_owner, .team_id = 1},
                        {.owner_id = k_enemy_owner, .team_id = 2}};
  return result;
}

auto troops(const QString& name,
            Troop troop,
            int owner_id,
            int count,
            int individuals,
            QVector3D origin,
            float facing = 90.0F) -> ArenaScenarioGroup {
  ArenaScenarioGroup result;
  result.name = name;
  result.troop_type = troop;
  result.nation_id =
      owner_id == k_enemy_owner ? Nation::Carthage : Nation::RomanRepublic;
  result.owner_id = owner_id;
  result.count = count;
  result.individuals_per_unit = individuals;
  result.origin = origin;
  result.spacing = QVector3D(3.0F, 0.0F, 0.0F);
  result.facing_degrees = facing;
  return result;
}

auto structure(const QString& name,
               Spawn type,
               int owner_id,
               QVector3D origin) -> ArenaScenarioGroup {
  ArenaScenarioGroup result;
  result.name = name;
  result.spawn_type = type;
  result.nation_id =
      owner_id == k_enemy_owner ? Nation::Carthage : Nation::RomanRepublic;
  result.owner_id = owner_id;
  result.count = 1;
  result.origin = origin;
  result.facing_degrees = 0.0F;
  return result;
}

auto expect(Expect kind,
            const QString& group,
            float threshold = 0.0F) -> ArenaExpectation {
  ArenaExpectation result;
  result.kind = kind;
  result.group = group;
  result.threshold = threshold;
  return result;
}

auto at_time(float seconds) -> ScenarioTrigger {
  return {Trigger::AtTime, seconds, {}, {}, 0.0F, {}};
}

auto wolves_at(QVector3D where) -> Game::Wildlife::WildlifeSettings {
  Game::Wildlife::WildlifeSettings settings = Game::Wildlife::default_settings();
  settings.enabled = true;
  settings.seed = 4113U;
  settings.sheep.enabled = false;
  settings.sheep.group_count = 0;
  settings.birds.enabled = false;
  settings.birds.group_count = 0;

  settings.wolves.enabled = true;
  settings.wolves.group_count = 1;
  settings.wolves.group_size_min = 2;
  settings.wolves.group_size_max = 2;
  settings.wolves.aggression = 1.0F;
  settings.wolves.roam_radius = 16.0F;
  settings.wolves.respawn = false;
  settings.wolves.spawn_areas = {{where.x(), where.z(), 2.0F}};
  return settings;
}

auto wolves_maul_an_ally() -> ArenaScenarioDefinition {
  auto scenario = definition(
      k_engagement_wolves_maul_an_ally_id,
      "Engagement: Swordsmen Answer a Wolf Attack",
      "A lone builder is set on by wolves with a file of swordsmen standing four "
      "metres away and under no orders. The swordsmen are expected to go to him.",
      22.0F);
  scenario.wildlife = wolves_at(QVector3D(3.0F, 0.0F, 0.0F));

  const QString victim = QStringLiteral("victim");
  const QString escort = QStringLiteral("escort");

  auto quarry =
      troops(victim, Troop::Builder, k_player_owner, 1, 1, QVector3D(0.0F, 0.0F, 0.0F));
  quarry.health_override = 400;
  quarry.max_health_override = 400;
  scenario.groups.push_back(std::move(quarry));
  scenario.groups.push_back(troops(
      escort, Troop::Swordsman, k_player_owner, 2, 1, QVector3D(-6.0F, 0.0F, 0.0F)));

  scenario.expectations = {
      expect(Expect::AutoEngagementObserved, escort),
      expect(Expect::GroupExists, escort),
  };
  return scenario;
}

auto infantry_under_arrow_fire() -> ArenaScenarioDefinition {
  auto scenario = definition(
      k_engagement_infantry_under_fire_id,
      "Engagement: Infantry Answers Arrow Fire",
      "Player spearmen stand with no current order while enemy archers shoot into "
      "them. They are expected to close rather than absorb the volley.",
      24.0F);

  const QString shield = QStringLiteral("shieldwall");
  const QString archers = QStringLiteral("archers");

  scenario.groups.push_back(troops(
      shield, Troop::Spearman, k_player_owner, 1, 6, QVector3D(-4.0F, 0.0F, 0.0F)));
  scenario.groups.push_back(troops(archers,
                                   Troop::Archer,
                                   k_enemy_owner,
                                   1,
                                   4,
                                   QVector3D(5.0F, 0.0F, 0.0F),
                                   270.0F));

  scenario.expectations = {
      expect(Expect::AutoEngagementObserved, shield),
      expect(Expect::GroupHealthReduced, shield),
  };
  return scenario;
}

auto village_defenders_answer_raiders() -> ArenaScenarioDefinition {
  auto scenario = definition(
      k_engagement_village_defense_id,
      "Engagement: Village Defenders Answer Raiders",
      "Enemy-owned spearmen stand behind their hut while player archers walk into "
      "the village and shoot over it. The garrison is expected to answer even "
      "though the hut is between them and the raiders.",
      26.0F);

  const QString garrison = QStringLiteral("garrison");
  const QString raiders = QStringLiteral("raiders");

  scenario.groups.push_back(
      structure(QStringLiteral("village_home"), Spawn::Home, k_enemy_owner, {}));
  auto watch = troops(
      garrison, Troop::Spearman, k_enemy_owner, 2, 3, QVector3D(-4.0F, 0.0F, 2.5F));
  watch.ai_controlled = true;
  scenario.groups.push_back(std::move(watch));
  scenario.groups.push_back(troops(raiders,
                                   Troop::Archer,
                                   k_player_owner,
                                   1,
                                   4,
                                   QVector3D(0.0F, 0.0F, -18.0F),
                                   0.0F));

  scenario.steps = {[&] {
    ArenaScenarioStep step;
    step.name = QStringLiteral("raiders_walk_in");
    step.trigger = at_time(1.0F);
    step.command = Command::FormationMove;
    step.group = raiders;

    step.destination = QVector3D(0.0F, 0.0F, -3.5F);
    return step;
  }()};

  scenario.expectations = {
      expect(Expect::AutoEngagementObserved, garrison),
      expect(Expect::GroupExists, garrison),
  };
  return scenario;
}

auto melee_assists_an_engaged_ally() -> ArenaScenarioDefinition {
  auto scenario = definition(
      k_engagement_melee_assist_id,
      "Engagement: Melee Assists an Engaged Ally",
      "One player spear unit is charged by an enemy block while a second player "
      "unit stands within the alert radius under no orders.",
      24.0F);

  const QString engaged = QStringLiteral("engaged");
  const QString reserve = QStringLiteral("reserve");
  const QString attackers = QStringLiteral("attackers");

  scenario.groups.push_back(troops(
      engaged, Troop::Spearman, k_player_owner, 1, 5, QVector3D(-4.0F, 0.0F, 0.0F)));
  scenario.groups.push_back(troops(
      reserve, Troop::Swordsman, k_player_owner, 1, 5, QVector3D(-12.0F, 0.0F, 6.0F)));
  scenario.groups.push_back(troops(attackers,
                                   Troop::Swordsman,
                                   k_enemy_owner,
                                   1,
                                   5,
                                   QVector3D(10.0F, 0.0F, 0.0F),
                                   270.0F));

  scenario.steps = {[&] {
    ArenaScenarioStep step;
    step.name = QStringLiteral("enemy_charges_the_spears");
    step.trigger = at_time(1.0F);
    step.command = Command::Attack;
    step.group = attackers;
    step.target_group = engaged;
    step.chase = true;
    return step;
  }()};

  scenario.expectations = {
      expect(Expect::AutoEngagementObserved, reserve),
      expect(Expect::GroupExists, reserve),
  };
  return scenario;
}

auto archers_hold_fire_until_in_range() -> ArenaScenarioDefinition {
  auto scenario = definition(
      k_engagement_ranged_awareness_id,
      "Engagement: Archers Wait For Weapon Range",
      "Player archers with a hostile block far outside their weapon range, which "
      "then walks into it. Nothing should be drawn on until the shot exists.",
      28.0F);

  const QString archers = QStringLiteral("archers");
  const QString approach = QStringLiteral("approach");

  scenario.groups.push_back(troops(
      archers, Troop::Archer, k_player_owner, 1, 4, QVector3D(-14.0F, 0.0F, 0.0F)));
  scenario.groups.push_back(troops(approach,
                                   Troop::Swordsman,
                                   k_enemy_owner,
                                   1,
                                   4,
                                   QVector3D(24.0F, 0.0F, 0.0F),
                                   270.0F));

  scenario.steps = {[&] {
    ArenaScenarioStep step;
    step.name = QStringLiteral("close_to_bowshot");
    step.trigger = at_time(6.0F);
    step.command = Command::FormationMove;
    step.group = approach;
    step.destination = QVector3D(-6.0F, 0.0F, 0.0F);
    return step;
  }()};

  auto quiet = expect(Expect::NoAutoEngagementObserved, archers);
  quiet.start_seconds = 0.0F;
  quiet.end_seconds = 5.0F;

  scenario.expectations = {
      std::move(quiet),
      expect(Expect::AutoEngagementObserved, archers),
      expect(Expect::GroupExists, archers),
  };
  return scenario;
}

auto a_player_order_breaks_the_engagement() -> ArenaScenarioDefinition {
  auto scenario = definition(
      k_engagement_order_overrides_id,
      "Engagement: A Player Order Breaks the Lock",
      "Player swordsmen auto-acquire an enemy that will not fight back, then are "
      "ordered away before contact. The automatic target must be released, not "
      "re-taken behind the order.",
      26.0F);

  const QString column = QStringLiteral("column");
  const QString bait = QStringLiteral("bait");

  scenario.groups.push_back(troops(
      column, Troop::Swordsman, k_player_owner, 1, 5, QVector3D(-6.0F, 0.0F, 0.0F)));

  auto stationary_bait = troops(
      bait, Troop::Civilian, k_enemy_owner, 1, 5, QVector3D(6.0F, 0.0F, 0.0F), 270.0F);
  stationary_bait.attacks_disabled = true;
  scenario.groups.push_back(std::move(stationary_bait));

  const QVector3D destination(-26.0F, 0.0F, -14.0F);

  scenario.steps = {
      [&] {
        ArenaScenarioStep step;
        step.name = QStringLiteral("bait_holds_its_ground");
        step.trigger = at_time(0.0F);
        step.command = Command::Hold;
        step.group = bait;
        step.enabled = true;
        return step;
      }(),
      [&] {
        ArenaScenarioStep step;
        step.name = QStringLiteral("recall_the_column");
        step.trigger = at_time(2.0F);
        step.command = Command::FormationMove;
        step.group = column;
        step.destination = destination;
        return step;
      }(),
  };

  auto released = expect(Expect::EngagementReleasedByOrder, column);
  released.start_seconds = 2.5F;
  released.end_seconds = 26.0F;

  auto arrived = expect(Expect::GroupReachedDestination, column);
  arrived.distance = 7.0F;
  arrived.position = destination;

  scenario.expectations = {
      expect(Expect::AutoEngagementObserved, column),
      std::move(released),
      std::move(arrived),
  };
  return scenario;
}

} // namespace

auto build_engagement_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;
  result.push_back(wolves_maul_an_ally());
  result.push_back(infantry_under_arrow_fire());
  result.push_back(village_defenders_answer_raiders());
  result.push_back(melee_assists_an_engaged_ally());
  result.push_back(archers_hold_fire_until_in_range());
  result.push_back(a_player_order_breaks_the_engagement());
  return result;
}

} // namespace Arena::Scenarios
