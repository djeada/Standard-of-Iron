#include "arena_showcase_scenarios.h"

#include <utility>

#include "arena_scenarios.h"

namespace Arena::Scenarios {
namespace {

using Command = ScenarioCommandKind;
using Expect = ArenaExpectationKind;
using Nation = Game::Systems::NationID;
using Trigger = ScenarioTriggerKind;
using Troop = Game::Units::TroopType;

auto performer(QString name,
               QString renderer,
               Troop troop,
               QVector3D origin,
               float facing,
               QStringList routine,
               float start_delay) -> ArenaScenarioGroup {
  ArenaScenarioGroup group;
  group.name = std::move(name);
  group.troop_type = troop;
  group.nation_id = Nation::RomanRepublic;
  group.owner_id = 1;
  group.count = 1;
  group.individuals_per_unit = 1;
  group.origin = origin;
  group.spacing = {0.0F, 0.0F, 0.0F};
  group.facing_degrees = facing;
  group.renderer_override = std::move(renderer);
  group.showcase_routine = std::move(routine);
  group.showcase_start_delay = start_delay;
  group.showcase_loop = true;
  group.render_scale_override = 1.0F;
  return group;
}

auto step_at(float time,
             Command command,
             QString source,
             QVector3D destination = {}) -> ArenaScenarioStep {
  ArenaScenarioStep step;
  step.name = QStringLiteral("%1_%2").arg(QString::number(time, 'f', 2), source);
  step.trigger = {Trigger::AtTime, time, {}, {}, 0.0F};
  step.command = command;
  step.group = std::move(source);
  step.destination = destination;
  return step;
}

auto expect(Expect kind,
            QString source = {},
            float threshold = 0.0F,
            float start = 0.0F) -> ArenaExpectation {
  ArenaExpectation result;
  result.kind = kind;
  result.group = std::move(source);
  result.threshold = threshold;
  result.start_seconds = start;
  return result;
}

auto prop(QString type,
          QVector3D origin,
          float scale,
          int count = 1) -> ArenaScenarioResourcePatch {
  ArenaScenarioResourcePatch patch;
  patch.prop_type = std::move(type);
  patch.count = count;
  patch.origin = origin;
  patch.spacing = {2.4F, 0.0F, 0.0F};
  patch.scale = scale;
  return patch;
}

} // namespace

auto build_showcase_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> out;

  ArenaScenarioDefinition s;
  s.id = QStringLiteral("promo_humanoid_showcase");
  s.label = QStringLiteral("Promo: The Humanoid");
  s.description =
      QStringLiteral("One lightly armoured, bare-headed fighter runs an acrobatic "
                     "routine while a blade master cuts air and a lancer throws at a "
                     "post, so every authored humanoid move can be filmed in one "
                     "deterministic pass.");
  s.duration_seconds = 82.0F;
  s.camera = {14.0F, 22.0F, 26.0F};
  s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
  s.arena_floor_half_extent = 34.0F;
  s.suppress_terrain_scatter = false;
  s.suppress_spawn_anchor = true;
  s.suppress_ui_overlays = true;
  s.select_spawned_units = false;
  s.force_full_creature_lod = true;
  s.collect_animation_diagnostics = false;
  s.graphics_quality = Render::GraphicsQuality::Ultra;
  s.environment.start_time = 10.5F;
  s.environment.time_mode = Game::Map::TimeMode::Locked;

  const QStringList acrobatics{QStringLiteral("jump:1.6:0.5"),
                               QStringLiteral("front_flip:1.7:0.7"),
                               QStringLiteral("side_aerial:1.9:0.7"),
                               QStringLiteral("handstand:3.4:0.8")};
  const QStringList blade{QStringLiteral("sword_flourish:2.6:0.6")};
  const QStringList lance{QStringLiteral("spear_throw:2.2:1.4")};

  s.groups = {
      performer(QStringLiteral("acrobat"),
                QStringLiteral("troops/roman/showcase_athlete"),
                Troop::Swordsman,
                {0.0F, 0.0F, -2.0F},
                0.0F,
                acrobatics,
                1.0F),
      performer(QStringLiteral("blademaster"),
                QStringLiteral("troops/roman/showcase_blademaster"),
                Troop::Swordsman,
                {9.5F, 0.0F, 1.0F},
                -28.0F,
                blade,
                0.6F),
      [&] {
        auto lancer = performer(QStringLiteral("lancer"),
                                QStringLiteral("troops/roman/showcase_lancer"),
                                Troop::Spearman,
                                {-10.0F, 0.0F, -9.0F},
                                0.0F,
                                lance,
                                1.2F);
        lancer.showcase_throw_target = QVector3D(-10.0F, 0.0F, -1.0F);
        lancer.showcase_released_renderer =
            QStringLiteral("troops/roman/showcase_athlete");
        return lancer;
      }(),
      performer(QStringLiteral("runner"),
                QStringLiteral("troops/roman/showcase_athlete"),
                Troop::Swordsman,
                {-16.0F, 0.0F, 8.0F},
                90.0F,
                {},
                0.0F),
  };

  s.resource_patches = {
      prop(QStringLiteral("statue"), {-10.0F, 0.0F, -1.0F}, 0.85F),
      prop(QStringLiteral("weapon_rack"), {13.5F, 0.0F, -4.0F}, 1.0F),
      prop(QStringLiteral("olive_tree"), {-19.0F, 0.0F, -16.0F}, 1.1F),
      prop(QStringLiteral("olive_tree"), {17.0F, 0.0F, -18.0F}, 1.0F),
      prop(QStringLiteral("olive_tree"), {24.0F, 0.0F, 4.0F}, 1.05F),
      prop(QStringLiteral("pine_tree"), {-25.0F, 0.0F, 6.0F}, 1.0F),
      prop(QStringLiteral("pine_tree"), {-22.0F, 0.0F, 18.0F}, 0.95F),
      prop(QStringLiteral("tent"), {19.0F, 0.0F, -8.0F}, 1.0F),
      prop(QStringLiteral("firecamp"), {15.0F, 0.0F, -10.0F}, 1.0F),
  };

  s.steps = {
      step_at(1.0F, Command::Move, QStringLiteral("runner"), {14.0F, 0.0F, 8.0F}),
      step_at(16.0F, Command::Run, QStringLiteral("runner"), {-16.0F, 0.0F, 8.0F}),
      step_at(30.0F, Command::Move, QStringLiteral("runner"), {14.0F, 0.0F, 8.0F}),
      step_at(46.0F, Command::Run, QStringLiteral("runner"), {-16.0F, 0.0F, 8.0F}),
      step_at(60.0F, Command::Move, QStringLiteral("runner"), {14.0F, 0.0F, 8.0F}),
  };

  s.expectations = {
      expect(Expect::GroupIsRendered, QStringLiteral("acrobat")),
      expect(Expect::GroupIsRendered, QStringLiteral("blademaster")),
      expect(Expect::GroupIsRendered, QStringLiteral("lancer")),
      expect(Expect::MovementAnimationObserved, QStringLiteral("runner")),
      expect(Expect::FrameBudget, {}, 33.34F, 0.5F),
  };

  out.push_back(std::move(s));
  return out;
}

} // namespace Arena::Scenarios
