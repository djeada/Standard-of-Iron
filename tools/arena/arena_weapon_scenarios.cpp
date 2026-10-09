#include "arena_weapon_scenarios.h"

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
using Trigger = ScenarioTriggerKind;
using Troop = Game::Units::TroopType;

} // namespace

auto build_weapon_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;

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
    s.expectations.push_back(expectation(Expect::ProjectileImpactSynchronized,
                                         QStringLiteral("blue_archers"),
                                         QStringLiteral("red_archers")));
    s.expectations.push_back(expectation(Expect::ProjectileImpactSynchronized,
                                         QStringLiteral("red_archers"),
                                         QStringLiteral("blue_archers")));
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
                      Troop::MountedSwordsman,
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
                      Troop::MountedSwordsman,
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
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("elephant"),
                                         QStringLiteral("infantry")));
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

  for (const auto nation : {Nation::RomanRepublic, Nation::Carthage}) {
    const bool roman = nation == Nation::RomanRepublic;
    auto s = definition(
        roman ? QStringLiteral("siege_roman_showcase")
              : QStringLiteral("siege_carthage_showcase"),
        QStringLiteral("Siege Engines: March, Wind and Release"),
        QStringLiteral(
            "Close inspection of rolling wheels, braced carriages, faction standards, "
            "winding mechanisms and recoil during repeated live shots."),
        24.0F,
        {11.0F, 35.0F, 145.0F});
    s.camera_focus = QVector3D(0.0F, 0.6F, 0.0F);
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_terrain_scatter = true;
    s.groups = {
        group(QStringLiteral("ballista"), Troop::Ballista, 1, 1, {-1.8F, 0, -3.0F}, 1),
        group(QStringLiteral("catapult"), Troop::Catapult, 1, 1, {1.8F, 0, -3.0F}, 1),
        group(QStringLiteral("target"), Troop::Spearman, 2, 1, {0, 0, 11.0F}, 6)};
    s.groups[0].nation_id = nation;
    s.groups[1].nation_id = nation;
    s.groups[2].health_override = 9000;
    s.groups[2].max_health_override = 9000;
    s.steps.push_back(at(0.0F, Command::Hold, QStringLiteral("target")));
    for (int engine = 0; engine < 2; ++engine) {
      const auto name =
          engine == 0 ? QStringLiteral("ballista") : QStringLiteral("catapult");
      auto march = at(0.0F, Command::Move, name);
      march.destination = {engine == 0 ? -1.8F : 1.8F, 0.0F, 0.0F};
      s.steps.push_back(march);
      s.steps.push_back(at(5.0F, Command::Attack, name, QStringLiteral("target")));
      s.expectations.push_back(expectation(Expect::GroupExists, name));
      s.expectations.push_back(expectation(
          Expect::ProjectileImpactSynchronized, name, QStringLiteral("target")));
    }
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
    s.expectations.push_back(expectation(Expect::ProjectileImpactSynchronized,
                                         QStringLiteral("catapult"),
                                         QStringLiteral("infantry")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_ballista_impact_id),
        QStringLiteral("Ballista Bolt Impact"),
        QStringLiteral("A ballista completes its loading stroke, releases a visible "
                       "heavy bolt, and damages infantry only when the bolt arrives."),
        7.0F,
        {14.0F, 32.0F, 8.0F});
    s.groups = {
        group(
            QStringLiteral("ballista"), Troop::Ballista, 1, 1, {0.0F, 0.0F, -7.0F}, 1),
        group(
            QStringLiteral("infantry"), Troop::Spearman, 2, 1, {0.0F, 0.0F, 3.0F}, 6)};
    s.groups[1].health_override = 900;
    s.groups[1].max_health_override = 900;
    s.select_spawned_units = false;
    s.steps = {at(0.0F, Command::Hold, QStringLiteral("infantry")),
               at(0.0F,
                  Command::Attack,
                  QStringLiteral("ballista"),
                  QStringLiteral("infantry"))};
    add_visual_stability(s, {QStringLiteral("infantry")});
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("ballista")));
    s.expectations.push_back(expectation(Expect::ProjectileImpactSynchronized,
                                         QStringLiteral("ballista"),
                                         QStringLiteral("infantry")));
    s.expectations.push_back(
        expectation(Expect::HitReactionObserved, QStringLiteral("infantry")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_structure_melee_assault_id),
        QStringLiteral("Structure Melee Assault"),
        QStringLiteral("Sword, spear, and elephant lanes close all the way to "
                       "visible facades. Infantry chips structures slowly while "
                       "the elephant produces heavy localized impacts."),
        13.0F,
        {23.0F, 50.0F, 4.0F});
    auto swords = group(QStringLiteral("structure_swords"),
                        Troop::Swordsman,
                        1,
                        1,
                        {-8.0F, 0.0F, -6.0F},
                        6);
    auto spears = group(QStringLiteral("structure_spears"),
                        Troop::Spearman,
                        1,
                        1,
                        {0.0F, 0.0F, -6.0F},
                        6);
    auto elephant = group(QStringLiteral("structure_elephant"),
                          Troop::Elephant,
                          1,
                          1,
                          {8.0F, 0.0F, -6.0F},
                          1);
    auto sword_wall = building(QStringLiteral("sword_wall"),
                               Game::Units::SpawnType::WallSegment,
                               Nation::Carthage,
                               2,
                               1,
                               {-8.0F, 0.0F, 2.0F});
    auto spear_wall = building(QStringLiteral("spear_wall"),
                               Game::Units::SpawnType::WallSegment,
                               Nation::Carthage,
                               2,
                               1,
                               {0.0F, 0.0F, 2.0F});
    auto elephant_home = building(QStringLiteral("elephant_home"),
                                  Game::Units::SpawnType::Home,
                                  Nation::Carthage,
                                  2,
                                  1,
                                  {8.0F, 0.0F, 2.0F});
    swords.health_override = swords.max_health_override = 1400;
    spears.health_override = spears.max_health_override = 1400;
    elephant.health_override = elephant.max_health_override = 1800;
    sword_wall.health_override = sword_wall.max_health_override = 1600;
    spear_wall.health_override = spear_wall.max_health_override = 1600;
    elephant_home.health_override = elephant_home.max_health_override = 3200;
    s.groups = {swords, spears, elephant, sword_wall, spear_wall, elephant_home};
    s.steps = {
        at(0.2F,
           Command::Attack,
           QStringLiteral("structure_swords"),
           QStringLiteral("sword_wall")),
        at(0.2F,
           Command::Attack,
           QStringLiteral("structure_spears"),
           QStringLiteral("spear_wall")),
        at(0.2F,
           Command::Attack,
           QStringLiteral("structure_elephant"),
           QStringLiteral("elephant_home")),
    };
    s.select_spawned_units = false;
    s.suppress_ui_overlays = true;
    s.suppress_spawn_anchor = true;
    add_visual_stability(
        s, {QStringLiteral("structure_swords"), QStringLiteral("structure_spears")});
    for (auto const& name : {QStringLiteral("sword_wall"),
                             QStringLiteral("spear_wall"),
                             QStringLiteral("elephant_home")}) {
      s.expectations.push_back(expectation(Expect::GroupExists, name));
      s.expectations.push_back(expectation(Expect::GroupHealthReduced, name, {}, 1.0F));
      s.expectations.push_back(expectation(Expect::StructureDamageCueObserved, name));
    }
    s.expectations.push_back(expectation(Expect::AttackAnimationObserved,
                                         QStringLiteral("structure_swords")));
    s.expectations.push_back(expectation(Expect::AttackAnimationObserved,
                                         QStringLiteral("structure_spears")));
    s.expectations.push_back(expectation(Expect::AttackAnimationObserved,
                                         QStringLiteral("structure_elephant")));
    s.expectations.push_back(expectation(Expect::StructureFacadeContactObserved,
                                         QStringLiteral("structure_swords"),
                                         QStringLiteral("sword_wall")));
    s.expectations.push_back(expectation(Expect::StructureFacadeContactObserved,
                                         QStringLiteral("structure_spears"),
                                         QStringLiteral("spear_wall")));
    s.expectations.push_back(expectation(Expect::StructureFacadeContactObserved,
                                         QStringLiteral("structure_elephant"),
                                         QStringLiteral("elephant_home")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_structure_projectile_assault_id),
        QStringLiteral("Structure Projectile Assault"),
        QStringLiteral("Arrow, ballista, and catapult lanes strike visible facades. "
                       "Arrows remain cosmetic while siege projectiles cause "
                       "localized damage cues."),
        12.0F,
        {24.0F, 50.0F, 4.0F});
    auto archers = group(QStringLiteral("structure_archers"),
                         Troop::Archer,
                         1,
                         1,
                         {-8.0F, 0.0F, -8.0F},
                         8);
    auto ballista = group(QStringLiteral("structure_ballista"),
                          Troop::Ballista,
                          1,
                          1,
                          {0.0F, 0.0F, -8.0F},
                          1);
    auto catapult = group(QStringLiteral("structure_catapult"),
                          Troop::Catapult,
                          1,
                          1,
                          {8.0F, 0.0F, -8.0F},
                          1);
    auto arrow_wall = building(QStringLiteral("arrow_wall"),
                               Game::Units::SpawnType::WallSegment,
                               Nation::Carthage,
                               2,
                               1,
                               {-8.0F, 0.0F, 4.0F});
    auto bolt_wall = building(QStringLiteral("bolt_wall"),
                              Game::Units::SpawnType::WallSegment,
                              Nation::Carthage,
                              2,
                              1,
                              {0.0F, 0.0F, 4.0F});
    auto stone_home = building(QStringLiteral("stone_home"),
                               Game::Units::SpawnType::Home,
                               Nation::Carthage,
                               2,
                               1,
                               {8.0F, 0.0F, 4.0F});
    arrow_wall.health_override = arrow_wall.max_health_override = 1400;
    bolt_wall.health_override = bolt_wall.max_health_override = 1800;
    stone_home.health_override = stone_home.max_health_override = 3200;
    s.groups = {archers, ballista, catapult, arrow_wall, bolt_wall, stone_home};
    s.steps = {
        at(0.2F,
           Command::Attack,
           QStringLiteral("structure_archers"),
           QStringLiteral("arrow_wall")),
        at(0.2F,
           Command::Attack,
           QStringLiteral("structure_ballista"),
           QStringLiteral("bolt_wall")),
        at(0.2F,
           Command::Attack,
           QStringLiteral("structure_catapult"),
           QStringLiteral("stone_home")),
    };
    s.select_spawned_units = false;
    s.suppress_ui_overlays = true;
    s.suppress_spawn_anchor = true;
    s.expectations.push_back(expectation(Expect::ProjectileImpactObserved,
                                         QStringLiteral("structure_archers"),
                                         QStringLiteral("arrow_wall")));
    s.expectations.push_back(
        expectation(Expect::GroupHealthUnchanged, QStringLiteral("arrow_wall")));
    for (auto const& pair :
         {std::pair{QStringLiteral("structure_ballista"), QStringLiteral("bolt_wall")},
          std::pair{QStringLiteral("structure_catapult"),
                    QStringLiteral("stone_home")}}) {
      s.expectations.push_back(
          expectation(Expect::ProjectileImpactSynchronized, pair.first, pair.second));
      s.expectations.push_back(
          expectation(Expect::GroupHealthReduced, pair.second, {}, 1.0F));
      s.expectations.push_back(
          expectation(Expect::StructureDamageCueObserved, pair.second));
    }
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("arrow_wall")));
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("bolt_wall")));
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("stone_home")));
    s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_structure_flaming_siege_id),
        QStringLiteral("Structure Flaming Siege"),
        QStringLiteral("A catapult lobs flaming stones into a home while swordsmen "
                       "hack at a wall beside it. Only the structure taking "
                       "incendiary shot catches fire; the melee lane produces dust "
                       "and rubble alone."),
        16.0F,
        {24.0F, 50.0F, 6.0F});
    auto catapult = group(QStringLiteral("fire_catapult"),
                          Troop::Catapult,
                          1,
                          1,
                          {6.0F, 0.0F, -10.0F},
                          1);
    auto swords = group(
        QStringLiteral("wall_swords"), Troop::Swordsman, 1, 1, {-8.0F, 0.0F, -4.0F}, 6);
    auto burning_home = building(QStringLiteral("burning_home"),
                                 Game::Units::SpawnType::Home,
                                 Nation::Carthage,
                                 2,
                                 1,
                                 {6.0F, 0.0F, 4.0F});
    auto chipped_wall = building(QStringLiteral("chipped_wall"),
                                 Game::Units::SpawnType::WallSegment,
                                 Nation::Carthage,
                                 2,
                                 1,
                                 {-8.0F, 0.0F, 2.0F});
    swords.health_override = swords.max_health_override = 1400;
    burning_home.health_override = burning_home.max_health_override = 3200;
    chipped_wall.health_override = chipped_wall.max_health_override = 1600;
    s.groups = {catapult, swords, burning_home, chipped_wall};
    s.steps = {at(0.2F,
                  Command::Attack,
                  QStringLiteral("fire_catapult"),
                  QStringLiteral("burning_home")),
               at(0.2F,
                  Command::Attack,
                  QStringLiteral("wall_swords"),
                  QStringLiteral("chipped_wall"))};
    s.select_spawned_units = false;
    s.suppress_ui_overlays = true;
    s.suppress_spawn_anchor = true;
    s.expectations.push_back(expectation(Expect::FlamingProjectileObserved,
                                         QStringLiteral("fire_catapult"),
                                         QStringLiteral("burning_home")));
    s.expectations.push_back(expectation(Expect::ProjectileImpactSynchronized,
                                         QStringLiteral("fire_catapult"),
                                         QStringLiteral("burning_home")));
    s.expectations.push_back(
        expectation(Expect::StructureFireObserved, QStringLiteral("burning_home")));
    s.expectations.push_back(expectation(Expect::StructureDamageCueObserved,
                                         QStringLiteral("burning_home")));
    s.expectations.push_back(expectation(
        Expect::GroupHealthReduced, QStringLiteral("burning_home"), {}, 1.0F));
    s.expectations.push_back(expectation(Expect::StructureFacadeContactObserved,
                                         QStringLiteral("wall_swords"),
                                         QStringLiteral("chipped_wall")));
    s.expectations.push_back(expectation(Expect::StructureDamageCueObserved,
                                         QStringLiteral("chipped_wall")));
    s.expectations.push_back(
        expectation(Expect::NoStructureFireObserved, QStringLiteral("chipped_wall")));
    s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_structure_melee_destruction_id),
        QStringLiteral("Structure Melee Destruction"),
        QStringLiteral("Swordsmen dismantle a weakened wall segment by hand. The "
                       "collapse is dust and debris from first blow to last, with "
                       "no flame anywhere in the sequence."),
        18.0F,
        {20.0F, 48.0F, 8.0F});
    auto swords = group(QStringLiteral("demolition_swords"),
                        Troop::Swordsman,
                        1,
                        2,
                        {-2.6F, 0.0F, -4.0F},
                        8);
    auto doomed_wall = building(QStringLiteral("doomed_wall"),
                                Game::Units::SpawnType::WallSegment,
                                Nation::Carthage,
                                2,
                                1,
                                {0.0F, 0.0F, 2.0F});
    swords.health_override = swords.max_health_override = 1400;
    doomed_wall.health_override = doomed_wall.max_health_override = 120;
    s.groups = {swords, doomed_wall};
    s.steps = {at(0.2F,
                  Command::Attack,
                  QStringLiteral("demolition_swords"),
                  QStringLiteral("doomed_wall"))};
    s.select_spawned_units = false;
    s.suppress_ui_overlays = true;
    s.suppress_spawn_anchor = true;
    s.expectations.push_back(expectation(Expect::StructureFacadeContactObserved,
                                         QStringLiteral("demolition_swords"),
                                         QStringLiteral("doomed_wall")));
    s.expectations.push_back(
        expectation(Expect::StructureDamageCueObserved, QStringLiteral("doomed_wall")));
    s.expectations.push_back(
        expectation(Expect::NoStructureFireObserved, QStringLiteral("doomed_wall")));
    s.expectations.push_back(
        expectation(Expect::GroupDestroyed, QStringLiteral("doomed_wall")));
    s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_catapult_ammunition_retarget_id),
        QStringLiteral("Catapult Ammunition Retarget"),
        QStringLiteral("A catapult opens on a barracks with flaming shot, then is "
                       "swung onto infantry. The loaded round changes with the "
                       "target: plain stone for troops, fire for the structure."),
        22.0F,
        {24.0F, 50.0F, 10.0F});
    auto catapult = group(QStringLiteral("swing_catapult"),
                          Troop::Catapult,
                          1,
                          1,
                          {0.0F, 0.0F, -11.0F},
                          1);
    auto infantry = group(
        QStringLiteral("swing_infantry"), Troop::Spearman, 2, 1, {8.0F, 0.0F, 2.0F}, 8);
    auto barracks = building(QStringLiteral("swing_barracks"),
                             Game::Units::SpawnType::Barracks,
                             Nation::Carthage,
                             2,
                             1,
                             {-4.0F, 0.0F, 3.0F});
    infantry.health_override = infantry.max_health_override = 1600;
    barracks.health_override = barracks.max_health_override = 3200;
    s.groups = {catapult, infantry, barracks};
    s.steps = {at(0.2F, Command::Hold, QStringLiteral("swing_infantry")),
               at(0.2F,
                  Command::Attack,
                  QStringLiteral("swing_catapult"),
                  QStringLiteral("swing_barracks")),
               at(9.0F,
                  Command::Attack,
                  QStringLiteral("swing_catapult"),
                  QStringLiteral("swing_infantry"))};
    s.select_spawned_units = false;
    s.suppress_ui_overlays = true;
    s.suppress_spawn_anchor = true;
    s.expectations.push_back(expectation(Expect::FlamingProjectileObserved,
                                         QStringLiteral("swing_catapult"),
                                         QStringLiteral("swing_barracks")));
    s.expectations.push_back(expectation(Expect::NoFlamingProjectileObserved,
                                         QStringLiteral("swing_catapult"),
                                         QStringLiteral("swing_infantry")));
    s.expectations.push_back(
        expectation(Expect::StructureFireObserved, QStringLiteral("swing_barracks")));
    s.expectations.push_back(expectation(Expect::ProjectileImpactSynchronized,
                                         QStringLiteral("swing_catapult"),
                                         QStringLiteral("swing_infantry")));
    s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F));
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_mounted_sword_duel_id),
                   QStringLiteral("Mounted Sword Duel"),
                   QStringLiteral("Mounted sword chamber, full swing, and recovery."),
                   10.0F,
                   {13.0F, 43.0F, 28.0F});
    s.groups = {group(QStringLiteral("blue_swordsmans"),
                      Troop::MountedSwordsman,
                      1,
                      1,
                      {-3.0F, 0.0F, 0.0F},
                      4),
                group(QStringLiteral("red_swordsmans"),
                      Troop::MountedSwordsman,
                      2,
                      1,
                      {3.0F, 0.0F, 0.0F},
                      4)};
    s.steps = {at(0.0F,
                  Command::Attack,
                  QStringLiteral("blue_swordsmans"),
                  QStringLiteral("red_swordsmans")),
               at(0.0F,
                  Command::Attack,
                  QStringLiteral("red_swordsmans"),
                  QStringLiteral("blue_swordsmans"))};
    add_visual_stability(
        s, {QStringLiteral("blue_swordsmans"), QStringLiteral("red_swordsmans")});
    s.expectations.push_back(
        expectation(Expect::NoLimbOverextension, QStringLiteral("blue_swordsmans")));
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("blue_swordsmans"),
                                         QStringLiteral("red_swordsmans")));
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
        QString::fromLatin1(k_mounted_spear_grip_id),
        QStringLiteral("Mounted Spear Grip"),
        QStringLiteral("Close look at how a horse spearman holds the spear: one rider "
                       "rests with it upright, the other thrusts at a dummy. The shaft "
                       "must leave the right fist and clear the horse's neck."),
        8.0F,
        {7.5F, 26.0F, 70.0F});
    auto resting = group(QStringLiteral("resting_rider"),
                         Troop::HorseSpearman,
                         1,
                         1,
                         {-2.4F, 0.0F, 0.0F},
                         1);
    resting.attacks_disabled = true;
    auto striker = group(
        QStringLiteral("striker"), Troop::HorseSpearman, 1, 1, {2.4F, 0.0F, -1.3F}, 1);
    auto dummy =
        group(QStringLiteral("dummy"), Troop::Spearman, 2, 1, {2.4F, 0.0F, 1.0F}, 1);
    dummy.facing_degrees = 180.0F;
    dummy.attacks_disabled = true;
    dummy.health_override = dummy.max_health_override = 100000;
    s.groups = {resting, striker, dummy};
    s.steps = {
        at(0.5F, Command::Attack, QStringLiteral("striker"), QStringLiteral("dummy"))};
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("striker"),
                                         QStringLiteral("dummy")));
    s.expectations.push_back(
        expectation(Expect::NoLimbOverextension, QStringLiteral("striker")));
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

  return result;
}

} // namespace Arena::Scenarios
