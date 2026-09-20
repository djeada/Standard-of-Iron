#include "arena_economy_scenarios.h"

#include <initializer_list>
#include <utility>

#include "arena_scenarios.h"
#include "game/wildlife/wildlife_config.h"

namespace Arena::Scenarios {
namespace {

using Expect = ArenaExpectationKind;
using Nation = Game::Systems::NationID;
using Troop = Game::Units::TroopType;

constexpr float k_floor_half_extent = 62.0F;

constexpr int k_owner = 2;

auto patch(const char* prop_type,
           int count,
           QVector3D origin,
           QVector3D spacing,
           float scale = 1.0F,
           bool exact = false) -> ArenaScenarioResourcePatch {
  return {QString::fromLatin1(prop_type), count, origin, spacing, scale, exact};
}

auto unit_group(QString name,
                Troop troop,
                int count,
                QVector3D origin,
                QVector3D spacing,
                float facing_degrees) -> ArenaScenarioGroup {
  ArenaScenarioGroup result;
  result.name = std::move(name);
  result.troop_type = troop;
  result.nation_id = Nation::Carthage;
  result.owner_id = k_owner;
  result.count = count;
  result.origin = origin;
  result.spacing = spacing;
  result.facing_degrees = facing_degrees;
  result.ai_controlled = true;
  return result;
}

auto seat_building(QString name,
                   Game::Units::SpawnType type,
                   QVector3D origin,
                   float facing_degrees,
                   int max_population = 0) -> ArenaScenarioGroup {
  ArenaScenarioGroup result;
  result.name = std::move(name);
  result.spawn_type = type;
  result.nation_id = Nation::Carthage;
  result.owner_id = k_owner;
  result.count = 1;
  result.origin = origin;
  result.spacing = {0.0F, 0.0F, 0.0F};
  result.facing_degrees = facing_degrees;
  result.ai_controlled = true;
  result.max_population = max_population;
  return result;
}

auto knoll(float x, float z, float radius, float height, Game::Map::HillShape shape)
    -> Game::Map::TerrainFeature {
  Game::Map::TerrainFeature feature;
  feature.type = Game::Map::TerrainType::Hill;
  feature.center_x = x;
  feature.center_z = z;
  feature.radius = radius;
  feature.height = height;
  feature.shape = shape;
  feature.thickness = radius * 0.55F;
  feature.rotation_deg = 0.0F;
  return feature;
}

void add_the_land(ArenaScenarioDefinition& scenario) {
  using Game::Map::HillShape;

  scenario.rivers.push_back(
      Game::Map::RiverSegment{{-96.0F, 0.0F, -56.0F}, {-8.0F, 0.0F, -50.0F}, 11.0F});
  scenario.rivers.push_back(
      Game::Map::RiverSegment{{-8.0F, 0.0F, -50.0F}, {96.0F, 0.0F, -56.0F}, 11.0F});
  scenario.bridges.push_back(
      Game::Map::Bridge{{-8.0F, 0.0F, -57.0F}, {-8.0F, 0.0F, -43.0F}, 10.0F, 0.5F});

  Game::Map::Lake lake;
  lake.center = QVector3D(46.0F, 0.0F, 40.0F);
  lake.width = 20.0F;
  lake.depth = 16.0F;
  scenario.lakes.push_back(lake);

  scenario.terrain_features.push_back(
      knoll(-48.0F, 30.0F, 13.0F, 4.2F, HillShape::Blob));
  scenario.terrain_features.push_back(
      knoll(48.0F, -28.0F, 12.0F, 3.8F, HillShape::Blob));
  scenario.terrain_features.push_back(knoll(-4.0F, 50.0F, 15.0F, 3.4F, HillShape::Arc));

  constexpr float k_lane = 3.4F;
  const auto lane = [&scenario](QVector3D from, QVector3D to, float width) {
    scenario.roads.push_back(Game::Map::RoadSegment{from, to, width});
  };

  lane({-96.0F, 0.0F, -41.0F}, {-40.0F, 0.0F, -43.0F}, k_lane);
  lane({-40.0F, 0.0F, -43.0F}, {-8.0F, 0.0F, -43.0F}, k_lane);
  lane({-8.0F, 0.0F, -43.0F}, {34.0F, 0.0F, -45.0F}, k_lane);
  lane({34.0F, 0.0F, -45.0F}, {96.0F, 0.0F, -42.0F}, k_lane);

  lane({-96.0F, 0.0F, 54.0F}, {-30.0F, 0.0F, 52.0F}, k_lane);
  lane({-30.0F, 0.0F, 52.0F}, {-4.0F, 0.0F, 53.4F}, k_lane);
  lane({-4.0F, 0.0F, 53.4F}, {24.0F, 0.0F, 55.0F}, k_lane);
  lane({24.0F, 0.0F, 55.0F}, {70.0F, 0.0F, 53.5F}, k_lane);
  lane({70.0F, 0.0F, 53.5F}, {96.0F, 0.0F, 53.0F}, k_lane);

  lane({34.0F, 0.0F, -45.0F}, {62.0F, 0.0F, -20.0F}, k_lane);
  lane({62.0F, 0.0F, -20.0F}, {68.0F, 0.0F, 18.0F}, k_lane);
  lane({68.0F, 0.0F, 18.0F}, {70.0F, 0.0F, 53.5F}, k_lane);

  constexpr float k_town_lane = 3.2F;
  lane({-8.0F, 0.0F, -96.0F}, {-8.0F, 0.0F, -57.0F}, k_town_lane);
  lane({-8.0F, 0.0F, -43.0F}, {-4.0F, 0.0F, -20.0F}, k_town_lane);
  lane({-4.0F, 0.0F, -20.0F}, {-6.0F, 0.0F, 4.0F}, k_town_lane);
  lane({-6.0F, 0.0F, 4.0F}, {-5.0F, 0.0F, 30.0F}, k_town_lane);
  lane({-5.0F, 0.0F, 30.0F}, {-4.0F, 0.0F, 53.4F}, k_town_lane);

  const auto grove = [&scenario](const char* type,
                                 QVector3D at,
                                 float scale,
                                 std::initializer_list<QVector3D> rows) {
    const float outward_x = at.x() < 0.0F ? -1.0F : 1.0F;
    const float outward_z = at.z() < 0.0F ? -1.0F : 1.0F;
    for (const QVector3D& row : rows) {
      const int count = static_cast<int>(row.y());
      scenario.resource_patches.push_back(
          patch(type,
                count,
                {at.x() + (row.x() * outward_x), 0.0F, at.z() + (row.z() * outward_z)},
                {2.9F * outward_x, 0.0F, 0.8F * outward_z},
                scale));
    }
  };

  grove("olive_tree",
        {-38.0F, 0.0F, -4.0F},
        1.12F,
        {{0.0F, 5.0F, 0.0F}, {-3.0F, 4.0F, 4.4F}, {1.5F, 4.0F, 9.0F}});
  grove("olive_tree",
        {-34.0F, 0.0F, 12.0F},
        1.08F,
        {{0.0F, 4.0F, 0.0F}, {-2.5F, 4.0F, 4.6F}});
  grove("pine_tree",
        {-34.0F, 0.0F, -26.0F},
        1.05F,
        {{0.0F, 5.0F, 0.0F}, {-2.0F, 4.0F, 4.8F}, {2.0F, 4.0F, 9.4F}});
  grove("cypress_tree", {-42.0F, 0.0F, 20.0F}, 1.0F, {{0.0F, 4.0F, 0.0F}});

  grove("boulder",
        {-18.0F, 0.0F, 34.0F},
        1.15F,
        {{0.0F, 5.0F, 0.0F}, {-1.5F, 4.0F, 4.2F}});
  grove(
      "boulder", {6.0F, 0.0F, 37.0F}, 1.05F, {{0.0F, 4.0F, 0.0F}, {1.0F, 3.0F, 4.0F}});
  grove("boulder", {32.0F, 0.0F, 22.0F}, 1.0F, {{0.0F, 4.0F, 0.0F}});

  grove("iron_ore",
        {38.0F, 0.0F, -6.0F},
        1.05F,
        {{0.0F, 3.0F, 0.0F}, {-1.5F, 3.0F, 4.0F}});
  grove("iron_ore", {40.0F, 0.0F, 8.0F}, 1.0F, {{0.0F, 3.0F, 0.0F}});
  grove("iron_ore", {30.0F, 0.0F, -20.0F}, 1.0F, {{0.0F, 3.0F, 0.0F}});

  scenario.resource_patches.push_back(
      patch("plant", 9, {-34.0F, 0.0F, 26.0F}, {3.0F, 0.0F, 1.2F}, 0.9F));
  scenario.resource_patches.push_back(
      patch("plant", 8, {20.0F, 0.0F, -26.0F}, {2.8F, 0.0F, -0.8F}, 0.9F));
  scenario.resource_patches.push_back(
      patch("palm_tree", 5, {28.0F, 0.0F, 44.0F}, {3.4F, 0.0F, 1.4F}, 1.0F));
  scenario.resource_patches.push_back(
      patch("plant", 6, {40.0F, 0.0F, 30.0F}, {2.6F, 0.0F, 1.4F}, 0.85F));

  scenario.resource_patches.push_back(
      patch("cursed_gold_vein", 1, {-54.0F, 0.0F, 8.0F}, {}, 1.15F, true));
  scenario.resource_patches.push_back(
      patch("cursed_gold_vein", 1, {58.0F, 0.0F, -8.0F}, {}, 1.1F, true));
}

auto estate_wildlife() -> Game::Wildlife::WildlifeSettings {
  Game::Wildlife::WildlifeSettings settings = Game::Wildlife::default_settings();
  settings.enabled = true;
  settings.seed = 20260908U;

  settings.wolves.enabled = false;
  settings.wolves.group_count = 0;

  settings.sheep.enabled = true;
  settings.sheep.group_count = 3;
  settings.sheep.group_size_min = 6;
  settings.sheep.group_size_max = 9;
  settings.sheep.roam_radius = 8.0F;

  settings.sheep.spawn_areas = {
      {-34.0F, 30.0F, 5.0F}, {26.0F, 30.0F, 5.0F}, {10.0F, -38.0F, 5.0F}};

  settings.birds = Game::Wildlife::default_bird_config();
  settings.birds.enabled = true;
  settings.birds.group_count = 2;
  settings.birds.spawn_areas = {{-20.0F, -34.0F, 10.0F}, {40.0F, 24.0F, 10.0F}};
  return settings;
}

} // namespace

auto build_economy_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;

  for (int variant = 0; variant < 4; ++variant) {
    const bool lazy_farmer = variant == 3;
    ArenaScenarioDefinition s;
    s.id = variant == 0   ? QStringLiteral("farm_activity_single")
           : variant == 1 ? QStringLiteral("farm_activity_dense")
           : variant == 2 ? QStringLiteral("farm_activity_terrain")
                          : QStringLiteral("farm_activity_lazy_farmer");
    s.label = s.id;
    s.description =
        lazy_farmer
            ? QStringLiteral(
                  "Close review of the rare lazy-farmer sequence: sit, hat over "
                  "the eyes, a coworker walks over to stare, then hurried work. "
                  "Set SOI_FARM_GAG_SECONDS=10 to force it; it is otherwise "
                  "roughly once per ten minutes per field.")
            : QStringLiteral(
                  "Cosmetic Roman/Punic field workers: tending, sickle harvest, "
                  "empty reset and destruction. Use --fog-of-war for visibility "
                  "review. No economic workers or decorative gameplay entities "
                  "are spawned.");
    // The dense scene carries 12 full crop fields; it is kept short so the
    // software-GL arena stays inside the harness watchdog.
    s.duration_seconds = variant == 1 ? 40 : lazy_farmer ? 40 : 90;
    s.camera = {variant == 1 ? 78.0F : lazy_farmer ? 13.0F : 34.0F, 45.0F, 30.0F};
    s.camera_focus = lazy_farmer ? QVector3D(5, 0, -4) : QVector3D(0, 0, 0);
    s.arena_floor_half_extent = variant == 1 ? 65 : 30;
    s.terrain_grid_extent = 160;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_terrain_scatter = true;
    s.ground_type = QStringLiteral("soil_fertile");
    if (variant == 2) {
      s.terrain_features.push_back(knoll(0, 0, 24, 2, Game::Map::HillShape::Blob));
    }
    const int rows = variant == 1 ? 3 : 1;
    for (int row = 0; row < rows; ++row) {
      for (int nation = 0; nation < 2; ++nation) {
        const auto name = QStringLiteral("field_%1_%2").arg(row).arg(nation);
        auto field = seat_building(name,
                                   Game::Units::SpawnType::Farm,
                                   {nation == 0 ? -8.0F : 8.0F,
                                    0,
                                    static_cast<float>(row) * 15 - (rows - 1) * 7.5F},
                                   0);
        field.nation_id = nation == 0 ? Nation::RomanRepublic : Nation::Carthage;
        field.owner_id = nation + 1;
        field.ai_controlled = false;
        if (variant == 1) {
          // Spacing is applied about the group's centre
          // (origin + spacing * (index - (count - 1) / 2)), so the origin has
          // to be offset by half the span or the two nations' inner columns
          // both land on x = 0 and sit inside each other.
          field.count = 2;
          field.origin = {nation == 0 ? -19.0F : 19.0F, 0, field.origin.z()};
          field.spacing = {nation == 0 ? -18.0F : 18.0F, 0, 0};
        }
        s.groups.push_back(std::move(field));
        ArenaExpectation lifecycle;
        lifecycle.kind = row == 0 && !lazy_farmer ? Expect::GroupDestroyed
                                                  : Expect::GroupHealthUnchanged;
        lifecycle.group = name;
        s.expectations.push_back(std::move(lifecycle));
        // The gag review scene stays ripe throughout: a growth change cancels
        // the sequence by design, which would cut every capture short.
        const int stages = lazy_farmer ? 1 : 3;
        for (int stage = 0; stage < stages; ++stage) {
          ArenaScenarioStep step;
          step.group = name;
          step.command = ScenarioCommandKind::SetFarmGrowth;
          step.trigger.time_seconds = stage == 0 ? 0.5F : stage == 1 ? 8.0F : 16.0F;
          step.value = lazy_farmer ? 100 : stage == 0 ? 40 : stage == 1 ? 100 : 0;
          s.steps.push_back(std::move(step));
        }
        if (row == 0 && !lazy_farmer) {
          ArenaScenarioStep destroy;
          destroy.group = name;
          destroy.command = ScenarioCommandKind::SetHealth;
          destroy.trigger.time_seconds = 24;
          destroy.value = 0;
          s.steps.push_back(std::move(destroy));
        }
      }
    }
    result.push_back(std::move(s));
  }

  for (int variant = 0; variant < 4; ++variant) {
    const bool soup = variant == 3;
    const bool night = variant == 2;
    ArenaScenarioDefinition s;
    s.id = variant == 0   ? QStringLiteral("home_ambient_street")
           : variant == 1 ? QStringLiteral("home_ambient_dense")
           : night        ? QStringLiteral("home_ambient_night")
                          : QStringLiteral("home_ambient_soup");
    s.label = s.id;
    s.description =
        soup ? QStringLiteral(
                   "Close review of the rare soup-spill sequence: a resident "
                   "carries a bowl out of the door, trips, spills, gets up and "
                   "goes back in. Set SOI_HOME_GAG_SECONDS=12 to force it.")
             : QStringLiteral(
                   "Roman and Punic houses venting hearth smoke: staggered per "
                   "house, from a roof the model actually has. Use --fog-of-war "
                   "for the visibility boundary. One house is destroyed mid-run "
                   "and must stop smoking.");
    s.duration_seconds = variant == 1 ? 45 : soup ? 40 : 90;
    s.camera = {variant == 1 ? 68.0F : soup ? 14.0F : 40.0F, 45.0F, 30.0F};
    s.camera_focus = soup ? QVector3D(-9, 0, 4) : QVector3D(0, 0, 0);
    s.arena_floor_half_extent = variant == 1 ? 52 : 30;
    s.terrain_grid_extent = 160;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_terrain_scatter = true;
    s.ground_type = QStringLiteral("grass_dry");
    if (night) {
      // Smoke has to stay legible against a dark sky, not just a bright one.
      s.environment.start_time = 21.0F;
      s.environment.lighting_profile = QStringLiteral("mediterranean_summer");
    }
    const int rows = variant == 1 ? 3 : 1;
    for (int row = 0; row < rows; ++row) {
      for (int nation = 0; nation < 2; ++nation) {
        const auto name = QStringLiteral("house_%1_%2").arg(row).arg(nation);
        auto house =
            seat_building(name,
                          Game::Units::SpawnType::Home,
                          {nation == 0 ? -9.0F : 9.0F,
                           0,
                           // Each row spans 2 x spacing about its own
                           // centre, so the row pitch has to clear
                           // that span or neighbouring rows interleave.
                           (static_cast<float>(row) * 20.0F) - ((rows - 1) * 10.0F)},
                          0);
        house.nation_id = nation == 0 ? Nation::RomanRepublic : Nation::Carthage;
        house.owner_id = nation + 1;
        house.ai_controlled = false;
        if (!soup) {
          // Several neighbours per row, so lockstep would be obvious.
          house.count = variant == 1 ? 3 : 3;
          house.spacing = {0, 0, 8.0F};
        }
        s.groups.push_back(std::move(house));
        ArenaExpectation lifecycle;
        lifecycle.kind = Expect::GroupHealthUnchanged;
        lifecycle.group = name;
        s.expectations.push_back(std::move(lifecycle));

        // One extra house per nation is levelled mid-run and must stop
        // smoking at once, while its neighbours carry on.
        if (row == 0 && !soup) {
          const auto doomed_name = QStringLiteral("doomed_%1").arg(nation);
          auto doomed = seat_building(doomed_name,
                                      Game::Units::SpawnType::Home,
                                      {nation == 0 ? -9.0F : 9.0F, 0, -19.0F},
                                      0);
          doomed.nation_id = nation == 0 ? Nation::RomanRepublic : Nation::Carthage;
          doomed.owner_id = nation + 1;
          doomed.ai_controlled = false;
          s.groups.push_back(std::move(doomed));
          ArenaExpectation gone;
          gone.kind = Expect::GroupDestroyed;
          gone.group = doomed_name;
          s.expectations.push_back(std::move(gone));
          ArenaScenarioStep destroy;
          destroy.group = doomed_name;
          destroy.command = ScenarioCommandKind::SetHealth;
          destroy.trigger.time_seconds = variant == 1 ? 28.0F : 40.0F;
          destroy.value = 0;
          s.steps.push_back(std::move(destroy));
        }
      }
    }
    result.push_back(std::move(s));
  }

  {
    ArenaScenarioDefinition s;
    s.id = QString::fromLatin1(k_ai_kingdom_rise_id);
    s.label = QStringLiteral("AI Kingdom: The Rise of a Town");
    s.description = QStringLiteral(
        "One Carthaginian AI commander, a full war chest and an estate with "
        "nobody on it. Hanno's economic doctrine has thirty minutes, wood to "
        "the west, stone to the south and iron to the east, and no enemy "
        "anywhere on the field -- so everything that happens is the computer "
        "running its own economy. The scene to watch when the question is "
        "whether a town the AI raises unwatched looks and behaves like a "
        "place rather than a pile of buildings.");
    s.duration_seconds = 1800.0F;

    s.camera = {78.0F, 42.0F, 30.0F};
    s.camera_focus = QVector3D(0.0F, 0.0F, 3.0F);
    s.arena_floor_half_extent = k_floor_half_extent;
    s.terrain_grid_extent = 180;
    s.ground_type = QStringLiteral("soil_fertile");
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.suppress_terrain_scatter = false;
    s.force_full_creature_lod = false;
    s.collect_animation_diagnostics = false;

    s.environment.start_time = 15.8F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;
    s.environment.lighting_profile = QStringLiteral("mediterranean_summer");
    s.environment.fog_density_override = 0.006F;
    s.environment.exposure_override = 1.05F;

    s.ai_starting_resources = {
        .gold = 12000, .food = 600, .wood = 900, .stone = 700, .iron = 400};

    add_the_land(s);
    s.wildlife = estate_wildlife();

    s.groups.push_back(seat_building(QStringLiteral("hanno_barracks"),
                                     Game::Units::SpawnType::Barracks,
                                     {0.0F, 0.0F, 0.0F},
                                     180.0F,
                                     160));
    s.groups.push_back(unit_group(QStringLiteral("hanno_commander"),
                                  Troop::CarthageSpearCommander,
                                  1,
                                  {0.0F, 0.0F, 6.0F},
                                  {0.0F, 0.0F, 0.0F},
                                  180.0F));
    s.groups.push_back(unit_group(QStringLiteral("hanno_builders"),
                                  Troop::Builder,
                                  5,
                                  {-9.0F, 0.0F, 9.0F},
                                  {4.4F, 0.0F, 0.0F},
                                  180.0F));

    ArenaScenarioBattleSide side;
    side.owner_id = k_owner;
    side.label = QStringLiteral("hanno");
    side.home = QVector3D(0.0F, 0.0F, 0.0F);
    side.home_radius = 34.0F;
    s.battle_sides.push_back(std::move(side));

    ArenaExpectation doctrine;
    doctrine.kind = Expect::SideDoctrineIs;
    doctrine.side = QStringLiteral("hanno");
    doctrine.counter_key = QStringLiteral("economic:garrison");
    s.expectations.push_back(std::move(doctrine));

    ArenaExpectation built;
    built.kind = Expect::SideBuildsAtLeast;
    built.side = QStringLiteral("hanno");
    built.threshold = 14.0F;
    s.expectations.push_back(std::move(built));

    ArenaExpectation recruited;
    recruited.kind = Expect::SideProducesReinforcements;
    recruited.side = QStringLiteral("hanno");
    recruited.threshold = 6.0F;
    s.expectations.push_back(std::move(recruited));

    ArenaExpectation garrison;
    garrison.kind = Expect::SideKeepsGarrison;
    garrison.side = QStringLiteral("hanno");
    garrison.threshold = 4.0F;
    s.expectations.push_back(std::move(garrison));

    ArenaExpectation harvests;
    harvests.kind = Expect::OwnerHarvestsResource;
    harvests.group = QStringLiteral("hanno_builders");
    harvests.threshold = 3.0F;
    s.expectations.push_back(std::move(harvests));

    ArenaExpectation constructs;
    constructs.kind = Expect::OwnerCompletesConstruction;
    constructs.group = QStringLiteral("hanno_builders");
    constructs.threshold = 4.0F;
    s.expectations.push_back(std::move(constructs));

    ArenaExpectation unstuck;
    unstuck.kind = Expect::NoPermanentStall;
    unstuck.group = QStringLiteral("hanno_builders");
    unstuck.threshold = 30.0F;
    s.expectations.push_back(std::move(unstuck));

    result.push_back(std::move(s));
  }

  return result;
}

} // namespace Arena::Scenarios
