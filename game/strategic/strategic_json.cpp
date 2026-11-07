#include "strategic_json.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

#include <algorithm>
#include <cmath>

namespace Game::Strategic {

namespace {

constexpr int k_coordinate_decimals = 4;

auto round_to(double value, int decimals) -> double {
  const double scale = std::pow(10.0, decimals);
  return std::round(value * scale) / scale;
}

auto lonlat_to_json(const LonLat& point) -> QJsonArray {
  return QJsonArray{round_to(point.lon, k_coordinate_decimals),
                    round_to(point.lat, k_coordinate_decimals)};
}

auto lonlat_from_json(const QJsonValue& value) -> std::optional<LonLat> {
  const QJsonArray pair = value.toArray();
  if (pair.size() != 2 || !pair.at(0).isDouble() || !pair.at(1).isDouble()) {
    return std::nullopt;
  }
  return LonLat{pair.at(0).toDouble(), pair.at(1).toDouble()};
}

auto lonlats_to_json(const QVector<LonLat>& points) -> QJsonArray {
  QJsonArray out;
  for (const auto& point : points) {
    out.append(lonlat_to_json(point));
  }
  return out;
}

auto strings_from_json(const QJsonValue& value) -> QStringList {
  QStringList out;
  for (const auto entry : value.toArray()) {
    const QString text = entry.toString().trimmed();
    if (!text.isEmpty()) {
      out.append(text);
    }
  }
  return out;
}

void put_if(QJsonObject& obj, const QString& key, const QString& value) {
  if (!value.isEmpty()) {
    obj.insert(key, value);
  }
}

void put_list_if(QJsonObject& obj, const QString& key, const QStringList& values) {
  if (!values.isEmpty()) {
    obj.insert(key, QJsonArray::fromStringList(values));
  }
}

template <typename Enum, typename Parser>
auto parse_enum(const QJsonObject& obj,
                const QString& key,
                Enum fallback,
                Parser parser,
                const QString& entity,
                QVector<LoadIssue>* issues) -> Enum {
  if (!obj.contains(key)) {
    return fallback;
  }
  const QString text = obj.value(key).toString();
  if (const auto parsed = parser(text)) {
    return *parsed;
  }
  if (issues != nullptr) {
    issues->append({entity, QStringLiteral("unknown %1 '%2'").arg(key, text)});
  }
  return fallback;
}

auto battlefield_from_json(const QJsonValue& value) -> BattlefieldLink {
  const QJsonObject obj = value.toObject();
  return {obj.value(QStringLiteral("map")).toString(),
          obj.value(QStringLiteral("mission")).toString(),
          obj.value(QStringLiteral("preset")).toString()};
}

auto battlefield_to_json(const BattlefieldLink& link) -> QJsonObject {
  QJsonObject obj;
  put_if(obj, QStringLiteral("map"), link.map);
  put_if(obj, QStringLiteral("mission"), link.mission);
  put_if(obj, QStringLiteral("preset"), link.preset);
  return obj;
}

auto resources_from_json(const QJsonObject& obj,
                         const QString& entity,
                         QVector<LoadIssue>* issues) -> ResourceProfile {
  ResourceProfile profile;
  for (auto it = obj.begin(); it != obj.end(); ++it) {
    const auto resource = resource_from_string(it.key());
    if (!resource.has_value()) {
      if (issues != nullptr) {
        issues->append({entity, QStringLiteral("unknown resource '%1'").arg(it.key())});
      }
      continue;
    }
    profile.set(*resource, it.value().toInt());
  }
  return profile;
}

auto resources_to_json(const ResourceProfile& profile) -> QJsonObject {
  QJsonObject obj;
  for (const Resource resource : k_all_resources) {
    if (profile.get(resource) != 0) {
      obj.insert(to_string(resource), profile.get(resource));
    }
  }
  return obj;
}

auto parse_faction(const QJsonObject& obj) -> Faction {
  return {obj.value(QStringLiteral("id")).toString(),
          obj.value(QStringLiteral("name")).toString(),
          obj.value(QStringLiteral("color")).toString(),
          obj.value(QStringLiteral("capital")).toString(),
          strings_from_json(obj.value(QStringLiteral("allies")))};
}

auto parse_region(const QJsonObject& obj, QVector<LoadIssue>* issues) -> Region {
  Region region;
  region.id = obj.value(QStringLiteral("id")).toString();
  region.name = obj.value(QStringLiteral("name")).toString();
  region.historical_name = obj.value(QStringLiteral("historical_name")).toString();
  region.terrain = parse_enum(obj,
                              QStringLiteral("terrain"),
                              Terrain::Plains,
                              terrain_from_string,
                              region.id,
                              issues);
  region.importance = obj.value(QStringLiteral("importance")).toInt(1);
  region.adjacent = strings_from_json(obj.value(QStringLiteral("adjacent")));
  region.tags = strings_from_json(obj.value(QStringLiteral("tags")));
  for (const auto value : obj.value(QStringLiteral("districts")).toArray()) {
    const QJsonObject district_obj = value.toObject();
    District district;
    district.id = district_obj.value(QStringLiteral("id")).toString();
    district.name = district_obj.value(QStringLiteral("name")).toString();
    for (const auto point : district_obj.value(QStringLiteral("outline")).toArray()) {
      if (const auto parsed = lonlat_from_json(point)) {
        district.outline.append(*parsed);
      } else if (issues != nullptr) {
        issues->append({district.id, QStringLiteral("malformed outline point")});
      }
    }
    region.districts.append(district);
  }
  return region;
}

auto parse_site(const QJsonObject& obj, QVector<LoadIssue>* issues) -> Site {
  Site site;
  site.id = obj.value(QStringLiteral("id")).toString();
  site.name = obj.value(QStringLiteral("name")).toString();
  site.latin_name = obj.value(QStringLiteral("latin")).toString();
  site.region = obj.value(QStringLiteral("region")).toString();
  site.district = obj.value(QStringLiteral("district")).toString();
  site.kind = parse_enum(obj,
                         QStringLiteral("kind"),
                         SiteKind::Town,
                         site_kind_from_string,
                         site.id,
                         issues);
  if (const auto position = lonlat_from_json(obj.value(QStringLiteral("lonlat")))) {
    site.position = *position;
  } else if (issues != nullptr) {
    issues->append({site.id, QStringLiteral("missing or malformed 'lonlat'")});
  }
  site.population = parse_enum(obj,
                               QStringLiteral("population"),
                               PopulationBand::Small,
                               population_from_string,
                               site.id,
                               issues);
  site.fortification = obj.value(QStringLiteral("fortification")).toInt(0);
  site.resources = resources_from_json(
      obj.value(QStringLiteral("resources")).toObject(), site.id, issues);
  site.port = parse_enum(obj,
                         QStringLiteral("port"),
                         PortClass::None,
                         port_class_from_string,
                         site.id,
                         issues);
  site.terrain = parse_enum(obj,
                            QStringLiteral("terrain"),
                            Terrain::Plains,
                            terrain_from_string,
                            site.id,
                            issues);
  site.tags = strings_from_json(obj.value(QStringLiteral("tags")));
  site.battlefield = battlefield_from_json(obj.value(QStringLiteral("battlefield")));
  return site;
}

auto parse_route(const QJsonObject& obj, QVector<LoadIssue>* issues) -> Route {
  Route route;
  route.id = obj.value(QStringLiteral("id")).toString();
  route.name = obj.value(QStringLiteral("name")).toString();
  route.from = obj.value(QStringLiteral("from")).toString();
  route.to = obj.value(QStringLiteral("to")).toString();
  route.kind = parse_enum(obj,
                          QStringLiteral("kind"),
                          RouteKind::Road,
                          route_kind_from_string,
                          route.id,
                          issues);
  route.length_km = obj.value(QStringLiteral("length_km")).toDouble(0.0);
  route.throughput = obj.value(QStringLiteral("throughput")).toInt(3);
  route.chokepoint = obj.value(QStringLiteral("chokepoint")).toBool(false);
  for (const auto value : obj.value(QStringLiteral("waypoints")).toArray()) {
    if (const auto point = lonlat_from_json(value)) {
      route.waypoints.append(*point);
    } else if (issues != nullptr) {
      issues->append({route.id, QStringLiteral("malformed waypoint")});
    }
  }
  route.tags = strings_from_json(obj.value(QStringLiteral("tags")));
  route.battlefield = battlefield_from_json(obj.value(QStringLiteral("battlefield")));
  return route;
}

auto army_from_json(const QJsonObject& obj) -> ArmyState {
  ArmyState army;
  army.id = obj.value(QStringLiteral("id")).toString();
  army.name = obj.value(QStringLiteral("name")).toString();
  army.faction = obj.value(QStringLiteral("faction")).toString();
  army.commander = obj.value(QStringLiteral("commander")).toString();
  army.location = obj.value(QStringLiteral("location")).toString();
  army.objective = obj.value(QStringLiteral("objective")).toString();
  army.infantry = obj.value(QStringLiteral("infantry")).toInt(0);
  army.cavalry = obj.value(QStringLiteral("cavalry")).toInt(0);
  army.elephants = obj.value(QStringLiteral("elephants")).toInt(0);
  army.flags = strings_from_json(obj.value(QStringLiteral("flags")));
  return army;
}

auto army_to_json(const ArmyState& army) -> QJsonObject {
  QJsonObject obj;
  obj.insert(QStringLiteral("id"), army.id);
  put_if(obj, QStringLiteral("name"), army.name);
  obj.insert(QStringLiteral("faction"), army.faction);
  put_if(obj, QStringLiteral("commander"), army.commander);
  obj.insert(QStringLiteral("location"), army.location);
  put_if(obj, QStringLiteral("objective"), army.objective);
  obj.insert(QStringLiteral("infantry"), army.infantry);
  obj.insert(QStringLiteral("cavalry"), army.cavalry);
  if (army.elephants != 0) {
    obj.insert(QStringLiteral("elephants"), army.elephants);
  }
  put_list_if(obj, QStringLiteral("flags"), army.flags);
  return obj;
}

const QStringList& preferred_key_order() {
  static const QStringList k_order = {
      QStringLiteral("format"),
      QStringLiteral("version"),
      QStringLiteral("schema_version"),
      QStringLiteral("world"),
      QStringLiteral("fingerprint"),
      QStringLiteral("_comment"),
      QStringLiteral("id"),
      QStringLiteral("op"),
      QStringLiteral("target"),
      QStringLiteral("name"),
      QStringLiteral("latin"),
      QStringLiteral("historical_name"),
      QStringLiteral("title"),
      QStringLiteral("campaign"),
      QStringLiteral("start_label"),
      QStringLiteral("player_faction"),
      QStringLiteral("geometry"),
      QStringLiteral("projection"),
      QStringLiteral("year"),
      QStringLiteral("season"),
      QStringLiteral("kind"),
      QStringLiteral("faction"),
      QStringLiteral("region"),
      QStringLiteral("district"),
      QStringLiteral("from"),
      QStringLiteral("to"),
      QStringLiteral("lonlat"),
      QStringLiteral("population"),
      QStringLiteral("fortification"),
      QStringLiteral("port"),
      QStringLiteral("terrain"),
      QStringLiteral("resources"),
      QStringLiteral("length_km"),
      QStringLiteral("throughput"),
      QStringLiteral("chokepoint"),
      QStringLiteral("waypoints"),
      QStringLiteral("commander"),
      QStringLiteral("location"),
      QStringLiteral("objective"),
      QStringLiteral("infantry"),
      QStringLiteral("cavalry"),
      QStringLiteral("elephants"),
      QStringLiteral("mission"),
      QStringLiteral("site"),
      QStringLiteral("summary"),
      QStringLiteral("battlefield"),
      QStringLiteral("tags"),
      QStringLiteral("factions"),
      QStringLiteral("regions"),
      QStringLiteral("sites"),
      QStringLiteral("routes"),
      QStringLiteral("commanders"),
      QStringLiteral("initial_state"),
      QStringLiteral("events"),
      QStringLiteral("effects"),
  };
  return k_order;
}

auto ordered_keys(const QJsonObject& obj) -> QStringList {
  QStringList keys = obj.keys();
  const QStringList& order = preferred_key_order();
  std::stable_sort(
      keys.begin(), keys.end(), [&order](const QString& a, const QString& b) {
        const qsizetype ia = order.indexOf(a);
        const qsizetype ib = order.indexOf(b);
        const qsizetype ra = ia < 0 ? order.size() : ia;
        const qsizetype rb = ib < 0 ? order.size() : ib;
        if (ra != rb) {
          return ra < rb;
        }
        return a < b;
      });
  return keys;
}

auto compact(const QJsonValue& value) -> QByteArray;

auto compact_string(const QString& text) -> QByteArray {
  QJsonArray wrapper{text};
  QByteArray bytes = QJsonDocument(wrapper).toJson(QJsonDocument::Compact);
  return bytes.mid(1, bytes.size() - 2);
}

auto compact_number(double value) -> QByteArray {
  QJsonArray wrapper{value};
  QByteArray bytes = QJsonDocument(wrapper).toJson(QJsonDocument::Compact);
  return bytes.mid(1, bytes.size() - 2);
}

auto compact(const QJsonValue& value) -> QByteArray {
  switch (value.type()) {
  case QJsonValue::Object: {
    const QJsonObject obj = value.toObject();
    QByteArray out = "{";
    bool first = true;
    for (const auto& key : ordered_keys(obj)) {
      if (!first) {
        out += ", ";
      }
      first = false;
      out += compact_string(key) + ": " + compact(obj.value(key));
    }
    return out + "}";
  }
  case QJsonValue::Array: {
    const QJsonArray arr = value.toArray();
    QByteArray out = "[";
    for (int i = 0; i < arr.size(); ++i) {
      if (i > 0) {
        out += ", ";
      }
      out += compact(arr.at(i));
    }
    return out + "]";
  }
  case QJsonValue::String:
    return compact_string(value.toString());
  case QJsonValue::Double:
    return compact_number(value.toDouble());
  case QJsonValue::Bool:
    return value.toBool() ? "true" : "false";
  case QJsonValue::Null:
  case QJsonValue::Undefined:
    return "null";
  }
  return "null";
}

void pretty_into(QByteArray& out, const QJsonValue& value, int indent, int wrap_width) {
  const QByteArray flat = compact(value);
  const bool container = value.isObject() || value.isArray();
  if (!container || indent + flat.size() <= wrap_width) {
    out += flat;
    return;
  }
  const QByteArray pad(indent + 2, ' ');
  const QByteArray close_pad(indent, ' ');
  if (value.isArray()) {
    const QJsonArray arr = value.toArray();
    bool all_scalar_pairs = !arr.isEmpty();
    for (const auto entry : arr) {
      all_scalar_pairs = all_scalar_pairs && entry.isArray() &&
                         entry.toArray().size() == 2 &&
                         entry.toArray().at(0).isDouble();
    }
    if (all_scalar_pairs) {
      out += "[\n" + pad;
      int column = indent + 2;
      for (int i = 0; i < arr.size(); ++i) {
        const QByteArray item = compact(arr.at(i));
        if (i > 0) {
          if (column + 2 + item.size() > wrap_width) {
            out += ",\n" + pad;
            column = indent + 2;
          } else {
            out += ", ";
            column += 2;
          }
        }
        out += item;
        column += static_cast<int>(item.size());
      }
      out += "\n" + close_pad + "]";
      return;
    }
    out += "[\n";
    for (int i = 0; i < arr.size(); ++i) {
      out += pad;
      pretty_into(out, arr.at(i), indent + 2, wrap_width);
      out += (i + 1 < arr.size()) ? ",\n" : "\n";
    }
    out += close_pad + "]";
    return;
  }
  const QJsonObject obj = value.toObject();
  const QStringList keys = ordered_keys(obj);
  out += "{\n";
  for (int i = 0; i < keys.size(); ++i) {
    out += pad + compact_string(keys[i]) + ": ";
    pretty_into(out, obj.value(keys[i]), indent + 2, wrap_width);
    out += (i + 1 < keys.size()) ? ",\n" : "\n";
  }
  out += close_pad + "}";
}

} // namespace

auto effect_to_json(const Effect& effect) -> QJsonObject {
  QJsonObject obj;
  obj.insert(QStringLiteral("op"), to_string(effect.op));
  put_if(obj, QStringLiteral("target"), effect.target);
  put_if(obj, QStringLiteral("faction"), effect.faction);
  put_if(obj, QStringLiteral("to"), effect.to);
  put_if(obj, QStringLiteral("commander"), effect.commander);
  put_if(obj, QStringLiteral("army"), effect.army);
  put_if(obj, QStringLiteral("name"), effect.name);
  put_if(obj, QStringLiteral("value"), effect.value);
  if (effect.route_status.has_value()) {
    obj.insert(QStringLiteral("status"), to_string(*effect.route_status));
  }
  if (effect.commander_status.has_value()) {
    obj.insert(QStringLiteral("status"), to_string(*effect.commander_status));
  }
  if (effect.enabled.has_value()) {
    obj.insert(QStringLiteral("enabled"), *effect.enabled);
  }
  if (effect.level.has_value()) {
    obj.insert(QStringLiteral("level"), *effect.level);
  }
  if (effect.infantry.has_value()) {
    obj.insert(QStringLiteral("infantry"), *effect.infantry);
  }
  if (effect.cavalry.has_value()) {
    obj.insert(QStringLiteral("cavalry"), *effect.cavalry);
  }
  if (effect.elephants.has_value()) {
    obj.insert(QStringLiteral("elephants"), *effect.elephants);
  }
  if (effect.factor.has_value()) {
    obj.insert(QStringLiteral("factor"), *effect.factor);
  }
  return obj;
}

auto effect_from_json(const QJsonObject& json,
                      QString* error) -> std::optional<Effect> {
  const QString op_text = json.value(QStringLiteral("op")).toString();
  const auto op = effect_op_from_string(op_text);
  if (!op.has_value()) {
    if (error != nullptr) {
      *error = QStringLiteral("unknown effect op '%1'").arg(op_text);
    }
    return std::nullopt;
  }
  Effect effect;
  effect.op = *op;
  effect.target = json.value(QStringLiteral("target")).toString();
  effect.faction = json.value(QStringLiteral("faction")).toString();
  effect.to = json.value(QStringLiteral("to")).toString();
  effect.commander = json.value(QStringLiteral("commander")).toString();
  effect.army = json.value(QStringLiteral("army")).toString();
  effect.name = json.value(QStringLiteral("name")).toString();
  effect.value = json.value(QStringLiteral("value")).toString();
  if (json.contains(QStringLiteral("status"))) {
    const QString status = json.value(QStringLiteral("status")).toString();
    if (effect.op == EffectOp::SetRouteStatus) {
      effect.route_status = route_status_from_string(status);
    } else if (effect.op == EffectOp::SetCommander) {
      effect.commander_status = commander_status_from_string(status);
    }
    if (!effect.route_status.has_value() && !effect.commander_status.has_value()) {
      if (error != nullptr) {
        *error = QStringLiteral("unknown status '%1' for %2").arg(status, op_text);
      }
      return std::nullopt;
    }
  }
  if (json.contains(QStringLiteral("enabled"))) {
    effect.enabled = json.value(QStringLiteral("enabled")).toBool();
  }
  if (json.contains(QStringLiteral("level"))) {
    effect.level = json.value(QStringLiteral("level")).toInt();
  }
  if (json.contains(QStringLiteral("infantry"))) {
    effect.infantry = json.value(QStringLiteral("infantry")).toInt();
  }
  if (json.contains(QStringLiteral("cavalry"))) {
    effect.cavalry = json.value(QStringLiteral("cavalry")).toInt();
  }
  if (json.contains(QStringLiteral("elephants"))) {
    effect.elephants = json.value(QStringLiteral("elephants")).toInt();
  }
  if (json.contains(QStringLiteral("factor"))) {
    effect.factor = json.value(QStringLiteral("factor")).toDouble();
  }
  return effect;
}

auto state_body_from_json(const QJsonObject& body,
                          QVector<LoadIssue>* issues) -> WorldState {
  WorldState state;
  state.applied_events =
      strings_from_json(body.value(QStringLiteral("applied_events")));
  const QJsonObject regions = body.value(QStringLiteral("regions")).toObject();
  for (auto it = regions.begin(); it != regions.end(); ++it) {
    const QJsonObject obj = it.value().toObject();
    state.regions[it.key()] = {obj.value(QStringLiteral("controller")).toString(),
                               obj.value(QStringLiteral("contested")).toBool(false)};
  }
  const QJsonObject sites = body.value(QStringLiteral("sites")).toObject();
  for (auto it = sites.begin(); it != sites.end(); ++it) {
    const QJsonObject obj = it.value().toObject();
    SiteState site;
    site.controller = obj.value(QStringLiteral("controller")).toString();
    site.fortification = obj.value(QStringLiteral("fortification")).toInt(-1);
    site.port_open = obj.value(QStringLiteral("port_open")).toBool(true);
    site.garrison = obj.value(QStringLiteral("garrison")).toInt(0);
    state.sites[it.key()] = site;
  }
  const QJsonObject routes = body.value(QStringLiteral("routes")).toObject();
  for (auto it = routes.begin(); it != routes.end(); ++it) {
    const QString text =
        it.value().toObject().value(QStringLiteral("status")).toString();
    const auto status = route_status_from_string(text);
    if (!status.has_value() && issues != nullptr) {
      issues->append({it.key(), QStringLiteral("unknown route status '%1'").arg(text)});
    }
    state.routes[it.key()] = {status.value_or(RouteStatus::Open)};
  }
  const QJsonObject commanders = body.value(QStringLiteral("commanders")).toObject();
  for (auto it = commanders.begin(); it != commanders.end(); ++it) {
    const QJsonObject obj = it.value().toObject();
    const QString text =
        obj.value(QStringLiteral("status")).toString(QStringLiteral("active"));
    const auto status = commander_status_from_string(text);
    if (!status.has_value() && issues != nullptr) {
      issues->append(
          {it.key(), QStringLiteral("unknown commander status '%1'").arg(text)});
    }
    state.commanders[it.key()] = {status.value_or(CommanderStatus::Active),
                                  obj.value(QStringLiteral("faction")).toString()};
  }
  for (const auto value : body.value(QStringLiteral("armies")).toArray()) {
    state.armies.append(army_from_json(value.toObject()));
  }
  const QJsonObject flags = body.value(QStringLiteral("flags")).toObject();
  for (auto it = flags.begin(); it != flags.end(); ++it) {
    state.flags[it.key()] = it.value().toVariant().toString();
  }
  return state;
}

auto state_body_to_json(const WorldState& state) -> QJsonObject {
  QJsonObject body;
  put_list_if(body, QStringLiteral("applied_events"), state.applied_events);
  QJsonObject regions;
  for (const auto& [id, region] : state.regions) {
    QJsonObject obj;
    put_if(obj, QStringLiteral("controller"), region.controller);
    if (region.contested) {
      obj.insert(QStringLiteral("contested"), true);
    }
    regions.insert(id, obj);
  }
  body.insert(QStringLiteral("regions"), regions);
  QJsonObject sites;
  for (const auto& [id, site] : state.sites) {
    QJsonObject obj;
    put_if(obj, QStringLiteral("controller"), site.controller);
    if (site.fortification >= 0) {
      obj.insert(QStringLiteral("fortification"), site.fortification);
    }
    if (!site.port_open) {
      obj.insert(QStringLiteral("port_open"), false);
    }
    if (site.garrison != 0) {
      obj.insert(QStringLiteral("garrison"), site.garrison);
    }
    if (!obj.isEmpty()) {
      sites.insert(id, obj);
    }
  }
  if (!sites.isEmpty()) {
    body.insert(QStringLiteral("sites"), sites);
  }
  QJsonObject routes;
  for (const auto& [id, route] : state.routes) {
    if (route.status != RouteStatus::Open) {
      routes.insert(id,
                    QJsonObject{{QStringLiteral("status"), to_string(route.status)}});
    }
  }
  if (!routes.isEmpty()) {
    body.insert(QStringLiteral("routes"), routes);
  }
  QJsonObject commanders;
  for (const auto& [id, commander] : state.commanders) {
    QJsonObject obj;
    if (commander.status != CommanderStatus::Active) {
      obj.insert(QStringLiteral("status"), to_string(commander.status));
    }
    put_if(obj, QStringLiteral("faction"), commander.faction);
    if (!obj.isEmpty()) {
      commanders.insert(id, obj);
    }
  }
  if (!commanders.isEmpty()) {
    body.insert(QStringLiteral("commanders"), commanders);
  }
  QJsonArray armies;
  for (const auto& army : state.armies) {
    armies.append(army_to_json(army));
  }
  body.insert(QStringLiteral("armies"), armies);
  if (!state.flags.empty()) {
    QJsonObject flags;
    for (const auto& [key, value] : state.flags) {
      flags.insert(key, value);
    }
    body.insert(QStringLiteral("flags"), flags);
  }
  return body;
}

auto definition_from_json(const QByteArray& json,
                          QString* error) -> std::optional<WorldDefinition> {
  QJsonParseError parse_error{};
  const QJsonDocument document = QJsonDocument::fromJson(json, &parse_error);
  if (parse_error.error != QJsonParseError::NoError) {
    if (error != nullptr) {
      *error = QStringLiteral("JSON parse error at offset %1: %2")
                   .arg(parse_error.offset)
                   .arg(parse_error.errorString());
    }
    return std::nullopt;
  }
  if (!document.isObject()) {
    if (error != nullptr) {
      *error = QStringLiteral("world file must be a JSON object");
    }
    return std::nullopt;
  }
  const QJsonObject root = document.object();
  WorldDefinition def;
  def.schema_version = root.value(QStringLiteral("schema_version")).toInt(0);
  if (def.schema_version != k_definition_schema_version) {
    if (error != nullptr) {
      *error = QStringLiteral("unsupported schema_version %1 (expected %2)")
                   .arg(def.schema_version)
                   .arg(k_definition_schema_version);
    }
    return std::nullopt;
  }
  def.id = root.value(QStringLiteral("id")).toString();
  def.name = root.value(QStringLiteral("name")).toString();
  def.campaign = root.value(QStringLiteral("campaign")).toString();
  def.start_label = root.value(QStringLiteral("start_label")).toString();
  def.player_faction = root.value(QStringLiteral("player_faction")).toString();
  def.geometry_source = root.value(QStringLiteral("geometry")).toString();
  const QJsonObject projection = root.value(QStringLiteral("projection")).toObject();
  def.projection.lon_min = projection.value(QStringLiteral("lon_min")).toDouble(-10.0);
  def.projection.lon_max = projection.value(QStringLiteral("lon_max")).toDouble(18.0);
  def.projection.lat_min = projection.value(QStringLiteral("lat_min")).toDouble(30.0);
  def.projection.lat_max = projection.value(QStringLiteral("lat_max")).toDouble(47.5);
  if (def.projection.lon_max <= def.projection.lon_min ||
      def.projection.lat_max <= def.projection.lat_min) {
    if (error != nullptr) {
      *error = QStringLiteral("projection bounds are empty or inverted");
    }
    return std::nullopt;
  }

  for (const auto value : root.value(QStringLiteral("factions")).toArray()) {
    def.factions.append(parse_faction(value.toObject()));
  }
  for (const auto value : root.value(QStringLiteral("regions")).toArray()) {
    def.regions.append(parse_region(value.toObject(), &def.load_issues));
  }
  for (const auto value : root.value(QStringLiteral("sites")).toArray()) {
    def.sites.append(parse_site(value.toObject(), &def.load_issues));
  }
  for (const auto value : root.value(QStringLiteral("routes")).toArray()) {
    def.routes.append(parse_route(value.toObject(), &def.load_issues));
  }
  for (const auto value : root.value(QStringLiteral("commanders")).toArray()) {
    const QJsonObject obj = value.toObject();
    def.commanders.append({obj.value(QStringLiteral("id")).toString(),
                           obj.value(QStringLiteral("name")).toString(),
                           obj.value(QStringLiteral("faction")).toString(),
                           obj.value(QStringLiteral("catalog_id")).toString(),
                           obj.value(QStringLiteral("title")).toString()});
  }
  for (const auto value : root.value(QStringLiteral("events")).toArray()) {
    const QJsonObject obj = value.toObject();
    WorldEvent event;
    event.id = obj.value(QStringLiteral("id")).toString();
    event.name = obj.value(QStringLiteral("name")).toString();
    event.year = obj.value(QStringLiteral("year")).toInt(0);
    event.season = parse_enum(obj,
                              QStringLiteral("season"),
                              Season::Spring,
                              season_from_string,
                              event.id,
                              &def.load_issues);
    event.mission = obj.value(QStringLiteral("mission")).toString();
    event.site = obj.value(QStringLiteral("site")).toString();
    event.summary = obj.value(QStringLiteral("summary")).toString();
    int index = 0;
    for (const auto effect_value : obj.value(QStringLiteral("effects")).toArray()) {
      QString effect_error;
      if (const auto effect =
              effect_from_json(effect_value.toObject(), &effect_error)) {
        event.effects.append(*effect);
      } else {
        def.load_issues.append(
            {event.id, QStringLiteral("effect %1: %2").arg(index).arg(effect_error)});
      }
      ++index;
    }
    def.events.append(event);
  }
  def.initial_state = state_body_from_json(
      root.value(QStringLiteral("initial_state")).toObject(), &def.load_issues);
  def.initial_state.world_id = def.id;
  def.initial_state.applied_events.clear();
  return def;
}

auto load_definition_file(const QString& path,
                          QString* error) -> std::optional<WorldDefinition> {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    if (error != nullptr) {
      *error = QStringLiteral("cannot open %1: %2").arg(path, file.errorString());
    }
    return std::nullopt;
  }
  return definition_from_json(file.readAll(), error);
}

auto definition_to_json(const WorldDefinition& def) -> QJsonObject {
  QJsonObject root;
  root.insert(QStringLiteral("schema_version"), def.schema_version);
  root.insert(QStringLiteral("id"), def.id);
  root.insert(QStringLiteral("name"), def.name);
  put_if(root, QStringLiteral("campaign"), def.campaign);
  put_if(root, QStringLiteral("start_label"), def.start_label);
  put_if(root, QStringLiteral("player_faction"), def.player_faction);
  put_if(root, QStringLiteral("geometry"), def.geometry_source);
  root.insert(QStringLiteral("projection"),
              QJsonObject{{QStringLiteral("lon_min"), def.projection.lon_min},
                          {QStringLiteral("lon_max"), def.projection.lon_max},
                          {QStringLiteral("lat_min"), def.projection.lat_min},
                          {QStringLiteral("lat_max"), def.projection.lat_max}});

  QJsonArray factions;
  for (const auto& faction : def.factions) {
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), faction.id);
    obj.insert(QStringLiteral("name"), faction.name);
    put_if(obj, QStringLiteral("color"), faction.color);
    put_if(obj, QStringLiteral("capital"), faction.capital);
    put_list_if(obj, QStringLiteral("allies"), faction.allies);
    factions.append(obj);
  }
  root.insert(QStringLiteral("factions"), factions);

  QJsonArray regions;
  for (const auto& region : def.regions) {
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), region.id);
    obj.insert(QStringLiteral("name"), region.name);
    put_if(obj, QStringLiteral("historical_name"), region.historical_name);
    obj.insert(QStringLiteral("terrain"), to_string(region.terrain));
    obj.insert(QStringLiteral("importance"), region.importance);
    put_list_if(obj, QStringLiteral("adjacent"), region.adjacent);
    put_list_if(obj, QStringLiteral("tags"), region.tags);
    if (!region.districts.isEmpty()) {
      QJsonArray districts;
      for (const auto& district : region.districts) {
        districts.append(QJsonObject{
            {QStringLiteral("id"), district.id},
            {QStringLiteral("name"), district.name},
            {QStringLiteral("outline"), lonlats_to_json(district.outline)}});
      }
      obj.insert(QStringLiteral("districts"), districts);
    }
    regions.append(obj);
  }
  root.insert(QStringLiteral("regions"), regions);

  QJsonArray sites;
  for (const auto& site : def.sites) {
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), site.id);
    obj.insert(QStringLiteral("name"), site.name);
    put_if(obj, QStringLiteral("latin"), site.latin_name);
    obj.insert(QStringLiteral("kind"), to_string(site.kind));
    obj.insert(QStringLiteral("region"), site.region);
    put_if(obj, QStringLiteral("district"), site.district);
    obj.insert(QStringLiteral("lonlat"), lonlat_to_json(site.position));
    obj.insert(QStringLiteral("population"), to_string(site.population));
    obj.insert(QStringLiteral("fortification"), site.fortification);
    if (site.port != PortClass::None) {
      obj.insert(QStringLiteral("port"), to_string(site.port));
    }
    obj.insert(QStringLiteral("terrain"), to_string(site.terrain));
    const QJsonObject resources = resources_to_json(site.resources);
    if (!resources.isEmpty()) {
      obj.insert(QStringLiteral("resources"), resources);
    }
    put_list_if(obj, QStringLiteral("tags"), site.tags);
    if (!site.battlefield.empty()) {
      obj.insert(QStringLiteral("battlefield"), battlefield_to_json(site.battlefield));
    }
    sites.append(obj);
  }
  root.insert(QStringLiteral("sites"), sites);

  QJsonArray routes;
  for (const auto& route : def.routes) {
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), route.id);
    put_if(obj, QStringLiteral("name"), route.name);
    obj.insert(QStringLiteral("from"), route.from);
    obj.insert(QStringLiteral("to"), route.to);
    obj.insert(QStringLiteral("kind"), to_string(route.kind));
    if (route.length_km > 0.0) {
      obj.insert(QStringLiteral("length_km"), round_to(route.length_km, 1));
    }
    obj.insert(QStringLiteral("throughput"), route.throughput);
    if (route.chokepoint) {
      obj.insert(QStringLiteral("chokepoint"), true);
    }
    if (!route.waypoints.isEmpty()) {
      obj.insert(QStringLiteral("waypoints"), lonlats_to_json(route.waypoints));
    }
    put_list_if(obj, QStringLiteral("tags"), route.tags);
    if (!route.battlefield.empty()) {
      obj.insert(QStringLiteral("battlefield"), battlefield_to_json(route.battlefield));
    }
    routes.append(obj);
  }
  root.insert(QStringLiteral("routes"), routes);

  QJsonArray commanders;
  for (const auto& commander : def.commanders) {
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), commander.id);
    obj.insert(QStringLiteral("name"), commander.name);
    obj.insert(QStringLiteral("faction"), commander.faction);
    put_if(obj, QStringLiteral("catalog_id"), commander.catalog_id);
    put_if(obj, QStringLiteral("title"), commander.title);
    commanders.append(obj);
  }
  root.insert(QStringLiteral("commanders"), commanders);

  WorldState initial = def.initial_state;
  initial.applied_events.clear();
  root.insert(QStringLiteral("initial_state"), state_body_to_json(initial));

  QJsonArray events;
  for (const auto& event : def.events) {
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), event.id);
    obj.insert(QStringLiteral("name"), event.name);
    obj.insert(QStringLiteral("year"), event.year);
    obj.insert(QStringLiteral("season"), to_string(event.season));
    put_if(obj, QStringLiteral("mission"), event.mission);
    put_if(obj, QStringLiteral("site"), event.site);
    put_if(obj, QStringLiteral("summary"), event.summary);
    QJsonArray effects;
    for (const auto& effect : event.effects) {
      effects.append(effect_to_json(effect));
    }
    obj.insert(QStringLiteral("effects"), effects);
    events.append(obj);
  }
  root.insert(QStringLiteral("events"), events);
  return root;
}

auto definition_to_bytes(const WorldDefinition& def) -> QByteArray {
  return pretty_json(definition_to_json(def)) + "\n";
}

auto definition_fingerprint(const WorldDefinition& def) -> QString {
  QJsonObject root = definition_to_json(def);
  root.remove(QStringLiteral("initial_state"));
  root.remove(QStringLiteral("events"));
  const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Compact);
  return QString::fromLatin1(
      QCryptographicHash::hash(bytes, QCryptographicHash::Sha1).toHex().left(16));
}

auto save_state(const WorldDefinition& def, const WorldState& state) -> QByteArray {
  QJsonObject root = state_body_to_json(state);
  root.insert(QStringLiteral("format"), QString::fromLatin1(k_state_format_tag));
  root.insert(QStringLiteral("version"), k_state_format_version);
  root.insert(QStringLiteral("world"), def.id);
  root.insert(QStringLiteral("fingerprint"), definition_fingerprint(def));
  root.insert(QStringLiteral("applied_events"),
              QJsonArray::fromStringList(state.applied_events));
  return pretty_json(root) + "\n";
}

auto load_state(const WorldDefinition& def,
                const QByteArray& json,
                QString* error) -> std::optional<WorldState> {
  QJsonParseError parse_error{};
  const QJsonDocument document = QJsonDocument::fromJson(json, &parse_error);
  if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
    if (error != nullptr) {
      *error = QStringLiteral("state is not a JSON object: %1")
                   .arg(parse_error.errorString());
    }
    return std::nullopt;
  }
  const QJsonObject root = document.object();
  if (root.value(QStringLiteral("format")).toString() !=
      QLatin1String(k_state_format_tag)) {
    if (error != nullptr) {
      *error = QStringLiteral("not a strategic state file");
    }
    return std::nullopt;
  }
  const int version = root.value(QStringLiteral("version")).toInt(0);
  if (version < 1 || version > k_state_format_version) {
    if (error != nullptr) {
      *error = QStringLiteral("unsupported state version %1").arg(version);
    }
    return std::nullopt;
  }
  const QString world = root.value(QStringLiteral("world")).toString();
  if (world != def.id) {
    if (error != nullptr) {
      *error =
          QStringLiteral("state belongs to world '%1', not '%2'").arg(world, def.id);
    }
    return std::nullopt;
  }
  QVector<LoadIssue> issues;
  WorldState state = state_body_from_json(root, &issues);
  if (!issues.isEmpty()) {
    if (error != nullptr) {
      *error =
          QStringLiteral("%1: %2").arg(issues.first().entity, issues.first().message);
    }
    return std::nullopt;
  }
  state.world_id = world;
  return state;
}

auto pretty_json(const QJsonValue& value, int wrap_width) -> QByteArray {
  QByteArray out;
  pretty_into(out, value, 0, wrap_width);
  return out;
}

} // namespace Game::Strategic
