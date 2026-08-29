#include "command_codec.h"

#include <QJsonArray>
#include <QString>

#include <string_view>
#include <type_traits>

#include "../systems/resource_types.h"
#include "../units/troop_type.h"

namespace Game::Command {

namespace {

auto id_to_json(Engine::Core::EntityID id) -> QJsonValue {
  return QJsonValue(static_cast<qint64>(id));
}

auto ids_to_json(const std::vector<Engine::Core::EntityID>& ids) -> QJsonArray {
  QJsonArray array;
  for (const auto id : ids) {
    array.push_back(id_to_json(id));
  }
  return array;
}

auto vec_to_json(const QVector3D& v) -> QJsonArray {
  return QJsonArray{static_cast<double>(v.x()),
                    static_cast<double>(v.y()),
                    static_cast<double>(v.z())};
}

auto vecs_to_json(const std::vector<QVector3D>& vs) -> QJsonArray {
  QJsonArray array;
  for (const auto& v : vs) {
    array.push_back(vec_to_json(v));
  }
  return array;
}

auto floats_to_json(const std::vector<float>& fs) -> QJsonArray {
  QJsonArray array;
  for (const auto f : fs) {
    array.push_back(static_cast<double>(f));
  }
  return array;
}

class Reader {
public:
  explicit Reader(const QJsonObject& object)
      : m_object(object) {}

  [[nodiscard]] auto ok() const -> bool { return m_ok; }

  auto id(const char* key) -> Engine::Core::EntityID {
    const auto value = m_object.value(QLatin1String(key));
    if (!value.isDouble()) {
      return fail<Engine::Core::EntityID>();
    }
    return static_cast<Engine::Core::EntityID>(value.toDouble());
  }

  auto ids(const char* key) -> std::vector<Engine::Core::EntityID> {
    std::vector<Engine::Core::EntityID> out;
    const auto value = m_object.value(QLatin1String(key));
    if (!value.isArray()) {
      return fail<std::vector<Engine::Core::EntityID>>();
    }
    for (const auto item : value.toArray()) {
      if (!item.isDouble()) {
        return fail<std::vector<Engine::Core::EntityID>>();
      }
      out.push_back(static_cast<Engine::Core::EntityID>(item.toDouble()));
    }
    return out;
  }

  auto vec(const char* key) -> QVector3D {
    return read_vec(m_object.value(QLatin1String(key)));
  }

  auto vecs(const char* key) -> std::vector<QVector3D> {
    std::vector<QVector3D> out;
    const auto value = m_object.value(QLatin1String(key));
    if (!value.isArray()) {
      return fail<std::vector<QVector3D>>();
    }
    for (const auto item : value.toArray()) {
      out.push_back(read_vec(item));
    }
    return out;
  }

  auto floats(const char* key) -> std::vector<float> {
    std::vector<float> out;
    const auto value = m_object.value(QLatin1String(key));
    if (!value.isArray()) {
      return fail<std::vector<float>>();
    }
    for (const auto item : value.toArray()) {
      if (!item.isDouble()) {
        return fail<std::vector<float>>();
      }
      out.push_back(static_cast<float>(item.toDouble()));
    }
    return out;
  }

  auto number(const char* key) -> double {
    const auto value = m_object.value(QLatin1String(key));
    if (!value.isDouble()) {
      return fail<double>();
    }
    return value.toDouble();
  }

  auto real(const char* key) -> float { return static_cast<float>(number(key)); }
  auto integer(const char* key) -> int { return static_cast<int>(number(key)); }

  auto boolean(const char* key) -> bool {
    const auto value = m_object.value(QLatin1String(key));
    if (!value.isBool()) {
      return fail<bool>();
    }
    return value.toBool();
  }

  auto text(const char* key) -> std::string {
    const auto value = m_object.value(QLatin1String(key));
    if (!value.isString()) {
      return fail<std::string>();
    }
    return value.toString().toStdString();
  }

  template <typename Enum>
  auto enumeration(const char* key, Enum last) -> Enum {
    const auto value = m_object.value(QLatin1String(key));
    if (!value.isDouble()) {
      return fail<Enum>();
    }
    const auto raw = value.toInt(-1);
    if (raw < 0 || raw > static_cast<int>(last)) {
      return fail<Enum>();
    }
    return static_cast<Enum>(raw);
  }

private:
  template <typename T>
  auto fail() -> T {
    m_ok = false;
    return T{};
  }

  auto read_vec(const QJsonValue& value) -> QVector3D {
    if (!value.isArray()) {
      return fail<QVector3D>();
    }
    const auto array = value.toArray();
    if (array.size() != 3 || !array[0].isDouble() || !array[1].isDouble() ||
        !array[2].isDouble()) {
      return fail<QVector3D>();
    }
    return {static_cast<float>(array[0].toDouble()),
            static_cast<float>(array[1].toDouble()),
            static_cast<float>(array[2].toDouble())};
  }

  const QJsonObject& m_object;
  bool m_ok = true;
};

template <typename Enum>
auto enum_value(Enum value) -> int {
  return static_cast<int>(static_cast<std::underlying_type_t<Enum>>(value));
}

void encode(QJsonObject& o, const Move& p) {
  o["units"] = ids_to_json(p.units);
  o["targets"] = vecs_to_json(p.targets);
  o["facing_angles"] = floats_to_json(p.facing_angles);
  o["kind"] = enum_value(p.kind);
  o["preserve_formation_mode"] = p.preserve_formation_mode;
}
void encode(QJsonObject& o, const AttackTarget& p) {
  o["units"] = ids_to_json(p.units);
  o["target"] = id_to_json(p.target);
  o["should_chase"] = p.should_chase;
}
void encode(QJsonObject& o, const Stop& p) {
  o["units"] = ids_to_json(p.units);
}
void encode(QJsonObject& o, const SetHold& p) {
  o["units"] = ids_to_json(p.units);
  o["active"] = p.active;
}
void encode(QJsonObject& o, const SetGuard& p) {
  o["units"] = ids_to_json(p.units);
  o["active"] = p.active;
  o["anchor"] = vec_to_json(p.anchor);
  o["has_anchor"] = p.has_anchor;
}
void encode(QJsonObject& o, const SetRunMode& p) {
  o["units"] = ids_to_json(p.units);
  o["active"] = p.active;
}
void encode(QJsonObject& o, const Patrol& p) {
  o["units"] = ids_to_json(p.units);
  o["first_waypoint"] = vec_to_json(p.first_waypoint);
  o["second_waypoint"] = vec_to_json(p.second_waypoint);
}
void encode(QJsonObject& o, const SetRallyPoint& p) {
  o["building"] = id_to_json(p.building);
  o["position"] = vec_to_json(p.position);
}
void encode(QJsonObject& o, const SetGateMode& p) {
  o["units"] = ids_to_json(p.units);
  o["mode"] = enum_value(p.mode);
}
void encode(QJsonObject& o, const SetAutoGather& p) {
  o["units"] = ids_to_json(p.units);
  o["active"] = p.active;
  o["priority_product_type"] = QString::fromStdString(p.priority_product_type);
}
void encode(QJsonObject& o, const Produce& p) {
  o["building"] = id_to_json(p.building);
  o["product"] = QString::fromStdString(Game::Units::troop_typeToString(p.product));
}
void encode(QJsonObject& o, const Trade& p) {
  o["resource"] = QLatin1String(Game::Systems::resource_type_key(p.resource));
  o["direction"] = enum_value(p.direction);
}
void encode(QJsonObject& o, const UseCommanderAbility& p) {
  o["commander"] = id_to_json(p.commander);
  o["ability"] = enum_value(p.ability);
  o["target"] = vec_to_json(p.target);
}
void encode(QJsonObject& o, const SetFormationMode& p) {
  o["units"] = ids_to_json(p.units);
  o["active"] = p.active;
}
void encode(QJsonObject& o, const DeployFormation& p) {
  o["units"] = ids_to_json(p.units);
  o["anchor"] = vec_to_json(p.anchor);
  o["facing"] = static_cast<double>(p.facing);
  o["frontage"] = static_cast<double>(p.frontage);
  o["spacing"] = static_cast<double>(p.spacing);
  o["intent"] = enum_value(p.intent);
  o["doctrine"] = QString::fromStdString(p.doctrine);
  QJsonObject options;
  options["flank_preference"] = enum_value(p.options.flank_preference);
  options["movement_policy"] = enum_value(p.options.movement_policy);
  options["ranged_placement"] = enum_value(p.options.ranged_placement);
  options["mixed_policy"] = enum_value(p.options.mixed_policy);
  options["frontage_scale"] = static_cast<double>(p.options.frontage_scale);
  options["depth_scale"] = static_cast<double>(p.options.depth_scale);
  options["spacing_scale"] = static_cast<double>(p.options.spacing_scale);
  options["reserve_rows"] = p.options.reserve_rows;
  options["preserve_member_order"] = p.options.preserve_member_order;
  options["doctrine_locked"] = p.options.doctrine_locked;
  o["options"] = options;
}
void encode(QJsonObject& o, const ReleaseFormation& p) {
  o["units"] = ids_to_json(p.units);
}
void encode(QJsonObject& o, const StartConstruction& p) {
  o["units"] = ids_to_json(p.units);
  o["construction_type"] = QString::fromStdString(p.construction_type);
  o["site"] = vec_to_json(p.site);
  o["rotation_y"] = static_cast<double>(p.rotation_y);
}
void encode(QJsonObject& o, const StartHarvest& p) {
  o["units"] = ids_to_json(p.units);
  o["construction_type"] = QString::fromStdString(p.construction_type);
  o["resource_target"] = id_to_json(p.resource_target);
  o["site"] = vec_to_json(p.site);
}
void encode(QJsonObject& o, const DeliverCivilians& p) {
  o["units"] = ids_to_json(p.units);
  o["barracks"] = id_to_json(p.barracks);
}
void encode(QJsonObject& o, const RepairStructure& p) {
  o["units"] = ids_to_json(p.units);
  o["structure"] = id_to_json(p.structure);
}
void encode(QJsonObject& o, const DivideSquads& p) {
  o["units"] = ids_to_json(p.units);
}
void encode(QJsonObject& o, const MergeSquads& p) {
  o["units"] = ids_to_json(p.units);
}
void encode(QJsonObject& o, const DismantleStructure& p) {
  o["units"] = ids_to_json(p.units);
  o["structure"] = id_to_json(p.structure);
}
void encode(QJsonObject& o, const PlaceWallPlan& p) {
  o["units"] = ids_to_json(p.units);
  o["gate"] = p.gate;
  o["anchor_x"] = p.anchor_x;
  o["anchor_z"] = p.anchor_z;
  o["target_x"] = p.target_x;
  o["target_z"] = p.target_z;
  o["rotation_y"] = static_cast<double>(p.rotation_y);
}
void encode(QJsonObject& o, const PlaceBuilding& p) {
  o["building_type"] = QString::fromStdString(p.building_type);
  o["position"] = vec_to_json(p.position);
  o["rotation_y"] = static_cast<double>(p.rotation_y);
}

template <typename T>
auto decode(Reader& r) -> T;

template <>
auto decode<Move>(Reader& r) -> Move {
  Move p;
  p.units = r.ids("units");
  p.targets = r.vecs("targets");
  p.facing_angles = r.floats("facing_angles");
  p.kind = r.enumeration("kind", Game::Systems::MoveOrderKind::PlannerMove);
  p.preserve_formation_mode = r.boolean("preserve_formation_mode");
  return p;
}
template <>
auto decode<AttackTarget>(Reader& r) -> AttackTarget {
  return {.units = r.ids("units"),
          .target = r.id("target"),
          .should_chase = r.boolean("should_chase")};
}
template <>
auto decode<Stop>(Reader& r) -> Stop {
  return {.units = r.ids("units")};
}
template <>
auto decode<SetHold>(Reader& r) -> SetHold {
  return {.units = r.ids("units"), .active = r.boolean("active")};
}
template <>
auto decode<SetGuard>(Reader& r) -> SetGuard {
  return {.units = r.ids("units"),
          .active = r.boolean("active"),
          .anchor = r.vec("anchor"),
          .has_anchor = r.boolean("has_anchor")};
}
template <>
auto decode<SetRunMode>(Reader& r) -> SetRunMode {
  return {.units = r.ids("units"), .active = r.boolean("active")};
}
template <>
auto decode<Patrol>(Reader& r) -> Patrol {
  return {.units = r.ids("units"),
          .first_waypoint = r.vec("first_waypoint"),
          .second_waypoint = r.vec("second_waypoint")};
}
template <>
auto decode<SetRallyPoint>(Reader& r) -> SetRallyPoint {
  return {.building = r.id("building"), .position = r.vec("position")};
}
template <>
auto decode<SetGateMode>(Reader& r) -> SetGateMode {
  return {.units = r.ids("units"),
          .mode = r.enumeration("mode",
                                Engine::Core::GateComponent::ManualMode::ForcedClosed)};
}
template <>
auto decode<SetAutoGather>(Reader& r) -> SetAutoGather {
  return {.units = r.ids("units"),
          .active = r.boolean("active"),
          .priority_product_type = r.text("priority_product_type")};
}
template <>
auto decode<Produce>(Reader& r) -> Produce {
  return {.building = r.id("building"),
          .product = Game::Units::troop_typeFromString(r.text("product"))};
}
template <>
auto decode<Trade>(Reader& r) -> Trade {
  Trade p;
  const auto key = QString::fromStdString(r.text("resource"));
  if (!Game::Systems::resource_type_from_key(key, p.resource)) {

    (void)r.number("resource");
  }
  p.direction = r.enumeration("direction", TradeDirection::Sell);
  return p;
}
template <>
auto decode<UseCommanderAbility>(Reader& r) -> UseCommanderAbility {
  return {.commander = r.id("commander"),
          .ability = r.enumeration("ability", CommanderAbility::FlagRally),
          .target = r.vec("target")};
}
template <>
auto decode<SetFormationMode>(Reader& r) -> SetFormationMode {
  return {.units = r.ids("units"), .active = r.boolean("active")};
}
template <>
auto decode<DeployFormation>(Reader& r) -> DeployFormation {
  DeployFormation p;
  p.units = r.ids("units");
  p.anchor = r.vec("anchor");
  p.facing = r.real("facing");
  p.frontage = r.real("frontage");
  p.spacing = r.real("spacing");
  p.intent = r.enumeration("intent", Game::Formation::ArmyFormationIntent::SiegeEscort);
  p.doctrine = r.text("doctrine");

  return p;
}
template <>
auto decode<ReleaseFormation>(Reader& r) -> ReleaseFormation {
  return {.units = r.ids("units")};
}
template <>
auto decode<StartConstruction>(Reader& r) -> StartConstruction {
  return {.units = r.ids("units"),
          .construction_type = r.text("construction_type"),
          .site = r.vec("site"),
          .rotation_y = r.real("rotation_y")};
}
template <>
auto decode<StartHarvest>(Reader& r) -> StartHarvest {
  return {.units = r.ids("units"),
          .construction_type = r.text("construction_type"),
          .resource_target = r.id("resource_target"),
          .site = r.vec("site")};
}
template <>
auto decode<DeliverCivilians>(Reader& r) -> DeliverCivilians {
  return {.units = r.ids("units"), .barracks = r.id("barracks")};
}
template <>
auto decode<RepairStructure>(Reader& r) -> RepairStructure {
  return {.units = r.ids("units"), .structure = r.id("structure")};
}
template <>
auto decode<DivideSquads>(Reader& r) -> DivideSquads {
  return {.units = r.ids("units")};
}
template <>
auto decode<MergeSquads>(Reader& r) -> MergeSquads {
  return {.units = r.ids("units")};
}
template <>
auto decode<DismantleStructure>(Reader& r) -> DismantleStructure {
  return {.units = r.ids("units"), .structure = r.id("structure")};
}
template <>
auto decode<PlaceWallPlan>(Reader& r) -> PlaceWallPlan {
  return {.units = r.ids("units"),
          .gate = r.boolean("gate"),
          .anchor_x = r.integer("anchor_x"),
          .anchor_z = r.integer("anchor_z"),
          .target_x = r.integer("target_x"),
          .target_z = r.integer("target_z"),
          .rotation_y = r.real("rotation_y")};
}
template <>
auto decode<PlaceBuilding>(Reader& r) -> PlaceBuilding {
  return {.building_type = r.text("building_type"),
          .position = r.vec("position"),
          .rotation_y = r.real("rotation_y")};
}

auto decode_formation_options(const QJsonObject& object,
                              bool& ok) -> Game::Formation::ArmyFormationOptions {
  Reader r(object);
  Game::Formation::ArmyFormationOptions options;
  options.flank_preference =
      r.enumeration("flank_preference", Game::Formation::FlankPreference::Split);
  options.movement_policy = r.enumeration(
      "movement_policy", Game::Formation::MovementPolicy::MaintainFormation);
  options.ranged_placement =
      r.enumeration("ranged_placement", Game::Formation::RangedPlacement::Skirmish);
  options.mixed_policy = r.enumeration(
      "mixed_policy", Game::Formation::MixedDoctrinePolicy::MajorityDoctrine);
  options.frontage_scale = r.real("frontage_scale");
  options.depth_scale = r.real("depth_scale");
  options.spacing_scale = r.real("spacing_scale");
  options.reserve_rows = r.integer("reserve_rows");
  options.preserve_member_order = r.boolean("preserve_member_order");
  options.doctrine_locked = r.boolean("doctrine_locked");
  ok = ok && r.ok();
  return options;
}

template <typename T>
auto decode_payload(const QJsonObject& object) -> std::optional<Payload> {
  Reader reader(object);
  T payload = decode<T>(reader);
  if (!reader.ok()) {
    return std::nullopt;
  }
  if constexpr (std::is_same_v<T, DeployFormation>) {
    const auto options = object.value(QLatin1String("options"));
    if (!options.isObject()) {
      return std::nullopt;
    }
    bool ok = true;
    payload.options = decode_formation_options(options.toObject(), ok);
    if (!ok) {
      return std::nullopt;
    }
    return Payload{std::move(payload)};
  } else {
    return Payload{std::move(payload)};
  }
}

template <typename... Ts>
auto decode_by_name(std::string_view name,
                    const QJsonObject& object,
                    std::variant<Ts...>*) -> std::optional<Payload> {
  std::optional<Payload> result;
  const bool matched = ((name == payload_name(Payload{Ts{}})
                             ? (result = decode_payload<Ts>(object), true)
                             : false) ||
                        ...);
  if (!matched) {
    return std::nullopt;
  }
  return result;
}

} // namespace

auto source_from_name(std::string_view name) -> std::optional<Source> {
  for (const Source source :
       {Source::LocalPlayer, Source::AI, Source::Replay, Source::Script}) {
    if (name == source_name(source)) {
      return source;
    }
  }
  return std::nullopt;
}

auto to_json(const Command& command) -> QJsonObject {
  QJsonObject object;
  object["type"] = QLatin1String(payload_name(command.payload));
  object["source"] = QLatin1String(source_name(command.source));
  object["owner"] = command.owner_id;
  object["tick"] = static_cast<qint64>(command.submitted_tick);
  QJsonObject body;
  std::visit([&body](const auto& payload) { encode(body, payload); }, command.payload);
  object["payload"] = body;
  return object;
}

auto from_json(const QJsonObject& object) -> std::optional<Command> {
  const auto type = object.value(QLatin1String("type"));
  const auto source = object.value(QLatin1String("source"));
  const auto owner = object.value(QLatin1String("owner"));
  const auto tick = object.value(QLatin1String("tick"));
  const auto body = object.value(QLatin1String("payload"));
  if (!type.isString() || !source.isString() || !owner.isDouble() || !tick.isDouble() ||
      !body.isObject()) {
    return std::nullopt;
  }
  const auto parsed_source = source_from_name(source.toString().toStdString());
  if (!parsed_source.has_value()) {
    return std::nullopt;
  }
  const std::string type_name = type.toString().toStdString();
  auto payload =
      decode_by_name(type_name, body.toObject(), static_cast<Payload*>(nullptr));
  if (!payload.has_value()) {
    return std::nullopt;
  }
  Command command;
  command.source = *parsed_source;
  command.owner_id = owner.toInt();
  command.submitted_tick = static_cast<std::uint64_t>(tick.toDouble());
  command.payload = std::move(*payload);
  return command;
}

} // namespace Game::Command
