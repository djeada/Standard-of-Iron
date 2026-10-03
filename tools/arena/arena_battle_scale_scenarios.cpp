#include "arena_battle_scale_scenarios.h"

#include <algorithm>
#include <cmath>
#include <numbers>
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

auto performance_battle_definition(QString id,
                                   QString label,
                                   int units_per_side) -> ArenaScenarioDefinition {
  auto s = definition(std::move(id),
                      std::move(label),
                      QStringLiteral("Mixed full-LOD battle with %1 units per side and "
                                     "a strict over-100-FPS p95 contract.")
                          .arg(units_per_side),
                      8.0F,
                      {units_per_side <= 20 ? 58.0F : 68.0F, 56.0F, 0.0F});
  s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
  s.select_spawned_units = false;
  s.suppress_spawn_anchor = true;
  s.suppress_ui_overlays = true;
  s.force_full_creature_lod = true;
  s.require_rigged_instancing = true;
  s.collect_animation_diagnostics = false;
  s.graphics_quality = Render::GraphicsQuality::Ultra;

  int const swords = units_per_side * 2 / 5;
  int const spears = units_per_side * 3 / 10;
  int const archers = units_per_side / 5;
  int const cavalry = units_per_side - swords - spears - archers;
  s.groups = {
      group(QStringLiteral("blue_swords"),
            Troop::Swordsman,
            1,
            swords,
            {-16.0F, 0.0F, -12.0F},
            1),
      group(QStringLiteral("blue_spears"),
            Troop::Spearman,
            1,
            spears,
            {-12.0F, 0.0F, -18.0F},
            1),
      group(QStringLiteral("blue_archers"),
            Troop::Archer,
            1,
            archers,
            {-8.0F, 0.0F, -25.0F},
            1),
      group(QStringLiteral("blue_cavalry"),
            Troop::MountedSwordsman,
            1,
            cavalry,
            {-23.0F, 0.0F, -20.0F},
            1),
      group(QStringLiteral("red_swords"),
            Troop::Swordsman,
            2,
            swords,
            {-16.0F, 0.0F, 12.0F},
            1),
      group(QStringLiteral("red_spears"),
            Troop::Spearman,
            2,
            spears,
            {-12.0F, 0.0F, 18.0F},
            1),
      group(QStringLiteral("red_archers"),
            Troop::Archer,
            2,
            archers,
            {-8.0F, 0.0F, 25.0F},
            1),
      group(QStringLiteral("red_cavalry"),
            Troop::MountedSwordsman,
            2,
            cavalry,
            {-23.0F, 0.0F, 20.0F},
            1),
  };
  s.steps = {
      at(0.25F,
         Command::AttackMove,
         QStringLiteral("blue_swords"),
         QStringLiteral("red_spears")),
      at(0.25F,
         Command::AttackMove,
         QStringLiteral("blue_spears"),
         QStringLiteral("red_swords")),
      at(0.25F,
         Command::Attack,
         QStringLiteral("blue_archers"),
         QStringLiteral("red_spears")),
      at(0.25F,
         Command::Charge,
         QStringLiteral("blue_cavalry"),
         QStringLiteral("red_archers")),
      at(0.25F,
         Command::AttackMove,
         QStringLiteral("red_swords"),
         QStringLiteral("blue_spears")),
      at(0.25F,
         Command::AttackMove,
         QStringLiteral("red_spears"),
         QStringLiteral("blue_swords")),
      at(0.25F,
         Command::Attack,
         QStringLiteral("red_archers"),
         QStringLiteral("blue_spears")),
      at(0.25F,
         Command::Charge,
         QStringLiteral("red_cavalry"),
         QStringLiteral("blue_archers")),
  };
  s.expectations = {
      expectation(Expect::GroupExists, QStringLiteral("blue_swords")),
      expectation(Expect::GroupExists, QStringLiteral("red_swords")),
      expectation(Expect::FrameBudget, {}, {}, 9.99F, 2.0F),
  };
  return s;
}

auto massed_battle_definition(QString id,
                              QString label,
                              int units_per_side,
                              int individuals_per_unit = 20)
    -> ArenaScenarioDefinition {
  int const soldiers_per_side = units_per_side * individuals_per_unit;

  auto s = definition(std::move(id),
                      std::move(label),
                      QStringLiteral("Full-field line battle: %1 squads and %2 "
                                     "rendered soldiers per side at forced full "
                                     "creature LOD, Ultra shadows, batching, combat, "
                                     "archery and cavalry.")
                          .arg(units_per_side)
                          .arg(soldiers_per_side),
                      16.0F,
                      {units_per_side >= 40 ? 132.0F : 96.0F, 58.0F, 0.0F});
  s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
  s.arena_floor_half_extent = 45.0F;
  s.select_spawned_units = false;
  s.suppress_spawn_anchor = true;
  s.suppress_ui_overlays = true;
  s.force_full_creature_lod = true;
  s.require_rigged_instancing = true;
  s.collect_animation_diagnostics = false;
  s.graphics_quality = Render::GraphicsQuality::Ultra;

  int const swords = units_per_side * 32 / 100;
  int const spears = units_per_side * 28 / 100;
  int const archers = units_per_side * 20 / 100;
  int const horse_archers = std::max(1, units_per_side * 4 / 100);
  int const healers = std::max(1, units_per_side * 2 / 100);
  int const cavalry =
      units_per_side - swords - spears - archers - horse_archers - healers;

  auto line = [individuals_per_unit](const QString& name,
                                     Troop troop,
                                     int owner,
                                     int count,
                                     float sign,
                                     float depth,
                                     float x_step) {
    return group(name,
                 troop,
                 owner,
                 count,
                 {0.0F, 0.0F, sign * depth},
                 individuals_per_unit,
                 {x_step, 0.0F, 0.0F});
  };
  auto column = [individuals_per_unit](const QString& name,
                                       Troop troop,
                                       int owner,
                                       int count,
                                       float sign,
                                       float x_offset,
                                       float depth) {
    return group(name,
                 troop,
                 owner,
                 count,
                 {x_offset, 0.0F, sign * depth},
                 individuals_per_unit,
                 {0.0F, 0.0F, sign * 6.5F});
  };

  auto add_side = [&](const QString& prefix, int owner, float sign) {
    s.groups.push_back(line(prefix + QStringLiteral("_swords_a"),
                            Troop::Swordsman,
                            owner,
                            (swords + 1) / 2,
                            sign,
                            11.0F,
                            6.0F));
    s.groups.push_back(line(prefix + QStringLiteral("_swords_b"),
                            Troop::Swordsman,
                            owner,
                            swords / 2,
                            sign,
                            17.0F,
                            6.0F));
    s.groups.push_back(line(prefix + QStringLiteral("_spears_a"),
                            Troop::Spearman,
                            owner,
                            (spears + 1) / 2,
                            sign,
                            23.0F,
                            6.5F));
    s.groups.push_back(line(prefix + QStringLiteral("_spears_b"),
                            Troop::Spearman,
                            owner,
                            spears / 2,
                            sign,
                            29.0F,
                            6.5F));
    s.groups.push_back(line(prefix + QStringLiteral("_archers_a"),
                            Troop::Archer,
                            owner,
                            (archers + 1) / 2,
                            sign,
                            35.0F,
                            8.0F));
    s.groups.push_back(line(prefix + QStringLiteral("_archers_b"),
                            Troop::Archer,
                            owner,
                            archers / 2,
                            sign,
                            41.0F,
                            8.0F));
    s.groups.push_back(column(prefix + QStringLiteral("_cavalry_left"),
                              Troop::MountedSwordsman,
                              owner,
                              (cavalry + 1) / 2,
                              sign,
                              -34.0F,
                              18.0F));
    s.groups.push_back(column(prefix + QStringLiteral("_cavalry_right"),
                              Troop::MountedSwordsman,
                              owner,
                              cavalry / 2,
                              sign,
                              34.0F,
                              18.0F));
    s.groups.push_back(column(prefix + QStringLiteral("_horse_archers"),
                              Troop::HorseArcher,
                              owner,
                              horse_archers,
                              sign,
                              -41.0F,
                              33.0F));
    s.groups.push_back(column(prefix + QStringLiteral("_healers"),
                              Troop::Healer,
                              owner,
                              healers,
                              sign,
                              41.0F,
                              38.0F));
  };
  add_side(QStringLiteral("blue"), 1, -1.0F);
  add_side(QStringLiteral("red"), 2, 1.0F);

  auto add_orders = [&](const QString& prefix, const QString& enemy) {
    s.steps.push_back(at(0.5F,
                         Command::AttackMove,
                         prefix + QStringLiteral("_swords_a"),
                         enemy + QStringLiteral("_spears_a")));
    s.steps.push_back(at(0.5F,
                         Command::AttackMove,
                         prefix + QStringLiteral("_swords_b"),
                         enemy + QStringLiteral("_spears_b")));
    s.steps.push_back(at(0.5F,
                         Command::AttackMove,
                         prefix + QStringLiteral("_spears_a"),
                         enemy + QStringLiteral("_swords_a")));
    s.steps.push_back(at(0.5F,
                         Command::AttackMove,
                         prefix + QStringLiteral("_spears_b"),
                         enemy + QStringLiteral("_swords_b")));
    s.steps.push_back(at(0.5F,
                         Command::Attack,
                         prefix + QStringLiteral("_archers_a"),
                         enemy + QStringLiteral("_spears_a")));
    s.steps.push_back(at(0.5F,
                         Command::Attack,
                         prefix + QStringLiteral("_archers_b"),
                         enemy + QStringLiteral("_swords_a")));
    s.steps.push_back(at(0.5F,
                         Command::Attack,
                         prefix + QStringLiteral("_horse_archers"),
                         enemy + QStringLiteral("_swords_a")));
    s.steps.push_back(at(0.5F,
                         Command::Charge,
                         prefix + QStringLiteral("_cavalry_left"),
                         enemy + QStringLiteral("_archers_a")));
    s.steps.push_back(at(0.5F,
                         Command::Charge,
                         prefix + QStringLiteral("_cavalry_right"),
                         enemy + QStringLiteral("_archers_b")));
  };
  add_orders(QStringLiteral("blue"), QStringLiteral("red"));
  add_orders(QStringLiteral("red"), QStringLiteral("blue"));

  s.expectations = {
      expectation(Expect::GroupExists, QStringLiteral("blue_swords_a")),
      expectation(Expect::GroupExists, QStringLiteral("red_swords_a")),
      expectation(Expect::FrameBudget, {}, {}, 16.67F, 4.0F),
  };
  return s;
}

auto seven_ai_scale_definition() -> ArenaScenarioDefinition {
  constexpr int k_ai_count = 7;
  constexpr float k_circle_radius = 72.0F;
  constexpr float k_unit_radius = 49.0F;
  constexpr int k_individuals = 10;

  auto s = definition(
      QString::fromLatin1(k_seven_ai_scale_id),
      QStringLiteral("Performance: Seven Full Economies"),
      QStringLiteral("Seven active AI economies with 140 initial buildings, 252 "
                     "simulation units, and roughly 2,000 full-detail rendered "
                     "soldiers. Exercises AI snapshots, construction, resources, "
                     "combat, building rendering, and maximum-player scheduling."),
      30.0F,
      {188.0F, 62.0F, 0.0F});
  s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
  s.arena_floor_half_extent = 120.0F;
  s.select_spawned_units = false;
  s.suppress_spawn_anchor = true;
  s.suppress_ui_overlays = true;
  s.force_full_creature_lod = true;
  s.require_rigged_instancing = true;
  s.collect_animation_diagnostics = false;
  s.graphics_quality = Render::GraphicsQuality::Ultra;

  auto add_ai_group = [&](ArenaScenarioGroup value) {
    value.ai_controlled = true;
    s.groups.push_back(std::move(value));
  };

  for (int index = 0; index < k_ai_count; ++index) {
    const int owner = index + 2;
    const auto nation = (index % 2) == 0 ? Nation::RomanRepublic : Nation::Carthage;
    const float angle = -std::numbers::pi_v<float> * 0.5F +
                        2.0F * std::numbers::pi_v<float> * static_cast<float>(index) /
                            static_cast<float>(k_ai_count);
    const QVector3D radial(std::cos(angle), 0.0F, std::sin(angle));
    const QVector3D tangent(-radial.z(), 0.0F, radial.x());
    const QVector3D center = radial * k_circle_radius;
    const QVector3D unit_center = radial * k_unit_radius;
    const QString prefix = QStringLiteral("ai_%1_").arg(owner);

    auto add_building_row = [&](const QString& suffix,
                                Game::Units::SpawnType type,
                                int count,
                                float radial_offset,
                                float tangent_offset,
                                float spacing) {
      auto row = building(prefix + suffix,
                          type,
                          nation,
                          owner,
                          count,
                          center + radial * radial_offset + tangent * tangent_offset,
                          tangent * spacing,
                          -angle * 180.0F / std::numbers::pi_v<float>);
      add_ai_group(std::move(row));
    };

    add_building_row(
        QStringLiteral("homes"), Game::Units::SpawnType::Home, 8, 7.0F, -17.5F, 5.0F);
    add_building_row(QStringLiteral("barracks"),
                     Game::Units::SpawnType::Barracks,
                     3,
                     0.0F,
                     -8.0F,
                     8.0F);
    add_building_row(QStringLiteral("markets"),
                     Game::Units::SpawnType::Marketplace,
                     3,
                     -8.0F,
                     -8.0F,
                     8.0F);
    add_building_row(QStringLiteral("temples"),
                     Game::Units::SpawnType::Temple,
                     2,
                     15.0F,
                     -5.0F,
                     10.0F);
    add_building_row(QStringLiteral("towers"),
                     Game::Units::SpawnType::DefenseTower,
                     4,
                     -16.0F,
                     -15.0F,
                     10.0F);

    auto add_troops = [&](const QString& suffix,
                          Troop troop,
                          int count,
                          float radial_offset,
                          int individuals) {
      auto troops = nation_group(prefix + suffix,
                                 troop,
                                 nation,
                                 owner,
                                 count,
                                 unit_center + radial * radial_offset - tangent * 9.0F,
                                 individuals,
                                 tangent * 2.8F);
      troops.facing_degrees = -angle * 180.0F / std::numbers::pi_v<float> - 90.0F;
      add_ai_group(std::move(troops));
    };
    add_troops(QStringLiteral("builders"), Troop::Builder, 8, 12.0F, 1);
    add_troops(QStringLiteral("swords"), Troop::Swordsman, 8, 6.0F, k_individuals);
    add_troops(QStringLiteral("spears"), Troop::Spearman, 8, 0.0F, k_individuals);
    add_troops(QStringLiteral("archers"), Troop::Archer, 8, -6.0F, k_individuals);
    add_troops(
        QStringLiteral("cavalry"), Troop::MountedSwordsman, 4, -12.0F, k_individuals);

    const QVector3D resources = radial * 94.0F - tangent * 8.0F;
    s.resource_patches.push_back(
        patch("olive_tree", 8, resources, tangent * 2.6F, 1.05F));
    s.resource_patches.push_back(
        patch("boulder", 6, resources + radial * 7.0F, tangent * 2.8F, 1.0F));
    s.resource_patches.push_back(
        patch("iron_ore", 4, resources - radial * 7.0F, tangent * 3.0F, 1.0F));
  }

  s.expectations.push_back(
      expectation(Expect::GroupExists, QStringLiteral("ai_2_builders")));
  s.expectations.push_back(
      expectation(Expect::GroupExists, QStringLiteral("ai_8_builders")));
  s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 1000.0F, 2.0F));
  return s;
}

} // namespace

auto build_battle_scale_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;

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
              Troop::MountedSwordsman,
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
    result.push_back(
        massed_battle_definition(QString::fromLatin1(k_massed_battle_250_id),
                                 QStringLiteral("Performance: 250 vs 250 Soldiers"),
                                 25,
                                 10));
    result.push_back(
        performance_battle_definition(QString::fromLatin1(k_performance_20v20_id),
                                      QStringLiteral("Performance: 20 vs 20 Units"),
                                      20));
    result.push_back(
        performance_battle_definition(QString::fromLatin1(k_performance_30v30_id),
                                      QStringLiteral("Performance: 30 vs 30 Units"),
                                      30));
    result.push_back(
        massed_battle_definition(QString::fromLatin1(k_massed_battle_500_id),
                                 QStringLiteral("Performance: 500 vs 500 Soldiers"),
                                 25));
    result.push_back(
        massed_battle_definition(QString::fromLatin1(k_massed_battle_1000_id),
                                 QStringLiteral("Performance: 1000 vs 1000 Soldiers"),
                                 50));
    result.push_back(
        massed_battle_definition(QString::fromLatin1(k_massed_battle_2000_id),
                                 QStringLiteral("Performance: 2000 vs 2000 Soldiers"),
                                 100));
    result.push_back(seven_ai_scale_definition());
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
              Troop::MountedSwordsman,
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
              Troop::MountedSwordsman,
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

  return result;
}

} // namespace Arena::Scenarios
