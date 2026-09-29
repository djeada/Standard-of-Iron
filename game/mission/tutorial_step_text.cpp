#include "game/mission/tutorial_director.h"

namespace Game::Mission {

auto TutorialDirector::focus_target_name(TutorialFocusTarget target) -> QString {
  switch (target) {
  case TutorialFocusTarget::None:
    return {};
  case TutorialFocusTarget::OwnTroops:
    return QStringLiteral("own_troops");
  case TutorialFocusTarget::Builders:
    return QStringLiteral("builders");
  case TutorialFocusTarget::EnemyScouts:
    return QStringLiteral("enemy_scouts");
  case TutorialFocusTarget::Timber:
    return QStringLiteral("timber");
  case TutorialFocusTarget::StoneAndIron:
    return QStringLiteral("stone_and_iron");
  case TutorialFocusTarget::Barracks:
    return QStringLiteral("barracks");
  case TutorialFocusTarget::Commander:
    return QStringLiteral("commander");
  case TutorialFocusTarget::WaveEntry:
    return QStringLiteral("wave_entry");
  case TutorialFocusTarget::EnemyCamp:
    return QStringLiteral("enemy_camp");
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
    return tr("Soldiers can be told how to behave, and the three orders are not "
              "the same. Guard: click a spot; they meet anything that comes near "
              "it, then walk back to it. Hold: they never take a step, but they "
              "reach further, hit harder and brace against a charge - archers and "
              "spearmen only. Patrol: click two waypoints and they march the beat "
              "between them, fighting whatever crosses it. Hover any of the three "
              "to read the full rules, then give one of them to your soldiers.");
  case TutorialStepId::Commander:
    return tr("Your commander carries the standard. Troops near him fight with "
              "higher morale, and while he is selected two commands appear: Aura "
              "empowers every soldier in a ring around him for a stretch and then "
              "recharges - the button prints the radius, how long it lasts and how "
              "long until it returns - and Rally plants a flag that the army "
              "marches to. If he dies, your lines break and the mission is lost - "
              "keep him behind the spears. Trigger the Aura now.");
  case TutorialStepId::Camera:
    return tr("Move the view with the arrow keys or WASD, or push the mouse to the "
              "screen edge. Scroll to zoom, Q and E rotate, Ctrl with the up and "
              "down arrows tilts, and Home returns to your camp. Follow keeps the "
              "camera on your selection. Move the camera now.");
  case TutorialStepId::GameSpeed:
    return tr("The top-left buttons pause the battle and set the speed, from half "
              "up to quadruple. Space pauses too, and + and - step through the "
              "speeds without reaching for the bar. The active speed stays lit "
              "while paused, and you can change it there. Change the speed or "
              "pause and resume now.");
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
    return tr("Destroy the Roman scouting party (%1 units)")
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

} // namespace Game::Mission
