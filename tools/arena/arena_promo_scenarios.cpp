#include "arena_promo_scenarios.h"

#include <cmath>
#include <numbers>
#include <utility>
#include <vector>

#include "arena_scenario_builders.h"
#include "arena_scenarios.h"
#include "game/wildlife/wildlife_config.h"

namespace Arena::Scenarios {
namespace {

using namespace builders;

using Command = ScenarioCommandKind;
using Expect = ArenaExpectationKind;
using Nation = Game::Systems::NationID;
using Trigger = ScenarioTriggerKind;
using Troop = Game::Units::TroopType;

} // namespace

auto build_promo_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;

  {
    auto s = definition(
        QString::fromLatin1(k_promo_last_stand_id),
        QStringLiteral("Promo: The Last Stand"),
        QStringLiteral("Golden-hour capture scene. A Roman shield line holds a "
                       "ridge against a Carthaginian horde while a cavalry wing "
                       "sweeps the archers behind it."),
        26.0F,
        {26.0F, 22.0F, 0.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.force_full_creature_lod = true;
    s.graphics_quality = Render::GraphicsQuality::Ultra;
    s.environment.start_time = 18.15F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;

    s.environment.exposure_override = 1.0F;

    s.environment.fog_density_override = 0.020F;
    s.groups = {
        group(QStringLiteral("roman_line"),
              Troop::Swordsman,
              1,
              8,
              {-9.1F, 0.0F, -7.0F},
              12),
        group(QStringLiteral("roman_spears"),
              Troop::Spearman,
              1,
              6,
              {-6.5F, 0.0F, -10.5F},
              12),
        group(QStringLiteral("roman_archers"),
              Troop::Archer,
              1,
              4,
              {-3.9F, 0.0F, -14.5F},
              8),
        group(QStringLiteral("roman_consul"),
              Troop::RomanVeteranConsul,
              1,
              1,
              {0.0F, 0.0F, -12.0F},
              1),
        group(QStringLiteral("punic_horde"),
              Troop::Swordsman,
              2,
              10,
              {-11.7F, 0.0F, 8.0F},
              12),
        group(QStringLiteral("punic_spears"),
              Troop::Spearman,
              2,
              8,
              {-9.1F, 0.0F, 12.0F},
              12),
        group(QStringLiteral("punic_cavalry"),
              Troop::MountedSwordsman,
              2,
              6,
              {16.0F, 0.0F, 6.0F},
              6),
    };
    s.resource_patches = {
        {QStringLiteral("pine"), 5, {-24.0F, 0.0F, -6.0F}, {3.0F, 0.0F, 2.0F}, 1.2F},
        {QStringLiteral("pine"), 4, {20.0F, 0.0F, -14.0F}, {3.0F, 0.0F, 2.0F}, 1.1F}};
    s.steps = {
        at(0.3F, Command::Hold, QStringLiteral("roman_line")),
        at(0.3F, Command::Hold, QStringLiteral("roman_spears")),
        at(0.4F,
           Command::AttackMove,
           QStringLiteral("punic_horde"),
           QStringLiteral("roman_line")),
        at(0.4F,
           Command::AttackMove,
           QStringLiteral("punic_spears"),
           QStringLiteral("roman_spears")),
        at(0.8F,
           Command::Attack,
           QStringLiteral("roman_archers"),
           QStringLiteral("punic_horde")),
        at(5.5F,
           Command::Charge,
           QStringLiteral("punic_cavalry"),
           QStringLiteral("roman_archers")),
        at(9.0F,
           Command::AttackMove,
           QStringLiteral("roman_line"),
           QStringLiteral("punic_horde")),
        at(9.0F,
           Command::AttackMove,
           QStringLiteral("roman_consul"),
           QStringLiteral("punic_horde")),
    };
    s.expectations = {
        expectation(Expect::GroupExists, QStringLiteral("roman_line")),
        expectation(Expect::GroupExists, QStringLiteral("punic_horde")),
        expectation(Expect::GroupIsRendered, QStringLiteral("roman_line")),
        expectation(Expect::GroupIsRendered, QStringLiteral("punic_horde")),
        expectation(Expect::GroupIsRendered, QStringLiteral("punic_cavalry")),
        expectation(Expect::AttackAnimationObserved, QStringLiteral("punic_horde")),
    };
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_promo_night_of_the_dead_id),
        QStringLiteral("Promo: Night of the Dead"),
        QStringLiteral("Night capture scene under the Iron Sepulcher profile. A "
                       "Roman column walks a cursed shrine awake and the risen "
                       "garrison closes on it from the haze."),
        30.0F,
        {24.0F, 24.0F, 0.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 2.0F);
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.force_full_creature_lod = true;
    s.collect_animation_diagnostics = false;
    s.graphics_quality = Render::GraphicsQuality::Ultra;
    s.environment.start_time = 21.4F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;
    s.environment.lighting_profile = QStringLiteral("iron_sepulcher");

    s.environment.exposure_override = 1.45F;
    s.environment.fog_density_override = 0.010F;
    s.resource_patches = {{QStringLiteral("magic_shrine"),
                           1,
                           {0.0F, 0.0F, 6.0F},
                           {0.0F, 0.0F, 0.0F},
                           1.2F},
                          {QStringLiteral("cursed_tree"),
                           4,
                           {-13.0F, 0.0F, 4.0F},
                           {4.0F, 0.0F, 3.0F},
                           1.2F},
                          {QStringLiteral("cursed_tree"),
                           3,
                           {11.0F, 0.0F, 8.0F},
                           {4.0F, 0.0F, 3.0F},
                           1.2F}};
    s.undead_zones = {
        undead_zone(QStringLiteral("shrine_garrison"),
                    Game::Map::WorldProp::Type::MagicShrine,
                    QVector3D(0.0F, 0.0F, 6.0F),
                    9.0F,
                    99,
                    {undead_wave(QStringLiteral("initial"),
                                 {{Game::Units::SpawnType::SkeletonSwordsman, 6},
                                  {Game::Units::SpawnType::SkeletonArcher, 3},
                                  {Game::Units::SpawnType::GravePriest, 1}}),
                     undead_wave(QStringLiteral("after_clear"),
                                 {{Game::Units::SpawnType::SkeletonSwordsman, 4}})})};
    s.groups = {group(QStringLiteral("roman_column"),
                      Troop::Swordsman,
                      1,
                      6,
                      {-6.5F, 0.0F, -16.0F},
                      10),
                group(QStringLiteral("roman_flank"),
                      Troop::Spearman,
                      1,
                      4,
                      {-3.9F, 0.0F, -20.0F},
                      10)};
    s.steps = {at(1.2F, Command::FormationMove, QStringLiteral("roman_column")),
               at(4.0F, Command::FormationMove, QStringLiteral("roman_flank"))};
    s.steps[0].destination = QVector3D(0.0F, 0.0F, 0.0F);
    s.steps[1].destination = QVector3D(0.0F, 0.0F, -4.0F);
    s.expectations = {
        expectation(Expect::GroupExists, QStringLiteral("roman_column")),
        expectation(Expect::GroupIsRendered, QStringLiteral("roman_column")),
        expectation(Expect::AttackAnimationObserved, QStringLiteral("roman_column")),
        zone_expectation(Expect::UndeadZoneDormantBefore,
                         QStringLiteral("shrine_garrison"),
                         0.0F,
                         1.5F),
        zone_expectation(
            Expect::UndeadZoneAwakened, QStringLiteral("shrine_garrison"), 6.0F),
    };
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_promo_storm_charge_id),
        QStringLiteral("Promo: Storm Charge"),
        QStringLiteral("Storm capture scene. Roman cavalry charges a braced "
                       "Carthaginian spear wall through driving rain while both "
                       "infantry lines close behind them."),
        24.0F,
        {22.0F, 20.0F, 0.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.force_full_creature_lod = true;
    s.collect_animation_diagnostics = false;
    s.graphics_quality = Render::GraphicsQuality::Ultra;
    s.environment.start_time = 16.2F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;
    s.environment.exposure_override = 1.0F;
    s.weather.rain = 0.85F;
    s.weather.storm = 0.65F;
    s.precipitation.enabled = true;
    s.precipitation.type = Game::Map::WeatherType::Rain;
    s.precipitation.intensity = 0.85F;
    s.precipitation.wind_strength = 0.55F;
    s.precipitation.wind_direction_deg = 205.0F;
    s.groups = {
        group(QStringLiteral("roman_cavalry"),
              Troop::MountedSwordsman,
              1,
              8,
              {-9.1F, 0.0F, -18.0F},
              6),
        group(QStringLiteral("roman_foot"),
              Troop::Swordsman,
              1,
              7,
              {-7.8F, 0.0F, -24.0F},
              12),
        group(QStringLiteral("punic_wall"),
              Troop::Spearman,
              2,
              9,
              {-10.4F, 0.0F, 6.0F},
              12),
        group(QStringLiteral("punic_support"),
              Troop::Swordsman,
              2,
              6,
              {-6.5F, 0.0F, 10.5F},
              12),
        group(QStringLiteral("punic_archers"),
              Troop::Archer,
              2,
              4,
              {-3.9F, 0.0F, 14.5F},
              8),
    };
    s.steps = {
        at(0.3F, Command::Hold, QStringLiteral("punic_wall")),
        at(0.8F,
           Command::Attack,
           QStringLiteral("punic_archers"),
           QStringLiteral("roman_cavalry")),
        at(1.5F,
           Command::Charge,
           QStringLiteral("roman_cavalry"),
           QStringLiteral("punic_wall")),
        at(3.0F,
           Command::AttackMove,
           QStringLiteral("roman_foot"),
           QStringLiteral("punic_wall")),
        at(8.0F,
           Command::AttackMove,
           QStringLiteral("punic_support"),
           QStringLiteral("roman_foot")),
    };
    s.expectations = {
        expectation(Expect::GroupExists, QStringLiteral("punic_wall")),
        expectation(Expect::GroupIsRendered, QStringLiteral("roman_cavalry")),
        expectation(Expect::GroupIsRendered, QStringLiteral("punic_wall")),
        expectation(Expect::AttackAnimationObserved, QStringLiteral("punic_wall")),
    };
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_promo_commander_duel_id),
        QStringLiteral("Promo: The Duel"),
        QStringLiteral("Golden-hour capture scene. Both armies halt in line and "
                       "Scipio walks out to meet Hannibal in the ground between "
                       "them. Long enough for the consular riposte and the "
                       "encircling cut to come round several times."),
        40.0F,
        {15.0F, 12.0F, 90.0F});
    s.camera_focus = QVector3D(0.0F, 1.0F, 0.0F);
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.force_full_creature_lod = true;
    s.graphics_quality = Render::GraphicsQuality::Ultra;
    s.arena_floor_half_extent = 30.0F;

    s.terrain_height_scale_override = 2.6F;

    s.suppress_boundary_mountains = true;
    {

      constexpr int k_ring_mounds = 18;
      constexpr float k_ring_radius = 41.0F;
      for (int i = 0; i < k_ring_mounds; ++i) {
        float const angle = 2.0F * std::numbers::pi_v<float> * static_cast<float>(i) /
                            static_cast<float>(k_ring_mounds);
        float const wobble = 0.5F * std::sin(static_cast<float>(i) * 2.3F);
        s.elevation_patches.push_back(
            {{k_ring_radius * std::sin(angle), 0.0F, k_ring_radius * std::cos(angle)},
             20.0F,
             13.5F + (2.5F * wobble)});
      }
    }
    s.environment.start_time = 16.4F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;

    s.environment.fog_density_override = 0.020F;
    s.environment.exposure_override = 1.18F;

    auto scipio = group(QStringLiteral("scipio"),
                        Troop::RomanVeteranConsul,
                        1,
                        1,
                        {0.0F, 0.0F, -9.0F},
                        1);
    auto hannibal = group(QStringLiteral("hannibal"),
                          Troop::CarthageSwordCommander,
                          2,
                          1,
                          {0.0F, 0.0F, 9.0F},
                          1);

    scipio.health_override = scipio.max_health_override = 4600;
    hannibal.health_override = hannibal.max_health_override = 1800;

    s.groups = {
        scipio,
        hannibal,
        group(QStringLiteral("roman_line"),
              Troop::Swordsman,
              1,
              9,
              {-13.6F, 0.0F, -16.0F},
              8,
              {3.4F, 0.0F, 0.0F}),
        group(QStringLiteral("roman_spears"),
              Troop::Spearman,
              1,
              7,
              {-10.2F, 0.0F, -20.5F},
              8,
              {3.4F, 0.0F, 0.0F}),
        group(QStringLiteral("roman_horse"),
              Troop::MountedSwordsman,
              1,
              4,
              {19.0F, 0.0F, -18.0F},
              4,
              {3.6F, 0.0F, 0.0F}),
        group(QStringLiteral("punic_line"),
              Troop::Swordsman,
              2,
              9,
              {-13.6F, 0.0F, 16.0F},
              8,
              {3.4F, 0.0F, 0.0F}),
        group(QStringLiteral("punic_spears"),
              Troop::Spearman,
              2,
              7,
              {-10.2F, 0.0F, 20.5F},
              8,
              {3.4F, 0.0F, 0.0F}),
        group(QStringLiteral("punic_horse"),
              Troop::MountedSwordsman,
              2,
              4,
              {-24.0F, 0.0F, 18.0F},
              4,
              {3.6F, 0.0F, 0.0F}),
    };

    s.resource_patches = {
        {QStringLiteral("pine"), 6, {-34.0F, 0.0F, -30.0F}, {4.0F, 0.0F, 2.5F}, 1.3F},
        {QStringLiteral("pine"), 5, {24.0F, 0.0F, -32.0F}, {4.2F, 0.0F, 2.0F}, 1.2F},
        {QStringLiteral("pine"), 5, {-30.0F, 0.0F, 30.0F}, {4.2F, 0.0F, 2.0F}, 1.25F},
        {QStringLiteral("boulder"), 3, {30.0F, 0.0F, 8.0F}, {3.4F, 0.0F, 2.0F}, 1.1F},
        {QStringLiteral("boulder"), 2, {-31.0F, 0.0F, -6.0F}, {3.6F, 0.0F, 2.0F}, 1.0F},
    };

    for (auto const& held : {QStringLiteral("roman_line"),
                             QStringLiteral("roman_spears"),
                             QStringLiteral("roman_horse"),
                             QStringLiteral("punic_line"),
                             QStringLiteral("punic_spears"),
                             QStringLiteral("punic_horse")}) {
      s.steps.push_back(at(0.2F, Command::Hold, held));
    }
    s.steps.push_back(at(
        0.6F, Command::Attack, QStringLiteral("scipio"), QStringLiteral("hannibal")));
    s.steps.push_back(at(
        0.6F, Command::Attack, QStringLiteral("hannibal"), QStringLiteral("scipio")));

    s.expectations = {
        expectation(Expect::GroupExists, QStringLiteral("scipio")),
        expectation(Expect::GroupDestroyed, QStringLiteral("hannibal")),
        expectation(Expect::GroupIsRendered, QStringLiteral("scipio")),
        expectation(Expect::GroupIsRendered, QStringLiteral("hannibal")),
        expectation(Expect::GroupIsRendered, QStringLiteral("roman_line")),
        expectation(Expect::GroupIsRendered, QStringLiteral("punic_line")),
        expectation(Expect::AttackHasVisibleContact,
                    QStringLiteral("scipio"),
                    QStringLiteral("hannibal")),
        expectation(Expect::HitReactionObserved, QStringLiteral("hannibal")),
    };
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_promo_commander_rally_id),
        QStringLiteral("Promo: The Consul Steps In"),
        QStringLiteral("Afternoon capture scene. A thinned Roman line is being "
                       "rolled up by twice its number until the consul signals "
                       "from his rise, calls the Consular Assault, and the "
                       "legion turns the field with the horse coming round the "
                       "flank."),
        50.0F,
        {30.0F, 26.0F, 0.0F});
    s.camera_focus = QVector3D(0.0F, 1.0F, 0.0F);
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.force_full_creature_lod = true;
    s.suppress_combat_dust = true;
    s.graphics_quality = Render::GraphicsQuality::Ultra;
    s.arena_floor_half_extent = 34.0F;
    s.terrain_height_scale_override = 2.6F;

    s.terrain_seed_override = 2024;
    s.suppress_boundary_mountains = true;
    {
      constexpr int k_ring_mounds = 14;
      constexpr float k_ring_radius = 47.0F;
      for (int i = 0; i < k_ring_mounds; ++i) {
        float const angle = 2.0F * std::numbers::pi_v<float> * static_cast<float>(i) /
                            static_cast<float>(k_ring_mounds);
        float const wobble = (0.6F * std::sin(static_cast<float>(i) * 2.3F)) +
                             (0.4F * std::sin(static_cast<float>(i) * 0.9F));
        s.elevation_patches.push_back(
            {{k_ring_radius * std::sin(angle), 0.0F, k_ring_radius * std::cos(angle)},
             26.0F,
             10.5F + (2.2F * wobble)});
      }
    }
    s.environment.start_time = 15.8F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;
    s.environment.fog_density_override = 0.006F;
    s.environment.exposure_override = 1.05F;

    s.elevation_patches.push_back({{0.0F, 0.0F, -17.5F}, 11.0F, 6.0F, 3.5F});

    auto consul = group(QStringLiteral("roman_consul"),
                        Troop::RomanVeteranConsul,
                        1,
                        1,
                        {0.0F, 0.0F, -16.0F},
                        1);
    consul.health_override = consul.max_health_override = 12000;

    consul.showcase_routine = {QStringLiteral("sword_flourish:2.6:1.4")};
    consul.showcase_start_delay = 16.3F;
    consul.showcase_loop = false;

    auto roman_line = group(QStringLiteral("roman_line"),
                            Troop::Swordsman,
                            1,
                            11,
                            {-11.9F, 0.0F, -6.0F},
                            12,
                            {3.4F, 0.0F, 0.0F});
    auto roman_spears = group(QStringLiteral("roman_spears"),
                              Troop::Spearman,
                              1,
                              8,
                              {-8.5F, 0.0F, -10.5F},
                              12,
                              {3.4F, 0.0F, 0.0F});

    roman_line.max_health_override = 3200;
    roman_line.health_override = 1400;
    roman_spears.max_health_override = 3200;
    roman_spears.health_override = 1500;

    auto roman_horse = group(QStringLiteral("roman_horse"),
                             Troop::MountedSwordsman,
                             1,
                             14,
                             {30.0F, 0.0F, -16.0F},
                             6,
                             {3.6F, 0.0F, 0.0F});
    roman_horse.max_health_override = roman_horse.health_override = 2400;

    auto punic_horde = group(QStringLiteral("punic_horde"),
                             Troop::Swordsman,
                             2,
                             14,
                             {-15.3F, 0.0F, 7.0F},
                             16,
                             {3.4F, 0.0F, 0.0F});
    auto punic_spears = group(QStringLiteral("punic_spears"),
                              Troop::Spearman,
                              2,
                              10,
                              {-10.2F, 0.0F, 11.5F},
                              16,
                              {3.4F, 0.0F, 0.0F});
    auto punic_horse = group(QStringLiteral("punic_horse"),
                             Troop::MountedSwordsman,
                             2,
                             7,
                             {-26.0F, 0.0F, 12.0F},
                             6,
                             {3.6F, 0.0F, 0.0F});
    punic_horde.max_health_override = punic_horde.health_override = 900;
    punic_spears.max_health_override = punic_spears.health_override = 900;
    punic_horse.max_health_override = punic_horse.health_override = 800;

    s.groups = {consul,
                roman_line,
                roman_spears,
                roman_horse,
                punic_horde,
                punic_spears,
                punic_horse};

    s.resource_patches = {
        {QStringLiteral("pine"), 6, {-36.0F, 0.0F, -30.0F}, {4.0F, 0.0F, 2.5F}, 1.3F},
        {QStringLiteral("pine"), 5, {-34.0F, 0.0F, 30.0F}, {4.2F, 0.0F, 2.0F}, 1.25F},
        {QStringLiteral("pine"), 4, {8.0F, 0.0F, 34.0F}, {4.2F, 0.0F, 2.0F}, 1.2F},
        {QStringLiteral("pine"), 3, {5.0F, 0.0F, -28.0F}, {4.6F, 0.0F, 1.6F}, 1.25F},
        {QStringLiteral("boulder"),
         3,
         {-9.0F, 0.0F, -25.0F},
         {3.2F, 0.0F, 1.4F},
         1.15F},
        {QStringLiteral("boulder"), 3, {-30.0F, 0.0F, 24.0F}, {3.4F, 0.0F, 2.0F}, 1.1F},
        {QStringLiteral("boulder"),
         2,
         {-34.0F, 0.0F, -10.0F},
         {3.6F, 0.0F, 2.0F},
         1.0F},
    };

    auto consul_hold = at(0.2F, Command::Hold, QStringLiteral("roman_consul"));

    auto rally =
        at(17.4F, Command::TriggerCommanderAura, QStringLiteral("roman_consul"));
    rally.value = 18;

    auto first_blood = at(6.0F, Command::ApplyDamage, QStringLiteral("roman_line"));
    first_blood.value = 110;
    auto second_blood = at(10.5F, Command::ApplyDamage, QStringLiteral("roman_spears"));
    second_blood.value = 130;
    auto third_blood = at(14.0F, Command::ApplyDamage, QStringLiteral("roman_line"));
    third_blood.value = 120;

    s.steps = {
        at(0.2F, Command::Hold, QStringLiteral("roman_line")),
        at(0.2F, Command::Hold, QStringLiteral("roman_spears")),
        at(0.2F, Command::Hold, QStringLiteral("roman_horse")),
        at(0.4F,
           Command::AttackMove,
           QStringLiteral("punic_horde"),
           QStringLiteral("roman_line")),
        at(0.6F,
           Command::AttackMove,
           QStringLiteral("punic_spears"),
           QStringLiteral("roman_spears")),
        consul_hold,
        at(0.2F, Command::Hold, QStringLiteral("roman_horse")),
        first_blood,
        at(6.5F,
           Command::Charge,
           QStringLiteral("punic_horse"),
           QStringLiteral("roman_spears")),
        second_blood,
        third_blood,
        rally,

        at(19.7F,
           Command::Charge,
           QStringLiteral("roman_horse"),
           QStringLiteral("punic_horde")),
        at(19.9F,
           Command::AttackMove,
           QStringLiteral("roman_line"),
           QStringLiteral("punic_horde")),
        at(20.0F,
           Command::AttackMove,
           QStringLiteral("roman_spears"),
           QStringLiteral("punic_spears")),
    };

    s.expectations = {
        expectation(Expect::GroupExists, QStringLiteral("roman_consul")),
        expectation(Expect::GroupIsRendered, QStringLiteral("roman_line")),
        expectation(Expect::GroupIsRendered, QStringLiteral("punic_horde")),
        expectation(Expect::GroupIsRendered, QStringLiteral("roman_consul")),
        expectation(Expect::GroupIsRendered, QStringLiteral("roman_horse")),
        expectation(Expect::CommanderAuraActivated, QStringLiteral("roman_consul")),

        expectation(Expect::AttackAnimationObserved, QStringLiteral("punic_horde")),
        expectation(Expect::DeathAnimationObserved, QStringLiteral("punic_horde")),
    };
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_promo_wolf_attack_id),
        QStringLiteral("Promo: Wolves on the Fold"),
        QStringLiteral("Late-afternoon capture scene. A pack comes out of the east "
                       "at villagers working the ground outside their houses, and "
                       "the riders of the watch come down the street to answer it."),
        44.0F,
        {30.0F, 26.0F, 20.0F});
    s.camera_focus = QVector3D(0.0F, 1.0F, 0.0F);
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.force_full_creature_lod = true;
    s.graphics_quality = Render::GraphicsQuality::Ultra;
    s.arena_floor_half_extent = 32.0F;
    s.terrain_height_scale_override = 2.6F;

    s.suppress_boundary_mountains = true;
    s.suppress_combat_dust = true;
    {

      constexpr int k_ring_mounds = 26;
      for (int i = 0; i < k_ring_mounds; ++i) {
        float const fi = static_cast<float>(i);
        float const angle =
            2.0F * std::numbers::pi_v<float> * fi / static_cast<float>(k_ring_mounds);
        float const wobble = std::sin(fi * 1.7F);
        float const wobble2 = std::sin((fi * 0.9F) + 1.3F);
        float const ring_radius = 44.0F + (2.0F * wobble2);
        float const mound_radius = 14.0F + (2.0F * wobble);
        float const height = 12.5F + (4.0F * wobble2);
        s.elevation_patches.push_back(
            {{ring_radius * std::sin(angle), 0.0F, ring_radius * std::cos(angle)},
             mound_radius,
             height});
      }
      for (int i = 0; i < k_ring_mounds; ++i) {
        float const fi = static_cast<float>(i) + 0.5F;
        float const angle =
            2.0F * std::numbers::pi_v<float> * fi / static_cast<float>(k_ring_mounds);
        float const wobble = std::sin((fi * 2.3F) + 0.7F);
        s.elevation_patches.push_back({{(48.0F + (2.0F * wobble)) * std::sin(angle),
                                        0.0F,
                                        (48.0F + (2.0F * wobble)) * std::cos(angle)},
                                       17.0F,
                                       15.0F + (3.0F * wobble)});
      }
    }

    s.environment.start_time = 15.2F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;
    s.environment.fog_density_override = 0.016F;
    s.environment.exposure_override = 1.5F;

    Game::Wildlife::WildlifeSettings wildlife = Game::Wildlife::default_settings();
    wildlife.enabled = true;
    wildlife.seed = 31337U;
    wildlife.sheep.enabled = false;
    wildlife.sheep.group_count = 0;
    wildlife.birds.enabled = false;
    wildlife.birds.group_count = 0;
    wildlife.wolves.enabled = true;
    wildlife.wolves.group_count = 1;
    wildlife.wolves.group_size_min = 8;
    wildlife.wolves.group_size_max = 8;
    wildlife.wolves.aggression = 1.0F;
    wildlife.wolves.alert_radius = 9.0F;
    wildlife.wolves.roam_radius = 30.0F;
    wildlife.wolves.respawn = false;
    wildlife.wolves.spawn_areas = {{34.0F, 0.0F, 7.0F}};
    s.wildlife = wildlife;

    s.roads = {
        street({-27.0F, 0.0F, -3.0F}, {18.0F, 0.0F, -3.0F}, 3.2F, "default"),
        street({-13.0F, 0.0F, -3.0F}, {-13.0F, 0.0F, 9.0F}, 2.4F, "default"),
    };

    s.resource_patches = {
        patch("fire_camp", 1, {-16.0F, 0.0F, 4.0F}, {}, 0.9F),
        patch("tent", 2, {-21.0F, 0.0F, 6.0F}, {4.5F, 0.0F, 0.0F}, 0.8F),
        patch("supply_cart", 2, {-9.0F, 0.0F, 6.0F}, {3.6F, 0.0F, 0.0F}, 0.95F),
        patch("olive_tree", 5, {-24.0F, 0.0F, -13.0F}, {0.0F, 0.0F, 4.5F}, 1.1F),
        patch("pine", 6, {30.0F, 0.0F, 16.0F}, {3.4F, 0.0F, 2.5F}, 1.2F),
        patch("pine", 4, {26.0F, 0.0F, -18.0F}, {3.6F, 0.0F, 2.0F}, 1.15F),
        patch("plant", 6, {6.0F, 0.0F, -12.0F}, {3.0F, 0.0F, 0.0F}, 0.9F),
    };

    auto villagers = residents(QStringLiteral("villagers"),
                               Nation::RomanRepublic,
                               1,
                               6,
                               {0.0F, 0.0F, 5.0F},
                               {2.8F, 0.0F, 0.0F},
                               3.0F);

    villagers.health_override = villagers.max_health_override = 120;

    auto riders = group(QStringLiteral("riders"),
                        Troop::MountedSwordsman,
                        1,
                        5,
                        {-22.0F, 0.0F, -3.0F},
                        1,
                        {2.8F, 0.0F, 0.0F});

    s.groups = {
        building(QStringLiteral("village_homes"),
                 Game::Units::SpawnType::Home,
                 Nation::RomanRepublic,
                 1,
                 3,
                 {-19.0F, 0.0F, -10.0F},
                 {6.0F, 0.0F, 0.0F}),
        building(QStringLiteral("village_market"),
                 Game::Units::SpawnType::Marketplace,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {-13.0F, 0.0F, 8.0F},
                 {},
                 180.0F),
        building(QStringLiteral("village_barracks"),
                 Game::Units::SpawnType::Barracks,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {-22.0F, 0.0F, 2.0F},
                 {},
                 90.0F),
        villagers,
        riders,
    };

    auto rescue = at(8.8F, Command::Run, QStringLiteral("riders"));
    rescue.destination = {-0.5F, 0.0F, 9.8F};

    auto sweep = at(20.0F, Command::Run, QStringLiteral("riders"));
    sweep.destination = {8.0F, 0.0F, 8.0F};

    auto picket = at(28.0F, Command::Run, QStringLiteral("riders"));
    picket.destination = {-1.0F, 0.0F, 6.0F};

    s.steps = {
        at(0.2F, Command::Hold, QStringLiteral("riders")),
        at(8.4F, Command::Stop, QStringLiteral("riders")),
        rescue,
        at(19.6F, Command::Stop, QStringLiteral("riders")),
        sweep,
        at(27.6F, Command::Stop, QStringLiteral("riders")),
        picket,
        at(34.0F, Command::Guard, QStringLiteral("riders")),
    };

    s.expectations = {
        expectation(Expect::GroupExists, QStringLiteral("villagers")),
        expectation(Expect::GroupIsRendered, QStringLiteral("villagers")),
        expectation(Expect::MovementAnimationObserved, QStringLiteral("villagers")),
        expectation(Expect::GroupIsRendered, QStringLiteral("riders")),
        expectation(Expect::DeathAnimationObserved, QStringLiteral("villagers")),
        expectation(Expect::WildlifeHuntObserved),
        expectation(Expect::GroupHealthReduced, QStringLiteral("villagers")),
        expectation(Expect::WildlifeCasualtyObserved),
    };
    result.push_back(std::move(s));
  }

  return result;
}

} // namespace Arena::Scenarios
