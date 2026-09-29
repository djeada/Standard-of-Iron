#include <QStringList>

#include <initializer_list>

#include "game/mission/tutorial_director.h"
#include "game/systems/economy/construction_cost_catalog.h"
#include "game/systems/resource_types.h"

namespace Game::Mission {

namespace {

auto home_cost() -> Game::Systems::ResourceAmounts {
  return Game::Systems::construction_cost_info("home").resource_costs;
}

auto no_troops_hint(const TutorialObservation& o) -> QString {
  if (o.selected_troop_count > 0) {
    return {};
  }
  if (o.selected_builder_count > 0) {
    return TutorialDirector::tr(
        "Builders cannot fight. This order needs soldiers - select the "
        "spearmen, archers or swordsmen near your camp.");
  }
  if (o.selected_building_count > 0) {
    return TutorialDirector::tr(
        "That is a building. Orders in this step need soldiers - left-click "
        "one of the troops standing near your camp.");
  }
  return TutorialDirector::tr(
      "Nothing is selected, so there is nobody to receive the order. "
      "Left-click a soldier or drag a box around several.");
}

auto no_builder_hint(const TutorialObservation& o) -> QString {
  if (o.selected_builder_count > 0) {
    return {};
  }
  if (o.selected_troop_count > 0 || o.selected_building_count > 0) {
    return TutorialDirector::tr(
        "Collect and Build are only available to builders - soldiers and "
        "buildings cannot do this work. Select a builder (the worker with "
        "the hammer) first.");
  }
  return TutorialDirector::tr(
      "No builder is selected. Left-click one of the builders standing by "
      "your barracks.");
}

auto build_home_hint(const TutorialObservation& o) -> QString {
  if (const QString reason = no_builder_hint(o); !reason.isEmpty()) {
    return reason;
  }
  const auto cost = home_cost();
  const int wood_cost = cost.get(Game::Systems::ResourceType::Wood);
  const int stone_cost = cost.get(Game::Systems::ResourceType::Stone);
  if (o.wood < wood_cost || o.stone < stone_cost) {
    return TutorialDirector::tr(
               "A Home costs %1 wood and %2 stone; you have %3 wood and %4 stone. "
               "The Build card stays grey until the yard holds enough - send a "
               "builder to collect the difference.")
        .arg(wood_cost)
        .arg(stone_cost)
        .arg(o.wood)
        .arg(o.stone);
  }
  if (o.construction_preview_active && !o.construction_preview_valid) {
    return TutorialDirector::tr(
        "A red outline means the site is blocked: too close to another "
        "building, on water or on a slope. Move it onto flat, open ground "
        "and left-click to confirm. Right-click cancels.");
  }
  if (!o.last_rejection_reason.isEmpty()) {
    return o.last_rejection_reason;
  }
  return {};
}

auto recruit_hint(const TutorialObservation& o) -> QString {
  if (o.selected_civilian_count > 0) {
    return TutorialDirector::tr(
        "Press Deliver, then click your barracks: the civilian walks there "
        "and joins its reserve, and you can recruit again.");
  }
  if (o.selected_home_count > 0) {
    return TutorialDirector::tr(
        "A Home raises civilians, not soldiers. Recruit a civilian here, "
        "select it when it steps out, press Deliver and click your "
        "barracks to refill its reserve.");
  }
  if (o.selected_barracks_count == 0) {
    return TutorialDirector::tr(
        "Recruits come from the barracks. Left-click your barracks to open "
        "its production panel on the right.");
  }
  if (o.barracks_manpower < 20) {
    return TutorialDirector::tr(
               "The barracks has only %1 reserve left to draw on. Every recruit "
               "costs reserve; when it runs dry, build Homes - each Home raises "
               "families, and a civilian recruited there and sent to the barracks "
               "with Deliver refills it.")
        .arg(o.barracks_manpower);
  }
  if (o.wood < 30 || o.iron < 15) {
    return TutorialDirector::tr(
        "A recruit card turns grey when a resource is short: soldiers need "
        "wood for shafts and iron for blades. Send a builder to collect "
        "more.");
  }
  return {};
}

auto defend_camp_hint(const TutorialObservation& o) -> QString {
  if (o.wave_live) {
    return TutorialDirector::tr(
        "The raiders are here. Keep your soldiers together near the "
        "barracks: spearmen in front, archers behind, and the commander "
        "close so his aura reaches them.");
  }
  return TutorialDirector::tr(
      "The Roman raid is on its way. The wave tracker under the top bar "
      "counts it down and the minimap marks where it will enter.");
}

auto commander_hint(const TutorialObservation& o) -> QString {
  if (!o.commander_selected) {
    return TutorialDirector::tr(
        "The Aura command only appears when your commander is selected. "
        "He is the standard-bearer with the crown badge, near the "
        "barracks.");
  }
  if (!o.aura_ready) {
    return TutorialDirector::tr(
        "The aura is recharging or the commander is wounded; the button "
        "lights up again when it is ready.");
  }
  return {};
}

} // namespace

auto TutorialDirector::hint_for(const TutorialObservation& o) const -> QString {
  switch (step()) {
  case TutorialStepId::SelectTroops:
    if (o.selected_building_count > 0 && o.selected_troop_count == 0) {
      return tr("That is a building. Left-click one of the soldiers standing near "
                "your camp instead.");
    }
    return {};

  case TutorialStepId::MoveTroops:
  case TutorialStepId::AttackScouts:
    if (!o.last_rejection_reason.isEmpty()) {
      return o.last_rejection_reason;
    }
    return no_troops_hint(o);

  case TutorialStepId::GatherWood:
  case TutorialStepId::GatherStoneAndIron:
    if (!o.last_rejection_reason.isEmpty()) {
      return o.last_rejection_reason;
    }
    return no_builder_hint(o);

  case TutorialStepId::BuildHome:
    return build_home_hint(o);

  case TutorialStepId::RecruitSoldier:
  case TutorialStepId::AssembleArmy:
    return recruit_hint(o);

  case TutorialStepId::DefendCamp:
    return defend_camp_hint(o);

  case TutorialStepId::Stances:
    return no_troops_hint(o);

  case TutorialStepId::Commander:
    return commander_hint(o);

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

auto TutorialDirector::focus_for(const TutorialObservation& o) const -> TutorialFocus {
  const auto actions = [](std::initializer_list<const char*> ids) -> QStringList {
    QStringList list;
    for (const auto* id : ids) {
      list.append(QString::fromLatin1(id));
    }
    return list;
  };

  switch (step()) {
  case TutorialStepId::SelectTroops:
    return {{}, {}, TutorialFocusTarget::OwnTroops};

  case TutorialStepId::MoveTroops:
    if (o.selected_troop_count == 0) {
      return {{}, {}, TutorialFocusTarget::OwnTroops};
    }
    return {};

  case TutorialStepId::AttackScouts:
    if (o.selected_troop_count == 0) {
      return {{}, {}, TutorialFocusTarget::OwnTroops};
    }
    return {actions({"attack"}), {}, TutorialFocusTarget::EnemyScouts};

  case TutorialStepId::GatherWood:
    if (o.selected_builder_count == 0) {
      return {{}, {}, TutorialFocusTarget::Builders};
    }
    return {actions({"collect", "auto_gather"}), {}, TutorialFocusTarget::Timber};

  case TutorialStepId::GatherStoneAndIron:
    if (o.selected_builder_count == 0) {
      return {{}, {}, TutorialFocusTarget::Builders};
    }
    return {actions({"collect", "auto_gather"}), {}, TutorialFocusTarget::StoneAndIron};

  case TutorialStepId::BuildHome:
    if (o.selected_builder_count == 0) {
      return {{}, {}, TutorialFocusTarget::Builders};
    }
    if (o.construction_preview_active) {
      return {};
    }
    return {actions({"build"}), {}, TutorialFocusTarget::None};

  case TutorialStepId::RecruitSoldier:
  case TutorialStepId::AssembleArmy:
    if (o.selected_barracks_count == 0) {
      return {{}, {}, TutorialFocusTarget::Barracks};
    }
    return {{}, QStringLiteral("production"), TutorialFocusTarget::None};

  case TutorialStepId::DefendCamp:
    if (o.wave_live) {
      return {};
    }
    return {{}, QStringLiteral("waves"), TutorialFocusTarget::WaveEntry};

  case TutorialStepId::Stances:
    if (o.selected_troop_count == 0) {
      return {{}, {}, TutorialFocusTarget::OwnTroops};
    }
    return {actions({"guard", "hold", "patrol"}), {}, TutorialFocusTarget::None};

  case TutorialStepId::Commander:
    if (!o.commander_selected) {
      return {{}, {}, TutorialFocusTarget::Commander};
    }
    return {actions({"aura"}), {}, TutorialFocusTarget::None};

  case TutorialStepId::Camera:
    return {{}, QStringLiteral("camera"), TutorialFocusTarget::None};

  case TutorialStepId::GameSpeed:
    return {{}, QStringLiteral("speed"), TutorialFocusTarget::None};

  case TutorialStepId::Objectives:
    return {{}, QStringLiteral("objective"), TutorialFocusTarget::None};

  case TutorialStepId::Assault:
    return {{}, {}, TutorialFocusTarget::EnemyCamp};
  }
  return {};
}

} // namespace Game::Mission
