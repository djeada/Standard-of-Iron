#include "arena_cinematic_scenarios.h"

#include <utility>

#include "arena_city_scenarios.h"
#include "arena_scenarios.h"

namespace Arena::Scenarios {
namespace {

using Command = ScenarioCommandKind;
using Intent = Game::Formation::ArmyFormationIntent;
using Nation = Game::Systems::NationID;
using Trigger = ScenarioTriggerKind;
using Troop = Game::Units::TroopType;

constexpr float k_rome_facing = 90.0F;
constexpr float k_carthage_facing = 270.0F;

auto definition(const char* id, QString label, QString description, float duration)
    -> ArenaScenarioDefinition {
  ArenaScenarioDefinition result;
  result.id = QString::fromLatin1(id);
  result.label = std::move(label);
  result.description = std::move(description);
  result.duration_seconds = duration;
  result.camera = {90.0F, 30.0F, 90.0F};
  result.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
  result.select_spawned_units = false;
  result.suppress_spawn_anchor = true;
  result.suppress_ui_overlays = true;
  result.force_full_creature_lod = true;
  result.collect_animation_diagnostics = false;
  result.graphics_quality = Render::GraphicsQuality::Ultra;
  result.environment.time_mode = Game::Map::TimeMode::Locked;
  result.wildlife = {};
  return result;
}

auto file(const QString& name,
          Troop troop,
          int owner,
          int count,
          QVector3D origin,
          int individuals,
          float step) -> ArenaScenarioGroup {
  ArenaScenarioGroup result;
  result.name = name;
  result.troop_type = troop;
  result.nation_id = owner == 1 ? Nation::RomanRepublic : Nation::Carthage;
  result.owner_id = owner;
  result.count = count;
  result.individuals_per_unit = individuals;
  result.origin = origin;
  result.spacing = {0.0F, 0.0F, step};
  result.facing_degrees = owner == 1 ? k_rome_facing : k_carthage_facing;
  return result;
}

auto sturdy(ArenaScenarioGroup group, int health) -> ArenaScenarioGroup {
  group.health_override = group.max_health_override = health;
  return group;
}

auto at(float time, Command command, QString source, QString target = {})
    -> ArenaScenarioStep {
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

auto form(float time,
          QStringList groups,
          Intent intent,
          QVector3D anchor,
          float facing,
          float frontage,
          bool keep_order = true) -> ArenaScenarioStep {
  ArenaScenarioStep result;
  result.name = QStringLiteral("%1_%2").arg(QString::number(time, 'f', 2), groups.value(0));
  result.trigger = {Trigger::AtTime, time, {}, {}, 0.0F};
  result.command = Command::FormArmy;
  result.group = groups.value(0);
  result.formation.groups = std::move(groups);
  result.formation.intent = intent;
  result.formation.anchor = anchor;
  result.formation.facing_degrees = facing;
  result.formation.frontage = frontage;
  if (keep_order) {
    result.formation.options.movement_policy =
        Game::Formation::MovementPolicy::MaintainFormation;
  }
  return result;
}

auto prop(const char* type,
          int count,
          QVector3D origin,
          QVector3D spacing,
          float scale,
          float jitter,
          float scale_spread = 0.25F) -> ArenaScenarioResourcePatch {
  ArenaScenarioResourcePatch result;
  result.prop_type = QString::fromLatin1(type);
  result.count = count;
  result.origin = origin;
  result.spacing = spacing;
  result.scale = scale;
  result.jitter = jitter;
  result.yaw_spread = 360.0F;
  result.scale_spread = scale_spread;
  return result;
}

auto exists(const char* group) -> ArenaExpectation {
  ArenaExpectation result;
  result.kind = ArenaExpectationKind::GroupExists;
  result.group = QString::fromLatin1(group);
  return result;
}

auto undead_wave(QString trigger, std::vector<Game::Map::UndeadWaveUnitSpawn> units)
    -> Game::Map::UndeadWave {
  Game::Map::UndeadWave wave;
  wave.trigger = std::move(trigger);
  wave.units = std::move(units);
  return wave;
}

// The Field: a pitched battle on dry Apulian grass. Two complete armies stand
// a long bowshot apart until 22 s, so the opening can be photographed in
// stillness; then Carthage comes on, the legion answers in assault order, the
// flanks ride and the elephants go in. Hannibal seeks out Scipio at the centre.
auto cine_field() -> ArenaScenarioDefinition {
  auto s = definition(k_cine_field_id,
                      QStringLiteral("Cinematic: The Field"),
                      QStringLiteral("Trailer film set. Rome and Carthage drawn up on a "
                                     "dry plain; they stand until 22 s, then close, "
                                     "with cavalry on the wings, elephants and the two "
                                     "commanders meeting at the centre."),
                      90.0F);
  s.arena_floor_half_extent = 150.0F;
  s.suppress_boundary_mountains = true;
  s.ground_type = QStringLiteral("grass_dry");
  s.terrain_seed_override = 71218;
  s.environment.start_time = 17.6F;
  s.environment.lighting_profile = QStringLiteral("mediterranean_summer");
  s.environment.fog_density_override = 0.004F;
  s.force_full_creature_lod = true;
  s.rpg_mode = true;
  s.rpg_commander_group = QStringLiteral("scipio");
  s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);

  s.elevation_patches = {
      {.center = {-78.0F, 0.0F, 0.0F}, .radius = 60.0F, .height = 3.5F, .plateau = 30.0F},
      {.center = {92.0F, 0.0F, -10.0F}, .radius = 55.0F, .height = 5.0F, .plateau = 20.0F},
  };

  constexpr float k_rome_front = -46.0F;
  constexpr float k_carthage_front = 46.0F;
  constexpr int k_units = 26;
  constexpr float k_step = 4.0F;

  auto add_line = [&](const char* prefix, Troop troop, int owner, float x, int men,
                      int health, float z_offset = 0.0F) {
    const QString name = QString::fromLatin1(prefix);
    s.groups.push_back(sturdy(
        file(name, troop, owner, k_units, {x, 0.0F, z_offset}, men, k_step), health));
  };

  add_line("rome_swords", Troop::Swordsman, 1, k_rome_front, 16, 2600);
  add_line("rome_spears", Troop::Spearman, 1, k_rome_front - 7.0F, 16, 2600);
  auto rome_bows =
      file(QStringLiteral("rome_bows"), Troop::Archer, 1, 18, {k_rome_front - 15.0F, 0.0F, 0.0F}, 14, 5.2F);
  s.groups.push_back(rome_bows);
  s.groups.push_back(sturdy(file(QStringLiteral("rome_horse_n"), Troop::MountedSwordsman, 1, 6,
                                 {k_rome_front - 4.0F, 0.0F, -68.0F}, 8, 4.4F),
                            2400));
  s.groups.push_back(sturdy(file(QStringLiteral("rome_horse_s"), Troop::MountedSwordsman, 1, 6,
                                 {k_rome_front - 4.0F, 0.0F, 68.0F}, 8, 4.4F),
                            2400));
  auto scipio =
      sturdy(file(QStringLiteral("scipio"), Troop::RomanVeteranConsul, 1, 1,
                  {k_rome_front - 3.5F, 0.0F, 2.0F}, 1, 0.0F),
             60000);
  scipio.stamina_override = scipio.max_stamina_override = 900.0F;
  s.groups.push_back(scipio);

  add_line("punic_swords", Troop::Swordsman, 2, k_carthage_front, 16, 2200);
  add_line("punic_spears", Troop::Spearman, 2, k_carthage_front + 7.0F, 16, 2200);
  auto punic_bows = file(QStringLiteral("punic_bows"), Troop::Archer, 2, 16,
                         {k_carthage_front + 15.0F, 0.0F, 0.0F}, 14, 5.6F);
  s.groups.push_back(punic_bows);
  s.groups.push_back(sturdy(file(QStringLiteral("punic_elephants"), Troop::Elephant, 2, 6,
                                 {k_carthage_front + 24.0F, 0.0F, 0.0F}, 1, 11.0F),
                            5200));
  s.groups.push_back(sturdy(file(QStringLiteral("numidians_n"), Troop::MountedSwordsman, 2, 8,
                                 {k_carthage_front + 4.0F, 0.0F, -74.0F}, 8, 4.4F),
                            2000));
  s.groups.push_back(sturdy(file(QStringLiteral("numidians_s"), Troop::MountedSwordsman, 2, 8,
                                 {k_carthage_front + 4.0F, 0.0F, 74.0F}, 8, 4.4F),
                            2000));
  s.groups.push_back(sturdy(file(QStringLiteral("punic_guard"), Troop::Swordsman, 2, 4,
                                 {k_carthage_front + 32.0F, 0.0F, 0.0F}, 16, 4.0F),
                            3600));
  s.groups.push_back(sturdy(file(QStringLiteral("hannibal"), Troop::CarthageSwordCommander, 2, 1,
                                 {k_carthage_front + 30.0F, 0.0F, -2.0F}, 1, 0.0F),
                            60000));

  s.resource_patches = {
      prop("olive_tree", 7, {-60.0F, 0.0F, 96.0F}, {9.0F, 0.0F, 2.0F}, 1.2F, 4.0F),
      prop("olive_tree", 6, {-20.0F, 0.0F, -104.0F}, {10.0F, 0.0F, -2.0F}, 1.1F, 4.0F),
      prop("olive_tree", 5, {60.0F, 0.0F, 104.0F}, {11.0F, 0.0F, 1.0F}, 1.15F, 5.0F),
      prop("cypress_tree", 4, {-98.0F, 0.0F, 30.0F}, {3.0F, 0.0F, 6.0F}, 1.3F, 1.5F),
      prop("cypress_tree", 3, {118.0F, 0.0F, -40.0F}, {4.0F, 0.0F, 5.0F}, 1.3F, 1.5F),
      prop("boulder", 5, {-8.0F, 0.0F, 92.0F}, {7.0F, 0.0F, 3.0F}, 1.1F, 3.0F, 0.5F),
      prop("boulder", 4, {20.0F, 0.0F, -96.0F}, {6.0F, 0.0F, -3.0F}, 1.0F, 3.0F, 0.5F),
      prop("ruins", 1, {-4.0F, 0.0F, 118.0F}, {}, 1.6F, 0.0F, 0.1F),
  };

  const QStringList rome_foot = {QStringLiteral("rome_swords"), QStringLiteral("rome_spears")};
  const QStringList punic_foot = {QStringLiteral("punic_swords"),
                                  QStringLiteral("punic_spears")};

  s.steps = {
      form(14.0F, punic_foot, Intent::Line, {12.0F, 0.0F, 0.0F}, k_carthage_facing, 104.0F),
      move_to(14.4F, QStringLiteral("punic_elephants"), {26.0F, 0.0F, 0.0F}),
      move_to(14.8F, QStringLiteral("punic_guard"), {32.0F, 0.0F, 0.0F}),
      move_to(15.0F, QStringLiteral("hannibal"), {30.0F, 0.0F, -2.0F}),
      move_to(16.0F, QStringLiteral("numidians_n"), {-6.0F, 0.0F, -96.0F}),
      move_to(16.0F, QStringLiteral("numidians_s"), {-6.0F, 0.0F, 96.0F}),

      form(18.0F, rome_foot, Intent::Assault, {-10.0F, 0.0F, 0.0F}, k_rome_facing, 96.0F),
      move_to(18.5F, QStringLiteral("rome_bows"), {-40.0F, 0.0F, 0.0F}),

      at(24.0F, Command::Charge, QStringLiteral("numidians_n"), QStringLiteral("rome_bows")),
      at(24.0F, Command::Charge, QStringLiteral("numidians_s"), QStringLiteral("rome_bows")),
      at(26.0F, Command::Attack, QStringLiteral("rome_bows"), QStringLiteral("punic_swords")),
      at(26.5F, Command::Attack, QStringLiteral("punic_bows"), QStringLiteral("rome_swords")),
      at(26.0F, Command::Charge, QStringLiteral("rome_horse_n"), QStringLiteral("numidians_n")),
      at(26.0F, Command::Charge, QStringLiteral("rome_horse_s"), QStringLiteral("numidians_s")),

      at(27.0F, Command::AttackMove, QStringLiteral("punic_swords"), QStringLiteral("rome_swords")),
      at(27.0F, Command::AttackMove, QStringLiteral("punic_spears"), QStringLiteral("rome_spears")),
      at(29.0F, Command::AttackMove, QStringLiteral("punic_elephants"),
         QStringLiteral("rome_swords")),
      at(31.0F, Command::AttackMove, QStringLiteral("rome_swords"), QStringLiteral("punic_swords")),
      at(31.0F, Command::AttackMove, QStringLiteral("rome_spears"), QStringLiteral("punic_spears")),
      at(36.0F, Command::AttackMove, QStringLiteral("punic_guard"), QStringLiteral("scipio")),
      at(36.0F, Command::AttackMove, QStringLiteral("hannibal"), QStringLiteral("scipio")),
  };

  const auto rpg_move = [](float time, QVector3D axes) {
    auto step = at(time, Command::RpgMove, QStringLiteral("scipio"));
    step.destination = axes;
    step.rpg_view_yaw_degrees = 90.0F;
    return step;
  };
  const auto hold_attack = [](float time, bool held) {
    auto step = at(time, Command::RpgAttackHold, QStringLiteral("scipio"));
    step.enabled = held;
    return step;
  };
  s.steps.push_back(rpg_move(0.2F, {0.0F, 0.0F, 0.0F}));
  s.steps.push_back(rpg_move(19.0F, {0.0F, 0.0F, 0.45F}));
  s.steps.push_back(rpg_move(31.0F, {0.0F, 0.0F, 1.0F}));
  s.steps.push_back(rpg_move(39.0F, {0.0F, 0.0F, 0.0F}));
  s.steps.push_back(hold_attack(39.1F, true));
  s.steps.push_back(hold_attack(88.0F, false));
  s.expectations = {exists("scipio"), exists("hannibal"), exists("rome_swords")};
  return s;
}

// The Siege: Hannibal's host before the east gate of Aurelia Magna at the end
// of the day. Carthaginian engines fire on the towers and the quarter behind
// the wall (flaming stones on structures), the elephants and infantry come on
// and the legion holds the gate. Rome fields no elephants.
auto cine_siege() -> ArenaScenarioDefinition {
  auto s = definition(k_cine_siege_id,
                      QStringLiteral("Cinematic: The Siege"),
                      QStringLiteral("Trailer film set. Carthage bombards the east wall "
                                     "of Aurelia Magna with flaming stones and storms "
                                     "the gate; the legion holds it."),
                      60.0F);
  constexpr float k_gate_z = -32.0F;
  dress_aurelia_magna(s, QRectF(176.0, -150.0, 184.0, 220.0));
  s.wildlife = {};
  s.camera_focus = QVector3D(228.0F, 0.0F, k_gate_z);
  s.environment.time_mode = Game::Map::TimeMode::Locked;
  s.environment.start_time = 18.0F;
  s.environment.fog_density_override = 0.0035F;
  s.force_full_creature_lod = false;

  auto add = [&](const char* name, Troop troop, int owner, int count, float x, float z,
                 int men, float step, int health) {
    s.groups.push_back(sturdy(
        file(QString::fromLatin1(name), troop, owner, count, {x, 0.0F, z}, men, step), health));
  };
  add("wall_swords", Troop::Swordsman, 1, 18, 214.0F, k_gate_z, 16, 3.6F, 3000);
  add("wall_spears", Troop::Spearman, 1, 18, 208.0F, k_gate_z, 16, 3.6F, 3000);
  add("wall_bows", Troop::Archer, 1, 12, 200.0F, k_gate_z, 12, 5.0F, 1400);
  add("rome_ballistas", Troop::Ballista, 1, 6, 193.0F, k_gate_z, 1, 10.0F, 1500);

  add("siege_swords", Troop::Swordsman, 2, 22, 262.0F, k_gate_z, 16, 3.6F, 2400);
  add("siege_spears", Troop::Spearman, 2, 22, 268.0F, k_gate_z, 16, 3.6F, 2400);
  add("siege_bows", Troop::Archer, 2, 12, 276.0F, k_gate_z, 14, 5.0F, 1400);
  add("siege_elephants", Troop::Elephant, 2, 5, 282.0F, k_gate_z, 1, 12.0F, 6000);
  add("siege_catapults_n", Troop::Catapult, 2, 4, 274.0F, k_gate_z - 26.0F, 1, 12.0F, 2000);
  add("siege_catapults_s", Troop::Catapult, 2, 4, 274.0F, k_gate_z + 26.0F, 1, 12.0F, 2000);
  add("siege_ballistas", Troop::Ballista, 2, 6, 290.0F, k_gate_z, 1, 14.0F, 2000);
  add("siege_hannibal", Troop::CarthageSwordCommander, 2, 1, 284.0F, k_gate_z + 6.0F, 1, 0.0F,
      12000);

  for (auto& group : s.groups) {
    if (group.troop_type == Troop::Catapult && group.owner_id == 2) {
      group.attack_range_override = 125.0F;
    }
    if (group.troop_type == Troop::Ballista) {
      group.attack_range_override = 60.0F;
    }
    if (group.troop_type == Troop::Archer) {
      group.attack_range_override = 48.0F;
    }
  }

  s.steps = {
      at(0.3F, Command::Hold, QStringLiteral("wall_swords")),
      at(0.3F, Command::Hold, QStringLiteral("wall_spears")),
      at(1.0F, Command::Attack, QStringLiteral("siege_catapults_n"),
         QStringLiteral("capital_tower_e1")),
      at(1.2F, Command::Attack, QStringLiteral("siege_catapults_s"),
         QStringLiteral("capital_tower_e2")),
      at(1.0F, Command::Attack, QStringLiteral("rome_ballistas"), QStringLiteral("siege_swords")),
      at(2.0F, Command::Attack, QStringLiteral("siege_ballistas"), QStringLiteral("wall_bows")),
      at(10.0F, Command::Attack, QStringLiteral("wall_bows"), QStringLiteral("siege_swords")),
      at(12.0F, Command::AttackMove, QStringLiteral("siege_swords"), QStringLiteral("wall_swords")),
      at(12.5F, Command::AttackMove, QStringLiteral("siege_spears"), QStringLiteral("wall_spears")),
      at(13.0F, Command::Attack, QStringLiteral("siege_bows"), QStringLiteral("wall_bows")),
      at(16.0F, Command::AttackMove, QStringLiteral("siege_elephants"),
         QStringLiteral("wall_swords")),
      at(20.0F, Command::AttackMove, QStringLiteral("wall_swords"), QStringLiteral("siege_swords")),
      at(20.0F, Command::AttackMove, QStringLiteral("wall_spears"), QStringLiteral("siege_spears")),
  };
  s.expectations = {exists("wall_swords"), exists("siege_catapults_n"), exists("capital_gate_road")};
  return s;
}

// The Sepulcher: a snowbound barrow at night. A legion column comes up the
// slope by the light of its fires; the barrow's dead rise to meet it, grave
// priests throw fire, and the consul fights among them.
auto cine_sepulcher() -> ArenaScenarioDefinition {
  auto s = definition(k_cine_sepulcher_id,
                      QStringLiteral("Cinematic: The Sepulcher"),
                      QStringLiteral("Trailer film set. Snow, night and mist over an "
                                     "Iron Sepulcher barrow; a legion column climbs to "
                                     "it and the dead rise."),
                      50.0F);
  s.ground_type = QStringLiteral("alpine_mix");
  s.terrain_snowbound = true;
  s.terrain_seed_override = 9931;
  s.arena_floor_half_extent = 70.0F;
  s.environment.start_time = 22.4F;
  s.environment.lighting_profile = QStringLiteral("iron_sepulcher");
  s.environment.fog_density_override = 0.018F;
  s.weather.snow = 0.6F;
  s.precipitation.enabled = true;
  s.precipitation.type = Game::Map::WeatherType::Snow;
  s.precipitation.intensity = 0.6F;
  s.precipitation.wind_strength = 0.5F;
  s.precipitation.wind_direction_deg = 40.0F;
  s.owner_teams = {{.owner_id = 1, .team_id = 1}, {.owner_id = 99, .team_id = 99}};
  s.rpg_mode = true;
  s.rpg_commander_group = QStringLiteral("consul");
  s.elevation_patches = {
      {.center = {0.0F, 0.0F, -22.0F}, .radius = 26.0F, .height = 2.5F, .plateau = 10.0F},
  };

  constexpr float k_barrow_z = -18.0F;
  struct Barrow {
    const char* id;
    float x;
    float z;
    int swords;
    int archers;
    int priests;
  };
  for (auto const& barrow : {Barrow{"barrow_heart", 0.0F, k_barrow_z, 8, 4, 3},
                             Barrow{"barrow_west", -22.0F, k_barrow_z + 4.0F, 6, 3, 2},
                             Barrow{"barrow_east", 22.0F, k_barrow_z + 2.0F, 6, 3, 2}}) {
    Game::Map::UndeadZone zone;
    zone.id = QString::fromLatin1(barrow.id);
    zone.anchor_type = Game::Map::WorldProp::Type::MagicShrine;
    zone.x = barrow.x;
    zone.z = barrow.z;
    zone.radius = 14.0F;
    zone.leash_radius = 30.0F;
    zone.owner_id = 99;
    zone.team_id = 99;
    zone.awaken_on = {QStringLiteral("unit_enters_radius")};
    zone.waves = {
        undead_wave(QStringLiteral("initial"),
                    {{Game::Units::SpawnType::SkeletonSwordsman, barrow.swords},
                     {Game::Units::SpawnType::SkeletonArcher, barrow.archers},
                     {Game::Units::SpawnType::GravePriest, barrow.priests}}),
        undead_wave(QStringLiteral("after_clear"),
                    {{Game::Units::SpawnType::SkeletonSwordsman, 4}})};
    s.undead_zones.push_back(zone);
  }

  s.resource_patches = {
      prop("ruins", 3, {-10.0F, 0.0F, k_barrow_z - 12.0F}, {9.0F, 0.0F, -3.0F}, 1.5F, 3.0F, 0.4F),
      prop("ruins", 2, {18.0F, 0.0F, k_barrow_z - 10.0F}, {7.0F, 0.0F, 3.0F}, 1.3F, 3.0F, 0.4F),
      prop("statue", 2, {-8.0F, 0.0F, k_barrow_z + 3.0F}, {16.0F, 0.0F, 0.0F}, 1.3F, 1.0F, 0.1F),
      prop("dead_tree", 6, {-40.0F, 0.0F, -6.0F}, {6.0F, 0.0F, -5.0F}, 1.2F, 3.5F, 0.4F),
      prop("dead_tree", 5, {34.0F, 0.0F, -2.0F}, {5.0F, 0.0F, 6.0F}, 1.2F, 3.5F, 0.4F),
      prop("dead_tree", 3, {-14.0F, 0.0F, 12.0F}, {12.0F, 0.0F, 4.0F}, 1.1F, 3.0F, 0.4F),
      prop("pine_tree", 8, {-54.0F, 0.0F, 30.0F}, {4.0F, 0.0F, -6.0F}, 1.3F, 4.0F, 0.3F),
      prop("pine_tree", 7, {50.0F, 0.0F, 26.0F}, {4.0F, 0.0F, -6.0F}, 1.3F, 4.0F, 0.3F),
      prop("boulder", 6, {-26.0F, 0.0F, 6.0F}, {9.0F, 0.0F, 3.0F}, 1.2F, 3.0F, 0.5F),
      prop("fire_camp", 2, {-8.0F, 0.0F, 30.0F}, {16.0F, 0.0F, 2.0F}, 0.55F, 1.0F, 0.1F),
      prop("fire_camp", 2, {-6.0F, 0.0F, 46.0F}, {12.0F, 0.0F, -3.0F}, 0.5F, 1.0F, 0.1F),
      prop("fire_camp", 1, {-4.0F, 0.0F, k_barrow_z - 3.0F}, {}, 0.45F, 0.5F, 0.1F),
  };

  auto legion = [](const char* name, Troop troop, int count, float z, int men) {
    auto group = file(QString::fromLatin1(name), troop, 1, count, {0.0F, 0.0F, z}, men, 0.0F);
    group.spacing = {3.6F, 0.0F, 0.0F};
    group.facing_degrees = 180.0F;
    return sturdy(group, 2600);
  };
  s.groups = {legion("snow_swords", Troop::Swordsman, 10, 40.0F, 12),
              legion("snow_spears", Troop::Spearman, 10, 46.0F, 12),
              legion("snow_bows", Troop::Archer, 8, 52.0F, 10)};
  auto consul = sturdy(file(QStringLiteral("consul"), Troop::RomanVeteranConsul, 1, 1,
                            {2.0F, 0.0F, 33.0F}, 1, 0.0F),
                       12000);
  consul.facing_degrees = 180.0F;
  s.groups.push_back(consul);

  const auto rpg_move = [](float time, QVector3D axes) {
    auto step = at(time, Command::RpgMove, QStringLiteral("consul"));
    step.destination = axes;
    step.rpg_view_yaw_degrees = 180.0F;
    return step;
  };
  const auto hold_attack = [](float time, bool held) {
    auto step = at(time, Command::RpgAttackHold, QStringLiteral("consul"));
    step.enabled = held;
    return step;
  };
  s.steps = {
      rpg_move(0.2F, {0.0F, 0.0F, 0.0F}),
      move_to(4.0F, QStringLiteral("snow_swords"), {0.0F, 0.0F, 14.0F}),
      move_to(4.3F, QStringLiteral("snow_spears"), {0.0F, 0.0F, 20.0F}),
      move_to(4.6F, QStringLiteral("snow_bows"), {0.0F, 0.0F, 27.0F}),
      rpg_move(5.0F, {0.0F, 0.0F, 0.55F}),
      rpg_move(16.0F, {0.0F, 0.0F, 0.0F}),
      move_to(18.0F, QStringLiteral("snow_swords"), {0.0F, 0.0F, -6.0F}),
      move_to(18.5F, QStringLiteral("snow_spears"), {0.0F, 0.0F, 2.0F}),
      rpg_move(19.0F, {0.0F, 0.0F, 1.0F}),
      rpg_move(23.0F, {0.0F, 0.0F, 0.0F}),
      hold_attack(23.1F, true),
      hold_attack(48.0F, false),
  };
  s.expectations = {exists("consul"), exists("snow_swords")};
  return s;
}

} // namespace

auto build_cinematic_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;
  result.push_back(cine_field());
  result.push_back(cine_siege());
  result.push_back(cine_sepulcher());
  return result;
}

} // namespace Arena::Scenarios
