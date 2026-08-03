#include "arena_formation_scenarios.h"

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

auto troop_group(QString name,
                 Troop troop,
                 Nation nation,
                 int owner,
                 int count,
                 QVector3D origin,
                 int individuals = 0,
                 QVector3D spacing = {3.0F, 0.0F, 0.0F}) -> ArenaScenarioGroup {
  ArenaScenarioGroup result;
  result.name = std::move(name);
  result.troop_type = troop;
  result.nation_id = nation;
  result.owner_id = owner;
  result.count = count;
  result.individuals_per_unit = individuals;
  result.origin = origin;
  result.spacing = spacing;
  result.facing_degrees = owner == 1 ? 0.0F : 180.0F;
  return result;
}

auto step_at(float time,
             Command command,
             QString source,
             QVector3D destination = {}) -> ArenaScenarioStep {
  ArenaScenarioStep result;
  result.name = QStringLiteral("%1_%2").arg(QString::number(time, 'f', 2), source);
  result.trigger = {Trigger::AtTime, time, {}, {}, 0.0F};
  result.command = command;
  result.group = std::move(source);
  result.destination = destination;
  return result;
}

auto expect(Expect kind,
            QString source = {},
            QString target = {},
            float threshold = 0.0F,
            float start = 0.0F) -> ArenaExpectation {
  ArenaExpectation result;
  result.kind = kind;
  result.group = std::move(source);
  result.target_group = std::move(target);
  result.threshold = threshold;
  result.start_seconds = start;
  return result;
}

auto formation_definition(QString id,
                          QString label,
                          QString description,
                          float duration,
                          ArenaCameraView camera) -> ArenaScenarioDefinition {
  ArenaScenarioDefinition result;
  result.id = std::move(id);
  result.label = std::move(label);
  result.description = std::move(description);
  result.duration_seconds = duration;
  result.camera = camera;
  result.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
  result.suppress_terrain_scatter = true;
  result.suppress_spawn_anchor = true;
  result.suppress_ui_overlays = true;
  result.select_spawned_units = false;
  result.force_full_creature_lod = true;
  return result;
}

void add_layout_expectations(ArenaScenarioDefinition& scenario,
                             std::initializer_list<QString> groups) {
  for (auto const& name : groups) {
    scenario.expectations.push_back(expect(Expect::GroupIsRendered, name));
    scenario.expectations.push_back(expect(Expect::NoPoseOscillation, name));
    scenario.expectations.push_back(expect(Expect::NoRootTeleport, name));
    scenario.expectations.push_back(expect(Expect::NoUnexpectedFallPose, name));
  }
  scenario.expectations.push_back(expect(Expect::FrameBudget, {}, {}, 33.34F, 0.25F));
}

auto overhead_camera(float distance) -> ArenaCameraView {
  return {distance, 68.0F, 0.0F};
}

auto three_quarter_camera(float distance) -> ArenaCameraView {
  return {distance, 48.0F, 24.0F};
}

void add_unit_layout_scenarios(std::vector<ArenaScenarioDefinition>& out) {
  {
    auto s = formation_definition(
        QStringLiteral("unit_layout_role_lineup"),
        QStringLiteral("Unit Layout: Role Lineup"),
        QStringLiteral("Every troop role at rest, side by side, so role-specific "
                       "silhouettes can be compared in one frame."),
        6.0F,
        overhead_camera(64.0F));
    s.groups = {
        troop_group(QStringLiteral("swords"),
                    Troop::Swordsman,
                    Nation::RomanRepublic,
                    1,
                    1,
                    {-24.0F, 0.0F, 0.0F},
                    16),
        troop_group(QStringLiteral("spears"),
                    Troop::Spearman,
                    Nation::RomanRepublic,
                    1,
                    1,
                    {-14.0F, 0.0F, 0.0F},
                    18),
        troop_group(QStringLiteral("archers"),
                    Troop::Archer,
                    Nation::RomanRepublic,
                    1,
                    1,
                    {-4.0F, 0.0F, 0.0F},
                    16),
        troop_group(QStringLiteral("cavalry"),
                    Troop::MountedKnight,
                    Nation::RomanRepublic,
                    1,
                    1,
                    {8.0F, 0.0F, 0.0F},
                    9),
        troop_group(QStringLiteral("siege"),
                    Troop::Catapult,
                    Nation::RomanRepublic,
                    1,
                    1,
                    {20.0F, 0.0F, 0.0F},
                    1),
        troop_group(QStringLiteral("builders"),
                    Troop::Builder,
                    Nation::RomanRepublic,
                    1,
                    1,
                    {28.0F, 0.0F, 0.0F},
                    6),
    };
    add_layout_expectations(s,
                            {QStringLiteral("swords"),
                             QStringLiteral("spears"),
                             QStringLiteral("archers"),
                             QStringLiteral("cavalry"),
                             QStringLiteral("siege"),
                             QStringLiteral("builders")});
    out.push_back(std::move(s));
  }

  {
    auto s = formation_definition(
        QStringLiteral("unit_layout_faction_comparison"),
        QStringLiteral("Unit Layout: Rome vs Carthage vs Sepulcher"),
        QStringLiteral("The same troop role in all three doctrines, rendered "
                       "overhead. Spacing, stagger and rank rhythm must read as "
                       "three different armies without relying on colour."),
        6.0F,
        overhead_camera(52.0F));
    s.groups = {
        troop_group(QStringLiteral("rome"),
                    Troop::Swordsman,
                    Nation::RomanRepublic,
                    1,
                    1,
                    {-18.0F, 0.0F, 0.0F},
                    24),
        troop_group(QStringLiteral("carthage"),
                    Troop::Swordsman,
                    Nation::Carthage,
                    2,
                    1,
                    {0.0F, 0.0F, 0.0F},
                    24),
        troop_group(QStringLiteral("sepulcher"),
                    Troop::SkeletonSwordsman,
                    Nation::IronSepulcher,
                    3,
                    1,
                    {18.0F, 0.0F, 0.0F},
                    24),
    };
    add_layout_expectations(s,
                            {QStringLiteral("rome"),
                             QStringLiteral("carthage"),
                             QStringLiteral("sepulcher")});
    out.push_back(std::move(s));
  }

  {
    auto s = formation_definition(
        QStringLiteral("unit_layout_size_range"),
        QStringLiteral("Unit Layout: Small, Medium and Large Units"),
        QStringLiteral("Three, twelve and forty soldiers in one troop unit. All "
                       "three counts must produce a readable block with no "
                       "overlapping bodies."),
        6.0F,
        overhead_camera(56.0F));
    s.groups = {
        troop_group(QStringLiteral("small"),
                    Troop::Swordsman,
                    Nation::RomanRepublic,
                    1,
                    1,
                    {-20.0F, 0.0F, 0.0F},
                    3),
        troop_group(QStringLiteral("medium"),
                    Troop::Swordsman,
                    Nation::RomanRepublic,
                    1,
                    1,
                    {-2.0F, 0.0F, 0.0F},
                    12),
        troop_group(QStringLiteral("large"),
                    Troop::Swordsman,
                    Nation::RomanRepublic,
                    1,
                    1,
                    {18.0F, 0.0F, 0.0F},
                    40),
    };
    add_layout_expectations(
        s,
        {QStringLiteral("small"), QStringLiteral("medium"), QStringLiteral("large")});
    out.push_back(std::move(s));
  }

  {
    auto s = formation_definition(
        QStringLiteral("unit_layout_move_and_rotate"),
        QStringLiteral("Unit Layout: Movement and Rotation"),
        QStringLiteral("A block marches out and turns ninety degrees. Soldiers "
                       "must not swap slots or cross through each other while "
                       "the unit rotates."),
        10.0F,
        three_quarter_camera(46.0F));
    s.groups = {troop_group(QStringLiteral("block"),
                            Troop::Swordsman,
                            Nation::RomanRepublic,
                            1,
                            1,
                            {0.0F, 0.0F, -14.0F},
                            20)};
    s.steps = {
        step_at(0.5F, Command::Move, QStringLiteral("block"), {0.0F, 0.0F, 6.0F}),
        step_at(5.0F, Command::Move, QStringLiteral("block"), {14.0F, 0.0F, 6.0F}),
    };
    s.expectations = {
        expect(Expect::MovementIsContinuous, QStringLiteral("block")),
        expect(Expect::FormationOrderPreserved, QStringLiteral("block")),
        expect(Expect::GroupReachedDestination, QStringLiteral("block")),
    };
    add_layout_expectations(s, {QStringLiteral("block")});
    out.push_back(std::move(s));
  }

  {
    auto s = formation_definition(
        QStringLiteral("unit_layout_casualty_reflow"),
        QStringLiteral("Unit Layout: Casualty Reflow"),
        QStringLiteral("Sustained losses thin a block out. The survivors must "
                       "close up smoothly instead of snapping to a new grid."),
        12.0F,
        three_quarter_camera(38.0F));
    s.groups = {
        troop_group(QStringLiteral("defenders"),
                    Troop::Swordsman,
                    Nation::RomanRepublic,
                    1,
                    1,
                    {-4.0F, 0.0F, 0.0F},
                    24),
        troop_group(QStringLiteral("attackers"),
                    Troop::Archer,
                    Nation::Carthage,
                    2,
                    2,
                    {12.0F, 0.0F, 0.0F},
                    12),
    };
    s.steps = {step_at(1.0F, Command::Attack, QStringLiteral("attackers"))};
    s.steps.front().target_group = QStringLiteral("defenders");
    s.expectations = {
        expect(Expect::GroupHealthReduced, QStringLiteral("defenders")),
        expect(Expect::FormationOrderPreserved, QStringLiteral("defenders")),
        expect(Expect::NoRootTeleport, QStringLiteral("defenders")),
    };
    add_layout_expectations(s,
                            {QStringLiteral("defenders"), QStringLiteral("attackers")});
    out.push_back(std::move(s));
  }

  {
    auto s = formation_definition(
        QStringLiteral("unit_layout_slope_adaptation"),
        QStringLiteral("Unit Layout: Slopes and Uneven Ground"),
        QStringLiteral("A block stands and then advances across a rise. Soldier "
                       "feet must follow the terrain without the block losing "
                       "its rank structure."),
        10.0F,
        three_quarter_camera(42.0F));
    s.suppress_terrain_scatter = false;
    ArenaScenarioElevationPatch rise;
    rise.center = QVector3D(0.0F, 0.0F, 4.0F);
    rise.radius = 16.0F;
    rise.height = 4.5F;
    s.elevation_patches = {rise};
    s.groups = {troop_group(QStringLiteral("climbers"),
                            Troop::Spearman,
                            Nation::RomanRepublic,
                            1,
                            1,
                            {0.0F, 0.0F, -14.0F},
                            20)};
    s.steps = {
        step_at(1.0F, Command::Move, QStringLiteral("climbers"), {0.0F, 0.0F, 10.0F})};
    s.expectations = {
        expect(Expect::MovementIsContinuous, QStringLiteral("climbers")),
        expect(Expect::ElevationGainObserved, QStringLiteral("climbers")),
        expect(Expect::NoUnexpectedFallPose, QStringLiteral("climbers")),
    };
    add_layout_expectations(s, {QStringLiteral("climbers")});
    out.push_back(std::move(s));
  }

  {
    auto s = formation_definition(
        QStringLiteral("unit_layout_defensive_transition"),
        QStringLiteral("Unit Layout: Entering and Leaving Defensive Layout"),
        QStringLiteral("A shielded block enters its defensive layout, holds, then "
                       "breaks out of it. The forming and breaking phases must "
                       "both be visible and must end in a valid layout."),
        12.0F,
        three_quarter_camera(34.0F));
    s.groups = {
        troop_group(QStringLiteral("shields"),
                    Troop::Swordsman,
                    Nation::RomanRepublic,
                    1,
                    1,
                    {0.0F, 0.0F, 0.0F},
                    20),
        troop_group(QStringLiteral("volley"),
                    Troop::Archer,
                    Nation::Carthage,
                    2,
                    2,
                    {16.0F, 0.0F, 0.0F},
                    12),
    };
    s.steps = {
        step_at(1.0F, Command::Hold, QStringLiteral("shields")),
        step_at(7.0F, Command::Stop, QStringLiteral("shields")),
        step_at(8.0F, Command::Move, QStringLiteral("shields"), {0.0F, 0.0F, 8.0F}),
    };
    s.expectations = {
        expect(Expect::HoldPoseMaintained, QStringLiteral("shields")),
        expect(Expect::MovementIsContinuous, QStringLiteral("shields"), {}, 0.0F, 8.5F),
    };
    add_layout_expectations(s, {QStringLiteral("shields"), QStringLiteral("volley")});
    out.push_back(std::move(s));
  }

  {
    auto s = formation_definition(
        QStringLiteral("unit_layout_disruption_recovery"),
        QStringLiteral("Unit Layout: Disruption and Recovery"),
        QStringLiteral("A cavalry charge disrupts an infantry block, which then "
                       "reforms. The interrupted transition must return to a "
                       "valid layout rather than leaving soldiers stranded."),
        14.0F,
        three_quarter_camera(40.0F));
    s.groups = {
        troop_group(QStringLiteral("infantry"),
                    Troop::Swordsman,
                    Nation::RomanRepublic,
                    1,
                    1,
                    {0.0F, 0.0F, 0.0F},
                    24),
        troop_group(QStringLiteral("chargers"),
                    Troop::MountedKnight,
                    Nation::Carthage,
                    2,
                    2,
                    {0.0F, 0.0F, 22.0F},
                    6),
    };
    s.steps = {
        step_at(1.0F, Command::Charge, QStringLiteral("chargers")),
        step_at(9.0F, Command::Move, QStringLiteral("infantry"), {-12.0F, 0.0F, 0.0F}),
    };
    s.steps.front().target_group = QStringLiteral("infantry");
    s.expectations = {
        expect(Expect::ChargeImpactPrecedesMeleeLock, QStringLiteral("chargers")),
        expect(
            Expect::MovementIsContinuous, QStringLiteral("infantry"), {}, 0.0F, 9.5F),
    };
    add_layout_expectations(s,
                            {QStringLiteral("infantry"), QStringLiteral("chargers")});
    out.push_back(std::move(s));
  }

  {
    auto s = formation_definition(
        QStringLiteral("unit_layout_large_army_cost"),
        QStringLiteral("Unit Layout: Large Army Performance"),
        QStringLiteral("Sixteen full-strength blocks at full creature detail. "
                       "Confirms the layout pass stays inside the frame budget "
                       "at battle scale."),
        8.0F,
        overhead_camera(78.0F));
    s.graphics_quality = Render::GraphicsQuality::Ultra;
    s.require_rigged_instancing = true;
    s.collect_animation_diagnostics = false;
    s.groups = {
        troop_group(QStringLiteral("rome_line"),
                    Troop::Swordsman,
                    Nation::RomanRepublic,
                    1,
                    8,
                    {-24.0F, 0.0F, -12.0F},
                    20,
                    {6.5F, 0.0F, 0.0F}),
        troop_group(QStringLiteral("carthage_line"),
                    Troop::Swordsman,
                    Nation::Carthage,
                    2,
                    8,
                    {-24.0F, 0.0F, 12.0F},
                    20,
                    {6.5F, 0.0F, 0.0F}),
    };
    s.expectations = {
        expect(Expect::FrameBudget, {}, {}, 10.0F, 0.25F),
        expect(Expect::GroupIsRendered, QStringLiteral("rome_line")),
        expect(Expect::GroupIsRendered, QStringLiteral("carthage_line")),
    };
    out.push_back(std::move(s));
  }
}

void add_army_formation_scenarios(std::vector<ArenaScenarioDefinition>& out) {
  auto mixed_force = [](Nation nation, int owner, float base_z) {
    return std::vector<ArenaScenarioGroup>{
        troop_group(QStringLiteral("swords"),
                    Troop::Swordsman,
                    nation,
                    owner,
                    4,
                    {-12.0F, 0.0F, base_z},
                    16,
                    {5.0F, 0.0F, 0.0F}),
        troop_group(QStringLiteral("spears"),
                    Troop::Spearman,
                    nation,
                    owner,
                    3,
                    {-8.0F, 0.0F, base_z - 6.0F},
                    18,
                    {5.0F, 0.0F, 0.0F}),
        troop_group(QStringLiteral("archers"),
                    Troop::Archer,
                    nation,
                    owner,
                    3,
                    {-8.0F, 0.0F, base_z - 12.0F},
                    14,
                    {5.0F, 0.0F, 0.0F}),
        troop_group(QStringLiteral("cavalry"),
                    Troop::MountedKnight,
                    nation,
                    owner,
                    4,
                    {14.0F, 0.0F, base_z - 6.0F},
                    8,
                    {5.0F, 0.0F, 0.0F}),
    };
  };

  auto deployment_scenario = [&](const char* id,
                                 const char* label,
                                 const char* description,
                                 Nation nation,
                                 float camera_distance) {
    auto s = formation_definition(QString::fromLatin1(id),
                                  QString::fromLatin1(label),
                                  QString::fromLatin1(description),
                                  10.0F,
                                  overhead_camera(camera_distance));
    s.groups = mixed_force(nation, 1, -18.0F);
    s.steps = {
        step_at(
            1.0F, Command::FormationMove, QStringLiteral("swords"), {0.0F, 0.0F, 6.0F}),
        step_at(
            1.0F, Command::FormationMove, QStringLiteral("spears"), {0.0F, 0.0F, 6.0F}),
        step_at(1.0F,
                Command::FormationMove,
                QStringLiteral("archers"),
                {0.0F, 0.0F, 6.0F}),
        step_at(1.0F,
                Command::FormationMove,
                QStringLiteral("cavalry"),
                {0.0F, 0.0F, 6.0F}),
    };
    s.expectations = {
        expect(Expect::AllGroupsRespondWithin, {}, {}, 2.0F),
        expect(Expect::FormationOrderPreserved, QStringLiteral("swords")),
        expect(Expect::GroupReachedDestination, QStringLiteral("swords")),
        expect(Expect::GroupReachedDestination, QStringLiteral("archers")),
    };
    add_layout_expectations(s,
                            {QStringLiteral("swords"),
                             QStringLiteral("spears"),
                             QStringLiteral("archers"),
                             QStringLiteral("cavalry")});
    return s;
  };

  out.push_back(deployment_scenario(
      "army_formation_rome_default",
      "Army Formation: Roman Default Battle Line",
      "The Roman faction-default deployment for a mixed force. Heavy centre, "
      "spears screening, ranged behind, balanced cavalry wings.",
      Nation::RomanRepublic,
      66.0F));

  out.push_back(deployment_scenario(
      "army_formation_carthage_default",
      "Army Formation: Carthaginian Default Deployment",
      "The same mixed force under the Carthaginian doctrine. Wider centre, "
      "echelon, and an asymmetric cavalry weighting.",
      Nation::Carthage,
      66.0F));

  {
    auto s = formation_definition(
        QStringLiteral("army_formation_rome_vs_carthage"),
        QStringLiteral("Army Formation: Rome vs Carthage Comparison"),
        QStringLiteral("Identical force compositions deployed side by side under "
                       "the two doctrines. The two blocks must read as visibly "
                       "different deployments from overhead."),
        10.0F,
        overhead_camera(82.0F));
    s.groups = {
        troop_group(QStringLiteral("rome_swords"),
                    Troop::Swordsman,
                    Nation::RomanRepublic,
                    1,
                    4,
                    {-34.0F, 0.0F, -8.0F},
                    16,
                    {5.0F, 0.0F, 0.0F}),
        troop_group(QStringLiteral("rome_archers"),
                    Troop::Archer,
                    Nation::RomanRepublic,
                    1,
                    2,
                    {-32.0F, 0.0F, -16.0F},
                    14,
                    {5.0F, 0.0F, 0.0F}),
        troop_group(QStringLiteral("rome_cavalry"),
                    Troop::MountedKnight,
                    Nation::RomanRepublic,
                    1,
                    2,
                    {-20.0F, 0.0F, -12.0F},
                    8,
                    {5.0F, 0.0F, 0.0F}),
        troop_group(QStringLiteral("carthage_swords"),
                    Troop::Swordsman,
                    Nation::Carthage,
                    2,
                    4,
                    {14.0F, 0.0F, -8.0F},
                    16,
                    {5.0F, 0.0F, 0.0F}),
        troop_group(QStringLiteral("carthage_archers"),
                    Troop::Archer,
                    Nation::Carthage,
                    2,
                    2,
                    {16.0F, 0.0F, -16.0F},
                    14,
                    {5.0F, 0.0F, 0.0F}),
        troop_group(QStringLiteral("carthage_cavalry"),
                    Troop::MountedKnight,
                    Nation::Carthage,
                    2,
                    2,
                    {28.0F, 0.0F, -12.0F},
                    8,
                    {5.0F, 0.0F, 0.0F}),
    };
    s.steps = {
        step_at(1.0F,
                Command::FormationMove,
                QStringLiteral("rome_swords"),
                {-20.0F, 0.0F, 8.0F}),
        step_at(1.0F,
                Command::FormationMove,
                QStringLiteral("rome_archers"),
                {-20.0F, 0.0F, 8.0F}),
        step_at(1.0F,
                Command::FormationMove,
                QStringLiteral("rome_cavalry"),
                {-20.0F, 0.0F, 8.0F}),
        step_at(1.0F,
                Command::FormationMove,
                QStringLiteral("carthage_swords"),
                {20.0F, 0.0F, 8.0F}),
        step_at(1.0F,
                Command::FormationMove,
                QStringLiteral("carthage_archers"),
                {20.0F, 0.0F, 8.0F}),
        step_at(1.0F,
                Command::FormationMove,
                QStringLiteral("carthage_cavalry"),
                {20.0F, 0.0F, 8.0F}),
    };
    add_layout_expectations(s,
                            {QStringLiteral("rome_swords"),
                             QStringLiteral("rome_archers"),
                             QStringLiteral("rome_cavalry"),
                             QStringLiteral("carthage_swords"),
                             QStringLiteral("carthage_archers"),
                             QStringLiteral("carthage_cavalry")});
    out.push_back(std::move(s));
  }

  {
    auto s = formation_definition(
        QStringLiteral("army_formation_sepulcher_shrine_defence"),
        QStringLiteral("Army Formation: Iron Sepulcher Shrine Defence"),
        QStringLiteral("Awakened troops close around a shrine position. Expendable "
                       "ranks hold the front while the grave priests stay behind "
                       "the mass."),
        10.0F,
        overhead_camera(58.0F));
    s.groups = {
        troop_group(QStringLiteral("guard"),
                    Troop::SkeletonSwordsman,
                    Nation::IronSepulcher,
                    3,
                    5,
                    {-10.0F, 0.0F, -14.0F},
                    18,
                    {5.0F, 0.0F, 0.0F}),
        troop_group(QStringLiteral("bows"),
                    Troop::SkeletonArcher,
                    Nation::IronSepulcher,
                    3,
                    3,
                    {-6.0F, 0.0F, -20.0F},
                    14,
                    {5.0F, 0.0F, 0.0F}),
        troop_group(QStringLiteral("priests"),
                    Troop::GravePriest,
                    Nation::IronSepulcher,
                    3,
                    2,
                    {0.0F, 0.0F, -26.0F},
                    1,
                    {5.0F, 0.0F, 0.0F}),
    };
    s.steps = {
        step_at(
            1.0F, Command::FormationMove, QStringLiteral("guard"), {0.0F, 0.0F, 0.0F}),
        step_at(
            1.0F, Command::FormationMove, QStringLiteral("bows"), {0.0F, 0.0F, 0.0F}),
        step_at(1.0F,
                Command::FormationMove,
                QStringLiteral("priests"),
                {0.0F, 0.0F, 0.0F}),
    };
    add_layout_expectations(
        s,
        {QStringLiteral("guard"), QStringLiteral("bows"), QStringLiteral("priests")});
    out.push_back(std::move(s));
  }

  {
    auto s = formation_definition(
        QStringLiteral("army_formation_mixed_contingents"),
        QStringLiteral("Army Formation: Mixed-Faction Doctrine Policy"),
        QStringLiteral("A force with both Roman and Carthaginian contingents "
                       "deploys under one anchor. The doctrine policy, not the "
                       "units' current positions, must decide the ordering."),
        10.0F,
        overhead_camera(64.0F));
    s.groups = {
        troop_group(QStringLiteral("roman_line"),
                    Troop::Swordsman,
                    Nation::RomanRepublic,
                    1,
                    3,
                    {-16.0F, 0.0F, -16.0F},
                    16,
                    {5.0F, 0.0F, 0.0F}),
        troop_group(QStringLiteral("punic_line"),
                    Troop::Spearman,
                    Nation::Carthage,
                    1,
                    3,
                    {4.0F, 0.0F, -16.0F},
                    18,
                    {5.0F, 0.0F, 0.0F}),
        troop_group(QStringLiteral("shared_bows"),
                    Troop::Archer,
                    Nation::RomanRepublic,
                    1,
                    2,
                    {-6.0F, 0.0F, -24.0F},
                    14,
                    {5.0F, 0.0F, 0.0F}),
    };
    s.steps = {
        step_at(1.0F,
                Command::FormationMove,
                QStringLiteral("roman_line"),
                {0.0F, 0.0F, 4.0F}),
        step_at(1.0F,
                Command::FormationMove,
                QStringLiteral("punic_line"),
                {0.0F, 0.0F, 4.0F}),
        step_at(1.0F,
                Command::FormationMove,
                QStringLiteral("shared_bows"),
                {0.0F, 0.0F, 4.0F}),
    };
    add_layout_expectations(s,
                            {QStringLiteral("roman_line"),
                             QStringLiteral("punic_line"),
                             QStringLiteral("shared_bows")});
    out.push_back(std::move(s));
  }

  {
    auto s = formation_definition(
        QStringLiteral("army_formation_narrow_gate"),
        QStringLiteral("Army Formation: Narrow Gate Passage"),
        QStringLiteral("A wide deployment is ordered through a walled gate. The "
                       "formation must convert to a column, pass, and reform "
                       "without units stacking on a single fallback point."),
        16.0F,
        three_quarter_camera(58.0F));
    s.suppress_terrain_scatter = false;
    s.groups = {
        troop_group(QStringLiteral("column"),
                    Troop::Swordsman,
                    Nation::RomanRepublic,
                    1,
                    6,
                    {-6.0F, 0.0F, -22.0F},
                    14,
                    {4.0F, 0.0F, 0.0F}),
    };
    s.groups.push_back([] {
      ArenaScenarioGroup wall;
      wall.name = QStringLiteral("west_wall");
      wall.spawn_type = Game::Units::SpawnType::WallSegment;
      wall.nation_id = Nation::RomanRepublic;
      wall.owner_id = 1;
      wall.count = 5;
      wall.origin = QVector3D(-8.0F, 0.0F, -4.0F);
      wall.spacing = QVector3D(2.0F, 0.0F, 0.0F);
      return wall;
    }());
    s.groups.push_back([] {
      ArenaScenarioGroup wall;
      wall.name = QStringLiteral("east_wall");
      wall.spawn_type = Game::Units::SpawnType::WallSegment;
      wall.nation_id = Nation::RomanRepublic;
      wall.owner_id = 1;
      wall.count = 5;
      wall.origin = QVector3D(8.0F, 0.0F, -4.0F);
      wall.spacing = QVector3D(2.0F, 0.0F, 0.0F);
      return wall;
    }());
    s.steps = {step_at(
        1.0F, Command::FormationMove, QStringLiteral("column"), {0.0F, 0.0F, 16.0F})};
    s.expectations = {
        expect(Expect::GroupReachedDestination, QStringLiteral("column")),
        expect(Expect::MovementIsContinuous, QStringLiteral("column")),
        expect(Expect::NoRootTeleport, QStringLiteral("column")),
    };
    add_layout_expectations(s, {QStringLiteral("column")});
    out.push_back(std::move(s));
  }

  {
    auto s = formation_definition(
        QStringLiteral("army_formation_around_buildings"),
        QStringLiteral("Army Formation: Placement Around Buildings"),
        QStringLiteral("A deployment anchor lands on occupied ground. Slots must "
                       "be nudged around the obstruction independently, never "
                       "collapsed onto one shared fallback."),
        12.0F,
        overhead_camera(52.0F));
    s.suppress_terrain_scatter = false;
    s.groups = {
        troop_group(QStringLiteral("deployers"),
                    Troop::Swordsman,
                    Nation::RomanRepublic,
                    1,
                    6,
                    {-4.0F, 0.0F, -22.0F},
                    12,
                    {4.5F, 0.0F, 0.0F}),
    };
    s.groups.push_back([] {
      ArenaScenarioGroup barracks;
      barracks.name = QStringLiteral("obstruction");
      barracks.spawn_type = Game::Units::SpawnType::Barracks;
      barracks.nation_id = Nation::RomanRepublic;
      barracks.owner_id = 1;
      barracks.count = 2;
      barracks.origin = QVector3D(-4.0F, 0.0F, 4.0F);
      barracks.spacing = QVector3D(10.0F, 0.0F, 0.0F);
      return barracks;
    }());
    s.steps = {step_at(
        1.0F, Command::FormationMove, QStringLiteral("deployers"), {0.0F, 0.0F, 4.0F})};
    s.expectations = {
        expect(Expect::MovementIsContinuous, QStringLiteral("deployers")),
        expect(Expect::NoRootTeleport, QStringLiteral("deployers")),
    };
    add_layout_expectations(s, {QStringLiteral("deployers")});
    out.push_back(std::move(s));
  }

  {
    auto s = formation_definition(
        QStringLiteral("army_formation_member_losses"),
        QStringLiteral("Army Formation: Member Death and Reform"),
        QStringLiteral("A formed group loses members mid-deployment. The group "
                       "record must drop them and replan rather than leaving "
                       "holes in the line."),
        14.0F,
        three_quarter_camera(46.0F));
    s.groups = {
        troop_group(QStringLiteral("line"),
                    Troop::Swordsman,
                    Nation::RomanRepublic,
                    1,
                    5,
                    {-10.0F, 0.0F, -6.0F},
                    12,
                    {5.0F, 0.0F, 0.0F}),
        troop_group(QStringLiteral("bombard"),
                    Troop::Catapult,
                    Nation::Carthage,
                    2,
                    2,
                    {0.0F, 0.0F, 26.0F},
                    1,
                    {6.0F, 0.0F, 0.0F}),
    };
    s.steps = {
        step_at(
            1.0F, Command::FormationMove, QStringLiteral("line"), {0.0F, 0.0F, 2.0F}),
        step_at(2.0F, Command::Attack, QStringLiteral("bombard")),
        step_at(
            10.0F, Command::FormationMove, QStringLiteral("line"), {0.0F, 0.0F, -8.0F}),
    };
    s.steps[1].target_group = QStringLiteral("line");
    s.expectations = {
        expect(Expect::GroupHealthReduced, QStringLiteral("line")),
        expect(Expect::MovementIsContinuous, QStringLiteral("line"), {}, 0.0F, 10.5F),
    };
    add_layout_expectations(s, {QStringLiteral("line"), QStringLiteral("bombard")});
    out.push_back(std::move(s));
  }

  {
    auto s = formation_definition(
        QStringLiteral("army_formation_maintain_advance"),
        QStringLiteral("Army Formation: Maintain Formation Advance"),
        QStringLiteral("A short battlefield advance under the maintain-formation "
                       "policy. The deployment must keep its shape while it "
                       "moves, at a visibly reduced speed."),
        14.0F,
        three_quarter_camera(52.0F));
    s.groups = {
        troop_group(QStringLiteral("advance_line"),
                    Troop::Swordsman,
                    Nation::RomanRepublic,
                    1,
                    5,
                    {-10.0F, 0.0F, -18.0F},
                    14,
                    {5.0F, 0.0F, 0.0F}),
        troop_group(QStringLiteral("advance_bows"),
                    Troop::Archer,
                    Nation::RomanRepublic,
                    1,
                    2,
                    {-4.0F, 0.0F, -24.0F},
                    12,
                    {5.0F, 0.0F, 0.0F}),
    };
    s.steps = {
        step_at(1.0F,
                Command::FormationMove,
                QStringLiteral("advance_line"),
                {0.0F, 0.0F, 8.0F}),
        step_at(1.0F,
                Command::FormationMove,
                QStringLiteral("advance_bows"),
                {0.0F, 0.0F, 8.0F}),
    };
    s.expectations = {
        expect(Expect::MovementIsContinuous, QStringLiteral("advance_line")),
        expect(Expect::FormationOrderPreserved, QStringLiteral("advance_line")),
        expect(Expect::GroupReachedDestination, QStringLiteral("advance_line")),
    };
    add_layout_expectations(
        s, {QStringLiteral("advance_line"), QStringLiteral("advance_bows")});
    out.push_back(std::move(s));
  }

  {
    auto s = formation_definition(
        QStringLiteral("army_formation_siege_escort"),
        QStringLiteral("Army Formation: Siege Escort"),
        QStringLiteral("Engines advance behind their escort. Siege units must "
                       "never end up in the front rank of the deployment."),
        12.0F,
        overhead_camera(60.0F));
    s.groups = {
        troop_group(QStringLiteral("escort"),
                    Troop::Swordsman,
                    Nation::RomanRepublic,
                    1,
                    4,
                    {-10.0F, 0.0F, -20.0F},
                    14,
                    {5.0F, 0.0F, 0.0F}),
        troop_group(QStringLiteral("engines"),
                    Troop::Catapult,
                    Nation::RomanRepublic,
                    1,
                    2,
                    {0.0F, 0.0F, -28.0F},
                    1,
                    {7.0F, 0.0F, 0.0F}),
        troop_group(QStringLiteral("screen"),
                    Troop::Spearman,
                    Nation::RomanRepublic,
                    1,
                    2,
                    {-4.0F, 0.0F, -14.0F},
                    16,
                    {5.0F, 0.0F, 0.0F}),
    };
    s.steps = {
        step_at(
            1.0F, Command::FormationMove, QStringLiteral("escort"), {0.0F, 0.0F, 2.0F}),
        step_at(1.0F,
                Command::FormationMove,
                QStringLiteral("engines"),
                {0.0F, 0.0F, 2.0F}),
        step_at(
            1.0F, Command::FormationMove, QStringLiteral("screen"), {0.0F, 0.0F, 2.0F}),
    };
    add_layout_expectations(s,
                            {QStringLiteral("escort"),
                             QStringLiteral("engines"),
                             QStringLiteral("screen")});
    out.push_back(std::move(s));
  }
}

void add_terrain_formation_scenarios(std::vector<ArenaScenarioDefinition>& out) {
  {
    auto s = formation_definition(
        QStringLiteral("army_formation_bridge_crossing"),
        QStringLiteral("Army Formation: Bridge Crossing"),
        QStringLiteral("A deployed line is ordered across a river with a single "
                       "bridge. It must funnel onto the deck, cross without a "
                       "slot landing in the water, and reform on the far bank."),
        18.0F,
        three_quarter_camera(56.0F));
    s.suppress_terrain_scatter = true;
    s.rivers.push_back(
        Game::Map::RiverSegment{{-30.0F, 0.0F, 0.0F}, {30.0F, 0.0F, 0.0F}, 6.0F});
    s.bridges.push_back(
        Game::Map::Bridge{{0.0F, 0.0F, -6.0F}, {0.0F, 0.0F, 6.0F}, 8.0F, 0.5F});
    s.groups = {
        troop_group(QStringLiteral("vanguard"),
                    Troop::Swordsman,
                    Nation::RomanRepublic,
                    1,
                    4,
                    {-6.0F, 0.0F, -18.0F},
                    12,
                    {4.5F, 0.0F, 0.0F}),
        troop_group(QStringLiteral("support"),
                    Troop::Archer,
                    Nation::RomanRepublic,
                    1,
                    2,
                    {-2.0F, 0.0F, -24.0F},
                    10,
                    {4.5F, 0.0F, 0.0F}),
    };
    s.steps = {
        step_at(1.0F,
                Command::FormationMove,
                QStringLiteral("vanguard"),
                {0.0F, 0.0F, 16.0F}),
        step_at(2.0F,
                Command::FormationMove,
                QStringLiteral("support"),
                {0.0F, 0.0F, 14.0F}),
    };
    s.expectations = {
        expect(Expect::BridgeTraversalObserved, QStringLiteral("vanguard")),
        expect(Expect::BridgeCenterlineAligned, QStringLiteral("vanguard")),
        expect(Expect::GroupReachedDestination, QStringLiteral("vanguard")),
        expect(Expect::MovementIsContinuous, QStringLiteral("vanguard")),
    };
    add_layout_expectations(s, {QStringLiteral("vanguard"), QStringLiteral("support")});
    out.push_back(std::move(s));
  }

  {
    auto s = formation_definition(
        QStringLiteral("army_formation_hill_deployment"),
        QStringLiteral("Army Formation: Deploying Onto A Hill"),
        QStringLiteral("A line deploys onto rising ground. Slots must follow the "
                       "slope and reach the crown through the entrance rather "
                       "than piling against an impassable flank."),
        16.0F,
        three_quarter_camera(50.0F));
    s.suppress_terrain_scatter = true;
    ArenaScenarioElevationPatch crown;
    crown.center = QVector3D(0.0F, 0.0F, 6.0F);
    crown.radius = 14.0F;
    crown.height = 6.0F;
    s.elevation_patches = {crown};
    s.groups = {
        troop_group(QStringLiteral("climbers"),
                    Troop::Spearman,
                    Nation::RomanRepublic,
                    1,
                    4,
                    {-6.0F, 0.0F, -18.0F},
                    14,
                    {5.0F, 0.0F, 0.0F}),
    };
    s.steps = {step_at(
        1.0F, Command::FormationMove, QStringLiteral("climbers"), {0.0F, 0.0F, 6.0F})};
    s.expectations = {
        expect(Expect::ElevationGainObserved, QStringLiteral("climbers")),
        expect(Expect::MovementIsContinuous, QStringLiteral("climbers")),
        expect(Expect::GroupReachedDestination, QStringLiteral("climbers")),
        expect(Expect::NoUnexpectedFallPose, QStringLiteral("climbers")),
    };
    add_layout_expectations(s, {QStringLiteral("climbers")});
    out.push_back(std::move(s));
  }

  {
    auto s = formation_definition(
        QStringLiteral("army_formation_obstacle_course"),
        QStringLiteral("Army Formation: Mixed Obstacle Course"),
        QStringLiteral("River, bridge, walls, buildings and rising ground in one "
                       "fixture. Every slot must end on walkable ground with no "
                       "two units sharing a tile."),
        20.0F,
        three_quarter_camera(70.0F));
    s.suppress_terrain_scatter = true;
    s.rivers.push_back(
        Game::Map::RiverSegment{{-34.0F, 0.0F, 4.0F}, {34.0F, 0.0F, 4.0F}, 5.0F});
    s.bridges.push_back(
        Game::Map::Bridge{{-6.0F, 0.0F, -2.0F}, {-6.0F, 0.0F, 10.0F}, 8.0F, 0.5F});
    ArenaScenarioElevationPatch rise;
    rise.center = QVector3D(16.0F, 0.0F, 16.0F);
    rise.radius = 10.0F;
    rise.height = 4.0F;
    s.elevation_patches = {rise};

    s.groups = {
        troop_group(QStringLiteral("column"),
                    Troop::Swordsman,
                    Nation::RomanRepublic,
                    1,
                    5,
                    {-8.0F, 0.0F, -20.0F},
                    12,
                    {4.5F, 0.0F, 0.0F}),
        troop_group(QStringLiteral("bows"),
                    Troop::Archer,
                    Nation::RomanRepublic,
                    1,
                    2,
                    {-2.0F, 0.0F, -26.0F},
                    10,
                    {4.5F, 0.0F, 0.0F}),
    };
    s.groups.push_back([] {
      ArenaScenarioGroup wall;
      wall.name = QStringLiteral("wall_line");
      wall.spawn_type = Game::Units::SpawnType::WallSegment;
      wall.nation_id = Nation::RomanRepublic;
      wall.owner_id = 1;
      wall.count = 5;
      wall.origin = QVector3D(4.0F, 0.0F, 12.0F);
      wall.spacing = QVector3D(2.0F, 0.0F, 0.0F);
      return wall;
    }());
    s.groups.push_back([] {
      ArenaScenarioGroup barracks;
      barracks.name = QStringLiteral("depot");
      barracks.spawn_type = Game::Units::SpawnType::Barracks;
      barracks.nation_id = Nation::RomanRepublic;
      barracks.owner_id = 1;
      barracks.count = 1;
      barracks.origin = QVector3D(-16.0F, 0.0F, 14.0F);
      return barracks;
    }());

    s.steps = {
        step_at(1.0F,
                Command::FormationMove,
                QStringLiteral("column"),
                {-6.0F, 0.0F, 16.0F}),
        step_at(
            2.0F, Command::FormationMove, QStringLiteral("bows"), {-6.0F, 0.0F, 12.0F}),
    };
    s.expectations = {
        expect(Expect::BridgeTraversalObserved, QStringLiteral("column")),
        expect(Expect::MovementIsContinuous, QStringLiteral("column")),
        expect(Expect::GroupReachedDestination, QStringLiteral("column")),
        expect(Expect::NoRootTeleport, QStringLiteral("column")),
    };
    add_layout_expectations(s, {QStringLiteral("column"), QStringLiteral("bows")});
    out.push_back(std::move(s));
  }
}

using Intent = Game::Formation::ArmyFormationIntent;

auto form_step(float time,
               QStringList groups,
               Intent intent,
               QVector3D anchor,
               float facing_degrees,
               float frontage = 0.0F) -> ArenaScenarioStep {
  ArenaScenarioStep result;
  result.name =
      QStringLiteral("%1_%2").arg(QString::number(time, 'f', 2), groups.value(0));
  result.trigger = {Trigger::AtTime, time, {}, {}, 0.0F};
  result.command = Command::FormArmy;
  result.group = groups.value(0);
  result.formation.groups = std::move(groups);
  result.formation.intent = intent;
  result.formation.anchor = anchor;
  result.formation.facing_degrees = facing_degrees;
  result.formation.frontage = frontage;
  return result;
}

void dress_for_capture(ArenaScenarioDefinition& scenario, float hour) {
  scenario.suppress_terrain_scatter = false;
  scenario.select_spawned_units = false;
  scenario.suppress_spawn_anchor = true;
  scenario.suppress_ui_overlays = true;
  scenario.force_full_creature_lod = true;
  scenario.collect_animation_diagnostics = false;
  scenario.graphics_quality = Render::GraphicsQuality::Ultra;
  scenario.arena_floor_half_extent = 40.0F;
  scenario.environment.start_time = hour;

  scenario.environment.time_mode = Game::Map::TimeMode::Locked;
}

void add_formation_promo_scenarios(std::vector<ArenaScenarioDefinition>& out) {
  {
    auto s = formation_definition(
        QStringLiteral("promo_rome_iron_line"),
        QStringLiteral("Promo: Rome, The Iron Line"),
        QStringLiteral("A full Roman army marches up in column, deploys into the "
                       "doctrine battle line, closes into a shield wall, and then "
                       "advances holding the shape."),
        46.0F,
        three_quarter_camera(72.0F));
    dress_for_capture(s, 10.5F);
    s.groups = {
        troop_group(QStringLiteral("first_line"),
                    Troop::Swordsman,
                    Nation::RomanRepublic,
                    1,
                    9,
                    {-24.0F, 0.0F, -26.0F},
                    18,
                    {6.0F, 0.0F, 0.0F}),
        troop_group(QStringLiteral("second_line"),
                    Troop::Swordsman,
                    Nation::RomanRepublic,
                    1,
                    9,
                    {-24.0F, 0.0F, -31.0F},
                    18,
                    {6.0F, 0.0F, 0.0F}),
        troop_group(QStringLiteral("spear_screen"),
                    Troop::Spearman,
                    Nation::RomanRepublic,
                    1,
                    7,
                    {-18.0F, 0.0F, -36.0F},
                    18,
                    {6.0F, 0.0F, 0.0F}),
        troop_group(QStringLiteral("velites"),
                    Troop::Archer,
                    Nation::RomanRepublic,
                    1,
                    5,
                    {-12.0F, 0.0F, -30.0F},
                    12,
                    {6.0F, 0.0F, 0.0F}),
        troop_group(QStringLiteral("equites"),
                    Troop::MountedKnight,
                    Nation::RomanRepublic,
                    1,
                    5,
                    {12.0F, 0.0F, -30.0F},
                    8,
                    {6.0F, 0.0F, 0.0F}),
        troop_group(QStringLiteral("consul"),
                    Troop::RomanVeteranConsul,
                    Nation::RomanRepublic,
                    1,
                    1,
                    {0.0F, 0.0F, -36.0F},
                    1),
    };
    const QStringList legion{QStringLiteral("first_line"),
                             QStringLiteral("second_line"),
                             QStringLiteral("spear_screen"),
                             QStringLiteral("velites"),
                             QStringLiteral("equites"),
                             QStringLiteral("consul")};
    s.steps = {
        form_step(1.0F, legion, Intent::Column, {0.0F, 0.0F, -24.0F}, 0.0F, 22.0F),
        form_step(14.0F, legion, Intent::Line, {0.0F, 0.0F, -6.0F}, 0.0F, 58.0F),
        form_step(27.0F, legion, Intent::Defensive, {0.0F, 0.0F, -4.0F}, 0.0F, 48.0F),
    };
    auto advance =
        form_step(36.0F, legion, Intent::Assault, {0.0F, 0.0F, 18.0F}, 0.0F, 54.0F);
    advance.formation.options.movement_policy =
        Game::Formation::MovementPolicy::MaintainFormation;
    s.steps.push_back(std::move(advance));
    s.expectations = {
        expect(Expect::GroupIsRendered, QStringLiteral("first_line")),
        expect(Expect::GroupIsRendered, QStringLiteral("equites")),
        expect(Expect::MovementIsContinuous, QStringLiteral("second_line")),
        expect(Expect::NoRootTeleport, QStringLiteral("spear_screen")),
        expect(Expect::NoUnexpectedFallPose, QStringLiteral("first_line")),
    };
    out.push_back(std::move(s));
  }

  {
    auto s = formation_definition(
        QStringLiteral("promo_carthage_crescent"),
        QStringLiteral("Promo: Carthage, The Crescent"),
        QStringLiteral("A Carthaginian host forms its wide bow, throws the "
                       "Numidian wings out to either flank, and then closes the "
                       "horns into an encirclement."),
        56.0F,
        three_quarter_camera(78.0F));
    dress_for_capture(s, 10.2F);
    s.groups = {
        troop_group(QStringLiteral("libyan_spears"),
                    Troop::Spearman,
                    Nation::Carthage,
                    1,
                    9,
                    {-24.0F, 0.0F, -24.0F},
                    18,
                    {6.0F, 0.0F, 0.0F}),
        troop_group(QStringLiteral("iberian_swords"),
                    Troop::Swordsman,
                    Nation::Carthage,
                    1,
                    7,
                    {-18.0F, 0.0F, -30.0F},
                    16,
                    {6.0F, 0.0F, 0.0F}),
        troop_group(QStringLiteral("balearic_slingers"),
                    Troop::Archer,
                    Nation::Carthage,
                    1,
                    6,
                    {-15.0F, 0.0F, -36.0F},
                    12,
                    {6.0F, 0.0F, 0.0F}),
        troop_group(QStringLiteral("numidian_left"),
                    Troop::HorseArcher,
                    Nation::Carthage,
                    1,
                    8,
                    {-32.0F, 0.0F, -30.0F},
                    8,
                    {6.0F, 0.0F, 0.0F}),
        troop_group(QStringLiteral("numidian_right"),
                    Troop::HorseSpearman,
                    Nation::Carthage,
                    1,
                    8,
                    {16.0F, 0.0F, -30.0F},
                    8,
                    {6.0F, 0.0F, 0.0F}),
        troop_group(QStringLiteral("war_elephants"),
                    Troop::Elephant,
                    Nation::Carthage,
                    1,
                    6,
                    {-15.0F, 0.0F, -18.0F},
                    1,
                    {6.0F, 0.0F, 0.0F}),
        troop_group(QStringLiteral("elephant_master"),
                    Troop::CarthageSwordCommander,
                    Nation::Carthage,
                    1,
                    1,
                    {0.0F, 0.0F, -38.0F},
                    1),
    };
    const QStringList host{QStringLiteral("libyan_spears"),
                           QStringLiteral("iberian_swords"),
                           QStringLiteral("balearic_slingers"),
                           QStringLiteral("war_elephants"),
                           QStringLiteral("elephant_master")};
    const QStringList wings{QStringLiteral("numidian_left"),
                            QStringLiteral("numidian_right")};

    auto bow = form_step(1.0F, host, Intent::Line, {0.0F, 0.0F, -12.0F}, 0.0F, 62.0F);
    bow.formation.options.ranged_placement = Game::Formation::RangedPlacement::Skirmish;
    s.steps.push_back(std::move(bow));

    auto left_wing = form_step(4.0F,
                               {QStringLiteral("numidian_left")},
                               Intent::Assault,
                               {-33.0F, 0.0F, -2.0F},
                               35.0F,
                               18.0F);
    left_wing.formation.options.flank_preference =
        Game::Formation::FlankPreference::StrongLeft;
    s.steps.push_back(std::move(left_wing));

    auto right_wing = form_step(4.0F,
                                {QStringLiteral("numidian_right")},
                                Intent::Assault,
                                {33.0F, 0.0F, -2.0F},
                                -35.0F,
                                18.0F);
    right_wing.formation.options.flank_preference =
        Game::Formation::FlankPreference::StrongRight;
    s.steps.push_back(std::move(right_wing));

    auto centre_holds =
        form_step(18.0F, host, Intent::Line, {0.0F, 0.0F, 2.0F}, 0.0F, 66.0F);
    centre_holds.formation.options.frontage_scale = 1.2F;
    s.steps.push_back(std::move(centre_holds));

    s.steps.push_back(form_step(
        26.0F, host + wings, Intent::Encirclement, {0.0F, 0.0F, 11.0F}, 0.0F, 58.0F));

    s.expectations = {
        expect(Expect::GroupIsRendered, QStringLiteral("libyan_spears")),
        expect(Expect::GroupIsRendered, QStringLiteral("war_elephants")),
        expect(Expect::GroupIsRendered, QStringLiteral("numidian_left")),
        expect(Expect::MovementIsContinuous, QStringLiteral("iberian_swords")),
        expect(Expect::NoRootTeleport, QStringLiteral("libyan_spears")),
    };
    out.push_back(std::move(s));
  }

  {
    auto s = formation_definition(
        QStringLiteral("promo_rome_hill_drill"),
        QStringLiteral("Promo: Rome, Drill On The Ridge"),
        QStringLiteral("The same legion cycles column, line, defensive and "
                       "assault across a ridge, so the doctrine shapes and the "
                       "terrain fitting read from overhead."),
        52.0F,
        overhead_camera(92.0F));
    dress_for_capture(s, 13.0F);
    ArenaScenarioElevationPatch ridge;
    ridge.center = QVector3D(0.0F, 0.0F, 4.0F);
    ridge.radius = 30.0F;
    ridge.height = 6.0F;
    s.elevation_patches = {ridge};
    s.groups = {
        troop_group(QStringLiteral("cohorts"),
                    Troop::Swordsman,
                    Nation::RomanRepublic,
                    1,
                    10,
                    {-27.0F, 0.0F, -26.0F},
                    18,
                    {6.0F, 0.0F, 0.0F}),
        troop_group(QStringLiteral("pikes"),
                    Troop::Spearman,
                    Nation::RomanRepublic,
                    1,
                    7,
                    {-18.0F, 0.0F, -32.0F},
                    18,
                    {6.0F, 0.0F, 0.0F}),
        troop_group(QStringLiteral("bows"),
                    Troop::Archer,
                    Nation::RomanRepublic,
                    1,
                    6,
                    {-15.0F, 0.0F, -38.0F},
                    12,
                    {6.0F, 0.0F, 0.0F}),
        troop_group(QStringLiteral("wings"),
                    Troop::MountedKnight,
                    Nation::RomanRepublic,
                    1,
                    6,
                    {12.0F, 0.0F, -34.0F},
                    8,
                    {6.0F, 0.0F, 0.0F}),
    };
    const QStringList legion{QStringLiteral("cohorts"),
                             QStringLiteral("pikes"),
                             QStringLiteral("bows"),
                             QStringLiteral("wings")};
    s.steps = {
        form_step(1.0F, legion, Intent::Column, {0.0F, 0.0F, -26.0F}, 0.0F, 16.0F),
        form_step(13.0F, legion, Intent::Line, {0.0F, 0.0F, 2.0F}, 0.0F, 44.0F),
        form_step(25.0F, legion, Intent::Defensive, {0.0F, 0.0F, 4.0F}, 0.0F, 32.0F),
        form_step(35.0F, legion, Intent::Assault, {0.0F, 0.0F, 6.0F}, 0.0F, 40.0F),
        form_step(45.0F, legion, Intent::Column, {0.0F, 0.0F, 26.0F}, 0.0F, 16.0F),
    };
    s.expectations = {
        expect(Expect::GroupIsRendered, QStringLiteral("cohorts")),
        expect(Expect::MovementIsContinuous, QStringLiteral("cohorts")),
        expect(Expect::NoUnexpectedFallPose, QStringLiteral("pikes")),
        expect(Expect::NoRootTeleport, QStringLiteral("bows")),
    };
    out.push_back(std::move(s));
  }
}

} // namespace

auto build_formation_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;
  result.reserve(28);
  add_unit_layout_scenarios(result);
  add_army_formation_scenarios(result);
  add_terrain_formation_scenarios(result);
  add_formation_promo_scenarios(result);
  return result;
}

} // namespace Arena::Scenarios
