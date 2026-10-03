#include "arena_sepulcher_scenarios.h"

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

auto build_sepulcher_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;

  {
    auto s = definition(
        QString::fromLatin1(k_sepulcher_roster_lineup_id),
        QStringLiteral("Iron Sepulcher Roster"),
        QStringLiteral("Presents the complete Iron Sepulcher roster - skeleton "
                       "swordsman, skeleton archer, and grave priest - beside a Roman "
                       "and a Carthaginian line for silhouette, scale, and material "
                       "review."),
        10.0F,
        {13.5F, 40.0F, 0.0F});
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.groups = {nation_group(QStringLiteral("skeleton_swordsmen"),
                             Troop::SkeletonSwordsman,
                             Nation::IronSepulcher,
                             2,
                             1,
                             {-4.5F, 0.0F, 2.0F},
                             18),
                nation_group(QStringLiteral("skeleton_archers"),
                             Troop::SkeletonArcher,
                             Nation::IronSepulcher,
                             2,
                             1,
                             {0.0F, 0.0F, 2.0F},
                             18),
                nation_group(QStringLiteral("grave_priest"),
                             Troop::GravePriest,
                             Nation::IronSepulcher,
                             2,
                             1,
                             {4.5F, 0.0F, 2.0F},
                             1),
                nation_group(QStringLiteral("roman_line"),
                             Troop::Swordsman,
                             Nation::RomanRepublic,
                             1,
                             1,
                             {-3.0F, 0.0F, -3.0F},
                             15),
                nation_group(QStringLiteral("carthage_line"),
                             Troop::Spearman,
                             Nation::Carthage,
                             3,
                             1,
                             {3.0F, 0.0F, -3.0F},
                             24)};
    for (auto const& name : {QStringLiteral("skeleton_swordsmen"),
                             QStringLiteral("skeleton_archers"),
                             QStringLiteral("grave_priest"),
                             QStringLiteral("roman_line"),
                             QStringLiteral("carthage_line")}) {
      s.expectations.push_back(expectation(Expect::GroupExists, name));
      s.expectations.push_back(expectation(Expect::GroupIsRendered, name));
    }
    s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_sepulcher_spell_fx_showcase_id),
        QStringLiteral("Iron Sepulcher Spell FX"),
        QStringLiteral("Close visual review of a grave priest casting fireballs "
                       "beside a skeleton guard, including projectile trail, impact "
                       "ignition, and the target's persistent burning treatment."),
        9.0F,
        {6.8F, 27.0F, 90.0F});
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.camera_focus = QVector3D(-1.3F, 0.0F, -0.1F);

    auto priest = nation_group(QStringLiteral("grave_priest"),
                               Troop::GravePriest,
                               Nation::IronSepulcher,
                               2,
                               1,
                               {-1.2F, 0.0F, -2.2F},
                               1);
    auto guard = nation_group(QStringLiteral("skeleton_guard"),
                              Troop::SkeletonSwordsman,
                              Nation::IronSepulcher,
                              2,
                              1,
                              {-3.0F, 0.0F, -1.5F},
                              1);
    guard.health_override = 420;
    guard.max_health_override = 1400;
    auto target = nation_group(QStringLiteral("roman_target"),
                               Troop::Swordsman,
                               Nation::RomanRepublic,
                               1,
                               1,
                               {-1.2F, 0.0F, 2.0F},
                               1);
    target.health_override = target.max_health_override = 1400;
    s.groups = {std::move(priest), std::move(guard), std::move(target)};
    s.steps = {at(0.45F,
                  Command::Attack,
                  QStringLiteral("grave_priest"),
                  QStringLiteral("roman_target"))};
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("grave_priest")));
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("skeleton_guard")));
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("roman_target")));
    s.expectations.push_back(expectation(Expect::ProjectileImpactSynchronized,
                                         QStringLiteral("grave_priest"),
                                         QStringLiteral("roman_target")));
    add_visual_stability(s,
                         {QStringLiteral("grave_priest"),
                          QStringLiteral("skeleton_guard"),
                          QStringLiteral("roman_target")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_sepulcher_fireball_review_id),
        QStringLiteral("Iron Sepulcher Fireball Review"),
        QStringLiteral("Fireball close-up on clean ground: no melee, no dust, no "
                       "burning bystanders. The camera sits on the flight path so "
                       "the cast, the ball in flight, its smoke trail and the "
                       "detonation can be judged frame by frame."),
        14.0F,
        {8.0F, 24.0F, 90.0F});
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.camera_focus = QVector3D(-1.2F, 0.9F, 0.0F);

    auto priest = nation_group(QStringLiteral("fireball_caster"),
                               Troop::GravePriest,
                               Nation::IronSepulcher,
                               2,
                               1,
                               {-1.2F, 0.0F, -3.4F},
                               1);
    auto target = nation_group(QStringLiteral("fireball_target"),
                               Troop::Swordsman,
                               Nation::RomanRepublic,
                               1,
                               1,
                               {-1.2F, 0.0F, 3.4F},
                               1);

    target.health_override = target.max_health_override = 20000;
    s.groups = {std::move(priest), std::move(target)};
    s.steps = {at(0.4F,
                  Command::Attack,
                  QStringLiteral("fireball_caster"),
                  QStringLiteral("fireball_target")),
               at(0.4F, Command::Hold, QStringLiteral("fireball_target"))};
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("fireball_caster")));
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("fireball_target")));
    s.expectations.push_back(expectation(Expect::ProjectileFlightObserved,
                                         QStringLiteral("fireball_caster"),
                                         QStringLiteral("fireball_target")));
    s.expectations.push_back(expectation(Expect::ProjectileImpactObserved,
                                         QStringLiteral("fireball_caster"),
                                         QStringLiteral("fireball_target")));
    add_visual_stability(
        s, {QStringLiteral("fireball_caster"), QStringLiteral("fireball_target")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_sepulcher_vs_rome_infantry_id),
        QStringLiteral("Sepulcher vs Rome: Infantry"),
        QStringLiteral("Equivalent-value melee test: three Roman swordsmen against a "
                       "skeleton warband of equal recruitment value. No eligible "
                       "soldier on either side may idle once the lines meet."),
        16.0F,
        {18.0F, 44.0F, 28.0F});
    s.groups = {nation_group(QStringLiteral("roman_swords"),
                             Troop::Swordsman,
                             Nation::RomanRepublic,
                             1,
                             3,
                             {0.0F, 0.0F, -6.0F}),
                nation_group(QStringLiteral("skeleton_swords"),
                             Troop::SkeletonSwordsman,
                             Nation::IronSepulcher,
                             2,
                             3,
                             {-1.5F, 0.0F, 6.0F}),
                nation_group(QStringLiteral("skeleton_bows"),
                             Troop::SkeletonArcher,
                             Nation::IronSepulcher,
                             2,
                             1,
                             {5.5F, 0.0F, 8.5F})};
    s.steps = {at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("roman_swords"),
                  QStringLiteral("skeleton_swords")),
               at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("skeleton_swords"),
                  QStringLiteral("roman_swords")),
               at(0.5F,
                  Command::Attack,
                  QStringLiteral("skeleton_bows"),
                  QStringLiteral("roman_swords"))};
    add_visual_stability(s,
                         {QStringLiteral("roman_swords"),
                          QStringLiteral("skeleton_swords"),
                          QStringLiteral("skeleton_bows")});
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("roman_swords"), {}, 0.45F));
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("skeleton_swords"), {}, 0.45F));
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("roman_swords")));
    s.expectations.push_back(expectation(Expect::AttackAnimationObserved,
                                         QStringLiteral("skeleton_swords")));
    s.expectations.push_back(expectation(Expect::NoEligibleTroopIdleDuringCombat,
                                         QStringLiteral("skeleton_swords"),
                                         QStringLiteral("roman_swords"),
                                         1.25F,
                                         1.0F,
                                         8.0F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_sepulcher_vs_rome_ranged_id),
        QStringLiteral("Sepulcher vs Rome: Ranged"),
        QStringLiteral("Missile exchange between a Roman archer line with a swordsman "
                       "screen and cursed skeleton archers led by a grave priest."),
        16.0F,
        {24.0F, 50.0F, 26.0F});
    s.groups = {nation_group(QStringLiteral("roman_bows"),
                             Troop::Archer,
                             Nation::RomanRepublic,
                             1,
                             4,
                             {0.0F, 0.0F, -10.0F}),
                nation_group(QStringLiteral("roman_screen"),
                             Troop::Swordsman,
                             Nation::RomanRepublic,
                             1,
                             1,
                             {0.0F, 0.0F, -6.0F}),
                nation_group(QStringLiteral("skeleton_bows"),
                             Troop::SkeletonArcher,
                             Nation::IronSepulcher,
                             2,
                             3,
                             {0.0F, 0.0F, 10.0F}),
                nation_group(QStringLiteral("grave_priest"),
                             Troop::GravePriest,
                             Nation::IronSepulcher,
                             2,
                             1,
                             {6.5F, 0.0F, 13.0F},
                             1)};
    s.select_spawned_units = false;
    s.steps = {at(0.5F,
                  Command::Attack,
                  QStringLiteral("roman_bows"),
                  QStringLiteral("skeleton_bows")),
               at(0.5F,
                  Command::Attack,
                  QStringLiteral("skeleton_bows"),
                  QStringLiteral("roman_bows")),
               at(0.5F,
                  Command::Attack,
                  QStringLiteral("grave_priest"),
                  QStringLiteral("roman_screen")),
               at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("roman_screen"),
                  QStringLiteral("skeleton_bows"))};
    add_visual_stability(s,
                         {QStringLiteral("roman_bows"),
                          QStringLiteral("roman_screen"),
                          QStringLiteral("skeleton_bows"),
                          QStringLiteral("grave_priest")});
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("skeleton_bows"), {}, 0.45F));
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("roman_bows")));
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("skeleton_bows")));
    s.expectations.push_back(expectation(Expect::ProjectileImpactSynchronized,
                                         QStringLiteral("roman_bows"),
                                         QStringLiteral("skeleton_bows")));
    s.expectations.push_back(expectation(Expect::ProjectileImpactSynchronized,
                                         QStringLiteral("skeleton_bows"),
                                         QStringLiteral("roman_bows")));
    s.expectations.push_back(expectation(Expect::ProjectileImpactSynchronized,
                                         QStringLiteral("grave_priest"),
                                         QStringLiteral("roman_screen")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_sepulcher_vs_carthage_infantry_id),
        QStringLiteral("Sepulcher vs Carthage: Infantry"),
        QStringLiteral("Equivalent-value melee test: a mixed Carthaginian sword and "
                       "spear line against four skeleton swordsmen."),
        16.0F,
        {23.0F, 48.0F, 28.0F});
    s.groups = {nation_group(QStringLiteral("punic_swords"),
                             Troop::Swordsman,
                             Nation::Carthage,
                             1,
                             2,
                             {-3.0F, 0.0F, -9.0F}),
                nation_group(QStringLiteral("punic_spears"),
                             Troop::Spearman,
                             Nation::Carthage,
                             1,
                             2,
                             {4.0F, 0.0F, -9.0F}),
                nation_group(QStringLiteral("skeleton_swords"),
                             Troop::SkeletonSwordsman,
                             Nation::IronSepulcher,
                             2,
                             4,
                             {0.0F, 0.0F, 9.0F})};
    s.steps = {at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("punic_swords"),
                  QStringLiteral("skeleton_swords")),
               at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("punic_spears"),
                  QStringLiteral("skeleton_swords")),
               at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("skeleton_swords"),
                  QStringLiteral("punic_swords"))};
    add_visual_stability(s,
                         {QStringLiteral("punic_swords"),
                          QStringLiteral("punic_spears"),
                          QStringLiteral("skeleton_swords")});
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("punic_spears"), {}, 0.45F));
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("punic_swords")));
    s.expectations.push_back(expectation(Expect::AttackAnimationObserved,
                                         QStringLiteral("skeleton_swords")));
    s.expectations.push_back(expectation(Expect::NoEligibleTroopIdleDuringCombat,
                                         QStringLiteral("skeleton_swords"),
                                         QStringLiteral("punic_swords"),
                                         1.25F,
                                         1.0F,
                                         8.0F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_sepulcher_vs_carthage_cavalry_id),
        QStringLiteral("Sepulcher vs Carthage: Cavalry"),
        QStringLiteral("Carthaginian cavalry charges a standing skeleton block to "
                       "prove impact displacement, contact damage, and melee lock "
                       "against undead formations."),
        14.0F,
        {18.0F, 44.0F, 20.0F});
    s.groups = {nation_group(QStringLiteral("punic_cavalry"),
                             Troop::MountedSwordsman,
                             Nation::Carthage,
                             1,
                             2,
                             {0.0F, 0.0F, -7.0F},
                             4),
                nation_group(QStringLiteral("skeleton_block"),
                             Troop::SkeletonSwordsman,
                             Nation::IronSepulcher,
                             2,
                             3,
                             {0.0F, 0.0F, 5.0F})};
    s.steps = {at(0.0F,
                  Command::Charge,
                  QStringLiteral("punic_cavalry"),
                  QStringLiteral("skeleton_block")),
               when_near(QStringLiteral("punic_cavalry"),
                         QStringLiteral("skeleton_block"),
                         4.5F,
                         Command::SetCamera)};
    s.steps.back().camera_distance = 15.0F;
    s.steps.back().camera_angle = 48.0F;
    s.steps.back().camera_yaw = 20.0F;
    add_visual_stability(
        s, {QStringLiteral("punic_cavalry"), QStringLiteral("skeleton_block")});
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("punic_cavalry"), {}, 0.45F));
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("punic_cavalry"),
                                         QStringLiteral("skeleton_block")));
    s.expectations.push_back(expectation(Expect::ChargeImpactPrecedesMeleeLock,
                                         QStringLiteral("punic_cavalry")));
    s.expectations.push_back(
        expectation(Expect::DeathAnimationObserved, QStringLiteral("skeleton_block")));
    s.expectations.push_back(expectation(Expect::LaunchedCasualtyObserved,
                                         QStringLiteral("skeleton_block")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_sepulcher_shrine_awakening_id),
        QStringLiteral("Sepulcher Shrine Awakening"),
        QStringLiteral("A cursed shrine stands alone on empty ground. Roman swordsmen "
                       "advance into its radius, the sepulcher wakes, and the summoned "
                       "guardians fight the intruders."),
        26.0F,
        {28.0F, 50.0F, 24.0F});
    s.suppress_spawn_anchor = true;
    s.camera_focus = QVector3D(0.0F, 0.0F, 2.0F);
    s.resource_patches = {{QStringLiteral("magic_shrine"),
                           1,
                           QVector3D(0.0F, 0.0F, 6.0F),
                           QVector3D(0.0F, 0.0F, 0.0F),
                           1.0F}};

    s.undead_zones = {undead_zone(QStringLiteral("shrine_sentinels"),
                                  Game::Map::WorldProp::Type::MagicShrine,
                                  QVector3D(0.0F, 0.0F, 6.0F),
                                  6.0F,
                                  99,
                                  {})};
    s.groups = {nation_group(QStringLiteral("intruders"),
                             Troop::Swordsman,
                             Nation::RomanRepublic,
                             1,
                             3,
                             {0.0F, 0.0F, -12.0F})};
    s.steps = {at(1.0F, Command::FormationMove, QStringLiteral("intruders"))};
    s.steps.back().destination = QVector3D(0.0F, 0.0F, 4.0F);
    add_visual_stability(s, {QStringLiteral("intruders")});
    s.expectations.push_back(
        expectation(Expect::MovementAnimationObserved, QStringLiteral("intruders")));
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("intruders")));
    s.expectations.push_back(zone_expectation(Expect::UndeadZoneDormantBefore,
                                              QStringLiteral("shrine_sentinels"),
                                              0.0F,
                                              2.0F));
    s.expectations.push_back(zone_expectation(
        Expect::UndeadZoneAwakened, QStringLiteral("shrine_sentinels"), 4.0F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_sepulcher_ruins_awakening_waves_id),
        QStringLiteral("Sepulcher Ruins Awakening Waves"),
        QStringLiteral("A Carthaginian column enters sepulcher ruins, clears the "
                       "opening guardians, and is met by the follow-up wave that the "
                       "zone releases only after the first is destroyed."),
        45.0F,
        {30.0F, 50.0F, 24.0F});
    s.suppress_spawn_anchor = true;
    s.camera_focus = QVector3D(0.0F, 0.0F, 2.0F);
    s.resource_patches = {{QStringLiteral("ruins"),
                           1,
                           QVector3D(0.0F, 0.0F, 6.0F),
                           QVector3D(0.0F, 0.0F, 0.0F),
                           1.1F}};
    s.undead_zones = {
        undead_zone(QStringLiteral("ruins_guard"),
                    Game::Map::WorldProp::Type::Ruins,
                    QVector3D(0.0F, 0.0F, 6.0F),
                    6.0F,
                    99,
                    {undead_wave(QStringLiteral("initial"),
                                 {{Game::Units::SpawnType::SkeletonSwordsman, 1}}),
                     undead_wave(QStringLiteral("after_clear"),
                                 {{Game::Units::SpawnType::SkeletonArcher, 1}})})};
    s.groups = {nation_group(QStringLiteral("punic_column"),
                             Troop::Swordsman,
                             Nation::Carthage,
                             1,
                             4,
                             {0.0F, 0.0F, -12.0F})};
    s.steps = {at(1.0F, Command::FormationMove, QStringLiteral("punic_column"))};
    s.steps.back().destination = QVector3D(0.0F, 0.0F, 4.0F);
    add_visual_stability(s, {QStringLiteral("punic_column")});
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("punic_column")));
    s.expectations.push_back(zone_expectation(
        Expect::UndeadZoneDormantBefore, QStringLiteral("ruins_guard"), 0.0F, 2.0F));
    s.expectations.push_back(zone_expectation(
        Expect::UndeadZoneAwakened, QStringLiteral("ruins_guard"), 2.0F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_sepulcher_wave_flare_review_id),
        QStringLiteral("Sepulcher Wave Flare Review"),
        QStringLiteral("Close review of the grave-light flare every guardian rises "
                       "in. A lone scout wakes the ruins, the opening wave bursts "
                       "out of the ground, and a timed second wave repeats the "
                       "burst without waiting for the first to be cleared."),
        16.0F,
        {17.0F, 38.0F, 18.0F});
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.select_spawned_units = false;
    s.camera_focus = QVector3D(0.0F, 0.0F, 4.0F);
    s.resource_patches = {patch("ruins", 1, QVector3D(0.0F, 0.0F, 5.0F), {}, 1.1F)};

    auto zone =
        undead_zone(QStringLiteral("flare_zone"),
                    Game::Map::WorldProp::Type::Ruins,
                    QVector3D(0.0F, 0.0F, 5.0F),
                    6.0F,
                    99,
                    {undead_wave(QStringLiteral("initial"),
                                 {{Game::Units::SpawnType::SkeletonSwordsman, 2},
                                  {Game::Units::SpawnType::SkeletonArcher, 1}}),
                     undead_wave(QStringLiteral("next_wave"),
                                 {{Game::Units::SpawnType::SkeletonSwordsman, 2}})});

    zone.wave_timeout_seconds = 5.0F;
    s.undead_zones = {std::move(zone)};

    s.groups = {nation_group(QStringLiteral("scout"),
                             Troop::Swordsman,
                             Nation::RomanRepublic,
                             1,
                             1,
                             {0.0F, 0.0F, -6.0F},
                             1)};
    s.steps = {at(0.5F, Command::FormationMove, QStringLiteral("scout"))};
    s.steps.back().destination = QVector3D(0.0F, 0.0F, 2.0F);
    add_visual_stability(s, {QStringLiteral("scout")});
    s.expectations.push_back(zone_expectation(
        Expect::UndeadZoneDormantBefore, QStringLiteral("flare_zone"), 0.0F, 1.0F));

    s.expectations.push_back(zone_expectation(
        Expect::UndeadZoneAwakened, QStringLiteral("flare_zone"), 5.0F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_sepulcher_shrine_siege_id),
        QStringLiteral("Sepulcher Shrine Siege"),
        QStringLiteral("A Roman assault wakes the shrine and fights for its flag. "
                       "The shrine is the sepulcher's barracks, but it cannot be "
                       "taken while a single guardian still stands, so the column "
                       "has to break the garrison before the banner comes down."),
        90.0F,
        {30.0F, 50.0F, 24.0F});
    s.suppress_spawn_anchor = true;
    s.camera_focus = QVector3D(0.0F, 0.0F, 2.0F);
    s.resource_patches = {{QStringLiteral("magic_shrine"),
                           1,
                           QVector3D(0.0F, 0.0F, 6.0F),
                           QVector3D(0.0F, 0.0F, 0.0F),
                           1.0F}};

    s.undead_zones = {
        undead_zone(QStringLiteral("shrine_sentinels"),
                    Game::Map::WorldProp::Type::MagicShrine,
                    QVector3D(0.0F, 0.0F, 6.0F),
                    6.0F,
                    99,
                    {undead_wave(QStringLiteral("initial"),
                                 {{Game::Units::SpawnType::SkeletonSwordsman, 2}})})};
    s.groups = {nation_group(QStringLiteral("assault"),
                             Troop::Swordsman,
                             Nation::RomanRepublic,
                             1,
                             6,
                             {0.0F, 0.0F, -12.0F})};
    s.steps = {at(1.0F, Command::FormationMove, QStringLiteral("assault"))};
    s.steps.back().destination = QVector3D(0.0F, 0.0F, 4.0F);
    add_visual_stability(s, {QStringLiteral("assault")});
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("assault")));
    s.expectations.push_back(zone_expectation(
        Expect::UndeadZoneAwakened, QStringLiteral("shrine_sentinels"), 2.0F));
    s.expectations.push_back(zone_expectation(Expect::UndeadZoneCleared,
                                              QStringLiteral("shrine_sentinels")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_sepulcher_zone_shrine_spawn_id),
        QStringLiteral("Sepulcher Zone Shrine Spawn"),
        QStringLiteral("Bare ground, no authored prop: the awakening zone raises "
                       "its own magic shrine at the centre. Roman scouts walk in, "
                       "the zone wakes, and the shrine stands through the fight as "
                       "the sepulcher's barracks."),
        26.0F,
        {28.0F, 50.0F, 24.0F});
    s.suppress_spawn_anchor = true;
    s.camera_focus = QVector3D(0.0F, 0.0F, 2.0F);
    s.undead_zones = {
        undead_zone(QStringLiteral("bare_barrow"),
                    Game::Map::WorldProp::Type::Ruins,
                    QVector3D(0.0F, 0.0F, 6.0F),
                    6.0F,
                    99,
                    {undead_wave(QStringLiteral("initial"),
                                 {{Game::Units::SpawnType::SkeletonSwordsman, 2}})})};
    s.groups = {nation_group(QStringLiteral("scouts"),
                             Troop::Swordsman,
                             Nation::RomanRepublic,
                             1,
                             3,
                             {0.0F, 0.0F, -12.0F})};
    s.steps = {at(1.0F, Command::FormationMove, QStringLiteral("scouts"))};
    s.steps.back().destination = QVector3D(0.0F, 0.0F, 4.0F);
    add_visual_stability(s, {QStringLiteral("scouts")});
    s.expectations.push_back(zone_expectation(
        Expect::UndeadZoneAwakened, QStringLiteral("bare_barrow"), 2.0F));
    s.expectations.push_back(zone_expectation(Expect::UndeadZoneShrineStands,
                                              QStringLiteral("bare_barrow")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_sepulcher_twin_zone_shrines_id),
        QStringLiteral("Sepulcher Twin Zone Shrines"),
        QStringLiteral("Two awakening zones share one field. Each raises its own "
                       "shrine, and a Roman column walks into each of them, so the "
                       "two garrisons and their two barracks stand side by side."),
        30.0F,
        {34.0F, 50.0F, 24.0F});
    s.suppress_spawn_anchor = true;
    s.camera_focus = QVector3D(0.0F, 0.0F, 2.0F);
    s.undead_zones = {
        undead_zone(QStringLiteral("west_barrow"),
                    Game::Map::WorldProp::Type::Ruins,
                    QVector3D(-10.0F, 0.0F, 6.0F),
                    5.0F,
                    99,
                    {undead_wave(QStringLiteral("initial"),
                                 {{Game::Units::SpawnType::SkeletonSwordsman, 1}})}),
        undead_zone(QStringLiteral("east_barrow"),
                    Game::Map::WorldProp::Type::Ruins,
                    QVector3D(10.0F, 0.0F, 6.0F),
                    5.0F,
                    99,
                    {undead_wave(QStringLiteral("initial"),
                                 {{Game::Units::SpawnType::SkeletonSwordsman, 1}})})};
    s.groups = {nation_group(QStringLiteral("west_column"),
                             Troop::Swordsman,
                             Nation::RomanRepublic,
                             1,
                             3,
                             {-10.0F, 0.0F, -12.0F}),
                nation_group(QStringLiteral("east_column"),
                             Troop::Swordsman,
                             Nation::RomanRepublic,
                             1,
                             3,
                             {10.0F, 0.0F, -12.0F})};
    s.steps = {at(1.0F, Command::FormationMove, QStringLiteral("west_column")),
               at(1.0F, Command::FormationMove, QStringLiteral("east_column"))};
    s.steps[0].destination = QVector3D(-10.0F, 0.0F, 4.0F);
    s.steps[1].destination = QVector3D(10.0F, 0.0F, 4.0F);
    add_visual_stability(
        s, {QStringLiteral("west_column"), QStringLiteral("east_column")});
    for (auto const& zone_id :
         {QStringLiteral("west_barrow"), QStringLiteral("east_barrow")}) {
      s.expectations.push_back(
          zone_expectation(Expect::UndeadZoneAwakened, zone_id, 1.0F));
      s.expectations.push_back(
          zone_expectation(Expect::UndeadZoneShrineStands, zone_id));
    }
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_sepulcher_shrine_demolition_id),
        QStringLiteral("Sepulcher Shrine Demolition"),
        QStringLiteral("The zone wakes on a Roman assault, then its shrine is "
                       "brought down. Losing the barracks crumbles every risen "
                       "guardian and leaves the ground quiet."),
        32.0F,
        {30.0F, 50.0F, 24.0F});
    s.suppress_spawn_anchor = true;
    s.camera_focus = QVector3D(0.0F, 0.0F, 2.0F);
    s.undead_zones = {
        undead_zone(QStringLiteral("doomed_shrine"),
                    Game::Map::WorldProp::Type::Ruins,
                    QVector3D(0.0F, 0.0F, 6.0F),
                    6.0F,
                    99,
                    {undead_wave(QStringLiteral("initial"),
                                 {{Game::Units::SpawnType::SkeletonSwordsman, 2}})})};
    s.groups = {nation_group(QStringLiteral("breakers"),
                             Troop::Swordsman,
                             Nation::RomanRepublic,
                             1,
                             4,
                             {0.0F, 0.0F, -12.0F})};
    s.steps = {at(1.0F, Command::FormationMove, QStringLiteral("breakers"))};
    s.steps.back().destination = QVector3D(0.0F, 0.0F, 4.0F);

    ArenaScenarioStep raze;
    raze.name = QStringLiteral("raze_shrine");
    raze.trigger = {Trigger::AtTime, 10.0F, {}, {}, 0.0F};
    raze.command = Command::ApplyDamage;
    raze.zone_id = QStringLiteral("doomed_shrine");
    raze.value = 100000;
    s.steps.push_back(std::move(raze));

    add_visual_stability(s, {QStringLiteral("breakers")});
    s.expectations.push_back(zone_expectation(
        Expect::UndeadZoneAwakened, QStringLiteral("doomed_shrine"), 2.0F));
    s.expectations.push_back(zone_expectation(Expect::UndeadZoneShrineDestroyed,
                                              QStringLiteral("doomed_shrine")));
    s.expectations.push_back(
        zone_expectation(Expect::UndeadZoneCleared, QStringLiteral("doomed_shrine")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_sepulcher_shrine_state_reload_id),
        QStringLiteral("Sepulcher Shrine State Reload"),
        QStringLiteral("A woken zone is written out and read back the way a saved "
                       "game does it. The reload must keep the shrine that already "
                       "stands and the guardians already raised, without planting a "
                       "second shrine or a fresh wave."),
        28.0F,
        {30.0F, 50.0F, 24.0F});
    s.suppress_spawn_anchor = true;
    s.camera_focus = QVector3D(0.0F, 0.0F, 2.0F);
    s.undead_zones = {
        undead_zone(QStringLiteral("saved_shrine"),
                    Game::Map::WorldProp::Type::Ruins,
                    QVector3D(0.0F, 0.0F, 6.0F),
                    6.0F,
                    99,
                    {undead_wave(QStringLiteral("initial"),
                                 {{Game::Units::SpawnType::SkeletonSwordsman, 2}})})};
    s.groups = {nation_group(QStringLiteral("visitors"),
                             Troop::Swordsman,
                             Nation::RomanRepublic,
                             1,
                             3,
                             {0.0F, 0.0F, -12.0F})};
    s.steps = {at(1.0F, Command::FormationMove, QStringLiteral("visitors"))};
    s.steps.back().destination = QVector3D(0.0F, 0.0F, 4.0F);

    ArenaScenarioStep reload;
    reload.name = QStringLiteral("reload_zone_state");
    reload.trigger = {Trigger::AtTime, 10.0F, {}, {}, 0.0F};
    reload.command = Command::ReloadUndeadZoneState;
    s.steps.push_back(std::move(reload));

    add_visual_stability(s, {QStringLiteral("visitors")});
    s.expectations.push_back(zone_expectation(
        Expect::UndeadZoneAwakened, QStringLiteral("saved_shrine"), 2.0F));
    s.expectations.push_back(zone_expectation(Expect::UndeadZoneShrineStands,
                                              QStringLiteral("saved_shrine")));
    result.push_back(std::move(s));
  }

  return result;
}

} // namespace Arena::Scenarios
