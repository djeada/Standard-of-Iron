#include "strategic_events.h"

#include <QSet>

#include <algorithm>
#include <cmath>

namespace Game::Strategic {

namespace {

auto fail(QString* error, const QString& message) -> bool {
  if (error != nullptr) {
    *error = message;
  }
  return false;
}

auto known_faction(const WorldDefinition& def, const QString& faction) -> bool {
  return faction == QLatin1String(k_neutral_faction) || def.faction(faction) != nullptr;
}

auto strength_text(const ArmyState& army) -> QString {
  QString text =
      QStringLiteral("%1 foot, %2 horse").arg(army.infantry).arg(army.cavalry);
  if (army.elephants > 0) {
    text += army.elephants == 1 ? QStringLiteral(", 1 elephant")
                                : QStringLiteral(", %1 elephants").arg(army.elephants);
  }
  return text;
}

auto bool_text(bool value) -> QString {
  return value ? QStringLiteral("yes") : QStringLiteral("no");
}

auto apply_controller(const WorldDefinition& def,
                      WorldState& state,
                      const Effect& effect,
                      QString* error) -> bool {
  if (!known_faction(def, effect.faction)) {
    return fail(error, QStringLiteral("unknown faction '%1'").arg(effect.faction));
  }
  if (def.site(effect.target) != nullptr) {
    state.sites[effect.target].controller = effect.faction;
    return true;
  }
  if (def.region(effect.target) != nullptr) {
    state.regions[effect.target].controller = effect.faction;
    return true;
  }
  return fail(error,
              QStringLiteral("set_controller target '%1' is not a site or region")
                  .arg(effect.target));
}

auto apply_army_effect(const WorldDefinition& def,
                       WorldState& state,
                       const Effect& effect,
                       QString* error) -> bool {
  if (effect.op == EffectOp::CreateArmy) {
    if (effect.target.isEmpty() || state.army(effect.target) != nullptr) {
      return fail(error, QStringLiteral("army '%1' already exists").arg(effect.target));
    }
    if (def.site(effect.to) == nullptr) {
      return fail(
          error,
          QStringLiteral("create_army location '%1' is not a site").arg(effect.to));
    }
    if (!known_faction(def, effect.faction)) {
      return fail(error, QStringLiteral("unknown faction '%1'").arg(effect.faction));
    }
    if (!effect.commander.isEmpty() && def.commander(effect.commander) == nullptr) {
      return fail(error,
                  QStringLiteral("unknown commander '%1'").arg(effect.commander));
    }
    ArmyState army;
    army.id = effect.target;
    army.name = effect.name.isEmpty() ? effect.target : effect.name;
    army.faction = effect.faction;
    army.commander = effect.commander;
    army.location = effect.to;
    army.infantry = effect.infantry.value_or(0);
    army.cavalry = effect.cavalry.value_or(0);
    army.elephants = effect.elephants.value_or(0);
    if (!army.commander.isEmpty()) {
      for (auto& other : state.armies) {
        if (other.commander == army.commander) {
          other.commander.clear();
        }
      }
    }
    state.armies.append(army);
    return true;
  }
  ArmyState* army = state.army(effect.target);
  if (army == nullptr) {
    return fail(
        error,
        QStringLiteral("army '%1' does not exist at this point").arg(effect.target));
  }
  switch (effect.op) {
  case EffectOp::MoveArmy:
    if (def.site(effect.to) == nullptr) {
      return fail(
          error,
          QStringLiteral("move_army destination '%1' is not a site").arg(effect.to));
    }
    army->location = effect.to;
    return true;
  case EffectOp::SetArmyStrength:
    army->infantry = std::max(0, effect.infantry.value_or(army->infantry));
    army->cavalry = std::max(0, effect.cavalry.value_or(army->cavalry));
    army->elephants = std::max(0, effect.elephants.value_or(army->elephants));
    return true;
  case EffectOp::ScaleArmy: {
    const double factor = effect.factor.value_or(1.0);
    if (factor < 0.0) {
      return fail(error, QStringLiteral("scale_army factor must not be negative"));
    }
    army->infantry = static_cast<int>(std::lround(army->infantry * factor));
    army->cavalry = static_cast<int>(std::lround(army->cavalry * factor));
    army->elephants = static_cast<int>(std::lround(army->elephants * factor));
    return true;
  }
  case EffectOp::DisbandArmy: {
    const QString id = army->id;
    state.armies.erase(
        std::remove_if(state.armies.begin(),
                       state.armies.end(),
                       [&id](const ArmyState& entry) { return entry.id == id; }),
        state.armies.end());
    return true;
  }
  default:
    return fail(error, QStringLiteral("not an army effect"));
  }
}

auto apply_commander(const WorldDefinition& def,
                     WorldState& state,
                     const Effect& effect,
                     QString* error) -> bool {
  if (def.commander(effect.target) == nullptr) {
    return fail(error, QStringLiteral("unknown commander '%1'").arg(effect.target));
  }
  if (!effect.faction.isEmpty() && !known_faction(def, effect.faction)) {
    return fail(error, QStringLiteral("unknown faction '%1'").arg(effect.faction));
  }
  if (!effect.army.isEmpty() && state.army(effect.army) == nullptr) {
    return fail(
        error,
        QStringLiteral("army '%1' does not exist at this point").arg(effect.army));
  }
  CommanderState& commander = state.commanders[effect.target];
  if (effect.commander_status.has_value()) {
    commander.status = *effect.commander_status;
  }
  if (!effect.faction.isEmpty()) {
    commander.faction = effect.faction;
  }
  const bool leaves_army = commander.status == CommanderStatus::Dead ||
                           commander.status == CommanderStatus::Captured ||
                           commander.status == CommanderStatus::Recalled;
  if (leaves_army || !effect.army.isEmpty()) {
    for (auto& army : state.armies) {
      if (army.commander == effect.target) {
        army.commander.clear();
      }
    }
  }
  if (!effect.army.isEmpty()) {
    state.army(effect.army)->commander = effect.target;
  }
  return true;
}

auto site_change(const WorldDefinition& def,
                 const QString& id,
                 const SiteState& a,
                 const SiteState& b,
                 const WorldState& before,
                 const WorldState& after,
                 QVector<StateChange>& out) -> void {
  const QString ca = site_controller(def, before, id);
  const QString cb = site_controller(def, after, id);
  if (ca != cb) {
    out.append({id, EntityKind::Site, QStringLiteral("controller"), ca, cb});
  }
  const int fa = site_fortification(def, before, id);
  const int fb = site_fortification(def, after, id);
  if (fa != fb) {
    out.append({id,
                EntityKind::Site,
                QStringLiteral("fortification"),
                QString::number(fa),
                QString::number(fb)});
  }
  if (a.port_open != b.port_open) {
    out.append({id,
                EntityKind::Site,
                QStringLiteral("port"),
                a.port_open ? QStringLiteral("open") : QStringLiteral("closed"),
                b.port_open ? QStringLiteral("open") : QStringLiteral("closed")});
  }
  if (a.garrison != b.garrison) {
    out.append({id,
                EntityKind::Site,
                QStringLiteral("garrison"),
                QString::number(a.garrison),
                QString::number(b.garrison)});
  }
}

} // namespace

auto apply_effect(const WorldDefinition& def,
                  WorldState& state,
                  const Effect& effect,
                  QString* error) -> bool {
  switch (effect.op) {
  case EffectOp::SetController:
    return apply_controller(def, state, effect, error);
  case EffectOp::SetContested:
    if (def.region(effect.target) == nullptr) {
      return fail(error,
                  QStringLiteral("set_contested target '%1' is not a region")
                      .arg(effect.target));
    }
    state.regions[effect.target].contested = effect.enabled.value_or(true);
    return true;
  case EffectOp::SetRouteStatus:
    if (def.route(effect.target) == nullptr) {
      return fail(error,
                  QStringLiteral("set_route_status target '%1' is not a route")
                      .arg(effect.target));
    }
    if (!effect.route_status.has_value()) {
      return fail(error, QStringLiteral("set_route_status needs a status"));
    }
    state.routes[effect.target].status = *effect.route_status;
    return true;
  case EffectOp::SetPort: {
    const Site* site = def.site(effect.target);
    if (site == nullptr || !site->has_port()) {
      return fail(
          error,
          QStringLiteral("set_port target '%1' is not a port").arg(effect.target));
    }
    state.sites[effect.target].port_open = effect.enabled.value_or(true);
    return true;
  }
  case EffectOp::SetFortification:
    if (def.site(effect.target) == nullptr) {
      return fail(error,
                  QStringLiteral("set_fortification target '%1' is not a site")
                      .arg(effect.target));
    }
    if (!effect.level.has_value() || *effect.level < 0 ||
        *effect.level > k_fortification_max) {
      return fail(error,
                  QStringLiteral("set_fortification level must be 0..%1")
                      .arg(k_fortification_max));
    }
    state.sites[effect.target].fortification = *effect.level;
    return true;
  case EffectOp::SetGarrison:
    if (def.site(effect.target) == nullptr) {
      return fail(
          error,
          QStringLiteral("set_garrison target '%1' is not a site").arg(effect.target));
    }
    state.sites[effect.target].garrison = std::max(0, effect.infantry.value_or(0));
    return true;
  case EffectOp::MoveArmy:
  case EffectOp::SetArmyStrength:
  case EffectOp::ScaleArmy:
  case EffectOp::CreateArmy:
  case EffectOp::DisbandArmy:
    return apply_army_effect(def, state, effect, error);
  case EffectOp::SetCommander:
    return apply_commander(def, state, effect, error);
  case EffectOp::SetFlag:
    if (effect.target.isEmpty()) {
      return fail(error, QStringLiteral("set_flag needs a target key"));
    }
    state.flags[effect.target] =
        effect.value.isEmpty() ? QStringLiteral("true") : effect.value;
    return true;
  }
  return fail(error, QStringLiteral("unhandled effect"));
}

auto apply_event(const WorldDefinition& def,
                 WorldState& state,
                 const WorldEvent& event,
                 QVector<EffectError>* errors) -> bool {
  if (state.applied_events.contains(event.id)) {
    if (errors != nullptr) {
      errors->append({event.id, -1, QStringLiteral("event already applied")});
    }
    return false;
  }
  bool ok = true;
  for (int i = 0; i < event.effects.size(); ++i) {
    QString message;
    if (!apply_effect(def, state, event.effects[i], &message)) {
      ok = false;
      if (errors != nullptr) {
        errors->append({event.id, i, message});
      }
    }
  }
  state.applied_events.append(event.id);
  return ok;
}

auto state_after(const WorldDefinition& def,
                 int events_applied,
                 QVector<EffectError>* errors) -> WorldState {
  WorldState state = def.initial_state;
  state.world_id = def.id;
  const int count = std::clamp(events_applied, 0, static_cast<int>(def.events.size()));
  for (int i = 0; i < count; ++i) {
    (void)apply_event(def, state, def.events[i], errors);
  }
  return state;
}

auto state_after_event(const WorldDefinition& def,
                       const QString& event_id,
                       QVector<EffectError>* errors) -> WorldState {
  for (int i = 0; i < def.events.size(); ++i) {
    if (def.events[i].id == event_id) {
      return state_after(def, i + 1, errors);
    }
  }
  return state_after(def, 0, errors);
}

auto replay(const WorldDefinition& def,
            const QStringList& applied_events,
            QVector<EffectError>* errors) -> WorldState {
  WorldState state = def.initial_state;
  state.world_id = def.id;
  for (const auto& id : applied_events) {
    const WorldEvent* event = def.event(id);
    if (event == nullptr) {
      if (errors != nullptr) {
        errors->append({id, -1, QStringLiteral("unknown event")});
      }
      continue;
    }
    (void)apply_event(def, state, *event, errors);
  }
  return state;
}

auto diff_states(const WorldDefinition& def,
                 const WorldState& before,
                 const WorldState& after) -> QVector<StateChange> {
  QVector<StateChange> out;
  for (const auto& region : def.regions) {
    const QString ca = region_controller(def, before, region.id);
    const QString cb = region_controller(def, after, region.id);
    if (ca != cb) {
      out.append({region.id, EntityKind::Region, QStringLiteral("controller"), ca, cb});
    }
    const auto ia = before.regions.find(region.id);
    const auto ib = after.regions.find(region.id);
    const bool xa = ia != before.regions.end() && ia->second.contested;
    const bool xb = ib != after.regions.end() && ib->second.contested;
    if (xa != xb) {
      out.append({region.id,
                  EntityKind::Region,
                  QStringLiteral("contested"),
                  bool_text(xa),
                  bool_text(xb)});
    }
  }
  for (const auto& site : def.sites) {
    const auto ia = before.sites.find(site.id);
    const auto ib = after.sites.find(site.id);
    const SiteState a = ia != before.sites.end() ? ia->second : SiteState{};
    const SiteState b = ib != after.sites.end() ? ib->second : SiteState{};
    site_change(def, site.id, a, b, before, after, out);
  }
  for (const auto& route : def.routes) {
    const RouteStatus a = route_status(before, route.id);
    const RouteStatus b = route_status(after, route.id);
    if (a != b) {
      out.append({route.id,
                  EntityKind::Route,
                  QStringLiteral("status"),
                  to_string(a),
                  to_string(b)});
    }
  }
  for (const auto& commander : def.commanders) {
    const CommanderStatus a = commander_status(before, commander.id);
    const CommanderStatus b = commander_status(after, commander.id);
    if (a != b) {
      out.append({commander.id,
                  EntityKind::Commander,
                  QStringLiteral("status"),
                  to_string(a),
                  to_string(b)});
    }
    const QString fa = commander_faction(def, before, commander.id);
    const QString fb = commander_faction(def, after, commander.id);
    if (fa != fb) {
      out.append(
          {commander.id, EntityKind::Commander, QStringLiteral("faction"), fa, fb});
    }
  }
  QSet<QString> seen;
  for (const auto& army : before.armies) {
    seen.insert(army.id);
    const ArmyState* next = after.army(army.id);
    if (next == nullptr) {
      out.append({army.id,
                  EntityKind::Army,
                  QStringLiteral("army"),
                  strength_text(army),
                  QStringLiteral("disbanded")});
      continue;
    }
    if (army.location != next->location) {
      out.append({army.id,
                  EntityKind::Army,
                  QStringLiteral("location"),
                  army.location,
                  next->location});
    }
    if (army.infantry != next->infantry || army.cavalry != next->cavalry ||
        army.elephants != next->elephants) {
      out.append({army.id,
                  EntityKind::Army,
                  QStringLiteral("strength"),
                  strength_text(army),
                  strength_text(*next)});
    }
    if (army.commander != next->commander) {
      out.append({army.id,
                  EntityKind::Army,
                  QStringLiteral("commander"),
                  army.commander,
                  next->commander});
    }
  }
  for (const auto& army : after.armies) {
    if (!seen.contains(army.id)) {
      out.append(
          {army.id,
           EntityKind::Army,
           QStringLiteral("army"),
           QStringLiteral("absent"),
           QStringLiteral("raised at %1: %2").arg(army.location, strength_text(army))});
    }
  }
  for (const auto& [key, value] : after.flags) {
    const auto it = before.flags.find(key);
    const QString old = it == before.flags.end() ? QString() : it->second;
    if (old != value) {
      out.append({key, EntityKind::None, QStringLiteral("flag"), old, value});
    }
  }
  return out;
}

auto describe_effect(const WorldDefinition& def, const Effect& effect) -> QString {
  const QString target = def.display_name(effect.target);
  switch (effect.op) {
  case EffectOp::SetController:
    return QStringLiteral("%1 passes to %2")
        .arg(target, def.display_name(effect.faction));
  case EffectOp::SetContested:
    return effect.enabled.value_or(true)
               ? QStringLiteral("%1 becomes contested").arg(target)
               : QStringLiteral("%1 is pacified").arg(target);
  case EffectOp::SetRouteStatus:
    return QStringLiteral("%1 is %2")
        .arg(target,
             effect.route_status.has_value() ? to_string(*effect.route_status)
                                             : QStringLiteral("?"));
  case EffectOp::SetPort:
    return effect.enabled.value_or(true)
               ? QStringLiteral("port of %1 reopens").arg(target)
               : QStringLiteral("port of %1 is closed").arg(target);
  case EffectOp::SetFortification:
    return QStringLiteral("%1 fortification → %2")
        .arg(target)
        .arg(effect.level.value_or(0));
  case EffectOp::SetGarrison:
    return QStringLiteral("%1 garrison → %2")
        .arg(target)
        .arg(effect.infantry.value_or(0));
  case EffectOp::MoveArmy:
    return QStringLiteral("%1 marches to %2").arg(target, def.display_name(effect.to));
  case EffectOp::SetArmyStrength: {
    QStringList parts;
    if (effect.infantry.has_value()) {
      parts.append(QStringLiteral("%1 foot").arg(*effect.infantry));
    }
    if (effect.cavalry.has_value()) {
      parts.append(QStringLiteral("%1 horse").arg(*effect.cavalry));
    }
    if (effect.elephants.has_value()) {
      parts.append(QStringLiteral("%1 elephants").arg(*effect.elephants));
    }
    return QStringLiteral("%1 reduced to %2")
        .arg(target, parts.join(QStringLiteral(", ")));
  }
  case EffectOp::ScaleArmy:
    return QStringLiteral("%1 strength × %2")
        .arg(target)
        .arg(effect.factor.value_or(1.0));
  case EffectOp::CreateArmy:
    return QStringLiteral("%1 raised at %2")
        .arg(effect.name.isEmpty() ? effect.target : effect.name,
             def.display_name(effect.to));
  case EffectOp::DisbandArmy:
    return QStringLiteral("%1 is destroyed or disbanded").arg(target);
  case EffectOp::SetCommander: {
    QStringList parts;
    if (effect.commander_status.has_value()) {
      parts.append(to_string(*effect.commander_status));
    }
    if (!effect.faction.isEmpty()) {
      parts.append(QStringLiteral("joins %1").arg(def.display_name(effect.faction)));
    }
    if (!effect.army.isEmpty()) {
      parts.append(QStringLiteral("takes %1").arg(def.display_name(effect.army)));
    }
    return QStringLiteral("%1: %2").arg(target, parts.join(QStringLiteral(", ")));
  }
  case EffectOp::SetFlag:
    return QStringLiteral("flag %1 = %2")
        .arg(effect.target,
             effect.value.isEmpty() ? QStringLiteral("true") : effect.value);
  }
  return {};
}

auto events_for_mission(const WorldDefinition& def,
                        const QString& mission) -> QStringList {
  QStringList out;
  for (const auto& event : def.events) {
    if (!mission.isEmpty() && event.mission == mission) {
      out.append(event.id);
    }
  }
  return out;
}

} // namespace Game::Strategic
