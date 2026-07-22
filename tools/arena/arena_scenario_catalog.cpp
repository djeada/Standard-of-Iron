#include <algorithm>
#include <initializer_list>
#include <utility>

#include "arena_scenarios.h"

namespace Arena::Scenarios {
namespace {

using Command = ScenarioCommandKind;
using Expect = ArenaExpectationKind;
using Nation = Game::Systems::NationID;
using Trigger = ScenarioTriggerKind;
using Troop = Game::Units::TroopType;

auto group(QString name,
           Troop troop,
           int owner,
           int count,
           QVector3D origin,
           int individuals = 0,
           QVector3D spacing = {2.6F, 0.0F, 0.0F}) -> ArenaScenarioGroup {
  ArenaScenarioGroup result;
  result.name = std::move(name);
  result.troop_type = troop;
  result.nation_id = owner == 1 ? Nation::RomanRepublic : Nation::Carthage;
  result.owner_id = owner;
  result.count = count;
  result.individuals_per_unit = individuals;
  result.origin = origin;
  result.spacing = spacing;
  result.facing_degrees = owner == 1 ? 0.0F : 180.0F;
  return result;
}

auto building(QString name,
              Game::Units::SpawnType type,
              Nation nation,
              int owner,
              int count,
              QVector3D origin,
              QVector3D spacing = {4.5F, 0.0F, 0.0F},
              float facing = 0.0F) -> ArenaScenarioGroup {
  ArenaScenarioGroup result;
  result.name = std::move(name);
  result.spawn_type = type;
  result.nation_id = nation;
  result.owner_id = owner;
  result.count = count;
  result.origin = origin;
  result.spacing = spacing;
  result.facing_degrees = facing;
  return result;
}

void add_settlement_acceptance(ArenaScenarioDefinition& scenario,
                               std::initializer_list<QString> groups) {
  for (auto const& name : groups) {
    ArenaExpectation exists;
    exists.kind = Expect::GroupExists;
    exists.group = name;
    scenario.expectations.push_back(std::move(exists));
  }
  ArenaExpectation frame_budget;
  frame_budget.kind = Expect::FrameBudget;
  frame_budget.threshold = 33.34F;
  frame_budget.start_seconds = 0.25F;
  scenario.expectations.push_back(std::move(frame_budget));
}

auto at(float time,
        Command command,
        QString source = {},
        QString target = {}) -> ArenaScenarioStep {
  ArenaScenarioStep result;
  result.name = QStringLiteral("%1_%2").arg(QString::number(time, 'f', 2), source);
  result.trigger = {Trigger::AtTime, time, {}, {}, 0.0F};
  result.command = command;
  result.group = std::move(source);
  result.target_group = std::move(target);
  return result;
}

auto when_destroyed(QString destroyed,
                    Command command,
                    QString source,
                    QString target) -> ArenaScenarioStep {
  ArenaScenarioStep result;
  result.name = QStringLiteral("after_%1_destroyed").arg(destroyed);
  result.trigger = {Trigger::GroupDestroyed, 0.0F, std::move(destroyed), {}, 0.0F};
  result.command = command;
  result.group = std::move(source);
  result.target_group = std::move(target);
  return result;
}

auto when_near(QString lhs,
               QString rhs,
               float distance,
               Command command) -> ArenaScenarioStep {
  ArenaScenarioStep result;
  result.name = QStringLiteral("when_%1_nears_%2").arg(lhs, rhs);
  result.trigger = {Trigger::GroupsWithinDistance, 0.0F, lhs, rhs, distance};
  result.command = command;
  result.group = std::move(lhs);
  result.target_group = std::move(rhs);
  return result;
}

auto expectation(Expect kind,
                 QString source = {},
                 QString target = {},
                 float threshold = 0.0F,
                 float start = 0.0F,
                 float distance = 0.0F) -> ArenaExpectation {
  ArenaExpectation result;
  result.kind = kind;
  result.group = std::move(source);
  result.target_group = std::move(target);
  result.threshold = threshold;
  result.start_seconds = start;
  result.distance = distance;
  return result;
}

void add_visual_stability(ArenaScenarioDefinition& scenario,
                          std::initializer_list<QString> groups) {
  for (auto const& name : groups) {
    scenario.expectations.push_back(expectation(Expect::NoPoseOscillation, name));
    scenario.expectations.push_back(expectation(Expect::NoRootTeleport, name));
    scenario.expectations.push_back(expectation(Expect::NoUnexpectedFallPose, name));
    scenario.expectations.push_back(expectation(Expect::MovementIsContinuous, name));
    scenario.expectations.push_back(expectation(Expect::GroupIsRendered, name));
  }
  scenario.expectations.push_back(
      expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F));
}

auto definition(QString id,
                QString label,
                QString description,
                float duration,
                ArenaCameraView camera = {}) -> ArenaScenarioDefinition {
  ArenaScenarioDefinition result;
  result.id = std::move(id);
  result.label = std::move(label);
  result.description = std::move(description);
  result.duration_seconds = duration;
  result.camera = camera;
  return result;
}

auto build_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;

  {
    auto s = definition(
        QString::fromLatin1(k_commander_aura_pulse_id),
        QStringLiteral("Commander Aura Pulse"),
        QStringLiteral(
            "Explicitly activates a short command aura, verifies nearby "
            "troops receive its visible timed bonus, verifies distant troops "
            "do not, and waits for cooldown."),
        5.0F,
        {26.0F, 55.0F, 25.0F});
    auto commander = group(QStringLiteral("commander"),
                           Troop::RomanVeteranConsul,
                           1,
                           1,
                           {0.0F, 0.0F, 0.0F},
                           1);
    commander.nation_id = Nation::RomanRepublic;
    auto near_troops = group(QStringLiteral("near_troops"),
                             Troop::Swordsman,
                             1,
                             2,
                             {5.0F, 0.0F, 0.0F},
                             4,
                             {0.0F, 0.0F, 3.0F});
    auto distant_troops = group(QStringLiteral("distant_troops"),
                                Troop::Swordsman,
                                1,
                                1,
                                {21.0F, 0.0F, 0.0F},
                                4);
    s.groups = {commander, near_troops, distant_troops};
    auto activate =
        at(0.5F, Command::TriggerCommanderAura, QStringLiteral("commander"));
    activate.value = 2;
    s.steps = {activate};
    s.expectations = {
        expectation(Expect::CommanderAuraActivated, QStringLiteral("commander")),
        expectation(Expect::CommanderAuraBuffObserved, QStringLiteral("near_troops")),
        expectation(Expect::NoCommanderAuraBuffObserved,
                    QStringLiteral("distant_troops")),
        expectation(Expect::CommanderAuraExpired, QStringLiteral("commander")),
        expectation(Expect::GroupIsRendered, QStringLiteral("commander")),
        expectation(Expect::GroupIsRendered, QStringLiteral("near_troops")),
        expectation(Expect::GroupIsRendered, QStringLiteral("distant_troops")),
        expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F)};
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_commander_identity_lineup_id),
        QStringLiteral("Commander Identity Lineup"),
        QStringLiteral("Displays all six commanders without bodyguards or supporting "
                       "units for direct silhouette, weapon, scale, color, and "
                       "ancient-dark-fantasy identity review."),
        12.0F,
        {9.8F, 36.0F, 0.0F});
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    struct CommanderLineupEntry {
      const char* group_name;
      Troop troop;
      Nation nation;
      int owner;
      QVector3D position;
      float facing;
    };
    const CommanderLineupEntry entries[] = {
        {"fabius",
         Troop::RomanLegionOrganizer,
         Nation::RomanRepublic,
         1,
         {-3.0F, 0.0F, -1.6F},
         0.0F},
        {"scipio",
         Troop::RomanVeteranConsul,
         Nation::RomanRepublic,
         2,
         {0.0F, 0.0F, -1.6F},
         0.0F},
        {"marcellus",
         Troop::RomanFieldCommander,
         Nation::RomanRepublic,
         3,
         {3.0F, 0.0F, -1.6F},
         0.0F},
        {"hanno",
         Troop::CarthageMercenaryBroker,
         Nation::Carthage,
         4,
         {-3.0F, 0.0F, 1.6F},
         0.0F},
        {"hasdrubal",
         Troop::CarthageCavalryPatron,
         Nation::Carthage,
         5,
         {0.0F, 0.0F, 1.6F},
         0.0F},
        {"hannibal",
         Troop::CarthageElephantMaster,
         Nation::Carthage,
         6,
         {3.0F, 0.0F, 1.6F},
         0.0F},
    };
    for (auto const& entry : entries) {
      auto commander = group(QString::fromLatin1(entry.group_name),
                             entry.troop,
                             entry.owner,
                             1,
                             entry.position,
                             1);
      commander.nation_id = entry.nation;
      commander.facing_degrees = entry.facing;
      s.groups.push_back(std::move(commander));
      s.expectations.push_back(
          expectation(Expect::GroupExists, QString::fromLatin1(entry.group_name)));
      s.expectations.push_back(
          expectation(Expect::GroupIsRendered, QString::fromLatin1(entry.group_name)));
    }
    s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F));
    result.push_back(std::move(s));
  }

  {
    struct DuelSpec {
      const char* id;
      const char* label;
      Troop roman;
      Troop carthaginian;
    };
    constexpr DuelSpec duels[] = {
        {k_commander_consul_vs_broker_id,
         "Consul vs Mercenary Broker",
         Troop::RomanVeteranConsul,
         Troop::CarthageMercenaryBroker},
        {k_commander_field_vs_cavalry_id,
         "Field Commander vs Cavalry Patron",
         Troop::RomanFieldCommander,
         Troop::CarthageCavalryPatron},
        {k_commander_legion_vs_elephant_id,
         "Legion Organizer vs Elephant Master",
         Troop::RomanLegionOrganizer,
         Troop::CarthageElephantMaster},
    };
    for (auto const& duel : duels) {
      auto s = definition(
          QString::fromLatin1(duel.id),
          QString::fromLatin1(duel.label),
          QStringLiteral("A durable one-on-one commander duel validating distinct "
                         "weapons, authored contact, reactions, attack cadence, and "
                         "readable silhouettes without bodyguards."),
          11.0F,
          {10.5F, 52.0F, 90.0F});
      s.suppress_terrain_scatter = true;
      s.select_spawned_units = false;
      s.suppress_spawn_anchor = true;
      s.suppress_ui_overlays = true;
      s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
      auto roman = group(
          QStringLiteral("roman_commander"), duel.roman, 1, 1, {0.0F, 0.0F, -4.5F}, 1);
      auto carthaginian = group(QStringLiteral("carthage_commander"),
                                duel.carthaginian,
                                2,
                                1,
                                {0.0F, 0.0F, 4.5F},
                                1);
      roman.health_override = roman.max_health_override = 1200;
      carthaginian.health_override = carthaginian.max_health_override = 1200;
      s.groups = {roman, carthaginian};
      s.steps = {
          at(0.4F,
             Command::Attack,
             QStringLiteral("roman_commander"),
             QStringLiteral("carthage_commander")),
          at(0.4F,
             Command::Attack,
             QStringLiteral("carthage_commander"),
             QStringLiteral("roman_commander")),
      };
      add_visual_stability(
          s, {QStringLiteral("roman_commander"), QStringLiteral("carthage_commander")});
      for (auto const& name :
           {QStringLiteral("roman_commander"), QStringLiteral("carthage_commander")}) {
        QString const opponent = name == QStringLiteral("roman_commander")
                                     ? QStringLiteral("carthage_commander")
                                     : QStringLiteral("roman_commander");
        s.expectations.push_back(expectation(Expect::AttackAnimationObserved, name));
        s.expectations.push_back(
            expectation(Expect::RepeatedAttackAnimationObserved, name, {}, 2.0F));
        s.expectations.push_back(
            expectation(Expect::AttackHasVisibleContact, name, opponent));
        s.expectations.push_back(expectation(Expect::HitReactionObserved, name));
      }
      result.push_back(std::move(s));
    }
  }

  {
    auto s = definition(QString::fromLatin1(k_sword_duel_id),
                        QStringLiteral("Sword Duel"),
                        QStringLiteral("Baseline reciprocal sword attack flow."),
                        8.0F,
                        {9.0F, 42.0F, 30.0F});
    s.groups = {
        group(QStringLiteral("blue"), Troop::Swordsman, 1, 1, {-1.4F, 0.0F, 0.0F}, 1),
        group(QStringLiteral("red"), Troop::Swordsman, 2, 1, {1.4F, 0.0F, 0.0F}, 1)};
    s.steps = {
        at(0.0F, Command::Attack, QStringLiteral("blue"), QStringLiteral("red")),
        at(0.0F, Command::Attack, QStringLiteral("red"), QStringLiteral("blue"))};
    add_visual_stability(s, {QStringLiteral("blue"), QStringLiteral("red")});
    s.expectations.push_back(
        expectation(Expect::NoLimbOverextension, QStringLiteral("blue")));
    s.expectations.push_back(
        expectation(Expect::NoLimbOverextension, QStringLiteral("red")));
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("blue"),
                                         QStringLiteral("red")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(QString::fromLatin1(k_spear_duel_id),
                        QStringLiteral("Spear Duel"),
                        QStringLiteral("Spear thrust, hit reaction, and recovery."),
                        8.0F,
                        {9.5F, 42.0F, 30.0F});
    s.groups = {
        group(QStringLiteral("blue"), Troop::Spearman, 1, 1, {-1.8F, 0.0F, 0.0F}, 1),
        group(QStringLiteral("red"), Troop::Spearman, 2, 1, {1.8F, 0.0F, 0.0F}, 1)};
    s.steps = {
        at(0.0F, Command::Attack, QStringLiteral("blue"), QStringLiteral("red")),
        at(0.0F, Command::Attack, QStringLiteral("red"), QStringLiteral("blue"))};
    add_visual_stability(s, {QStringLiteral("blue"), QStringLiteral("red")});
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("blue"),
                                         QStringLiteral("red")));
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_bow_exchange_id),
                   QStringLiteral("Bow Exchange"),
                   QStringLiteral("Bow draw, release, reload, and pose stability."),
                   10.0F,
                   {18.0F, 45.0F, 35.0F});
    s.groups = {
        group(QStringLiteral("blue_archers"),
              Troop::Archer,
              1,
              1,
              {-8.0F, 0.0F, 0.0F},
              6),
        group(
            QStringLiteral("red_archers"), Troop::Archer, 2, 1, {8.0F, 0.0F, 0.0F}, 6)};
    s.steps = {at(0.0F,
                  Command::Attack,
                  QStringLiteral("blue_archers"),
                  QStringLiteral("red_archers")),
               at(0.0F,
                  Command::Attack,
                  QStringLiteral("red_archers"),
                  QStringLiteral("blue_archers"))};

    s.steps[0].chase = true;
    s.steps[1].chase = true;
    add_visual_stability(
        s, {QStringLiteral("blue_archers"), QStringLiteral("red_archers")});
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("blue_archers")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_held_weapon_stances_id),
        QStringLiteral("Held Spear and Bow Stances"),
        QStringLiteral("Spearmen and archers stay fully kneeling with raised weapons "
                       "while combat remains active."),
        7.0F,
        {10.0F, 30.0F, 0.0F});
    s.groups = {
        group(QStringLiteral("held_spear"),
              Troop::Spearman,
              1,
              1,
              {-1.6F, 0.0F, -1.0F},
              1),
        group(QStringLiteral("spear_target"),
              Troop::Civilian,
              2,
              1,
              {-1.6F, 0.0F, 1.1F},
              1),
        group(
            QStringLiteral("held_archer"), Troop::Archer, 1, 1, {1.6F, 0.0F, -2.0F}, 1),
        group(QStringLiteral("archer_target"),
              Troop::Civilian,
              2,
              1,
              {1.6F, 0.0F, 2.0F},
              1),
    };
    s.groups[1].health_override = 5000;
    s.groups[1].max_health_override = 5000;
    s.groups[3].health_override = 5000;
    s.groups[3].max_health_override = 5000;
    s.steps = {
        at(0.0F, Command::Hold, QStringLiteral("held_spear")),
        at(0.0F, Command::Hold, QStringLiteral("held_archer")),
    };
    add_visual_stability(s,
                         {QStringLiteral("held_spear"), QStringLiteral("held_archer")});
    s.expectations.push_back(expectation(
        Expect::HoldPoseMaintained, QStringLiteral("held_spear"), {}, 0.0F, 1.5F));
    s.expectations.push_back(expectation(
        Expect::HoldPoseMaintained, QStringLiteral("held_archer"), {}, 0.0F, 1.5F));
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("held_spear")));
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("held_archer")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_mounted_charge_id),
        QStringLiteral("Mounted Charge"),
        QStringLiteral("Mounted approach and legitimate impact displacement."),
        12.0F,
        {22.0F, 48.0F, 20.0F});
    s.groups = {group(QStringLiteral("blue_cavalry"),
                      Troop::MountedKnight,
                      1,
                      2,
                      {0.0F, 0.0F, -10.0F},
                      4),
                group(QStringLiteral("red_infantry"),
                      Troop::Swordsman,
                      2,
                      2,
                      {0.0F, 0.0F, 10.0F},
                      4)};
    s.steps = {at(0.0F,
                  Command::Charge,
                  QStringLiteral("blue_cavalry"),
                  QStringLiteral("red_infantry")),
               when_near(QStringLiteral("blue_cavalry"),
                         QStringLiteral("red_infantry"),
                         4.5F,
                         Command::SetCamera)};
    s.steps.back().camera_distance = 14.0F;
    s.steps.back().camera_angle = 48.0F;
    s.steps.back().camera_yaw = 20.0F;
    add_visual_stability(
        s, {QStringLiteral("blue_cavalry"), QStringLiteral("red_infantry")});
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("blue_cavalry"), {}, 0.45F));
    s.expectations.push_back(
        expectation(Expect::DeathAnimationObserved, QStringLiteral("red_infantry")));
    s.expectations.push_back(
        expectation(Expect::LaunchedCasualtyObserved, QStringLiteral("red_infantry")));
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("blue_cavalry"),
                                         QStringLiteral("red_infantry")));
    s.expectations.push_back(expectation(Expect::ChargeImpactPrecedesMeleeLock,
                                         QStringLiteral("blue_cavalry")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_braced_spear_charge_id),
        QStringLiteral("Charge Into Braced Spears"),
        QStringLiteral("Held spears inflict catastrophic losses before melee lock."),
        12.0F,
        {22.0F, 48.0F, 20.0F});
    s.groups = {group(QStringLiteral("cavalry"),
                      Troop::MountedKnight,
                      1,
                      1,
                      {0.0F, 0.0F, -10.0F},
                      4),
                group(QStringLiteral("braced_spears"),
                      Troop::Spearman,
                      2,
                      1,
                      {0.0F, 0.0F, 10.0F},
                      4)};
    s.steps = {at(0.0F, Command::Hold, QStringLiteral("braced_spears")),
               at(0.0F,
                  Command::Charge,
                  QStringLiteral("cavalry"),
                  QStringLiteral("braced_spears")),
               when_near(QStringLiteral("cavalry"),
                         QStringLiteral("braced_spears"),
                         4.5F,
                         Command::SetCamera)};
    s.steps.back().camera_distance = 14.0F;
    s.steps.back().camera_angle = 48.0F;
    s.steps.back().camera_yaw = 20.0F;
    add_visual_stability(s,
                         {QStringLiteral("cavalry"), QStringLiteral("braced_spears")});
    s.expectations.push_back(
        expectation(Expect::DeathAnimationObserved, QStringLiteral("cavalry")));
    s.expectations.push_back(
        expectation(Expect::LaunchedCasualtyObserved, QStringLiteral("cavalry")));
    s.expectations.push_back(expectation(Expect::NoLaunchedCasualtyObserved,
                                         QStringLiteral("braced_spears")));
    s.expectations.push_back(
        expectation(Expect::ChargeImpactPrecedesMeleeLock, QStringLiteral("cavalry")));
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_elephant_trample_id),
                   QStringLiteral("Elephant Trample Impact"),
                   QStringLiteral("A war elephant tramples infantry and launches fresh "
                                  "casualties away from the impact."),
                   10.0F,
                   {18.0F, 47.0F, 18.0F});
    s.groups = {
        group(
            QStringLiteral("elephant"), Troop::Elephant, 1, 1, {0.0F, 0.0F, -6.0F}, 1),
        group(
            QStringLiteral("infantry"), Troop::Swordsman, 2, 1, {0.0F, 0.0F, 2.0F}, 8)};
    // Keep enough health for repeated trunk/leg strikes and a launched casualty,
    // but guarantee a no-opponent tail where the elephant must recover to idle.
    s.groups[1].health_override = 150;
    s.groups[1].max_health_override = 150;
    s.steps = {at(0.0F,
                  Command::AttackMove,
                  QStringLiteral("elephant"),
                  QStringLiteral("infantry"))};
    add_visual_stability(s, {QStringLiteral("infantry")});
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("elephant")));
    s.expectations.push_back(expectation(
        Expect::FormationBodyOverlapObserved, QStringLiteral("elephant"), {}, 1.10F));
    s.expectations.push_back(
        expectation(Expect::DeathAnimationObserved, QStringLiteral("infantry")));
    s.expectations.push_back(
        expectation(Expect::LaunchedCasualtyObserved, QStringLiteral("infantry")));
    s.expectations.push_back(
        expectation(Expect::GroupDestroyed, QStringLiteral("infantry")));
    s.expectations.push_back(
        expectation(Expect::NoActiveCombatAtEnd, QStringLiteral("elephant")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_catapult_impact_id),
        QStringLiteral("Catapult Infantry Impact"),
        QStringLiteral("A catapult stone hits an infantry formation and launches "
                       "fresh casualties from the impact."),
        12.0F,
        {22.0F, 50.0F, 12.0F});
    s.groups = {
        group(
            QStringLiteral("catapult"), Troop::Catapult, 1, 1, {0.0F, 0.0F, -9.0F}, 1),
        group(
            QStringLiteral("infantry"), Troop::Spearman, 2, 1, {0.0F, 0.0F, 4.0F}, 8)};
    s.groups[1].health_override = 240;
    s.groups[1].max_health_override = 240;
    s.steps = {at(0.0F, Command::Hold, QStringLiteral("infantry")),
               at(0.0F,
                  Command::Attack,
                  QStringLiteral("catapult"),
                  QStringLiteral("infantry"))};
    add_visual_stability(s, {QStringLiteral("infantry")});
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("catapult")));
    s.expectations.push_back(
        expectation(Expect::DeathAnimationObserved, QStringLiteral("infantry")));
    s.expectations.push_back(
        expectation(Expect::LaunchedCasualtyObserved, QStringLiteral("infantry")));
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_mounted_sword_duel_id),
                   QStringLiteral("Mounted Sword Duel"),
                   QStringLiteral("Mounted sword chamber, full swing, and recovery."),
                   10.0F,
                   {13.0F, 43.0F, 28.0F});
    s.groups = {group(QStringLiteral("blue_knights"),
                      Troop::MountedKnight,
                      1,
                      1,
                      {-3.0F, 0.0F, 0.0F},
                      4),
                group(QStringLiteral("red_knights"),
                      Troop::MountedKnight,
                      2,
                      1,
                      {3.0F, 0.0F, 0.0F},
                      4)};
    s.steps = {at(0.0F,
                  Command::Attack,
                  QStringLiteral("blue_knights"),
                  QStringLiteral("red_knights")),
               at(0.0F,
                  Command::Attack,
                  QStringLiteral("red_knights"),
                  QStringLiteral("blue_knights"))};
    add_visual_stability(
        s, {QStringLiteral("blue_knights"), QStringLiteral("red_knights")});
    s.expectations.push_back(
        expectation(Expect::NoLimbOverextension, QStringLiteral("blue_knights")));
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("blue_knights"),
                                         QStringLiteral("red_knights")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(QString::fromLatin1(k_mounted_spear_duel_id),
                        QStringLiteral("Mounted Spear Duel"),
                        QStringLiteral("Mounted couch, downward thrust, and recovery."),
                        10.0F,
                        {14.0F, 44.0F, 28.0F});
    s.groups = {group(QStringLiteral("blue_spears"),
                      Troop::HorseSpearman,
                      1,
                      1,
                      {-3.8F, 0.0F, 0.0F},
                      4),
                group(QStringLiteral("red_spears"),
                      Troop::HorseSpearman,
                      2,
                      1,
                      {3.8F, 0.0F, 0.0F},
                      4)};
    s.steps = {at(0.0F,
                  Command::Attack,
                  QStringLiteral("blue_spears"),
                  QStringLiteral("red_spears")),
               at(0.0F,
                  Command::Attack,
                  QStringLiteral("red_spears"),
                  QStringLiteral("blue_spears"))};
    add_visual_stability(s,
                         {QStringLiteral("blue_spears"), QStringLiteral("red_spears")});
    s.expectations.push_back(
        expectation(Expect::NoLimbOverextension, QStringLiteral("blue_spears")));
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("blue_spears"),
                                         QStringLiteral("red_spears")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_mounted_bow_exchange_id),
        QStringLiteral("Mounted Bow Exchange"),
        QStringLiteral("Mounted bow raise, draw, release, and riding stability."),
        12.0F,
        {20.0F, 46.0F, 30.0F});
    s.groups = {group(QStringLiteral("blue_archers"),
                      Troop::HorseArcher,
                      1,
                      1,
                      {-9.0F, 0.0F, 0.0F},
                      6),
                group(QStringLiteral("red_archers"),
                      Troop::HorseArcher,
                      2,
                      1,
                      {9.0F, 0.0F, 0.0F},
                      6)};
    s.steps = {at(0.0F,
                  Command::Attack,
                  QStringLiteral("blue_archers"),
                  QStringLiteral("red_archers")),
               at(0.0F,
                  Command::Attack,
                  QStringLiteral("red_archers"),
                  QStringLiteral("blue_archers"))};
    add_visual_stability(
        s, {QStringLiteral("blue_archers"), QStringLiteral("red_archers")});
    s.expectations.push_back(
        expectation(Expect::NoLimbOverextension, QStringLiteral("blue_archers")));
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("blue_archers")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_infantry_locomotion_matrix_id),
        QStringLiteral("Infantry Locomotion Matrix"),
        QStringLiteral(
            "Archers, swordsmen, and spearmen start, march, stop, and reverse."),
        13.0F,
        {26.0F, 48.0F, 30.0F});
    s.groups = {
        group(QStringLiteral("archers"), Troop::Archer, 1, 1, {-6.0F, 0.0F, -9.0F}, 8),
        group(QStringLiteral("swords"), Troop::Swordsman, 1, 1, {0.0F, 0.0F, -9.0F}, 8),
        group(QStringLiteral("spears"), Troop::Spearman, 1, 1, {6.0F, 0.0F, -9.0F}, 8)};
    s.steps = {at(0.5F, Command::FormationMove, QStringLiteral("archers")),
               at(0.5F, Command::FormationMove, QStringLiteral("swords")),
               at(0.5F, Command::FormationMove, QStringLiteral("spears")),
               at(6.0F, Command::Stop, QStringLiteral("archers")),
               at(6.0F, Command::Stop, QStringLiteral("swords")),
               at(6.0F, Command::Stop, QStringLiteral("spears")),
               at(8.0F, Command::FormationMove, QStringLiteral("archers")),
               at(8.0F, Command::FormationMove, QStringLiteral("swords")),
               at(8.0F, Command::FormationMove, QStringLiteral("spears"))};
    for (int lane = 0; lane < 3; ++lane) {
      s.steps[static_cast<std::size_t>(lane)].destination = {
          -6.0F + 6.0F * static_cast<float>(lane), 0.0F, 8.0F};
      s.steps[static_cast<std::size_t>(lane + 6)].destination = {
          -6.0F + 6.0F * static_cast<float>(lane), 0.0F, -16.0F};
    }
    add_visual_stability(s,
                         {QStringLiteral("archers"),
                          QStringLiteral("swords"),
                          QStringLiteral("spears")});
    for (auto const& name : {QStringLiteral("archers"),
                             QStringLiteral("swords"),
                             QStringLiteral("spears")}) {
      s.expectations.push_back(
          expectation(Expect::AllGroupsRespondWithin, name, {}, 0.45F));
      s.expectations.push_back(expectation(Expect::NoLimbOverextension, name));
      s.expectations.push_back(
          expectation(Expect::FormationOrderPreserved, name, {}, 0.8F));
    }
    result.push_back(std::move(s));
  }

  {
    auto s = definition(QString::fromLatin1(k_mounted_locomotion_matrix_id),
                        QStringLiteral("Mounted Locomotion Matrix"),
                        QStringLiteral("Horse archers, knights, and horse spearmen "
                                       "start, ride, stop, and reverse."),
                        13.0F,
                        {30.0F, 50.0F, 30.0F});
    s.groups = {group(QStringLiteral("horse_archers"),
                      Troop::HorseArcher,
                      1,
                      1,
                      {-7.0F, 0.0F, -10.0F},
                      6),
                group(QStringLiteral("knights"),
                      Troop::MountedKnight,
                      1,
                      1,
                      {0.0F, 0.0F, -10.0F},
                      6),
                group(QStringLiteral("horse_spears"),
                      Troop::HorseSpearman,
                      1,
                      1,
                      {7.0F, 0.0F, -10.0F},
                      6)};
    s.steps = {at(0.5F, Command::FormationMove, QStringLiteral("horse_archers")),
               at(0.5F, Command::FormationMove, QStringLiteral("knights")),
               at(0.5F, Command::FormationMove, QStringLiteral("horse_spears")),
               at(6.0F, Command::Stop, QStringLiteral("horse_archers")),
               at(6.0F, Command::Stop, QStringLiteral("knights")),
               at(6.0F, Command::Stop, QStringLiteral("horse_spears")),
               at(8.0F, Command::FormationMove, QStringLiteral("horse_archers")),
               at(8.0F, Command::FormationMove, QStringLiteral("knights")),
               at(8.0F, Command::FormationMove, QStringLiteral("horse_spears"))};
    for (int lane = 0; lane < 3; ++lane) {
      s.steps[static_cast<std::size_t>(lane)].destination = {
          -7.0F + 7.0F * static_cast<float>(lane), 0.0F, 9.0F};
      s.steps[static_cast<std::size_t>(lane + 6)].destination = {
          -7.0F + 7.0F * static_cast<float>(lane), 0.0F, -20.0F};
    }
    add_visual_stability(s,
                         {QStringLiteral("horse_archers"),
                          QStringLiteral("knights"),
                          QStringLiteral("horse_spears")});
    for (auto const& name : {QStringLiteral("horse_archers"),
                             QStringLiteral("knights"),
                             QStringLiteral("horse_spears")}) {
      s.expectations.push_back(
          expectation(Expect::AllGroupsRespondWithin, name, {}, 0.45F));
      s.expectations.push_back(expectation(Expect::NoLimbOverextension, name));
      s.expectations.push_back(
          expectation(Expect::FormationOrderPreserved, name, {}, 0.8F));
    }
    result.push_back(std::move(s));
  }

  {
    auto s = definition(QString::fromLatin1(k_elephant_locomotion_matrix_id),
                        QStringLiteral("Elephant Locomotion Matrix"),
                        QStringLiteral("War elephants start, travel, stop, and reverse "
                                       "using the production authored skin."),
                        13.0F,
                        {24.0F, 48.0F, 28.0F});
    s.groups = {group(QStringLiteral("elephants"),
                      Troop::Elephant,
                      2,
                      3,
                      {0.0F, 0.0F, -10.0F},
                      1,
                      {5.5F, 0.0F, 0.0F})};
    s.steps = {at(0.5F, Command::FormationMove, QStringLiteral("elephants")),
               at(6.0F, Command::Stop, QStringLiteral("elephants")),
               at(8.0F, Command::FormationMove, QStringLiteral("elephants"))};
    s.steps[0].destination = {0.0F, 0.0F, 9.0F};
    s.steps[2].destination = {0.0F, 0.0F, -20.0F};
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("elephants")));
    s.expectations.push_back(
        expectation(Expect::NoRootTeleport, QStringLiteral("elephants")));
    s.expectations.push_back(
        expectation(Expect::MovementIsContinuous, QStringLiteral("elephants")));
    s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F));
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("elephants"), {}, 0.45F));
    s.expectations.push_back(expectation(
        Expect::FormationOrderPreserved, QStringLiteral("elephants"), {}, 0.8F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(QString::fromLatin1(k_infantry_damage_matrix_id),
                        QStringLiteral("Infantry Damage Matrix"),
                        QStringLiteral("Archers, swordsmen, and spearmen absorb a hit "
                                       "and enter stable death sequences."),
                        10.0F,
                        {19.0F, 46.0F, 30.0F});
    auto archers =
        group(QStringLiteral("archers"), Troop::Archer, 1, 1, {-5.0F, 0.0F, 0.0F}, 1);
    auto swords =
        group(QStringLiteral("swords"), Troop::Swordsman, 1, 1, {0.0F, 0.0F, 0.0F}, 1);
    auto spears =
        group(QStringLiteral("spears"), Troop::Spearman, 1, 1, {5.0F, 0.0F, 0.0F}, 1);
    for (auto* troop : {&archers, &swords, &spears}) {
      troop->nation_id = Nation::Carthage;
      troop->owner_id = 2;
      troop->facing_degrees = 180.0F;
      troop->health_override = 30;
      troop->max_health_override = 30;
    }
    archers.origin.setZ(1.8F);
    swords.origin.setZ(1.8F);
    spears.origin.setZ(1.8F);
    s.groups = {archers,
                swords,
                spears,
                group(QStringLiteral("attack_archers"),
                      Troop::Swordsman,
                      1,
                      1,
                      {-5.0F, 0.0F, -1.8F},
                      1),
                group(QStringLiteral("attack_swords"),
                      Troop::Swordsman,
                      1,
                      1,
                      {0.0F, 0.0F, -1.8F},
                      1),
                group(QStringLiteral("attack_spears"),
                      Troop::Swordsman,
                      1,
                      1,
                      {5.0F, 0.0F, -1.8F},
                      1)};
    s.steps = {at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("attack_archers"),
                  QStringLiteral("archers")),
               at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("attack_swords"),
                  QStringLiteral("swords")),
               at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("attack_spears"),
                  QStringLiteral("spears"))};
    add_visual_stability(s,
                         {QStringLiteral("archers"),
                          QStringLiteral("swords"),
                          QStringLiteral("spears")});
    for (auto const& name : {QStringLiteral("archers"),
                             QStringLiteral("swords"),
                             QStringLiteral("spears")}) {
      s.expectations.push_back(expectation(Expect::DeathAnimationObserved, name));
      s.expectations.push_back(expectation(Expect::HitReactionObserved, name));
      s.expectations.push_back(expectation(Expect::NoLimbOverextension, name));
    }
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_mounted_damage_matrix_id),
                   QStringLiteral("Mounted Damage Matrix"),
                   QStringLiteral("Horse archers, knights, and horse spearmen absorb a "
                                  "hit and dismount through stable death sequences."),
                   10.0F,
                   {23.0F, 48.0F, 30.0F});
    auto archers = group(QStringLiteral("horse_archers"),
                         Troop::HorseArcher,
                         1,
                         1,
                         {-6.0F, 0.0F, 0.0F},
                         1);
    auto knights = group(
        QStringLiteral("knights"), Troop::MountedKnight, 1, 1, {0.0F, 0.0F, 0.0F}, 1);
    auto spears = group(QStringLiteral("horse_spears"),
                        Troop::HorseSpearman,
                        1,
                        1,
                        {6.0F, 0.0F, 0.0F},
                        1);
    for (auto* troop : {&archers, &knights, &spears}) {
      troop->nation_id = Nation::Carthage;
      troop->owner_id = 2;
      troop->facing_degrees = 180.0F;
      troop->health_override = 40;
      troop->max_health_override = 40;
    }
    archers.origin.setZ(2.4F);
    knights.origin.setZ(2.4F);
    spears.origin.setZ(2.4F);
    s.groups = {archers,
                knights,
                spears,
                group(QStringLiteral("attack_archers"),
                      Troop::MountedKnight,
                      1,
                      1,
                      {-6.0F, 0.0F, -2.4F},
                      1),
                group(QStringLiteral("attack_knights"),
                      Troop::MountedKnight,
                      1,
                      1,
                      {0.0F, 0.0F, -2.4F},
                      1),
                group(QStringLiteral("attack_spears"),
                      Troop::MountedKnight,
                      1,
                      1,
                      {6.0F, 0.0F, -2.4F},
                      1)};
    s.steps = {at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("attack_archers"),
                  QStringLiteral("horse_archers")),
               at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("attack_knights"),
                  QStringLiteral("knights")),
               at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("attack_spears"),
                  QStringLiteral("horse_spears"))};
    add_visual_stability(s,
                         {QStringLiteral("horse_archers"),
                          QStringLiteral("knights"),
                          QStringLiteral("horse_spears")});
    for (auto const& name : {QStringLiteral("horse_archers"),
                             QStringLiteral("knights"),
                             QStringLiteral("horse_spears")}) {
      s.expectations.push_back(expectation(Expect::DeathAnimationObserved, name));
      s.expectations.push_back(expectation(Expect::HitReactionObserved, name));
      s.expectations.push_back(expectation(Expect::NoLimbOverextension, name));
    }
    result.push_back(std::move(s));
  }

  struct ActionTransitionSpec {
    const char* id;
    const char* label;
    Troop troop;
    bool mounted;
  };
  for (auto const& spec : {ActionTransitionSpec{k_archer_action_transition_id,
                                                "Archer Action Transition",
                                                Troop::Archer,
                                                false},
                           ActionTransitionSpec{k_swordsman_action_transition_id,
                                                "Swordsman Action Transition",
                                                Troop::Swordsman,
                                                false},
                           ActionTransitionSpec{k_spearman_action_transition_id,
                                                "Spearman Action Transition",
                                                Troop::Spearman,
                                                false},
                           ActionTransitionSpec{k_horse_archer_action_transition_id,
                                                "Horse Archer Action Transition",
                                                Troop::HorseArcher,
                                                true},
                           ActionTransitionSpec{k_mounted_knight_action_transition_id,
                                                "Mounted Knight Action Transition",
                                                Troop::MountedKnight,
                                                true},
                           ActionTransitionSpec{k_horse_spearman_action_transition_id,
                                                "Horse Spearman Action Transition",
                                                Troop::HorseSpearman,
                                                true}}) {
    auto s = definition(
        QString::fromLatin1(spec.id),
        QString::fromLatin1(spec.label),
        QStringLiteral("Move, acquire one target, complete the authored attack and "
                       "recovery, then return to player-controlled locomotion."),
        10.0F,
        {spec.mounted ? 15.0F : 12.0F, 46.0F, 28.0F});
    auto target =
        group(QStringLiteral("target"), Troop::Healer, 2, 1, {0.0F, 0.0F, 2.0F}, 1);
    target.health_override = 8;
    target.max_health_override = 8;
    s.groups = {group(QStringLiteral("actor"),
                      spec.troop,
                      1,
                      1,
                      {0.0F, 0.0F, spec.mounted ? -9.0F : -8.0F},
                      1),
                target};
    s.steps = {at(0.0F, Command::Move, QStringLiteral("actor")),
               at(2.0F,
                  Command::AttackMove,
                  QStringLiteral("actor"),
                  QStringLiteral("target")),
               when_destroyed(QStringLiteral("target"),
                              Command::FormationMove,
                              QStringLiteral("actor"),
                              {})};
    s.steps[0].destination = {0.0F, 0.0F, -3.0F};
    s.steps[2].destination = {0.0F, 0.0F, -8.0F};
    add_visual_stability(s, {QStringLiteral("actor")});
    s.expectations.push_back(
        expectation(Expect::MovementAnimationObserved, QStringLiteral("actor")));
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("actor")));
    s.expectations.push_back(
        expectation(Expect::AttackRecoveryObserved, QStringLiteral("actor")));
    s.expectations.push_back(
        expectation(Expect::NoLimbOverextension, QStringLiteral("actor")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_melee_lock_id),
        QStringLiteral("Melee Lock"),
        QStringLiteral("Reciprocal melee-lock ownership and animation flow."),
        7.0F,
        {8.0F, 40.0F, 28.0F});
    s.groups = {
        group(QStringLiteral("blue"), Troop::Swordsman, 1, 1, {-0.8F, 0.0F, 0.0F}, 1),
        group(QStringLiteral("red"), Troop::Swordsman, 2, 1, {0.8F, 0.0F, 0.0F}, 1)};
    s.steps = {
        at(0.0F, Command::MeleeLock, QStringLiteral("blue"), QStringLiteral("red")),
        at(0.0F, Command::MeleeLock, QStringLiteral("red"), QStringLiteral("blue"))};
    add_visual_stability(s, {QStringLiteral("blue"), QStringLiteral("red")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_chase_to_attack_id),
        QStringLiteral("Chase To Attack"),
        QStringLiteral("Out-of-range attacker chases into a visible strike."),
        12.0F,
        {17.0F, 46.0F, 20.0F});
    s.groups = {
        group(QStringLiteral("attacker"),
              Troop::Swordsman,
              1,
              1,
              {0.0F, 0.0F, -10.0F},
              6),
        group(
            QStringLiteral("defender"), Troop::Swordsman, 2, 1, {0.0F, 0.0F, 4.0F}, 6)};
    s.steps = {at(0.0F,
                  Command::AttackMove,
                  QStringLiteral("attacker"),
                  QStringLiteral("defender"))};
    add_visual_stability(s, {QStringLiteral("attacker"), QStringLiteral("defender")});
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("attacker"), {}, 0.45F));
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("attacker"),
                                         QStringLiteral("defender")));
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_attack_to_chase_id),
                   QStringLiteral("Attack To Chase"),
                   QStringLiteral("Target disengages and attacker returns to chase."),
                   10.0F,
                   {14.0F, 42.0F, 28.0F});
    s.groups = {
        group(
            QStringLiteral("attacker"), Troop::Swordsman, 1, 1, {0.0F, 0.0F, -2.0F}, 4),
        group(QStringLiteral("runner"), Troop::Swordsman, 2, 1, {0.0F, 0.0F, 1.6F}, 4)};
    s.steps = {at(0.0F,
                  Command::AttackMove,
                  QStringLiteral("attacker"),
                  QStringLiteral("runner")),
               at(0.85F, Command::Move, QStringLiteral("runner"))};
    s.steps.back().destination = {0.0F, 0.0F, 10.0F};
    add_visual_stability(s, {QStringLiteral("attacker"), QStringLiteral("runner")});
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("runner"), {}, 0.45F));
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_target_death_id),
                   QStringLiteral("Target Death"),
                   QStringLiteral("Normal damage path into a stable death sequence."),
                   8.0F,
                   {10.0F, 42.0F, 28.0F});
    auto target =
        group(QStringLiteral("target"), Troop::Healer, 2, 1, {0.0F, 0.0F, 1.8F}, 1);
    target.health_override = 6;
    target.max_health_override = 6;
    s.groups = {
        group(
            QStringLiteral("attacker"), Troop::Swordsman, 1, 1, {0.0F, 0.0F, -2.0F}, 1),
        target};
    s.steps = {at(0.0F,
                  Command::AttackMove,
                  QStringLiteral("attacker"),
                  QStringLiteral("target"))};
    add_visual_stability(s, {QStringLiteral("attacker"), QStringLiteral("target")});
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("attacker"),
                                         QStringLiteral("target")));
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_retargeting_id),
                   QStringLiteral("Retargeting"),
                   QStringLiteral("Attacker kills one target and reacquires another."),
                   12.0F,
                   {13.0F, 42.0F, 28.0F});
    auto primary =
        group(QStringLiteral("primary"), Troop::Healer, 2, 1, {-0.9F, 0.0F, 1.8F}, 1);
    primary.health_override = 6;
    primary.max_health_override = 6;
    s.groups = {
        group(
            QStringLiteral("attacker"), Troop::Swordsman, 1, 1, {0.0F, 0.0F, -2.0F}, 1),
        primary,
        group(QStringLiteral("secondary"),
              Troop::Swordsman,
              2,
              1,
              {2.4F, 0.0F, 2.8F},
              1)};
    s.steps = {at(0.0F,
                  Command::AttackMove,
                  QStringLiteral("attacker"),
                  QStringLiteral("primary")),
               when_destroyed(QStringLiteral("primary"),
                              Command::AttackMove,
                              QStringLiteral("attacker"),
                              QStringLiteral("secondary"))};
    add_visual_stability(s,
                         {QStringLiteral("attacker"),
                          QStringLiteral("primary"),
                          QStringLiteral("secondary")});
    s.expectations.push_back(expectation(Expect::TargetRetakenAfterDeath,
                                         QStringLiteral("attacker"),
                                         QStringLiteral("secondary")));
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_hold_guard_exit_id),
                   QStringLiteral("Hold / Guard Exit"),
                   QStringLiteral("Hold and guard units exit stance under pressure."),
                   10.0F,
                   {14.0F, 44.0F, 32.0F});
    s.groups = {
        group(QStringLiteral("hold"), Troop::Spearman, 1, 1, {-1.8F, 0.0F, 0.0F}, 4),
        group(QStringLiteral("guard"), Troop::Swordsman, 1, 1, {1.8F, 0.0F, 0.0F}, 4),
        group(QStringLiteral("enemy"), Troop::Swordsman, 2, 1, {0.0F, 0.0F, 7.0F}, 6)};
    s.steps = {
        at(0.0F, Command::Hold, QStringLiteral("hold")),
        at(0.0F, Command::Guard, QStringLiteral("guard"), QStringLiteral("hold")),
        at(0.0F, Command::AttackMove, QStringLiteral("enemy"), QStringLiteral("hold")),
        at(1.0F, Command::Hold, QStringLiteral("hold")),
        at(1.0F, Command::Guard, QStringLiteral("guard"), QStringLiteral("hold")),
        at(1.0F,
           Command::AttackMove,
           QStringLiteral("guard"),
           QStringLiteral("enemy"))};
    s.steps[3].enabled = false;
    s.steps[4].enabled = false;
    add_visual_stability(
        s, {QStringLiteral("hold"), QStringLiteral("guard"), QStringLiteral("enemy")});
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_lod_switch_id),
                   QStringLiteral("LOD Switch"),
                   QStringLiteral("Dense battle with near/far camera LOD transitions."),
                   8.0F,
                   {34.0F, 52.0F, 24.0F});
    s.groups = {group(QStringLiteral("blue"),
                      Troop::Swordsman,
                      1,
                      5,
                      {0.0F, 0.0F, -10.0F},
                      20,
                      {5.0F, 0.0F, 0.0F}),
                group(QStringLiteral("red"),
                      Troop::Swordsman,
                      2,
                      5,
                      {0.0F, 0.0F, 10.0F},
                      20,
                      {5.0F, 0.0F, 0.0F})};
    s.steps = {
        at(0.0F, Command::SetFullCreatureLod),
        at(0.0F, Command::AttackMove, QStringLiteral("blue"), QStringLiteral("red")),
        at(0.0F, Command::AttackMove, QStringLiteral("red"), QStringLiteral("blue")),
        at(1.5F, Command::SetCamera),
        at(3.0F, Command::SetCamera)};
    s.steps[0].enabled = false;
    s.steps[3].camera_distance = 12.0F;
    s.steps[3].camera_angle = 44.0F;
    s.steps[3].camera_yaw = 28.0F;
    s.steps[4].camera_distance = 34.0F;
    s.steps[4].camera_angle = 52.0F;
    s.steps[4].camera_yaw = 24.0F;
    add_visual_stability(s, {QStringLiteral("blue"), QStringLiteral("red")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_three_swords_vs_two_spears_id),
        QStringLiteral("3 Swords vs 2 Spears"),
        QStringLiteral("Unequal groups meet; no eligible unit may stay idle."),
        14.0F,
        {22.0F, 48.0F, 28.0F});
    s.groups = {
        group(
            QStringLiteral("blue_swords"), Troop::Swordsman, 1, 3, {0.0F, 0.0F, -9.0F}),
        group(QStringLiteral("red_spears"), Troop::Spearman, 2, 2, {0.0F, 0.0F, 9.0F})};
    s.steps = {at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("blue_swords"),
                  QStringLiteral("red_spears")),
               at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("red_spears"),
                  QStringLiteral("blue_swords"))};
    add_visual_stability(s,
                         {QStringLiteral("blue_swords"), QStringLiteral("red_spears")});
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("blue_swords"), {}, 0.45F));
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("red_spears"), {}, 0.45F));
    s.expectations.push_back(expectation(Expect::NoEligibleTroopIdleDuringCombat,
                                         QStringLiteral("blue_swords"),
                                         QStringLiteral("red_spears"),
                                         1.25F,
                                         1.0F,
                                         8.0F));
    s.expectations.push_back(expectation(Expect::NoEligibleTroopIdleDuringCombat,
                                         QStringLiteral("red_spears"),
                                         QStringLiteral("blue_swords"),
                                         1.25F,
                                         1.0F,
                                         8.0F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_spear_walk_contact_id),
        QStringLiteral("Spear Walk Contact"),
        QStringLiteral("Spearmen walk into swords without root jets or falls."),
        12.0F,
        {19.0F, 46.0F, 30.0F});
    s.groups = {
        group(QStringLiteral("spears"), Troop::Spearman, 1, 2, {0.0F, 0.0F, -8.0F}),
        group(QStringLiteral("swords"), Troop::Swordsman, 2, 2, {0.0F, 0.0F, 4.0F})};
    s.steps = {
        at(0.25F, Command::FormationMove, QStringLiteral("spears")),
        at(2.0F,
           Command::AttackMove,
           QStringLiteral("spears"),
           QStringLiteral("swords")),
        at(2.0F, Command::Attack, QStringLiteral("swords"), QStringLiteral("spears"))};
    s.steps[0].destination = {0.0F, 0.0F, 0.0F};
    add_visual_stability(s, {QStringLiteral("spears"), QStringLiteral("swords")});
    s.expectations.push_back(
        expectation(Expect::NoLimbOverextension, QStringLiteral("spears")));
    s.expectations.push_back(expectation(
        Expect::FormationOrderPreserved, QStringLiteral("spears"), {}, 0.8F));
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("spears"),
                                         QStringLiteral("swords")));
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("swords"),
                                         QStringLiteral("spears")));
    s.expectations.push_back(
        expectation(Expect::FormationEngagementIsStable, QStringLiteral("spears")));
    s.expectations.push_back(
        expectation(Expect::FormationEngagementIsStable, QStringLiteral("swords")));
    s.expectations.push_back(
        expectation(Expect::AllLivingSoldiersFight, QStringLiteral("spears")));
    s.expectations.push_back(
        expectation(Expect::AllLivingSoldiersFight, QStringLiteral("swords")));
    s.expectations.push_back(
        expectation(Expect::CombatIndicatorIsContinuous, QStringLiteral("spears")));
    s.expectations.push_back(
        expectation(Expect::CombatIndicatorIsContinuous, QStringLiteral("swords")));
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_archer_stability_id),
                   QStringLiteral("Archer Stability"),
                   QStringLiteral("Three standing archers fire without pose epilepsy."),
                   12.0F,
                   {23.0F, 48.0F, 35.0F});
    s.groups = {
        group(QStringLiteral("archers"), Troop::Archer, 1, 3, {0.0F, 0.0F, -7.0F}, 10),
        group(QStringLiteral("infantry"),
              Troop::Swordsman,
              2,
              2,
              {0.0F, 0.0F, 7.0F},
              10)};
    for (auto& infantry : s.groups) {
      if (infantry.name == QStringLiteral("infantry")) {
        infantry.health_override = 2000;
        infantry.max_health_override = 2000;
      }
    }
    s.steps = {at(
        0.5F, Command::Attack, QStringLiteral("archers"), QStringLiteral("infantry"))};
    add_visual_stability(s, {QStringLiteral("archers"), QStringLiteral("infantry")});
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("archers"),
                                         QStringLiteral("infantry")));
    s.expectations.push_back(expectation(
        Expect::RepeatedAttackAnimationObserved, QStringLiteral("archers"), {}, 2.0F));
    s.expectations.push_back(expectation(
        Expect::FormationOrderPreserved, QStringLiteral("archers"), {}, 0.9F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_archer_melee_lock_id),
        QStringLiteral("Archers Forced Into Melee"),
        QStringLiteral("Swordsmen close through bow fire; locked archers stop "
                       "shooting and fight with a distinct two-handed bow strike."),
        12.0F,
        {18.0F, 48.0F, 24.0F});
    s.groups = {
        group(QStringLiteral("swords"), Troop::Swordsman, 1, 2, {0.0F, 0.0F, -7.0F}, 6),
        group(QStringLiteral("archers"), Troop::Archer, 2, 2, {0.0F, 0.0F, 4.0F}, 6)};
    for (auto& formation : s.groups) {
      formation.health_override = 1200;
      formation.max_health_override = 1200;
    }
    s.steps = {at(0.0F, Command::Hold, QStringLiteral("archers")),
               at(0.35F,
                  Command::Attack,
                  QStringLiteral("swords"),
                  QStringLiteral("archers"))};
    add_visual_stability(s, {QStringLiteral("swords"), QStringLiteral("archers")});
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("swords"),
                                         QStringLiteral("archers")));
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("archers"),
                                         QStringLiteral("swords")));
    s.expectations.push_back(
        expectation(Expect::AllLivingSoldiersFight, QStringLiteral("archers")));
    s.expectations.push_back(expectation(
        Expect::RepeatedAttackAnimationObserved, QStringLiteral("archers"), {}, 2.0F));
    s.expectations.push_back(
        expectation(Expect::FormationEngagementIsStable, QStringLiteral("archers")));
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_infantry_charge_id),
                   QStringLiteral("Infantry Charge"),
                   QStringLiteral("Three infantry charge two stationary defenders."),
                   12.0F,
                   {22.0F, 48.0F, 25.0F});
    s.groups = {
        group(QStringLiteral("chargers"), Troop::Swordsman, 1, 3, {0.0F, 0.0F, -11.0F}),
        group(QStringLiteral("defenders"), Troop::Spearman, 2, 2, {0.0F, 0.0F, 6.0F})};
    s.steps = {at(0.5F,
                  Command::Charge,
                  QStringLiteral("chargers"),
                  QStringLiteral("defenders")),
               at(0.5F, Command::Hold, QStringLiteral("defenders")),
               at(4.0F, Command::Hold, QStringLiteral("defenders")),
               at(4.0F,
                  Command::Attack,
                  QStringLiteral("defenders"),
                  QStringLiteral("chargers"))};
    s.steps[2].enabled = false;
    add_visual_stability(s, {QStringLiteral("chargers"), QStringLiteral("defenders")});
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("chargers"), {}, 0.4F));
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_flank_ambush_id),
                   QStringLiteral("Flank Ambush"),
                   QStringLiteral("Marching infantry react to a delayed flank ambush."),
                   14.0F,
                   {23.0F, 50.0F, 32.0F});
    auto ambushers = group(QStringLiteral("ambushers"),
                           Troop::Spearman,
                           2,
                           2,
                           {8.0F, 0.0F, 0.0F},
                           10,
                           {0.0F, 0.0F, 2.6F});
    ambushers.spawn_at_start = false;
    s.groups = {group(QStringLiteral("column"),
                      Troop::Swordsman,
                      1,
                      3,
                      {0.0F, 0.0F, -7.0F},
                      10,
                      {0.0F, 0.0F, 2.6F}),
                ambushers};
    s.steps = {at(0.25F, Command::FormationMove, QStringLiteral("column")),
               at(2.5F,
                  Command::SpawnAmbush,
                  QStringLiteral("ambushers"),
                  QStringLiteral("column")),
               at(3.0F,
                  Command::AttackMove,
                  QStringLiteral("column"),
                  QStringLiteral("ambushers"))};
    s.steps[0].destination = {0.0F, 0.0F, 7.0F};
    add_visual_stability(s, {QStringLiteral("column"), QStringLiteral("ambushers")});
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("column"), {}, 0.55F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(QString::fromLatin1(k_reserve_release_id),
                        QStringLiteral("Reserve Release"),
                        QStringLiteral("Held reserve joins after front-line contact."),
                        15.0F,
                        {25.0F, 50.0F, 28.0F});
    s.groups = {
        group(QStringLiteral("front"), Troop::Swordsman, 1, 2, {0.0F, 0.0F, -5.0F}),
        group(QStringLiteral("reserve"), Troop::Spearman, 1, 1, {0.0F, 0.0F, -11.0F}),
        group(QStringLiteral("enemy"), Troop::Swordsman, 2, 2, {0.0F, 0.0F, 7.0F})};
    s.steps = {
        at(0.0F, Command::Hold, QStringLiteral("reserve")),
        at(0.5F, Command::AttackMove, QStringLiteral("front"), QStringLiteral("enemy")),
        at(0.5F, Command::AttackMove, QStringLiteral("enemy"), QStringLiteral("front")),
        at(3.5F, Command::Hold, QStringLiteral("reserve")),
        at(3.5F,
           Command::ReleaseReserve,
           QStringLiteral("reserve"),
           QStringLiteral("enemy"))};
    s.steps[3].enabled = false;
    add_visual_stability(
        s,
        {QStringLiteral("front"), QStringLiteral("reserve"), QStringLiteral("enemy")});
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("reserve"), {}, 0.55F));
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("reserve")));
    s.expectations.push_back(
        expectation(Expect::AllLivingSoldiersFight, QStringLiteral("reserve")));
    s.expectations.push_back(
        expectation(Expect::FormationEngagementIsStable, QStringLiteral("reserve")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_mixed_roles_id),
        QStringLiteral("Mixed Roles"),
        QStringLiteral("Two melee groups and archers fight three melee groups."),
        15.0F,
        {27.0F, 52.0F, 32.0F});
    s.groups = {
        group(
            QStringLiteral("blue_melee"), Troop::Swordsman, 1, 2, {-3.0F, 0.0F, -8.0F}),
        group(QStringLiteral("blue_archer"), Troop::Archer, 1, 1, {4.0F, 0.0F, -10.0F}),
        group(QStringLiteral("red_melee"), Troop::Spearman, 2, 3, {0.0F, 0.0F, 8.0F})};
    s.steps = {at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("blue_melee"),
                  QStringLiteral("red_melee")),
               at(0.5F,
                  Command::Attack,
                  QStringLiteral("blue_archer"),
                  QStringLiteral("red_melee")),
               at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("red_melee"),
                  QStringLiteral("blue_melee"))};
    add_visual_stability(s,
                         {QStringLiteral("blue_melee"),
                          QStringLiteral("blue_archer"),
                          QStringLiteral("red_melee")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_bot_skirmish_id),
        QStringLiteral("Bot Skirmish"),
        QStringLiteral("AI-controlled group must make a useful battlefield move."),
        18.0F,
        {24.0F, 50.0F, 28.0F});
    auto bot = group(QStringLiteral("bot"), Troop::Spearman, 2, 3, {0.0F, 0.0F, 8.0F});
    bot.ai_controlled = true;
    s.groups = {
        group(QStringLiteral("player"), Troop::Swordsman, 1, 2, {0.0F, 0.0F, -8.0F}),
        bot};
    s.steps = {
        at(0.5F, Command::AttackMove, QStringLiteral("player"), QStringLiteral("bot"))};
    add_visual_stability(s, {QStringLiteral("player"), QStringLiteral("bot")});
    s.expectations.push_back(
        expectation(Expect::BotIssuesUsefulCommand, QStringLiteral("bot")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_path_bridge_crossing_id),
        QStringLiteral("Pathfinding: Bridge Crossing"),
        QStringLiteral("Two infantry files funnel onto a production bridge deck, "
                       "cross the river, and reform on the far bank."),
        15.0F,
        {32.0F, 54.0F, 18.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.suppress_terrain_scatter = true;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.rivers.push_back(
        Game::Map::RiverSegment{{-28.0F, 0.0F, 0.0F}, {28.0F, 0.0F, 0.0F}, 5.5F});
    s.bridges.push_back(
        Game::Map::Bridge{{0.0F, 0.0F, -5.0F}, {0.0F, 0.0F, 5.0F}, 4.5F, 0.45F});
    s.groups = {group(QStringLiteral("crossers"),
                      Troop::Swordsman,
                      1,
                      2,
                      {-1.5F, 0.0F, -11.0F},
                      6,
                      {3.0F, 0.0F, 0.0F})};
    auto move = at(0.5F, Command::FormationMove, QStringLiteral("crossers"));
    move.destination = {0.0F, 0.0F, 11.0F};
    s.steps = {move};
    add_visual_stability(s, {QStringLiteral("crossers")});
    s.expectations.push_back(
        expectation(Expect::MovementAnimationObserved, QStringLiteral("crossers")));
    s.expectations.push_back(
        expectation(Expect::BridgeTraversalObserved, QStringLiteral("crossers")));
    s.expectations.push_back(expectation(Expect::BridgeCenterlineAligned,
                                         QStringLiteral("crossers"),
                                         {},
                                         0.0F,
                                         0.0F,
                                         0.50F));
    auto reached = expectation(Expect::GroupReachedDestination,
                               QStringLiteral("crossers"),
                               {},
                               0.0F,
                               0.0F,
                               3.0F);
    reached.position = move.destination;
    s.expectations.push_back(reached);
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_path_uphill_advance_id),
        QStringLiteral("Pathfinding: Uphill Advance"),
        QStringLiteral("Spearmen climb a smooth four-metre rise and settle on its "
                       "crown with grounded, continuous locomotion."),
        11.0F,
        {27.0F, 48.0F, 16.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, -1.0F);
    s.suppress_terrain_scatter = true;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.elevation_patches.push_back({{0.0F, 0.0F, 2.0F}, 10.0F, 4.0F});
    s.groups = {group(QStringLiteral("climbers"),
                      Troop::Spearman,
                      1,
                      2,
                      {-1.5F, 0.0F, -10.0F},
                      6,
                      {3.0F, 0.0F, 0.0F})};
    auto move = at(0.5F, Command::FormationMove, QStringLiteral("climbers"));
    move.destination = {0.0F, 0.0F, 2.0F};
    s.steps = {move};
    add_visual_stability(s, {QStringLiteral("climbers")});
    s.expectations.push_back(
        expectation(Expect::MovementAnimationObserved, QStringLiteral("climbers")));
    s.expectations.push_back(expectation(
        Expect::ElevationGainObserved, QStringLiteral("climbers"), {}, 2.5F));
    auto reached = expectation(Expect::GroupReachedDestination,
                               QStringLiteral("climbers"),
                               {},
                               0.0F,
                               0.0F,
                               2.5F);
    reached.position = move.destination;
    s.expectations.push_back(reached);
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_path_wall_detour_id),
        QStringLiteral("Pathfinding: Wall Detour"),
        QStringLiteral("Infantry ordered through an intact palisade must remain "
                       "blocked by its footprint and route around the visible end."),
        15.0F,
        {32.0F, 55.0F, 20.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.suppress_terrain_scatter = true;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.groups = {
        group(QStringLiteral("detour_troops"),
              Troop::Swordsman,
              1,
              2,
              {-1.5F, 0.0F, -9.0F},
              6,
              {3.0F, 0.0F, 0.0F}),
        building(QStringLiteral("blocking_wall"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 9,
                 {-8.0F, 0.0F, 0.0F},
                 {2.0F, 0.0F, 0.0F}),
    };
    auto move = at(0.5F, Command::FormationMove, QStringLiteral("detour_troops"));
    move.destination = {0.0F, 0.0F, 9.0F};
    s.steps = {move};
    add_visual_stability(s, {QStringLiteral("detour_troops")});
    s.expectations.push_back(expectation(Expect::MovementAnimationObserved,
                                         QStringLiteral("detour_troops")));
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("blocking_wall")));
    auto reached = expectation(Expect::GroupReachedDestination,
                               QStringLiteral("detour_troops"),
                               {},
                               0.0F,
                               0.0F,
                               3.5F);
    reached.position = move.destination;
    s.expectations.push_back(reached);
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_path_wall_breach_id),
        QStringLiteral("Pathfinding: Wall Breach"),
        QStringLiteral("Swordsmen destroy a designated weak wall section, then "
                       "path through the opened gap without crossing intact "
                       "neighbouring segments."),
        16.0F,
        {30.0F, 53.0F, 18.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.suppress_terrain_scatter = true;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    auto breach = building(QStringLiteral("breach_wall"),
                           Game::Units::SpawnType::WallSegment,
                           Nation::Carthage,
                           2,
                           1,
                           {0.0F, 0.0F, 0.0F});
    breach.health_override = breach.max_health_override = 55;
    s.groups = {
        group(QStringLiteral("breachers"),
              Troop::Swordsman,
              1,
              2,
              {-1.5F, 0.0F, -8.0F},
              8,
              {3.0F, 0.0F, 0.0F}),
        building(QStringLiteral("left_wall"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 3,
                 {-10.0F, 0.0F, 0.0F},
                 {3.0F, 0.0F, 0.0F}),
        breach,
        building(QStringLiteral("right_wall"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 3,
                 {4.0F, 0.0F, 0.0F},
                 {3.0F, 0.0F, 0.0F}),
    };
    auto attack = at(0.4F,
                     Command::Attack,
                     QStringLiteral("breachers"),
                     QStringLiteral("breach_wall"));
    auto pass = when_destroyed(QStringLiteral("breach_wall"),
                               Command::FormationMove,
                               QStringLiteral("breachers"),
                               {});
    pass.destination = {0.0F, 0.0F, 8.0F};
    s.steps = {attack, pass};
    add_visual_stability(s, {QStringLiteral("breachers")});
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("breachers")));
    s.expectations.push_back(
        expectation(Expect::MovementAnimationObserved, QStringLiteral("breachers")));
    s.expectations.push_back(
        expectation(Expect::GroupDestroyed, QStringLiteral("breach_wall")));
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("left_wall")));
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("right_wall")));
    auto reached = expectation(Expect::GroupReachedDestination,
                               QStringLiteral("breachers"),
                               {},
                               0.0F,
                               0.0F,
                               3.0F);
    reached.position = pass.destination;
    s.expectations.push_back(reached);
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_crossing_formations_id),
        QStringLiteral("Crossing Formations"),
        QStringLiteral("Two friendly groups cross without formation collapse."),
        10.0F,
        {24.0F, 52.0F, 30.0F});
    s.groups = {
        group(QStringLiteral("left"), Troop::Swordsman, 1, 3, {-7.0F, 0.0F, -5.0F}),
        group(QStringLiteral("right"), Troop::Spearman, 1, 3, {7.0F, 0.0F, 5.0F})};
    s.steps = {at(0.5F, Command::FormationMove, QStringLiteral("left")),
               at(0.5F, Command::FormationMove, QStringLiteral("right"))};
    s.steps[0].destination = {7.0F, 0.0F, 5.0F};
    s.steps[1].destination = {-7.0F, 0.0F, -5.0F};
    add_visual_stability(s, {QStringLiteral("left"), QStringLiteral("right")});
    s.expectations.push_back(
        expectation(Expect::FormationOrderPreserved, QStringLiteral("left"), {}, 1.0F));
    s.expectations.push_back(expectation(
        Expect::FormationOrderPreserved, QStringLiteral("right"), {}, 1.0F));
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_sustained_battle_id),
                   QStringLiteral("Sustained Battle"),
                   QStringLiteral("Large sustained fight for smoothness and stalls."),
                   30.0F,
                   {32.0F, 54.0F, 28.0F});
    s.groups = {group(QStringLiteral("blue_swords"),
                      Troop::Swordsman,
                      1,
                      4,
                      {-4.0F, 0.0F, -11.0F},
                      16),
                group(QStringLiteral("blue_archers"),
                      Troop::Archer,
                      1,
                      2,
                      {7.0F, 0.0F, -14.0F},
                      16),
                group(QStringLiteral("blue_catapult"),
                      Troop::Catapult,
                      1,
                      1,
                      {-9.0F, 0.0F, -8.0F},
                      1),
                group(QStringLiteral("red_spears"),
                      Troop::Spearman,
                      2,
                      5,
                      {0.0F, 0.0F, 11.0F},
                      16),
                group(QStringLiteral("red_ballista"),
                      Troop::Ballista,
                      2,
                      1,
                      {9.0F, 0.0F, 8.0F},
                      1)};
    s.steps = {at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("blue_swords"),
                  QStringLiteral("red_spears")),
               at(0.5F,
                  Command::Attack,
                  QStringLiteral("blue_archers"),
                  QStringLiteral("red_spears")),
               at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("red_spears"),
                  QStringLiteral("blue_swords")),
               at(0.5F,
                  Command::Attack,
                  QStringLiteral("blue_catapult"),
                  QStringLiteral("red_spears")),
               at(0.5F,
                  Command::Attack,
                  QStringLiteral("red_ballista"),
                  QStringLiteral("blue_swords"))};
    add_visual_stability(s,
                         {QStringLiteral("blue_swords"),
                          QStringLiteral("blue_archers"),
                          QStringLiteral("red_spears")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_render_continuity_id),
        QStringLiteral("Render Continuity Stress"),
        QStringLiteral("Fixed-camera Ultra battle that samples every frame for "
                       "scene-wide flashes, rejects every reduced creature LOD, "
                       "and tracks each living formation member for submission "
                       "disappearance."),
        14.0F,
        {29.0F, 54.0F, 28.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.suppress_terrain_scatter = true;
    s.force_full_creature_lod = false;
    s.collect_animation_diagnostics = true;
    s.graphics_quality = Render::GraphicsQuality::Ultra;
    s.groups = {
        group(QStringLiteral("blue_swords"),
              Troop::Swordsman,
              1,
              3,
              {-5.0F, 0.0F, -9.0F},
              16),
        group(QStringLiteral("blue_archers"),
              Troop::Archer,
              1,
              2,
              {7.0F, 0.0F, -12.0F},
              16),
        group(QStringLiteral("blue_healer"),
              Troop::Healer,
              1,
              1,
              {-11.0F, 0.0F, -8.0F},
              1),
        group(QStringLiteral("blue_cavalry"),
              Troop::MountedKnight,
              1,
              2,
              {-12.0F, 0.0F, -16.0F},
              12),
        group(QStringLiteral("red_spears"),
              Troop::Spearman,
              2,
              4,
              {-4.0F, 0.0F, 9.0F},
              16),
        group(QStringLiteral("red_archers"),
              Troop::Archer,
              2,
              2,
              {8.0F, 0.0F, 12.0F},
              16),
        group(
            QStringLiteral("red_healer"), Troop::Healer, 2, 1, {12.0F, 0.0F, 8.0F}, 1),
        group(QStringLiteral("red_cavalry"),
              Troop::HorseSpearman,
              2,
              2,
              {-12.0F, 0.0F, 16.0F},
              12),
    };
    for (auto& continuity_group : s.groups) {
      continuity_group.max_health_override = 2000;
      continuity_group.health_override = 2000;
    }
    s.steps = {
        at(0.35F,
           Command::AttackMove,
           QStringLiteral("blue_swords"),
           QStringLiteral("red_spears")),
        at(0.35F,
           Command::Attack,
           QStringLiteral("blue_archers"),
           QStringLiteral("red_spears")),
        at(0.35F,
           Command::Charge,
           QStringLiteral("blue_cavalry"),
           QStringLiteral("red_archers")),
        at(0.35F,
           Command::AttackMove,
           QStringLiteral("red_spears"),
           QStringLiteral("blue_swords")),
        at(0.35F,
           Command::Attack,
           QStringLiteral("red_archers"),
           QStringLiteral("blue_swords")),
        at(0.35F,
           Command::Charge,
           QStringLiteral("red_cavalry"),
           QStringLiteral("blue_archers")),
    };
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    for (auto const& continuity_group : s.groups) {
      s.expectations.push_back(expectation(
          Expect::NoRenderVisibilityChurn, continuity_group.name, {}, 0.0F, 0.5F));
      s.expectations.push_back(
          expectation(Expect::FullCreatureDetailOnly, continuity_group.name));
      s.expectations.push_back(
          expectation(Expect::GroupIsRendered, continuity_group.name));
    }
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_campaign_scale_battle_id),
        QStringLiteral("Campaign-Scale Battle Performance"),
        QStringLiteral("Cannae-sized 79-unit mixed battle using production LOD, "
                       "batching, combat, cavalry, archery, and healing paths."),
        15.0F,
        {72.0F, 56.0F, 24.0F});
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.force_full_creature_lod = false;
    s.collect_animation_diagnostics = false;
    s.groups = {
        group(QStringLiteral("blue_spears"),
              Troop::Spearman,
              1,
              11,
              {-15.0F, 0.0F, -18.0F},
              16),
        group(QStringLiteral("blue_swords"),
              Troop::Swordsman,
              1,
              8,
              {-11.0F, 0.0F, -12.0F},
              16),
        group(QStringLiteral("blue_archers"),
              Troop::Archer,
              1,
              6,
              {-8.0F, 0.0F, -25.0F},
              16),
        group(QStringLiteral("blue_healers"),
              Troop::Healer,
              1,
              1,
              {-2.0F, 0.0F, -28.0F},
              16),
        group(QStringLiteral("blue_horse_archers"),
              Troop::HorseArcher,
              1,
              3,
              {-25.0F, 0.0F, -22.0F},
              16),
        group(QStringLiteral("blue_cavalry"),
              Troop::MountedKnight,
              1,
              6,
              {9.0F, 0.0F, -20.0F},
              16),
        group(QStringLiteral("red_spears"),
              Troop::Spearman,
              2,
              13,
              {-17.0F, 0.0F, 17.0F},
              16),
        group(QStringLiteral("red_swords"),
              Troop::Swordsman,
              2,
              11,
              {-14.0F, 0.0F, 11.0F},
              16),
        group(QStringLiteral("red_archers"),
              Troop::Archer,
              2,
              7,
              {-9.0F, 0.0F, 25.0F},
              16),
        group(QStringLiteral("red_healers"),
              Troop::Healer,
              2,
              2,
              {0.0F, 0.0F, 28.0F},
              16),
        group(QStringLiteral("red_horse_archers"),
              Troop::HorseArcher,
              2,
              2,
              {-25.0F, 0.0F, 22.0F},
              16),
        group(QStringLiteral("red_cavalry"),
              Troop::MountedKnight,
              2,
              9,
              {7.0F, 0.0F, 20.0F},
              16),
    };
    s.steps = {
        at(0.25F,
           Command::AttackMove,
           QStringLiteral("blue_spears"),
           QStringLiteral("red_swords")),
        at(0.25F,
           Command::AttackMove,
           QStringLiteral("blue_swords"),
           QStringLiteral("red_spears")),
        at(0.25F,
           Command::Attack,
           QStringLiteral("blue_archers"),
           QStringLiteral("red_spears")),
        at(0.25F,
           Command::Attack,
           QStringLiteral("blue_horse_archers"),
           QStringLiteral("red_swords")),
        at(0.25F,
           Command::Charge,
           QStringLiteral("blue_cavalry"),
           QStringLiteral("red_archers")),
        at(0.25F,
           Command::AttackMove,
           QStringLiteral("red_spears"),
           QStringLiteral("blue_swords")),
        at(0.25F,
           Command::AttackMove,
           QStringLiteral("red_swords"),
           QStringLiteral("blue_spears")),
        at(0.25F,
           Command::Attack,
           QStringLiteral("red_archers"),
           QStringLiteral("blue_spears")),
        at(0.25F,
           Command::Attack,
           QStringLiteral("red_horse_archers"),
           QStringLiteral("blue_swords")),
        at(0.25F,
           Command::Charge,
           QStringLiteral("red_cavalry"),
           QStringLiteral("blue_archers")),
    };
    s.expectations = {
        expectation(Expect::GroupExists, QStringLiteral("blue_spears")),
        expectation(Expect::GroupExists, QStringLiteral("red_spears")),
        expectation(Expect::FrameBudget, {}, {}, 10.0F, 2.0F),
    };
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_roman_marching_camp_id),
        QStringLiteral("Roman Marching Camp"),
        QStringLiteral("Ordered castrum: principia, barracks street, housing and "
                       "defensive perimeter."),
        18.0F,
        {38.0F, 58.0F, 34.0F});
    s.resource_patches = {
        {QStringLiteral("tent"), 4, {-7.5F, 0.0F, -3.5F}, {5.0F, 0.0F, 0.0F}, 0.9F},
        {QStringLiteral("weapon_rack"),
         2,
         {-3.0F, 0.0F, 3.5F},
         {6.0F, 0.0F, 0.0F},
         1.0F},
        {QStringLiteral("supply_cart"),
         2,
         {-8.5F, 0.0F, 9.5F},
         {17.0F, 0.0F, 0.0F},
         0.9F},
        {QStringLiteral("fire_camp"), 1, {0.0F, 0.0F, 4.0F}, {}, 0.85F},
    };
    s.groups = {
        building(QStringLiteral("roman_principia"),
                 Game::Units::SpawnType::Marketplace,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {0.0F, 0.0F, 0.0F}),
        building(QStringLiteral("roman_barracks"),
                 Game::Units::SpawnType::Barracks,
                 Nation::RomanRepublic,
                 1,
                 2,
                 {0.0F, 0.0F, -7.0F},
                 {8.0F, 0.0F, 0.0F}),
        building(QStringLiteral("roman_houses"),
                 Game::Units::SpawnType::Home,
                 Nation::RomanRepublic,
                 1,
                 4,
                 {0.0F, 0.0F, 7.0F},
                 {4.8F, 0.0F, 0.0F}),
        building(QStringLiteral("roman_north_wall"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::RomanRepublic,
                 1,
                 11,
                 {0.0F, 0.0F, -12.0F},
                 {2.0F, 0.0F, 0.0F}),
        building(QStringLiteral("roman_south_wall"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::RomanRepublic,
                 1,
                 11,
                 {0.0F, 0.0F, 12.0F},
                 {2.0F, 0.0F, 0.0F}),
        building(QStringLiteral("roman_west_wall"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::RomanRepublic,
                 1,
                 11,
                 {-12.0F, 0.0F, 0.0F},
                 {0.0F, 0.0F, 2.0F},
                 90.0F),
        building(QStringLiteral("roman_east_wall"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::RomanRepublic,
                 1,
                 11,
                 {12.0F, 0.0F, 0.0F},
                 {0.0F, 0.0F, 2.0F},
                 90.0F),
        building(QStringLiteral("roman_tower_nw"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {-12.0F, 0.0F, -12.0F}),
        building(QStringLiteral("roman_tower_ne"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {12.0F, 0.0F, -12.0F}),
        building(QStringLiteral("roman_tower_sw"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {-12.0F, 0.0F, 12.0F}),
        building(QStringLiteral("roman_tower_se"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {12.0F, 0.0F, 12.0F}),
        group(QStringLiteral("roman_builders"),
              Troop::Builder,
              1,
              2,
              {-3.0F, 0.0F, 3.0F},
              1),
    };
    add_settlement_acceptance(s,
                              {QStringLiteral("roman_principia"),
                               QStringLiteral("roman_barracks"),
                               QStringLiteral("roman_houses"),
                               QStringLiteral("roman_north_wall"),
                               QStringLiteral("roman_south_wall"),
                               QStringLiteral("roman_west_wall"),
                               QStringLiteral("roman_east_wall"),
                               QStringLiteral("roman_tower_nw"),
                               QStringLiteral("roman_tower_ne"),
                               QStringLiteral("roman_tower_sw"),
                               QStringLiteral("roman_tower_se")});
    add_visual_stability(s, {QStringLiteral("roman_builders")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_carthage_trade_town_id),
        QStringLiteral("Carthaginian Trade Town"),
        QStringLiteral("Dense Punic courtyard town organized around a market and "
                       "fortified mercantile quarter."),
        18.0F,
        {38.0F, 58.0F, 326.0F});
    s.resource_patches = {
        {QStringLiteral("supply_cart"),
         3,
         {-7.0F, 0.0F, 2.5F},
         {7.0F, 0.0F, 0.0F},
         0.9F},
        {QStringLiteral("tent"), 3, {-7.0F, 0.0F, -2.5F}, {7.0F, 0.0F, 0.0F}, 0.8F},
        {QStringLiteral("olive_tree"),
         4,
         {-9.0F, 0.0F, 10.5F},
         {6.0F, 0.0F, 0.0F},
         0.8F},
    };
    s.groups = {
        building(QStringLiteral("punic_market"),
                 Game::Units::SpawnType::Marketplace,
                 Nation::Carthage,
                 2,
                 1,
                 {0.0F, 0.0F, 0.0F},
                 {},
                 180.0F),
        building(QStringLiteral("punic_houses_north"),
                 Game::Units::SpawnType::Home,
                 Nation::Carthage,
                 2,
                 5,
                 {0.0F, 0.0F, -7.0F},
                 {4.2F, 0.0F, 0.0F},
                 180.0F),
        building(QStringLiteral("punic_houses_south"),
                 Game::Units::SpawnType::Home,
                 Nation::Carthage,
                 2,
                 5,
                 {0.0F, 0.0F, 7.0F},
                 {4.2F, 0.0F, 0.0F}),
        building(QStringLiteral("punic_barracks"),
                 Game::Units::SpawnType::Barracks,
                 Nation::Carthage,
                 2,
                 2,
                 {0.0F, 0.0F, 12.0F},
                 {9.0F, 0.0F, 0.0F},
                 180.0F),
        building(QStringLiteral("punic_north_wall"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 11,
                 {0.0F, 0.0F, -14.0F},
                 {2.0F, 0.0F, 0.0F},
                 180.0F),
        building(QStringLiteral("punic_south_wall"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 11,
                 {0.0F, 0.0F, 14.0F},
                 {2.0F, 0.0F, 0.0F}),
        building(QStringLiteral("punic_west_wall"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 13,
                 {-12.0F, 0.0F, 0.0F},
                 {0.0F, 0.0F, 2.0F},
                 90.0F),
        building(QStringLiteral("punic_east_wall"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 13,
                 {12.0F, 0.0F, 0.0F},
                 {0.0F, 0.0F, 2.0F},
                 90.0F),
        building(QStringLiteral("punic_tower_nw"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::Carthage,
                 2,
                 1,
                 {-12.0F, 0.0F, -14.0F},
                 {},
                 180.0F),
        building(QStringLiteral("punic_tower_ne"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::Carthage,
                 2,
                 1,
                 {12.0F, 0.0F, -14.0F},
                 {},
                 180.0F),
        building(QStringLiteral("punic_tower_sw"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::Carthage,
                 2,
                 1,
                 {-12.0F, 0.0F, 14.0F}),
        building(QStringLiteral("punic_tower_se"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::Carthage,
                 2,
                 1,
                 {12.0F, 0.0F, 14.0F}),
        group(QStringLiteral("punic_builders"),
              Troop::Builder,
              2,
              2,
              {3.0F, 0.0F, 3.0F},
              1),
    };
    add_settlement_acceptance(s,
                              {QStringLiteral("punic_market"),
                               QStringLiteral("punic_houses_north"),
                               QStringLiteral("punic_houses_south"),
                               QStringLiteral("punic_barracks"),
                               QStringLiteral("punic_north_wall"),
                               QStringLiteral("punic_south_wall"),
                               QStringLiteral("punic_west_wall"),
                               QStringLiteral("punic_east_wall"),
                               QStringLiteral("punic_tower_nw"),
                               QStringLiteral("punic_tower_ne"),
                               QStringLiteral("punic_tower_sw"),
                               QStringLiteral("punic_tower_se")});
    add_visual_stability(s, {QStringLiteral("punic_builders")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_architecture_and_props_showcase_id),
        QStringLiteral("Architecture and Dark Props"),
        QStringLiteral("Clean side-by-side Roman and Carthaginian architecture "
                       "review with authored ritual, ruin, and cursed-world props."),
        12.0F,
        {36.0F, 50.0F, 0.0F});
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.groups = {
        building(QStringLiteral("showcase_roman_market"),
                 Game::Units::SpawnType::Marketplace,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {-9.0F, 0.0F, -5.0F}),
        building(QStringLiteral("showcase_roman_barracks"),
                 Game::Units::SpawnType::Barracks,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {-3.0F, 0.0F, -5.0F}),
        building(QStringLiteral("showcase_roman_home"),
                 Game::Units::SpawnType::Home,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {3.0F, 0.0F, -5.0F}),
        building(QStringLiteral("showcase_roman_tower"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {9.0F, 0.0F, -5.0F}),
        building(QStringLiteral("showcase_punic_market"),
                 Game::Units::SpawnType::Marketplace,
                 Nation::Carthage,
                 2,
                 1,
                 {-9.0F, 0.0F, 2.5F},
                 {},
                 180.0F),
        building(QStringLiteral("showcase_punic_barracks"),
                 Game::Units::SpawnType::Barracks,
                 Nation::Carthage,
                 2,
                 1,
                 {-3.0F, 0.0F, 2.5F},
                 {},
                 180.0F),
        building(QStringLiteral("showcase_punic_home"),
                 Game::Units::SpawnType::Home,
                 Nation::Carthage,
                 2,
                 1,
                 {3.0F, 0.0F, 2.5F},
                 {},
                 180.0F),
        building(QStringLiteral("showcase_punic_tower"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::Carthage,
                 2,
                 1,
                 {9.0F, 0.0F, 2.5F},
                 {},
                 180.0F),
        building(QStringLiteral("showcase_roman_wall"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::RomanRepublic,
                 1,
                 9,
                 {-10.0F, 0.0F, -8.5F},
                 {2.5F, 0.0F, 0.0F}),
        building(QStringLiteral("showcase_punic_wall"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 9,
                 {-10.0F, 0.0F, 6.5F},
                 {2.5F, 0.0F, 0.0F}),
    };
    s.resource_patches = {
        {QStringLiteral("magic_shrine"), 1, {-8.0F, 0.0F, 10.5F}, {}, 0.78F},
        {QStringLiteral("ruins"), 1, {-4.0F, 0.0F, 10.5F}, {}, 0.68F},
        {QStringLiteral("dead_tree"), 1, {0.0F, 0.0F, 10.5F}, {}, 0.90F},
        {QStringLiteral("iron_ore"), 1, {4.0F, 0.0F, 10.5F}, {}, 0.90F},
        {QStringLiteral("weapon_rack"), 1, {8.0F, 0.0F, 10.5F}, {}, 0.85F},
    };
    add_settlement_acceptance(s,
                              {QStringLiteral("showcase_roman_market"),
                               QStringLiteral("showcase_roman_barracks"),
                               QStringLiteral("showcase_roman_home"),
                               QStringLiteral("showcase_roman_tower"),
                               QStringLiteral("showcase_punic_market"),
                               QStringLiteral("showcase_punic_barracks"),
                               QStringLiteral("showcase_punic_home"),
                               QStringLiteral("showcase_punic_tower"),
                               QStringLiteral("showcase_roman_wall"),
                               QStringLiteral("showcase_punic_wall")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_roman_fortification_showcase_id),
        QStringLiteral("Roman Timber Fortification"),
        QStringLiteral(
            "Roman palisade review with disciplined wall runs, reinforced "
            "corners, a defended gate opening, towers, and an occupied ward."),
        16.0F,
        {42.0F, 56.0F, 28.0F});
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.groups = {
        building(QStringLiteral("roman_fort_north"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::RomanRepublic,
                 1,
                 9,
                 {0.0F, 0.0F, -8.0F},
                 {2.0F, 0.0F, 0.0F}),
        building(QStringLiteral("roman_fort_west"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::RomanRepublic,
                 1,
                 9,
                 {-8.0F, 0.0F, 0.0F},
                 {0.0F, 0.0F, 2.0F},
                 90.0F),
        building(QStringLiteral("roman_fort_east"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::RomanRepublic,
                 1,
                 9,
                 {8.0F, 0.0F, 0.0F},
                 {0.0F, 0.0F, 2.0F},
                 90.0F),
        building(QStringLiteral("roman_fort_south_west"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::RomanRepublic,
                 1,
                 4,
                 {-5.0F, 0.0F, 8.0F},
                 {2.0F, 0.0F, 0.0F}),
        building(QStringLiteral("roman_fort_south_east"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::RomanRepublic,
                 1,
                 4,
                 {5.0F, 0.0F, 8.0F},
                 {2.0F, 0.0F, 0.0F}),
        building(QStringLiteral("roman_fort_tower_nw"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {-8.0F, 0.0F, -8.0F}),
        building(QStringLiteral("roman_fort_tower_ne"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {8.0F, 0.0F, -8.0F}),
        building(QStringLiteral("roman_fort_tower_sw"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {-8.0F, 0.0F, 8.0F}),
        building(QStringLiteral("roman_fort_tower_se"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {8.0F, 0.0F, 8.0F}),
        building(QStringLiteral("roman_fort_gate_left"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {-2.0F, 0.0F, 8.0F}),
        building(QStringLiteral("roman_fort_gate_right"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {2.0F, 0.0F, 8.0F}),
        building(QStringLiteral("roman_fort_barracks"),
                 Game::Units::SpawnType::Barracks,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {-2.8F, 0.0F, -1.0F}),
        building(QStringLiteral("roman_fort_market"),
                 Game::Units::SpawnType::Marketplace,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {3.2F, 0.0F, 1.5F}),
    };
    add_settlement_acceptance(s,
                              {QStringLiteral("roman_fort_north"),
                               QStringLiteral("roman_fort_west"),
                               QStringLiteral("roman_fort_east"),
                               QStringLiteral("roman_fort_south_west"),
                               QStringLiteral("roman_fort_south_east"),
                               QStringLiteral("roman_fort_tower_nw"),
                               QStringLiteral("roman_fort_tower_ne"),
                               QStringLiteral("roman_fort_tower_sw"),
                               QStringLiteral("roman_fort_tower_se"),
                               QStringLiteral("roman_fort_gate_left"),
                               QStringLiteral("roman_fort_gate_right"),
                               QStringLiteral("roman_fort_barracks"),
                               QStringLiteral("roman_fort_market")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_carthage_fortification_showcase_id),
        QStringLiteral("Carthaginian Dread Palisade"),
        QStringLiteral("Carthaginian timber fortress with bronze-bound logs, jagged "
                       "towers, a ritual gate, and a layered inner defensive ward."),
        16.0F,
        {46.0F, 60.0F, 330.0F});
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.groups = {
        building(QStringLiteral("punic_fort_north"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 11,
                 {0.0F, 0.0F, -10.0F},
                 {2.0F, 0.0F, 0.0F},
                 180.0F),
        building(QStringLiteral("punic_fort_west"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 11,
                 {-10.0F, 0.0F, 0.0F},
                 {0.0F, 0.0F, 2.0F},
                 90.0F),
        building(QStringLiteral("punic_fort_east"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 11,
                 {10.0F, 0.0F, 0.0F},
                 {0.0F, 0.0F, 2.0F},
                 90.0F),
        building(QStringLiteral("punic_fort_south_west"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 4,
                 {-7.0F, 0.0F, 10.0F},
                 {2.0F, 0.0F, 0.0F}),
        building(QStringLiteral("punic_fort_south_east"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 4,
                 {7.0F, 0.0F, 10.0F},
                 {2.0F, 0.0F, 0.0F}),
        building(QStringLiteral("punic_fort_tower_nw"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::Carthage,
                 2,
                 1,
                 {-10.0F, 0.0F, -10.0F},
                 {},
                 180.0F),
        building(QStringLiteral("punic_fort_tower_ne"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::Carthage,
                 2,
                 1,
                 {10.0F, 0.0F, -10.0F},
                 {},
                 180.0F),
        building(QStringLiteral("punic_fort_tower_sw"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::Carthage,
                 2,
                 1,
                 {-10.0F, 0.0F, 10.0F}),
        building(QStringLiteral("punic_fort_tower_se"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::Carthage,
                 2,
                 1,
                 {10.0F, 0.0F, 10.0F}),
        building(QStringLiteral("punic_fort_gate_left"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::Carthage,
                 2,
                 1,
                 {-3.0F, 0.0F, 10.0F}),
        building(QStringLiteral("punic_fort_gate_right"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::Carthage,
                 2,
                 1,
                 {3.0F, 0.0F, 10.0F}),
        building(QStringLiteral("punic_inner_north"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 5,
                 {0.0F, 0.0F, -2.0F},
                 {2.0F, 0.0F, 0.0F},
                 180.0F),
        building(QStringLiteral("punic_inner_west"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 3,
                 {-4.0F, 0.0F, 0.0F},
                 {0.0F, 0.0F, 2.0F},
                 90.0F),
        building(QStringLiteral("punic_inner_east"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 3,
                 {4.0F, 0.0F, 0.0F},
                 {0.0F, 0.0F, 2.0F},
                 90.0F),
        building(QStringLiteral("punic_fort_barracks"),
                 Game::Units::SpawnType::Barracks,
                 Nation::Carthage,
                 2,
                 1,
                 {0.0F, 0.0F, 1.0F},
                 {},
                 180.0F),
        building(QStringLiteral("punic_fort_market"),
                 Game::Units::SpawnType::Marketplace,
                 Nation::Carthage,
                 2,
                 1,
                 {6.8F, 0.0F, 3.4F},
                 {},
                 180.0F),
    };
    s.resource_patches = {
        {QStringLiteral("fire_camp"), 2, {-2.0F, 0.0F, 6.0F}, {4.0F, 0.0F, 0.0F}, 0.8F},
        {QStringLiteral("weapon_rack"),
         2,
         {-6.5F, 0.0F, 5.2F},
         {13.0F, 0.0F, 0.0F},
         0.9F},
    };
    add_settlement_acceptance(s,
                              {QStringLiteral("punic_fort_north"),
                               QStringLiteral("punic_fort_west"),
                               QStringLiteral("punic_fort_east"),
                               QStringLiteral("punic_fort_south_west"),
                               QStringLiteral("punic_fort_south_east"),
                               QStringLiteral("punic_fort_tower_nw"),
                               QStringLiteral("punic_fort_tower_ne"),
                               QStringLiteral("punic_fort_tower_sw"),
                               QStringLiteral("punic_fort_tower_se"),
                               QStringLiteral("punic_fort_gate_left"),
                               QStringLiteral("punic_fort_gate_right"),
                               QStringLiteral("punic_inner_north"),
                               QStringLiteral("punic_inner_west"),
                               QStringLiteral("punic_inner_east"),
                               QStringLiteral("punic_fort_barracks"),
                               QStringLiteral("punic_fort_market")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rival_economies_id),
        QStringLiteral("Rival Economies"),
        QStringLiteral("Roman and Carthaginian AI builders develop opposing starter "
                       "settlements from equal economic positions."),
        80.0F,
        {58.0F, 62.0F, 25.0F});
    auto roman_builders = group(QStringLiteral("roman_economy_builders"),
                                Troop::Builder,
                                2,
                                3,
                                {-18.0F, 0.0F, 0.0F},
                                1);
    roman_builders.ai_controlled = true;
    roman_builders.nation_id = Nation::RomanRepublic;
    auto punic_builders = group(QStringLiteral("punic_economy_builders"),
                                Troop::Builder,
                                3,
                                3,
                                {18.0F, 0.0F, 0.0F},
                                1);
    punic_builders.ai_controlled = true;
    punic_builders.nation_id = Nation::Carthage;
    s.resource_patches = {
        {QStringLiteral("olive_tree"),
         8,
         {-31.0F, 0.0F, -9.0F},
         {0.0F, 0.0F, 2.5F},
         1.15F},
        {QStringLiteral("boulder"),
         6,
         {-27.0F, 0.0F, -12.0F},
         {2.3F, 0.0F, 0.0F},
         1.1F},
        {QStringLiteral("iron_ore"),
         4,
         {-30.0F, 0.0F, 12.0F},
         {2.4F, 0.0F, 0.0F},
         1.0F},
        {QStringLiteral("olive_tree"),
         8,
         {31.0F, 0.0F, -9.0F},
         {0.0F, 0.0F, 2.5F},
         1.15F},
        {QStringLiteral("boulder"), 6, {15.5F, 0.0F, 15.0F}, {2.3F, 0.0F, 0.0F}, 1.1F},
        {QStringLiteral("iron_ore"),
         4,
         {23.0F, 0.0F, -14.0F},
         {2.4F, 0.0F, 0.0F},
         1.0F},
    };
    s.groups = {
        building(QStringLiteral("roman_economy_home"),
                 Game::Units::SpawnType::Home,
                 Nation::RomanRepublic,
                 2,
                 2,
                 {-18.0F, 0.0F, 5.0F}),
        building(QStringLiteral("roman_economy_market"),
                 Game::Units::SpawnType::Marketplace,
                 Nation::RomanRepublic,
                 2,
                 1,
                 {-18.0F, 0.0F, -3.0F}),
        building(QStringLiteral("roman_economy_barracks"),
                 Game::Units::SpawnType::Barracks,
                 Nation::RomanRepublic,
                 2,
                 1,
                 {-18.0F, 0.0F, -10.0F}),
        building(QStringLiteral("punic_economy_home"),
                 Game::Units::SpawnType::Home,
                 Nation::Carthage,
                 3,
                 2,
                 {18.0F, 0.0F, 5.0F}),
        building(QStringLiteral("punic_economy_market"),
                 Game::Units::SpawnType::Marketplace,
                 Nation::Carthage,
                 3,
                 1,
                 {18.0F, 0.0F, -3.0F}),
        building(QStringLiteral("punic_economy_barracks"),
                 Game::Units::SpawnType::Barracks,
                 Nation::Carthage,
                 3,
                 1,
                 {18.0F, 0.0F, -10.0F},
                 {},
                 180.0F),
        roman_builders,
        punic_builders,
    };
    for (auto& economy_group : s.groups) {
      economy_group.ai_controlled = true;
    }
    add_settlement_acceptance(s,
                              {QStringLiteral("roman_economy_home"),
                               QStringLiteral("roman_economy_market"),
                               QStringLiteral("roman_economy_barracks"),
                               QStringLiteral("punic_economy_home"),
                               QStringLiteral("punic_economy_market"),
                               QStringLiteral("punic_economy_barracks")});
    s.expectations.push_back(
        expectation(Expect::GroupIsRendered, QStringLiteral("roman_economy_builders")));
    s.expectations.push_back(
        expectation(Expect::GroupIsRendered, QStringLiteral("punic_economy_builders")));
    s.expectations.push_back(expectation(Expect::OwnerCompletesConstruction,
                                         QStringLiteral("roman_economy_builders"),
                                         {},
                                         2.0F));
    s.expectations.push_back(expectation(Expect::OwnerCompletesConstruction,
                                         QStringLiteral("punic_economy_builders"),
                                         {},
                                         2.0F));
    s.expectations.push_back(expectation(Expect::OwnerHarvestsResource,
                                         QStringLiteral("roman_economy_builders")));
    s.expectations.push_back(expectation(Expect::OwnerHarvestsResource,
                                         QStringLiteral("punic_economy_builders")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_water_showcase_id),
        QStringLiteral("River and Lake Water Showcase"),
        QStringLiteral(
            "Places a flowing river and an irregular calm lake side by side for "
            "shared material, foam, shoreline, depth, and silhouette review."),
        10.0F,
        {42.0F, 56.0F, 18.0F});
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.rivers.push_back(
        Game::Map::RiverSegment{{-12.0F, 0.0F, -28.0F}, {-10.0F, 0.0F, 28.0F}, 5.5F});
    s.lakes.push_back(Game::Map::Lake{{10.0F, 0.0F, 1.0F}, 19.0F, 14.0F, -18.0F});
    auto observer = group(
        QStringLiteral("water_observer"), Troop::Archer, 1, 1, {0.0F, 0.0F, 0.0F}, 1);
    observer.nation_id = Nation::Carthage;
    s.groups = {observer};
    s.expectations = {
        expectation(Expect::GroupIsRendered, QStringLiteral("water_observer")),
        expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F)};
    result.push_back(std::move(s));
  }

  return result;
}

} // namespace

auto definitions() -> const std::vector<ArenaScenarioDefinition>& {
  static const std::vector<ArenaScenarioDefinition> catalog = build_definitions();
  return catalog;
}

auto find_definition(const QString& scenario_id) -> const ArenaScenarioDefinition* {
  auto const found =
      std::find_if(definitions().begin(),
                   definitions().end(),
                   [&](auto const& scenario) { return scenario.id == scenario_id; });
  return found == definitions().end() ? nullptr : &*found;
}

auto options() -> const std::vector<ScenarioOption>& {
  static const std::vector<ScenarioOption> catalog = [] {
    std::vector<ScenarioOption> values;
    values.reserve(definitions().size());
    for (auto const& scenario : definitions()) {
      values.push_back({scenario.id, scenario.label, scenario.description});
    }
    return values;
  }();
  return catalog;
}

auto find_option(const QString& scenario_id) -> const ScenarioOption* {
  auto const found =
      std::find_if(options().begin(), options().end(), [&](auto const& option) {
        return option.id == scenario_id;
      });
  return found == options().end() ? nullptr : &*found;
}

} // namespace Arena::Scenarios
