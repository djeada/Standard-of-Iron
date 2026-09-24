#include "arena_reel_scenarios.h"

#include <array>
#include <utility>

#include "matchup_short.h"

namespace Arena::Scenarios {
namespace {

using Command = ScenarioCommandKind;
using Expect = ArenaExpectationKind;
using Nation = Game::Systems::NationID;
using Trigger = ScenarioTriggerKind;
using Troop = Game::Units::TroopType;

auto line(QString name,
          Troop troop,
          int owner,
          int count,
          QVector3D origin,
          QVector3D spacing = {5.0F, 0.0F, 0.0F}) -> ArenaScenarioGroup {
  ArenaScenarioGroup result;
  result.name = std::move(name);
  result.troop_type = troop;
  result.nation_id = owner == 1 ? Nation::RomanRepublic : Nation::Carthage;
  result.owner_id = owner;
  result.count = count;
  result.origin = origin;
  result.spacing = spacing;
  result.facing_degrees = owner == 1 ? 0.0F : 180.0F;
  return result;
}

auto at(float time,
        Command command,
        QString source,
        QString target = {}) -> ArenaScenarioStep {
  ArenaScenarioStep result;
  result.name = QStringLiteral("%1_%2").arg(QString::number(time, 'f', 2), source);
  result.trigger = {Trigger::AtTime, time, {}, {}, 0.0F};
  result.command = command;
  result.group = std::move(source);
  result.target_group = std::move(target);
  return result;
}

auto move_to(float time, QString source, QVector3D destination) -> ArenaScenarioStep {
  auto result = at(time, Command::Move, std::move(source));
  result.destination = destination;
  return result;
}

auto expect(Expect kind, QString group = {}) -> ArenaExpectation {
  ArenaExpectation result;
  result.kind = kind;
  result.group = std::move(group);
  return result;
}

void dress(ArenaScenarioDefinition& s) {
  s.ground_type = QStringLiteral("grass_dry");
  s.select_spawned_units = false;
  s.suppress_spawn_anchor = true;
  s.suppress_ui_overlays = true;
  s.force_full_creature_lod = true;
  s.collect_animation_diagnostics = false;
  s.graphics_quality = Render::GraphicsQuality::Ultra;
  s.environment.start_time = 16.2F;
  s.environment.time_mode = Game::Map::TimeMode::Locked;
  s.environment.exposure_override = 1.3F;
  s.environment.fog_density_override = 0.010F;
  s.suppress_boundary_mountains = true;
}

auto cannae() -> ArenaScenarioDefinition {
  ArenaScenarioDefinition s;
  s.id = QStringLiteral("reel_cannae");
  s.label = QStringLiteral("Reel: Cannae, 216 BC");
  s.description =
      QStringLiteral("A deep Roman block advances into a Carthaginian crescent. "
                     "The centre falls back, the Libyan flanks turn in and the "
                     "cavalry closes the rear: the double envelopment.");
  s.duration_seconds = 75.0F;
  s.arena_floor_half_extent = 90.0F;
  s.terrain_grid_extent = 240;
  s.camera = {110.0F, 55.0F, 0.0F};
  s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
  dress(s);
  s.environment.time_mode = Game::Map::TimeMode::Locked;

  s.groups = {
      line(QStringLiteral("rome_hastati"),
           Troop::Swordsman,
           1,
           12,
           {-24.75F, 0.0F, -12.0F},
           {4.5F, 0.0F, 0.0F}),
      line(QStringLiteral("rome_principes"),
           Troop::Swordsman,
           1,
           12,
           {-24.75F, 0.0F, -17.0F},
           {4.5F, 0.0F, 0.0F}),
      line(QStringLiteral("rome_legion_reserve"),
           Troop::Swordsman,
           1,
           12,
           {-24.75F, 0.0F, -22.0F},
           {4.5F, 0.0F, 0.0F}),
      line(QStringLiteral("rome_triarii"),
           Troop::Spearman,
           1,
           12,
           {-24.75F, 0.0F, -27.0F},
           {4.5F, 0.0F, 0.0F}),
      line(QStringLiteral("rome_horse_left"),
           Troop::MountedSwordsman,
           1,
           5,
           {-62.0F, 0.0F, -16.0F},
           {4.5F, 0.0F, 0.0F}),
      line(QStringLiteral("rome_horse_right"),
           Troop::MountedSwordsman,
           1,
           5,
           {44.0F, 0.0F, -16.0F},
           {4.5F, 0.0F, 0.0F}),
      line(QStringLiteral("rome_consul"),
           Troop::RomanVeteranConsul,
           1,
           1,
           {0.0F, 0.0F, -31.0F}),

      line(QStringLiteral("punic_gauls"),
           Troop::Swordsman,
           2,
           10,
           {-20.25F, 0.0F, 3.0F},
           {4.5F, 0.0F, 0.0F}),
      line(QStringLiteral("punic_iberians_left"),
           Troop::Swordsman,
           2,
           5,
           {-40.0F, 0.0F, 8.0F},
           {4.5F, 0.0F, 0.0F}),
      line(QStringLiteral("punic_iberians_right"),
           Troop::Swordsman,
           2,
           5,
           {22.0F, 0.0F, 8.0F},
           {4.5F, 0.0F, 0.0F}),
      line(QStringLiteral("punic_libyans_left"),
           Troop::Spearman,
           2,
           12,
           {-40.0F, 0.0F, 18.0F},
           {0.0F, 0.0F, -3.2F}),
      line(QStringLiteral("punic_libyans_right"),
           Troop::Spearman,
           2,
           12,
           {40.0F, 0.0F, 18.0F},
           {0.0F, 0.0F, -3.2F}),
      line(QStringLiteral("punic_horse_left"),
           Troop::MountedSwordsman,
           2,
           10,
           {-84.0F, 0.0F, 0.0F},
           {4.0F, 0.0F, 0.0F}),
      line(QStringLiteral("punic_horse_right"),
           Troop::HorseSpearman,
           2,
           10,
           {48.0F, 0.0F, 0.0F},
           {4.0F, 0.0F, 0.0F}),
      line(QStringLiteral("hannibal"),
           Troop::CarthageSwordCommander,
           2,
           1,
           {0.0F, 0.0F, 18.0F}),
  };

  ArenaScenarioBattleSide rome;
  rome.owner_id = 1;
  rome.label = QStringLiteral("ROME");
  rome.home = QVector3D(0.0F, 0.0F, -17.0F);
  rome.home_radius = 45.0F;
  ArenaScenarioBattleSide carthage;
  carthage.owner_id = 2;
  carthage.label = QStringLiteral("CARTHAGE");
  carthage.home = QVector3D(0.0F, 0.0F, 10.0F);
  carthage.home_radius = 45.0F;
  s.battle_sides = {rome, carthage};

  s.steps = {

      at(0.5F,
         Command::AttackMove,
         QStringLiteral("rome_hastati"),
         QStringLiteral("punic_gauls")),
      at(1.5F,
         Command::AttackMove,
         QStringLiteral("rome_principes"),
         QStringLiteral("punic_gauls")),
      at(2.0F,
         Command::AttackMove,
         QStringLiteral("rome_legion_reserve"),
         QStringLiteral("punic_gauls")),
      at(2.5F,
         Command::AttackMove,
         QStringLiteral("rome_triarii"),
         QStringLiteral("punic_gauls")),
      at(3.0F,
         Command::AttackMove,
         QStringLiteral("rome_consul"),
         QStringLiteral("punic_gauls")),
      at(0.5F, Command::Hold, QStringLiteral("punic_gauls")),
      at(0.5F, Command::Hold, QStringLiteral("punic_iberians_left")),
      at(0.5F, Command::Hold, QStringLiteral("punic_iberians_right")),
      at(0.5F, Command::Hold, QStringLiteral("punic_libyans_left")),
      at(0.5F, Command::Hold, QStringLiteral("punic_libyans_right")),

      at(1.0F,
         Command::Charge,
         QStringLiteral("punic_horse_left"),
         QStringLiteral("rome_horse_left")),
      at(1.0F,
         Command::Charge,
         QStringLiteral("punic_horse_right"),
         QStringLiteral("rome_horse_right")),
      at(1.0F,
         Command::AttackMove,
         QStringLiteral("rome_horse_left"),
         QStringLiteral("punic_horse_left")),
      at(1.0F,
         Command::AttackMove,
         QStringLiteral("rome_horse_right"),
         QStringLiteral("punic_horse_right")),

      move_to(9.0F, QStringLiteral("punic_gauls"), {0.0F, 0.0F, 24.0F}),
      move_to(9.5F, QStringLiteral("punic_iberians_left"), {-26.0F, 0.0F, 22.0F}),
      move_to(9.5F, QStringLiteral("punic_iberians_right"), {26.0F, 0.0F, 22.0F}),
      at(16.0F,
         Command::AttackMove,
         QStringLiteral("punic_gauls"),
         QStringLiteral("rome_hastati")),
      at(16.0F,
         Command::AttackMove,
         QStringLiteral("punic_iberians_left"),
         QStringLiteral("rome_hastati")),
      at(16.0F,
         Command::AttackMove,
         QStringLiteral("punic_iberians_right"),
         QStringLiteral("rome_hastati")),
      at(16.0F,
         Command::AttackMove,
         QStringLiteral("hannibal"),
         QStringLiteral("rome_hastati")),

      at(13.0F,
         Command::AttackMove,
         QStringLiteral("punic_libyans_left"),
         QStringLiteral("rome_principes")),
      at(13.0F,
         Command::AttackMove,
         QStringLiteral("punic_libyans_right"),
         QStringLiteral("rome_principes")),

      move_to(11.0F, QStringLiteral("punic_horse_left"), {-30.0F, 0.0F, -50.0F}),
      move_to(11.0F, QStringLiteral("punic_horse_right"), {30.0F, 0.0F, -50.0F}),
      at(19.0F,
         Command::Charge,
         QStringLiteral("punic_horse_left"),
         QStringLiteral("rome_triarii")),
      at(19.0F,
         Command::Charge,
         QStringLiteral("punic_horse_right"),
         QStringLiteral("rome_triarii")),
  };

  for (const float time : {20.0F, 25.0F, 30.0F, 35.0F}) {
    for (const char* legion :
         {"rome_hastati", "rome_principes", "rome_legion_reserve", "rome_triarii"}) {
      auto crush = at(time, Command::ApplyDamage, QString::fromLatin1(legion));
      crush.value = 170;
      s.steps.push_back(std::move(crush));
    }
  }

  s.expectations = {
      expect(Expect::GroupExists, QStringLiteral("rome_hastati")),
      expect(Expect::GroupIsRendered, QStringLiteral("punic_gauls")),
      expect(Expect::AttackAnimationObserved, QStringLiteral("rome_hastati")),
  };
  return s;
}

auto front_rank() -> ArenaScenarioDefinition {
  ArenaScenarioDefinition s;
  s.id = QStringLiteral("reel_front_rank");
  s.label = QStringLiteral("Reel: From Inside The Line");
  s.description = QStringLiteral("Rome and Carthage, three lines a side, collide "
                                 "head on so the clash can be filmed at eye level.");
  s.duration_seconds = 70.0F;
  s.arena_floor_half_extent = 90.0F;
  s.terrain_grid_extent = 240;
  s.camera = {90.0F, 40.0F, 0.0F};
  s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
  dress(s);

  const auto side = [&s](const QString& prefix, int owner, float sign) {
    const auto z = [sign](float depth) {
      return QVector3D(-31.5F, 0.0F, sign * depth);
    };
    s.groups.push_back(line(prefix + QStringLiteral("_front"),
                            Troop::Swordsman,
                            owner,
                            15,
                            z(14.0F),
                            {4.5F, 0.0F, 0.0F}));
    s.groups.push_back(line(prefix + QStringLiteral("_second"),
                            Troop::Swordsman,
                            owner,
                            15,
                            z(19.0F),
                            {4.5F, 0.0F, 0.0F}));
    s.groups.push_back(line(prefix + QStringLiteral("_spears"),
                            Troop::Spearman,
                            owner,
                            15,
                            z(24.0F),
                            {4.5F, 0.0F, 0.0F}));
    s.groups.push_back(line(prefix + QStringLiteral("_archers"),
                            Troop::Archer,
                            owner,
                            10,
                            QVector3D(-22.5F, 0.0F, sign * 31.0F),
                            {5.0F, 0.0F, 0.0F}));
    s.groups.push_back(line(prefix + QStringLiteral("_horse_left"),
                            owner == 1 ? Troop::MountedSwordsman : Troop::HorseSpearman,
                            owner,
                            6,
                            QVector3D(-62.0F, 0.0F, sign * 18.0F),
                            {4.0F, 0.0F, 0.0F}));
    s.groups.push_back(line(prefix + QStringLiteral("_horse_right"),
                            Troop::MountedSwordsman,
                            owner,
                            6,
                            QVector3D(42.0F, 0.0F, sign * 18.0F),
                            {4.0F, 0.0F, 0.0F}));
  };
  side(QStringLiteral("rome"), 1, -1.0F);
  side(QStringLiteral("carthage"), 2, 1.0F);

  ArenaScenarioBattleSide rome;
  rome.owner_id = 1;
  rome.label = QStringLiteral("ROME");
  rome.home = QVector3D(0.0F, 0.0F, -20.0F);
  rome.home_radius = 45.0F;
  ArenaScenarioBattleSide carthage;
  carthage.owner_id = 2;
  carthage.label = QStringLiteral("CARTHAGE");
  carthage.home = QVector3D(0.0F, 0.0F, 20.0F);
  carthage.home_radius = 45.0F;
  s.battle_sides = {rome, carthage};

  s.steps = {
      at(0.3F, Command::Hold, QStringLiteral("rome_front")),
      at(0.3F, Command::Hold, QStringLiteral("rome_second")),
      at(0.3F, Command::Hold, QStringLiteral("rome_spears")),
      at(1.0F,
         Command::AttackMove,
         QStringLiteral("carthage_front"),
         QStringLiteral("rome_front")),
      at(1.6F,
         Command::AttackMove,
         QStringLiteral("carthage_second"),
         QStringLiteral("rome_front")),
      at(2.2F,
         Command::AttackMove,
         QStringLiteral("carthage_spears"),
         QStringLiteral("rome_second")),
      at(4.0F,
         Command::Attack,
         QStringLiteral("rome_archers"),
         QStringLiteral("carthage_front")),
      at(4.0F,
         Command::Attack,
         QStringLiteral("carthage_archers"),
         QStringLiteral("rome_front")),
      at(6.0F,
         Command::Charge,
         QStringLiteral("carthage_horse_left"),
         QStringLiteral("rome_horse_left")),
      at(6.0F,
         Command::Charge,
         QStringLiteral("rome_horse_right"),
         QStringLiteral("carthage_horse_right")),
      at(6.0F,
         Command::AttackMove,
         QStringLiteral("rome_horse_left"),
         QStringLiteral("carthage_horse_left")),
      at(6.0F,
         Command::AttackMove,
         QStringLiteral("carthage_horse_right"),
         QStringLiteral("rome_horse_right")),
      at(12.0F,
         Command::AttackMove,
         QStringLiteral("rome_front"),
         QStringLiteral("carthage_front")),
      at(12.0F,
         Command::AttackMove,
         QStringLiteral("rome_second"),
         QStringLiteral("carthage_front")),
      at(14.0F,
         Command::AttackMove,
         QStringLiteral("rome_spears"),
         QStringLiteral("carthage_second")),
  };
  s.expectations = {
      expect(Expect::GroupIsRendered, QStringLiteral("rome_front")),
      expect(Expect::GroupIsRendered, QStringLiteral("carthage_front")),
      expect(Expect::AttackAnimationObserved, QStringLiteral("rome_front")),
  };
  return s;
}

struct Round {
  const char* id;
  const char* matchup;
  float fight_seconds;
};

constexpr std::array k_rounds{
    Round{"reel_who_wins_round_1",
          "24 roman archer vs 10 carthage horse_swordsman",
          60.0F},
    Round{
        "reel_who_wins_round_2", "20 roman swordsman vs 20 carthage swordsman", 60.0F},
    Round{"reel_who_wins_round_3",
          "12 roman horse_swordsman vs 20 carthage spearman",
          60.0F},
};

auto round_definition(const Round& round) -> ArenaScenarioDefinition {
  QString error;
  auto matchup = Matchup::parse(QString::fromLatin1(round.matchup), &error);
  Q_ASSERT_X(matchup.has_value(), "reel round", qPrintable(error));
  matchup->fight_seconds = round.fight_seconds;
  matchup->report_seconds = 3.0F;
  auto s = Matchup::build_scenario(*matchup);
  s.id = QString::fromLatin1(round.id);
  s.label = QStringLiteral("Reel: %1").arg(Matchup::title(*matchup));
  dress(s);
  s.suppress_ui_overlays = false;
  return s;
}

} // namespace

auto build_reel_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;
  result.push_back(cannae());
  result.push_back(front_rank());
  for (const auto& round : k_rounds) {
    result.push_back(round_definition(round));
  }
  return result;
}

} // namespace Arena::Scenarios
