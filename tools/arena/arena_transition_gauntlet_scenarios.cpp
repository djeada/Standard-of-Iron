#include "arena_transition_gauntlet_scenarios.h"

#include <initializer_list>
#include <utility>
#include <vector>

#include "arena_scenario_builders.h"
#include "arena_scenarios.h"

namespace Arena::Scenarios {
namespace {

using namespace builders;

using Command = ScenarioCommandKind;
using Expect = ArenaExpectationKind;
using Nation = Game::Systems::NationID;
using SpawnType = Game::Units::SpawnType;
using Troop = Game::Units::TroopType;

auto order(float time,
           Command command,
           QString group,
           QVector3D destination) -> ArenaScenarioStep {
  auto step = at(time, command, std::move(group));
  step.destination = destination;
  return step;
}

auto toggle(float time,
            Command command,
            QString group,
            bool enabled) -> ArenaScenarioStep {
  auto step = at(time, command, std::move(group));
  step.enabled = enabled;
  return step;
}

auto sparring_partner(QString name,
                      Troop troop,
                      QVector3D origin,
                      int individuals,
                      bool fights_back) -> ArenaScenarioGroup {
  auto result = group(std::move(name), troop, 2, 1, origin, individuals);
  result.facing_degrees = 180.0F;
  result.health_override = result.max_health_override = 6000;
  result.attacks_disabled = !fights_back;
  return result;
}

auto staged(QString id,
            QString label,
            QString description,
            float duration,
            ArenaCameraView camera,
            QVector3D focus) -> ArenaScenarioDefinition {
  auto s = definition(
      std::move(id), std::move(label), std::move(description), duration, camera);
  s.camera_focus = focus;
  s.suppress_terrain_scatter = true;
  s.select_spawned_units = false;
  s.suppress_spawn_anchor = true;
  s.suppress_ui_overlays = true;
  s.collect_animation_diagnostics = true;
  return s;
}

auto infantry() -> ArenaScenarioDefinition {
  auto s = staged(
      QString::fromLatin1(k_transition_gauntlet_infantry_id),
      QStringLiteral("Transition Gauntlet: Infantry"),
      QStringLiteral(
          "Swordsmen, spearmen and a field commander take a new order every "
          "half second: replanned marches, a run, a stop, two reversals, a hold "
          "toggled while the kneel is still blending, a camera pull that crosses "
          "the LOD bands, an attack, a retarget mid-fight and a disengage. No "
          "stride may restart, no swing may restart before it lands, and no body "
          "may snap between poses."),
      16.0F,
      {30.0F, 48.0F, 30.0F},
      {0.0F, 0.0F, 6.0F});
  QString const swords = QStringLiteral("swords");
  QString const spears = QStringLiteral("spears");
  QString const commander = QStringLiteral("commander");
  s.groups = {
      group(swords, Troop::Swordsman, 1, 1, {-5.0F, 0.0F, -6.0F}, 6),
      group(spears, Troop::Spearman, 1, 1, {5.0F, 0.0F, -6.0F}, 6),
      group(commander, Troop::RomanFieldCommander, 1, 1, {0.0F, 0.0F, -8.0F}, 1),
      sparring_partner(QStringLiteral("enemy_left"),
                       Troop::Swordsman,
                       {-4.0F, 0.0F, 15.0F},
                       6,
                       true),
      sparring_partner(QStringLiteral("enemy_right"),
                       Troop::Swordsman,
                       {4.0F, 0.0F, 15.0F},
                       6,
                       true),
  };
  for (auto const& [name, lane] : {std::pair{swords, -5.0F},
                                   std::pair{spears, 5.0F},
                                   std::pair{commander, 0.0F}}) {
    s.steps.push_back(order(0.3F, Command::FormationMove, name, {lane, 0.0F, 3.0F}));
    s.steps.push_back(
        order(0.8F, Command::FormationMove, name, {lane + 1.0F, 0.0F, 4.0F}));
    s.steps.push_back(
        order(1.3F, Command::FormationMove, name, {lane - 1.0F, 0.0F, 4.5F}));
    s.steps.push_back(order(1.8F, Command::Run, name, {lane, 0.0F, 8.0F}));
    s.steps.push_back(at(3.0F, Command::Stop, name));
    s.steps.push_back(order(3.5F, Command::FormationMove, name, {lane, 0.0F, 0.0F}));
    s.steps.push_back(order(4.3F, Command::FormationMove, name, {lane, 0.0F, 7.0F}));
    s.steps.push_back(at(5.6F, Command::Stop, name));
  }
  s.steps.push_back(toggle(5.8F, Command::Hold, spears, true));
  s.steps.push_back(toggle(6.4F, Command::Hold, spears, false));
  s.steps.push_back(toggle(6.7F, Command::Hold, spears, true));
  s.steps.push_back(toggle(7.6F, Command::Hold, spears, false));
  {
    auto far = at(6.0F, Command::SetCamera);
    far.camera_distance = 62.0F;
    far.camera_angle = 52.0F;
    far.camera_yaw = 30.0F;
    auto near = at(7.2F, Command::SetCamera);
    near.camera_distance = 30.0F;
    near.camera_angle = 48.0F;
    near.camera_yaw = 30.0F;
    s.steps.push_back(far);
    s.steps.push_back(near);
  }
  s.steps.push_back(
      at(8.0F, Command::AttackMove, swords, QStringLiteral("enemy_left")));
  s.steps.push_back(
      at(8.0F, Command::AttackMove, spears, QStringLiteral("enemy_right")));
  s.steps.push_back(
      at(8.0F, Command::AttackMove, commander, QStringLiteral("enemy_left")));
  s.steps.push_back(at(10.6F, Command::Attack, swords, QStringLiteral("enemy_right")));
  s.steps.push_back(at(10.6F, Command::Attack, spears, QStringLiteral("enemy_left")));
  s.steps.push_back(
      at(11.0F, Command::AttackMove, QStringLiteral("enemy_left"), swords));
  s.steps.push_back(
      at(11.0F, Command::AttackMove, QStringLiteral("enemy_right"), spears));
  s.steps.push_back(order(13.0F, Command::FormationMove, swords, {-5.0F, 0.0F, 2.0F}));
  s.steps.push_back(order(13.0F, Command::FormationMove, spears, {5.0F, 0.0F, 2.0F}));
  s.steps.push_back(
      order(13.0F, Command::FormationMove, commander, {0.0F, 0.0F, 0.0F}));
  s.steps.push_back(at(15.0F, Command::Stop, swords));
  s.steps.push_back(at(15.0F, Command::Stop, spears));
  s.steps.push_back(at(15.0F, Command::Stop, commander));
  add_transition_continuity(s, {swords, spears, commander});
  for (auto const& name : {swords, spears, commander}) {
    s.expectations.push_back(expectation(Expect::MovementAnimationObserved, name));
    s.expectations.push_back(expectation(Expect::AttackAnimationObserved, name));
  }
  return s;
}

auto archers() -> ArenaScenarioDefinition {
  auto s = staged(
      QString::fromLatin1(k_transition_gauntlet_archers_id),
      QStringLiteral("Transition Gauntlet: Archers"),
      QStringLiteral(
          "Archers march, are re-ordered mid-stride, kneel, shoot from the hold, "
          "switch targets between draws, stand, break off into a march and are "
          "sent back to shooting. The draw may not restart before the release "
          "and the stride may not reset across the re-orders."),
      14.0F,
      {28.0F, 48.0F, 30.0F},
      {0.0F, 0.0F, 4.0F});
  QString const bows = QStringLiteral("archers");
  s.groups = {
      group(bows, Troop::Archer, 1, 1, {0.0F, 0.0F, -6.0F}, 6),
      sparring_partner(QStringLiteral("target_left"),
                       Troop::Swordsman,
                       {-5.0F, 0.0F, 13.0F},
                       4,
                       false),
      sparring_partner(QStringLiteral("target_right"),
                       Troop::Swordsman,
                       {5.0F, 0.0F, 13.0F},
                       4,
                       false),
  };
  s.steps = {
      order(0.3F, Command::FormationMove, bows, {0.0F, 0.0F, 0.0F}),
      order(0.9F, Command::FormationMove, bows, {1.0F, 0.0F, 1.0F}),
      order(1.5F, Command::FormationMove, bows, {-1.0F, 0.0F, 0.5F}),
      at(2.6F, Command::Stop, bows),
      toggle(3.0F, Command::Hold, bows, true),
      at(4.0F, Command::Attack, bows, QStringLiteral("target_left")),
      at(5.6F, Command::Attack, bows, QStringLiteral("target_right")),
      toggle(7.0F, Command::Hold, bows, false),
      order(7.4F, Command::FormationMove, bows, {0.0F, 0.0F, -4.0F}),
      at(8.6F, Command::Attack, bows, QStringLiteral("target_left")),
      order(10.6F, Command::FormationMove, bows, {-3.0F, 0.0F, -1.0F}),
      at(12.6F, Command::Stop, bows),
  };
  for (auto index : {5, 6, 9}) {
    s.steps[static_cast<std::size_t>(index)].chase = false;
  }
  add_transition_continuity(s, {bows});
  s.expectations.push_back(expectation(Expect::MovementAnimationObserved, bows));
  s.expectations.push_back(expectation(Expect::AttackAnimationObserved, bows));
  s.expectations.push_back(expectation(
      Expect::ProjectileFlightObserved, bows, QStringLiteral("target_left")));
  return s;
}

auto cavalry() -> ArenaScenarioDefinition {
  auto s =
      staged(QString::fromLatin1(k_transition_gauntlet_cavalry_id),
             QStringLiteral("Transition Gauntlet: Cavalry"),
             QStringLiteral(
                 "Mounted swordsmen and horse archers walk, are re-ordered, gallop, "
                 "rein in, reverse twice, charge, retarget and ride clear. Riders, "
                 "saddles and weapons have to stay on the horse through every change."),
             16.0F,
             {38.0F, 50.0F, 30.0F},
             {0.0F, 0.0F, 4.0F});
  s.arena_floor_half_extent = 26.0F;
  QString const riders = QStringLiteral("riders");
  QString const horse_archers = QStringLiteral("horse_archers");
  s.groups = {
      group(riders, Troop::MountedSwordsman, 1, 1, {-6.0F, 0.0F, -12.0F}, 4),
      group(horse_archers, Troop::HorseArcher, 1, 1, {6.0F, 0.0F, -12.0F}, 4),
      sparring_partner(
          QStringLiteral("enemy_left"), Troop::Spearman, {-5.0F, 0.0F, 16.0F}, 6, true),
      sparring_partner(
          QStringLiteral("enemy_right"), Troop::Spearman, {5.0F, 0.0F, 16.0F}, 6, true),
  };
  for (auto const& [name, lane] :
       {std::pair{riders, -6.0F}, std::pair{horse_archers, 6.0F}}) {
    s.steps.push_back(order(0.3F, Command::FormationMove, name, {lane, 0.0F, -2.0F}));
    s.steps.push_back(
        order(0.9F, Command::FormationMove, name, {lane + 1.5F, 0.0F, -1.0F}));
    s.steps.push_back(order(1.6F, Command::Run, name, {lane, 0.0F, 6.0F}));
    s.steps.push_back(at(3.2F, Command::Stop, name));
    s.steps.push_back(order(3.8F, Command::FormationMove, name, {lane, 0.0F, -8.0F}));
    s.steps.push_back(order(5.0F, Command::FormationMove, name, {lane, 0.0F, 4.0F}));
    s.steps.push_back(at(6.6F, Command::Stop, name));
  }
  s.steps.push_back(at(7.2F, Command::Charge, riders, QStringLiteral("enemy_left")));
  s.steps.push_back(
      at(7.2F, Command::Attack, horse_archers, QStringLiteral("enemy_right")));
  s.steps.push_back(at(10.4F, Command::Attack, riders, QStringLiteral("enemy_right")));
  s.steps.push_back(
      at(10.4F, Command::Attack, horse_archers, QStringLiteral("enemy_left")));
  s.steps.push_back(order(13.0F, Command::Run, riders, {-6.0F, 0.0F, -6.0F}));
  s.steps.push_back(order(13.0F, Command::Run, horse_archers, {6.0F, 0.0F, -6.0F}));
  s.steps.push_back(at(15.2F, Command::Stop, riders));
  s.steps.push_back(at(15.2F, Command::Stop, horse_archers));
  add_transition_continuity(s, {riders, horse_archers});
  for (auto const& name : {riders, horse_archers}) {
    s.expectations.push_back(expectation(Expect::MovementAnimationObserved, name));
    s.expectations.push_back(expectation(Expect::AttackAnimationObserved, name));
  }
  return s;
}

auto elephants() -> ArenaScenarioDefinition {
  auto s = staged(
      QString::fromLatin1(k_transition_gauntlet_elephants_id),
      QStringLiteral("Transition Gauntlet: Elephants"),
      QStringLiteral(
          "War elephants are re-ordered mid-stride, stopped, reversed, sent into "
          "a spear line, retargeted and walked clear, while the infantry they "
          "hit has to react without snapping."),
      16.0F,
      {36.0F, 50.0F, 30.0F},
      {0.0F, 0.0F, 4.0F});
  s.arena_floor_half_extent = 26.0F;
  QString const elephants = QStringLiteral("elephants");
  s.groups = {
      group(elephants,
            Troop::Elephant,
            1,
            2,
            {-3.0F, 0.0F, -12.0F},
            1,
            {6.0F, 0.0F, 0.0F}),
      sparring_partner(
          QStringLiteral("enemy_left"), Troop::Spearman, {-5.0F, 0.0F, 14.0F}, 6, true),
      sparring_partner(
          QStringLiteral("enemy_right"), Troop::Spearman, {5.0F, 0.0F, 14.0F}, 6, true),
  };
  s.steps = {
      order(0.3F, Command::FormationMove, elephants, {0.0F, 0.0F, -3.0F}),
      order(1.0F, Command::FormationMove, elephants, {1.5F, 0.0F, -2.0F}),
      order(1.7F, Command::FormationMove, elephants, {-1.0F, 0.0F, -1.0F}),
      at(3.2F, Command::Stop, elephants),
      order(3.8F, Command::FormationMove, elephants, {0.0F, 0.0F, -10.0F}),
      order(5.2F, Command::FormationMove, elephants, {0.0F, 0.0F, 2.0F}),
      at(7.0F, Command::Stop, elephants),
      at(7.6F, Command::AttackMove, elephants, QStringLiteral("enemy_left")),
      at(10.8F, Command::Attack, elephants, QStringLiteral("enemy_right")),
      order(13.2F, Command::FormationMove, elephants, {0.0F, 0.0F, -6.0F}),
      at(15.2F, Command::Stop, elephants),
  };
  s.expectations.push_back(expectation(Expect::GroupExists, elephants));
  s.expectations.push_back(expectation(Expect::MovementIsContinuous, elephants));
  s.expectations.push_back(
      expectation(Expect::AllGroupsRespondWithin, elephants, {}, 0.45F));
  add_transition_continuity(
      s, {QStringLiteral("enemy_left"), QStringLiteral("enemy_right")});
  return s;
}

auto builders_crews() -> ArenaScenarioDefinition {
  auto s =
      staged(QString::fromLatin1(k_transition_gauntlet_builders_id),
             QStringLiteral("Transition Gauntlet: Builders"),
             QStringLiteral(
                 "One crew raises a house and is pulled off and sent back twice, a "
                 "second crew repairs a damaged home through a move, an abandon and a "
                 "resume, and a third quarries stone and is interrupted while it "
                 "carries. Tools stay in hand and work poses blend into the walk."),
             18.0F,
             {30.0F, 50.0F, 20.0F},
             {0.0F, 0.0F, 0.0F});
  QString const raisers = QStringLiteral("raisers");
  QString const menders = QStringLiteral("menders");
  QString const quarriers = QStringLiteral("quarriers");
  auto crew = [](QString name, QVector3D origin) {
    auto result = group(std::move(name), Troop::Builder, 1, 2, origin, 1);
    result.spacing = {1.6F, 0.0F, 0.0F};
    return result;
  };
  auto home = building(QStringLiteral("home"),
                       SpawnType::Home,
                       Nation::RomanRepublic,
                       1,
                       1,
                       {-8.0F, 0.0F, 4.0F});
  home.health_override = home.max_health_override = 1000;
  s.groups = {
      home,
      building(QStringLiteral("depot"),
               SpawnType::Barracks,
               Nation::RomanRepublic,
               1,
               1,
               {9.0F, 0.0F, 7.0F}),
      crew(raisers, {-1.0F, 0.0F, -7.0F}),
      crew(menders, {-8.0F, 0.0F, -4.0F}),
      crew(quarriers, {6.0F, 0.0F, -6.0F}),
  };
  s.resource_patches = {patch("boulder", 3, {10.0F, 0.0F, -10.0F}, {2.4F, 0.0F, 0.0F})};

  auto build = [](float time, QString group) {
    auto step = at(time, Command::StartConstruction, std::move(group));
    step.construction_type = QStringLiteral("home");
    step.destination = QVector3D(1.0F, 0.0F, 1.0F);
    return step;
  };
  auto harvest = [](float time, QString group) {
    auto step = at(time, Command::HarvestResource, std::move(group));
    step.resource_kind = QStringLiteral("boulder");
    return step;
  };
  auto mend = [](float time, QString group) {
    return at(time, Command::RepairStructure, std::move(group), QStringLiteral("home"));
  };
  s.steps = {
      build(0.5F, raisers),
      order(4.0F, Command::Move, raisers, {-2.0F, 0.0F, -6.0F}),
      build(5.0F, raisers),
      at(9.0F, Command::Stop, raisers),
      build(9.5F, raisers),

      [] {
        auto damage = at(0.2F, Command::SetHealth, QStringLiteral("home"));
        damage.value = 250;
        return damage;
      }(),
      mend(1.0F, menders),
      order(5.0F, Command::Move, menders, {-6.0F, 0.0F, -4.0F}),
      mend(6.0F, menders),
      [&] {
        auto abandon =
            order(10.0F, Command::AbandonWork, menders, {-10.0F, 0.0F, -4.0F});
        abandon.value = 20;
        return abandon;
      }(),
      mend(11.0F, menders),

      harvest(0.5F, quarriers),
      order(7.0F, Command::Move, quarriers, {4.0F, 0.0F, -2.0F}),
      harvest(8.0F, quarriers),
      order(13.0F, Command::Move, quarriers, {6.0F, 0.0F, 0.0F}),
      harvest(14.0F, quarriers),
  };
  add_transition_continuity(s, {raisers, menders, quarriers});
  for (auto const& name : {raisers, menders, quarriers}) {
    s.expectations.push_back(expectation(Expect::MovementAnimationObserved, name));
  }
  s.expectations.push_back(
      expectation(Expect::StructureRepairObserved, QStringLiteral("home")));
  return s;
}

auto civilians() -> ArenaScenarioDefinition {
  auto s =
      staged(QString::fromLatin1(k_transition_gauntlet_civilians_id),
             QStringLiteral("Transition Gauntlet: Civilians"),
             QStringLiteral(
                 "Townsfolk are re-ordered every few steps, run, stop, double back and "
                 "finally walk to the barracks to enlist. A civilian has no weapon to "
                 "hide a stride reset behind, so every start and stop is on show."),
             14.0F,
             {24.0F, 46.0F, 30.0F},
             {0.0F, 0.0F, 0.0F});
  QString const townsfolk = QStringLiteral("townsfolk");
  auto folk = group(townsfolk, Troop::Civilian, 1, 4, {-4.5F, 0.0F, -6.0F}, 1);
  folk.spacing = {3.0F, 0.0F, 0.0F};
  s.groups = {
      folk,
      building(QStringLiteral("barracks"),
               SpawnType::Barracks,
               Nation::RomanRepublic,
               1,
               1,
               {0.0F, 0.0F, 10.0F}),
  };
  s.steps = {
      order(0.3F, Command::Move, townsfolk, {0.0F, 0.0F, -1.0F}),
      order(0.8F, Command::Move, townsfolk, {2.0F, 0.0F, 0.0F}),
      order(1.3F, Command::Move, townsfolk, {-2.0F, 0.0F, 1.0F}),
      order(2.0F, Command::Run, townsfolk, {0.0F, 0.0F, 4.0F}),
      at(3.2F, Command::Stop, townsfolk),
      order(3.6F, Command::Move, townsfolk, {0.0F, 0.0F, -5.0F}),
      order(4.4F, Command::Move, townsfolk, {0.0F, 0.0F, 3.0F}),
      at(5.0F, Command::Stop, townsfolk),
      order(5.3F, Command::Move, townsfolk, {4.0F, 0.0F, 0.0F}),
      at(5.8F, Command::Stop, townsfolk),
      order(6.1F, Command::Move, townsfolk, {-4.0F, 0.0F, 0.0F}),
      at(7.4F, Command::Stop, townsfolk),
      at(8.5F, Command::DeliverToStructure, townsfolk, QStringLiteral("barracks")),
  };
  add_transition_continuity(s, {townsfolk});
  s.expectations.push_back(expectation(Expect::MovementAnimationObserved, townsfolk));
  return s;
}

auto flanked() -> ArenaScenarioDefinition {
  auto s =
      staged(QString::fromLatin1(k_transition_gauntlet_flanked_id),
             QStringLiteral("Transition Gauntlet: Flanked Unit"),
             QStringLiteral(
                 "One large swordsman unit is engaged from both flanks by single-body "
                 "units: an enemy commander on the left, a civilian on the right and a "
                 "healer behind. The unit is turned onto each in turn, so ranks wheel, "
                 "men change targets mid-fight and the singles take hits and die. "
                 "Nobody may teleport or snap while the fight swings round."),
             18.0F,
             {26.0F, 50.0F, 30.0F},
             {0.0F, 0.0F, 0.0F});
  QString const unit = QStringLiteral("swords");
  QString const commander_left = QStringLiteral("enemy_commander");
  QString const civilian_right = QStringLiteral("civilian");
  QString const healer_rear = QStringLiteral("healer");
  auto enemy_commander = group(
      commander_left, Troop::CarthageSwordCommander, 2, 1, {-7.0F, 0.0F, 0.0F}, 1);
  enemy_commander.facing_degrees = 90.0F;
  enemy_commander.health_override = enemy_commander.max_health_override = 6000;
  auto civilian = group(civilian_right, Troop::Civilian, 2, 1, {7.0F, 0.0F, 0.0F}, 1);
  civilian.facing_degrees = -90.0F;
  auto healer = group(healer_rear, Troop::Healer, 2, 1, {0.0F, 0.0F, -6.0F}, 1);
  s.groups = {
      group(unit, Troop::Swordsman, 1, 1, {0.0F, 0.0F, 0.0F}, 12),
      enemy_commander,
      civilian,
      healer,
  };
  s.steps = {
      at(0.5F, Command::AttackMove, commander_left, unit),
      at(0.5F, Command::AttackMove, civilian_right, unit),
      at(0.5F, Command::AttackMove, healer_rear, unit),
      at(1.0F, Command::Attack, unit, commander_left),
      at(5.0F, Command::Attack, unit, civilian_right),
      at(8.0F, Command::Attack, unit, commander_left),
      at(11.0F, Command::Attack, unit, healer_rear),
      at(14.0F, Command::Attack, unit, commander_left),
  };
  add_transition_continuity(s, {unit, commander_left, civilian_right, healer_rear});
  s.expectations.push_back(expectation(Expect::AttackAnimationObserved, unit));
  return s;
}

auto commander() -> ArenaScenarioDefinition {
  auto s =
      staged(QString::fromLatin1(k_transition_gauntlet_commander_id),
             QStringLiteral("Transition Gauntlet: Commander"),
             QStringLiteral(
                 "The direct-control commander walks, breaks into a run, halts, "
                 "reverses, strafes, swings while moving, raises and drops the guard, "
                 "dodges out of a swing and chains attacks into a walk. Every input is "
                 "honoured on its frame and the body blends through all of it."),
             12.0F,
             {},
             {0.0F, 0.0F, 0.0F});
  s.rpg_mode = true;
  s.rpg_commander_group = QStringLiteral("rpg_commander");
  QString const hero = QStringLiteral("rpg_commander");
  auto commander_group =
      group(hero, Troop::RomanVeteranConsul, 1, 1, {0.0F, 0.0F, -4.0F}, 1);
  commander_group.stamina_override = 600.0F;
  commander_group.max_stamina_override = 600.0F;
  auto crowd = sparring_partner(
      QStringLiteral("enemy_crowd"), Troop::Swordsman, {0.0F, 0.0F, 3.0F}, 5, false);
  s.groups = {commander_group, crowd};
  auto move = [hero](float time, QVector3D axes, bool run) {
    auto step = at(time, Command::RpgMove, hero);
    step.destination = axes;
    step.value = run ? 1 : 0;
    return step;
  };
  s.steps = {
      move(0.30F, {0.0F, 0.0F, 1.0F}, false),
      move(1.10F, {0.0F, 0.0F, 1.0F}, true),
      move(1.80F, {0.0F, 0.0F, 0.0F}, false),
      move(2.20F, {0.0F, 0.0F, -1.0F}, false),
      move(2.90F, {1.0F, 0.0F, 0.0F}, false),
      move(3.50F, {0.0F, 0.0F, 1.0F}, false),
      at(3.90F, Command::RpgPrimaryAttack, hero),
      move(4.60F, {0.0F, 0.0F, 0.0F}, false),
      toggle(4.80F, Command::RpgGuard, hero, true),
      toggle(5.50F, Command::RpgGuard, hero, false),
      at(5.70F, Command::RpgPrimaryAttack, hero),
      [hero] {
        auto step = at(6.05F, Command::RpgDodge, hero);
        step.destination = {-1.0F, 0.0F, 0.0F};
        return step;
      }(),
      at(6.80F, Command::RpgPrimaryAttack, hero),
      at(7.40F, Command::RpgPrimaryAttack, hero),
      move(8.00F, {-1.0F, 0.0F, 0.0F}, false),
      at(8.40F, Command::RpgHeavyAttack, hero),
      move(9.40F, {0.0F, 0.0F, -1.0F}, true),
      move(10.40F, {0.0F, 0.0F, 1.0F}, false),
      move(11.20F, {0.0F, 0.0F, 0.0F}, false),
  };
  add_transition_continuity(s, {hero});
  s.expectations.push_back(expectation(Expect::RpgWalkObserved, hero));
  s.expectations.push_back(expectation(Expect::RpgRunObserved, hero));
  s.expectations.push_back(expectation(Expect::AttackAnimationObserved, hero));
  s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
  return s;
}

} // namespace

auto build_transition_gauntlet_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;
  result.push_back(infantry());
  result.push_back(archers());
  result.push_back(cavalry());
  result.push_back(elephants());
  result.push_back(builders_crews());
  result.push_back(civilians());
  result.push_back(flanked());
  result.push_back(commander());
  for (auto& scenario : result) {
    scenario.expectations.push_back(expectation(Expect::EntityMotionIsSmooth));
  }
  return result;
}

} // namespace Arena::Scenarios
