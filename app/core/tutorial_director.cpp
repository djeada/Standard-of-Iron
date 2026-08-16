#include "tutorial_director.h"

#include <QVariantMap>

#include <algorithm>
#include <array>

#include "game/systems/construction_cost_catalog.h"
#include "game/systems/resource_types.h"

namespace App::Core {

namespace {

constexpr float k_step_complete_hold_seconds = 4.0F;

constexpr std::array k_steps = {
    TutorialStepId::SelectTroops,
    TutorialStepId::MoveTroops,
    TutorialStepId::AttackScouts,
    TutorialStepId::GatherWood,
    TutorialStepId::GatherStoneAndIron,
    TutorialStepId::BuildHome,
    TutorialStepId::RecruitSoldier,
    TutorialStepId::AssembleArmy,
    TutorialStepId::DefendCamp,
    TutorialStepId::Stances,
    TutorialStepId::Commander,
    TutorialStepId::Camera,
    TutorialStepId::GameSpeed,
    TutorialStepId::Objectives,
    TutorialStepId::Assault,
};

auto home_cost() -> Game::Systems::ResourceAmounts {
  return Game::Systems::construction_cost_info("home").resource_costs;
}

auto ratio(int done, int target) -> qreal {
  if (target <= 0) {
    return 1.0;
  }
  return std::clamp(static_cast<qreal>(done) / static_cast<qreal>(target), 0.0, 1.0);
}

auto count_text(int done, int target) -> QString {
  return QStringLiteral("%1 / %2").arg(std::min(done, target)).arg(target);
}

} // namespace

TutorialDirector::TutorialDirector(QObject* parent)
    : QObject(parent)
    , m_done(k_steps.size(), false) {
}

auto TutorialDirector::step_count() -> int {
  return static_cast<int>(k_steps.size());
}

auto TutorialDirector::step() const -> TutorialStepId {
  return k_steps[std::min(m_index, k_steps.size() - 1)];
}

auto TutorialDirector::step_id() const -> QString {
  return step_id_name(step());
}

auto TutorialDirector::title() const -> QString {
  return step_title(step());
}

auto TutorialDirector::body() const -> QString {
  return step_body(step());
}

auto TutorialDirector::objective() const -> QString {
  return step_objective(step());
}

auto TutorialDirector::objective_state() const -> QString {
  return m_step_complete ? QStringLiteral("complete") : QStringLiteral("active");
}

auto TutorialDirector::holds_mission_clock() const -> bool {
  if (!m_active || m_finished) {
    return false;
  }
  return m_index <
         static_cast<std::size_t>(std::distance(
             k_steps.begin(),
             std::find(k_steps.begin(), k_steps.end(), TutorialStepId::DefendCamp)));
}

auto TutorialDirector::steps() const -> QVariantList {
  QVariantList list;
  for (std::size_t i = 0; i < k_steps.size(); ++i) {
    QVariantMap entry;
    entry["id"] = step_id_name(k_steps[i]);
    entry["title"] = step_title(k_steps[i]);
    entry["objective"] = step_objective(k_steps[i]);
    QString state = QStringLiteral("pending");
    if (m_done[i]) {
      state = QStringLiteral("complete");
    } else if (m_active && i == m_index) {
      state = m_step_complete ? QStringLiteral("complete") : QStringLiteral("active");
    }
    entry["state"] = state;
    entry["current"] = m_active && i == m_index;
    list.append(entry);
  }
  return list;
}

void TutorialDirector::begin() {
  m_active = true;
  m_finished = false;
  m_visible = true;
  m_objectives_opened = false;
  std::fill(m_done.begin(), m_done.end(), false);
  enter_step(0, false);
  emit state_changed();
}

void TutorialDirector::end() {
  if (!m_active && !m_finished) {
    return;
  }
  m_active = false;
  m_finished = false;
  m_step_complete = false;
  m_index = 0;
  std::fill(m_done.begin(), m_done.end(), false);
  publish(-1.0, {}, {});
  emit state_changed();
  emit step_changed();
}

void TutorialDirector::stop() {
  if (!m_active) {
    return;
  }
  m_active = false;
  m_step_complete = false;
  publish(-1.0, {}, {});
  emit state_changed();
}

void TutorialDirector::start() {
  emit start_requested();
}

void TutorialDirector::restart() {
  begin();
}

void TutorialDirector::set_visible(bool visible) {
  if (m_visible == visible) {
    return;
  }
  m_visible = visible;
  emit state_changed();
}

void TutorialDirector::note_objectives_opened() {
  m_objectives_opened = true;
}

void TutorialDirector::enter_step(std::size_t index, bool from_replay) {
  m_index = std::min(index, k_steps.size() - 1);
  m_step_complete = false;
  m_complete_timer = 0.0F;
  m_baseline = Baseline{};
  if (from_replay) {
    m_done[m_index] = false;
  }
  if (step() == TutorialStepId::Objectives) {
    m_objectives_opened = false;
  }
  m_progress = -1.0;
  m_progress_text.clear();
  m_hint.clear();
  emit step_changed();
  emit state_changed();
}

void TutorialDirector::mark_step_complete() {
  if (m_step_complete) {
    return;
  }
  m_step_complete = true;
  m_complete_timer = 0.0F;
  m_done[m_index] = true;
  emit step_completed(static_cast<int>(m_index));
  emit state_changed();
}

void TutorialDirector::go_to_next_step() {
  if (m_index + 1 >= k_steps.size()) {
    m_done[m_index] = true;
    m_finished = true;
    m_active = false;
    m_step_complete = true;
    emit tutorial_finished();
    emit state_changed();
    return;
  }
  enter_step(m_index + 1, false);
}

void TutorialDirector::skip_step() {
  if (!m_active) {
    return;
  }
  m_done[m_index] = true;
  go_to_next_step();
}

void TutorialDirector::replay_step() {
  if (!m_active) {
    return;
  }
  enter_step(m_index, true);
}

void TutorialDirector::continue_step() {
  if (!m_active || !m_step_complete) {
    return;
  }
  go_to_next_step();
}

void TutorialDirector::publish(qreal progress,
                               const QString& progress_text,
                               const QString& hint) {
  const bool changed = !qFuzzyCompare(1.0 + m_progress, 1.0 + progress) ||
                       m_progress_text != progress_text || m_hint != hint;
  m_progress = progress;
  m_progress_text = progress_text;
  m_hint = hint;
  if (changed) {
    emit state_changed();
  }
}

void TutorialDirector::advance(const TutorialObservation& observation, float real_dt) {
  if (!m_active || !observation.mission_running) {
    return;
  }
  if (observation.defeat) {
    stop();
    return;
  }

  if (!m_baseline.captured) {
    m_baseline.captured = true;
    m_baseline.enemy_troops_defeated = observation.enemy_troops_defeated;
    m_baseline.harvested_wood = observation.harvested_wood;
    m_baseline.harvested_stone = observation.harvested_stone;
    m_baseline.harvested_iron = observation.harvested_iron;
    m_baseline.home_count = observation.home_count;
    m_baseline.soldier_count = observation.soldier_count;
    m_baseline.waves_cleared = observation.waves_cleared;
  }

  if (m_step_complete) {
    m_complete_timer += std::max(0.0F, real_dt);
    if (m_complete_timer >= k_step_complete_hold_seconds) {
      go_to_next_step();
    }
    return;
  }

  qreal progress = -1.0;
  QString progress_text;
  const bool completed = evaluate(observation, progress, progress_text);
  if (completed) {
    publish(1.0, progress_text, {});
    mark_step_complete();
    if (step() == TutorialStepId::Assault) {

      go_to_next_step();
    }
    return;
  }
  publish(progress, progress_text, hint_for(observation));
}

auto TutorialDirector::evaluate(const TutorialObservation& o,
                                qreal& progress,
                                QString& progress_text) const -> bool {
  switch (step()) {
  case TutorialStepId::SelectTroops:
    return o.selected_troop_count > 0;

  case TutorialStepId::MoveTroops:
    return o.move_order_accepted;

  case TutorialStepId::AttackScouts: {
    const int killed = o.enemy_troops_defeated - m_baseline.enemy_troops_defeated;
    progress = ratio(killed, k_tutorial_scout_count);
    progress_text = count_text(killed, k_tutorial_scout_count);
    return killed >= k_tutorial_scout_count;
  }

  case TutorialStepId::GatherWood: {
    const int wood = o.harvested_wood - m_baseline.harvested_wood;
    progress = ratio(wood, k_tutorial_wood_target);
    progress_text = count_text(wood, k_tutorial_wood_target);
    return wood >= k_tutorial_wood_target;
  }

  case TutorialStepId::GatherStoneAndIron: {
    const int stone = o.harvested_stone - m_baseline.harvested_stone;
    const int iron = o.harvested_iron - m_baseline.harvested_iron;
    progress =
        (ratio(stone, k_tutorial_stone_target) + ratio(iron, k_tutorial_iron_target)) *
        0.5;
    progress_text = tr("stone %1 / %2 · iron %3 / %4")
                        .arg(std::min(stone, k_tutorial_stone_target))
                        .arg(k_tutorial_stone_target)
                        .arg(std::min(iron, k_tutorial_iron_target))
                        .arg(k_tutorial_iron_target);
    return stone >= k_tutorial_stone_target && iron >= k_tutorial_iron_target;
  }

  case TutorialStepId::BuildHome:
    progress = o.construction_preview_active ? 0.5 : 0.0;
    progress_text.clear();
    return o.home_count > m_baseline.home_count;

  case TutorialStepId::RecruitSoldier:
    progress = o.production_in_progress ? 0.5 : 0.0;
    progress_text.clear();
    return o.soldier_count > m_baseline.soldier_count;

  case TutorialStepId::AssembleArmy:
    progress = ratio(o.soldier_count, k_tutorial_army_size);
    progress_text = count_text(o.soldier_count, k_tutorial_army_size);
    return o.soldier_count >= k_tutorial_army_size;

  case TutorialStepId::DefendCamp:
    progress = o.wave_live ? 0.5 : (o.wave_pending ? 0.1 : 0.0);
    progress_text.clear();
    return o.waves_cleared > m_baseline.waves_cleared;

  case TutorialStepId::Stances:
    return o.hold_order_accepted || o.guard_order_accepted || o.patrol_order_accepted;

  case TutorialStepId::Commander:
    return o.aura_active;

  case TutorialStepId::Camera:
    return o.camera_used;

  case TutorialStepId::GameSpeed:
    return o.speed_changed;

  case TutorialStepId::Objectives:
    return m_objectives_opened || o.objectives_opened;

  case TutorialStepId::Assault:
    return o.victory;
  }
  return false;
}

auto TutorialDirector::hint_for(const TutorialObservation& o) const -> QString {
  const auto no_troops = [&]() -> QString {
    if (o.selected_troop_count > 0) {
      return {};
    }
    if (o.selected_builder_count > 0) {
      return tr("Builders cannot fight. This order needs soldiers - select the "
                "spearmen, archers or swordsmen near your camp.");
    }
    if (o.selected_building_count > 0) {
      return tr("That is a building. Orders in this step need soldiers - left-click "
                "one of the troops standing near your camp.");
    }
    return tr("Nothing is selected, so there is nobody to receive the order. "
              "Left-click a soldier or drag a box around several.");
  };

  const auto no_builder = [&]() -> QString {
    if (o.selected_builder_count > 0) {
      return {};
    }
    if (o.selected_troop_count > 0 || o.selected_building_count > 0) {
      return tr("Collect and Build are only available to builders - soldiers and "
                "buildings cannot do this work. Select a builder (the worker with "
                "the hammer) first.");
    }
    return tr("No builder is selected. Left-click one of the builders standing by "
              "your barracks.");
  };

  switch (step()) {
  case TutorialStepId::SelectTroops:
    if (o.selected_building_count > 0 && o.selected_troop_count == 0) {
      return tr("That is a building. Left-click one of the soldiers standing near "
                "your camp instead.");
    }
    return {};

  case TutorialStepId::MoveTroops:
    if (!o.last_rejection_reason.isEmpty()) {
      return o.last_rejection_reason;
    }
    return no_troops();

  case TutorialStepId::AttackScouts:
    if (!o.last_rejection_reason.isEmpty()) {
      return o.last_rejection_reason;
    }
    return no_troops();

  case TutorialStepId::GatherWood:
  case TutorialStepId::GatherStoneAndIron:
    if (!o.last_rejection_reason.isEmpty()) {
      return o.last_rejection_reason;
    }
    return no_builder();

  case TutorialStepId::BuildHome: {
    if (const QString reason = no_builder(); !reason.isEmpty()) {
      return reason;
    }
    const auto cost = home_cost();
    const int wood_cost = cost.get(Game::Systems::ResourceType::Wood);
    const int stone_cost = cost.get(Game::Systems::ResourceType::Stone);
    if (o.wood < wood_cost || o.stone < stone_cost) {
      return tr("A Home costs %1 wood and %2 stone; you have %3 wood and %4 stone. "
                "The Build card stays grey until the yard holds enough - send a "
                "builder to collect the difference.")
          .arg(wood_cost)
          .arg(stone_cost)
          .arg(o.wood)
          .arg(o.stone);
    }
    if (o.construction_preview_active && !o.construction_preview_valid) {
      return tr("A red outline means the site is blocked: too close to another "
                "building, on water or on a slope. Move it onto flat, open ground "
                "and left-click to confirm. Right-click cancels.");
    }
    if (!o.last_rejection_reason.isEmpty()) {
      return o.last_rejection_reason;
    }
    return {};
  }

  case TutorialStepId::RecruitSoldier:
  case TutorialStepId::AssembleArmy: {
    if (o.selected_barracks_count == 0) {
      return tr("Recruits come from the barracks. Left-click your barracks to open "
                "its production panel on the right.");
    }
    if (o.barracks_manpower < 50) {
      return tr("The barracks has only %1 population left to draw on. Every recruit "
                "costs population; when it runs dry, build Homes - each Home raises "
                "families, and a civilian recruited there and sent to the barracks "
                "with Deliver refills it.")
          .arg(o.barracks_manpower);
    }
    if (o.wood < 10 || o.iron < 4) {
      return tr("A recruit card turns grey when a resource is short: soldiers need "
                "wood for shafts and iron for blades. Send a builder to collect "
                "more.");
    }
    return {};
  }

  case TutorialStepId::DefendCamp:
    if (o.wave_live) {
      return tr("The raiders are here. Keep your soldiers together near the "
                "barracks: spearmen in front, archers behind, and the commander "
                "close so his aura reaches them.");
    }
    return tr("The Roman raid is on its way. The wave tracker under the top bar "
              "counts it down and the minimap marks where it will enter.");

  case TutorialStepId::Stances:
    return no_troops();

  case TutorialStepId::Commander:
    if (!o.commander_selected) {
      return tr("The Aura command only appears when your commander is selected. "
                "He is the standard-bearer with the crown badge, near the "
                "barracks.");
    }
    if (!o.aura_ready) {
      return tr("The aura is recharging or the commander is wounded; the button "
                "lights up again when it is ready.");
    }
    return {};

  case TutorialStepId::Assault:
    if (o.enemy_commanders_alive > 0) {
      return tr("The Roman commander is still alive. A nation dies with the man "
                "who leads it - kill him and the camp falls.");
    }
    return {};

  case TutorialStepId::Camera:
  case TutorialStepId::GameSpeed:
  case TutorialStepId::Objectives:
    return {};
  }
  return {};
}

auto TutorialDirector::step_id_name(TutorialStepId id) -> QString {
  switch (id) {
  case TutorialStepId::SelectTroops:
    return QStringLiteral("select");
  case TutorialStepId::MoveTroops:
    return QStringLiteral("move");
  case TutorialStepId::AttackScouts:
    return QStringLiteral("attack");
  case TutorialStepId::GatherWood:
    return QStringLiteral("gather_wood");
  case TutorialStepId::GatherStoneAndIron:
    return QStringLiteral("gather_stone_iron");
  case TutorialStepId::BuildHome:
    return QStringLiteral("build_home");
  case TutorialStepId::RecruitSoldier:
    return QStringLiteral("recruit");
  case TutorialStepId::AssembleArmy:
    return QStringLiteral("army");
  case TutorialStepId::DefendCamp:
    return QStringLiteral("defend");
  case TutorialStepId::Stances:
    return QStringLiteral("stances");
  case TutorialStepId::Commander:
    return QStringLiteral("commander");
  case TutorialStepId::Camera:
    return QStringLiteral("camera");
  case TutorialStepId::GameSpeed:
    return QStringLiteral("speed");
  case TutorialStepId::Objectives:
    return QStringLiteral("objectives");
  case TutorialStepId::Assault:
    return QStringLiteral("assault");
  }
  return {};
}

auto TutorialDirector::step_title(TutorialStepId id) -> QString {
  switch (id) {
  case TutorialStepId::SelectTroops:
    return tr("Select your troops");
  case TutorialStepId::MoveTroops:
    return tr("Move, and read the feedback");
  case TutorialStepId::AttackScouts:
    return tr("Attack the Roman scouts");
  case TutorialStepId::GatherWood:
    return tr("Fell timber");
  case TutorialStepId::GatherStoneAndIron:
    return tr("Quarry stone, mine iron");
  case TutorialStepId::BuildHome:
    return tr("Raise a Home");
  case TutorialStepId::RecruitSoldier:
    return tr("Recruit a soldier");
  case TutorialStepId::AssembleArmy:
    return tr("Assemble an army");
  case TutorialStepId::DefendCamp:
    return tr("Defend the camp");
  case TutorialStepId::Stances:
    return tr("Guard, Hold and Patrol");
  case TutorialStepId::Commander:
    return tr("Your commander");
  case TutorialStepId::Camera:
    return tr("The camera");
  case TutorialStepId::GameSpeed:
    return tr("Pause and game speed");
  case TutorialStepId::Objectives:
    return tr("Objectives");
  case TutorialStepId::Assault:
    return tr("Take the Roman camp");
  }
  return {};
}

auto TutorialDirector::step_body(TutorialStepId id) -> QString {
  switch (id) {
  case TutorialStepId::SelectTroops:
    return tr("Everything starts with a selection. Left-click a soldier to select "
              "it, or hold the left button and drag a box around several. Selected "
              "troops show a ring at their feet, and the panel at the bottom lists "
              "who is under your command.");
  case TutorialStepId::MoveTroops:
    return tr("With troops selected, right-click on the ground to march there. A "
              "marker appears where they are headed and the banner above the "
              "command grid confirms the order. A rejected order says why it could "
              "not be carried out - read the banner when nothing happens.");
  case TutorialStepId::AttackScouts:
    return tr("A Roman scouting party stands just beyond your tents; the minimap "
              "marks enemies in red. Right-click an enemy to attack it, or press "
              "Attack and click the target. While your troops fight, the target "
              "panel above the command grid tracks the enemy's health.");
  case TutorialStepId::GatherWood:
    return tr("Wood pays for nearly everything. Select a builder, press Collect and "
              "click a pine tree. The builder fells it, carries the logs to the "
              "stone yard beside your barracks, and only when the load is dropped "
              "there does the wood counter in the top bar rise. Auto Gather keeps a "
              "builder working the nearest nodes on its own.");
  case TutorialStepId::GatherStoneAndIron:
    return tr("Boulders yield stone for buildings and towers; ore seams yield iron "
              "for blades and armour. Both are hauled to the same barracks yard, "
              "which fills up as your stores grow. Gold comes with the camp and "
              "from trade at a marketplace. Set one builder on the boulders and "
              "another on the ore.");
  case TutorialStepId::BuildHome:
    return tr("Select a builder and press Build to open the structure list. Each "
              "card shows its cost in wood, stone and gold; a grey card means you "
              "cannot afford it yet. Choose Home, then move the outline: green "
              "means the ground is flat and clear, red means it is blocked. Scroll "
              "to rotate, left-click to confirm. Homes raise the families your "
              "barracks will later recruit from.");
  case TutorialStepId::RecruitSoldier:
    return tr("Left-click your barracks and pick a soldier from the production "
              "panel. Every recruit costs population and resources: the population "
              "comes from the barracks' own pool, shown on the card, and refills "
              "when civilians from your Homes are delivered to it. The bar in the "
              "top panel shows your army against the map's population cap.");
  case TutorialStepId::AssembleArmy:
    return tr("One soldier is not an army. Keep recruiting until you field eight - "
              "mix spearmen to hold a line with archers to punish whatever charges "
              "it. If a card turns grey, the tutorial hint tells you what ran "
              "short. New recruits gather at the barracks' rally flag; set one "
              "from the production panel.");
  case TutorialStepId::DefendCamp:
    return tr("A Roman raid is coming. The wave tracker counts it down and the "
              "minimap marks its entry point. Form up near your barracks: spearmen "
              "in front, archers behind, and the commander close so his aura "
              "reaches them. Break the raid to continue.");
  case TutorialStepId::Stances:
    return tr("Soldiers can be told how to behave. Guard: hold a spot and chase "
              "anything that comes near, then return. Hold: stand your ground and "
              "do not pursue - best for archers on a hill or a wall line. Patrol: "
              "walk between two points and engage whatever crosses the route. Give "
              "one of these orders to your soldiers.");
  case TutorialStepId::Commander:
    return tr("Your commander carries the standard. Troops near him fight with "
              "higher morale, and while he is selected two commands appear: Aura "
              "briefly empowers every soldier around him, and Rally plants a flag "
              "that the army marches to. If he dies, your lines break and the "
              "mission is lost - keep him behind the spears. Trigger the Aura "
              "now.");
  case TutorialStepId::Camera:
    return tr("Move the view with the arrow keys or WASD, or push the mouse to the "
              "screen edge. Scroll to zoom, Q and E rotate, and the reset button in "
              "the top bar returns to your camp. Follow keeps the camera on your "
              "selection. Move the camera now.");
  case TutorialStepId::GameSpeed:
    return tr("The top-left buttons pause the battle and set the speed to half, "
              "normal or double. Space pauses too. Speed buttons stay disabled "
              "while paused - resume first. Change the speed or pause and resume "
              "now.");
  case TutorialStepId::Objectives:
    return tr("The star in the top bar shows your current objective. Press Escape "
              "and choose Objectives to read the full briefing: what wins the "
              "mission, what loses it, and any optional goals. Open it now, then "
              "return to the battle.");
  case TutorialStepId::Assault:
    return tr("You know everything the field will ask of you. The Roman camp lies "
              "across the meadow. Gather your army, keep the commander behind the "
              "line, and take the camp: kill the Roman commander and the mission "
              "is won.");
  }
  return {};
}

auto TutorialDirector::step_objective(TutorialStepId id) -> QString {
  switch (id) {
  case TutorialStepId::SelectTroops:
    return tr("Select at least one of your soldiers");
  case TutorialStepId::MoveTroops:
    return tr("Right-click the ground to move your selected troops");
  case TutorialStepId::AttackScouts:
    return tr("Destroy the Roman scouting party (%1 soldiers)")
        .arg(k_tutorial_scout_count);
  case TutorialStepId::GatherWood:
    return tr("Deliver %1 wood to your barracks yard").arg(k_tutorial_wood_target);
  case TutorialStepId::GatherStoneAndIron:
    return tr("Deliver %1 stone and %2 iron")
        .arg(k_tutorial_stone_target)
        .arg(k_tutorial_iron_target);
  case TutorialStepId::BuildHome:
    return tr("Build a Home with a builder");
  case TutorialStepId::RecruitSoldier:
    return tr("Recruit a soldier at the barracks and wait for it to march out");
  case TutorialStepId::AssembleArmy:
    return tr("Field at least %1 soldiers").arg(k_tutorial_army_size);
  case TutorialStepId::DefendCamp:
    return tr("Break the Roman raid");
  case TutorialStepId::Stances:
    return tr("Give a Guard, Hold or Patrol order");
  case TutorialStepId::Commander:
    return tr("Select your commander and trigger the Aura");
  case TutorialStepId::Camera:
    return tr("Move, zoom or rotate the camera");
  case TutorialStepId::GameSpeed:
    return tr("Change the game speed or pause and resume");
  case TutorialStepId::Objectives:
    return tr("Open the Objectives screen");
  case TutorialStepId::Assault:
    return tr("Kill the Roman commander and take the camp");
  }
  return {};
}

} // namespace App::Core
