#include "army_formation_codec.h"

#include <QJsonArray>

#include <cstdint>

namespace Game::Formation::Codec {

namespace {

auto vector_to_json(const QVector3D& value) -> QJsonArray {
  QJsonArray array;
  array.append(static_cast<double>(value.x()));
  array.append(static_cast<double>(value.y()));
  array.append(static_cast<double>(value.z()));
  return array;
}

auto vector_from_json(const QJsonArray& array) -> QVector3D {
  if (array.size() < 3) {
    return {};
  }
  return {static_cast<float>(array.at(0).toDouble()),
          static_cast<float>(array.at(1).toDouble()),
          static_cast<float>(array.at(2).toDouble())};
}

auto slot_to_json(const FormationSlot& slot) -> QJsonObject {
  QJsonObject obj;
  obj["id"] = slot.id;
  obj["role"] = static_cast<int>(slot.role);
  obj["local"] = vector_to_json(slot.local_offset);
  obj["world"] = vector_to_json(slot.world_position);
  obj["facing"] = static_cast<double>(slot.facing);
  obj["local_facing"] = static_cast<double>(slot.local_facing);
  obj["rank"] = slot.rank;
  obj["file"] = slot.file;
  obj["status"] = static_cast<int>(slot.status);
  obj["occupant"] = static_cast<qint64>(slot.occupant);
  obj["half_width"] = static_cast<double>(slot.half_width);
  obj["half_depth"] = static_cast<double>(slot.half_depth);
  obj["heavy"] = slot.heavy;
  if (slot.band != BattleBand::None) {
    obj["band"] = QString::fromLatin1(battle_band_to_string(slot.band));
  }
  if (slot.yield_depth != 0.0F) {
    obj["yield_depth"] = static_cast<double>(slot.yield_depth);
  }
  if (!slot.manoeuvre_offset.isNull() || slot.manoeuvre_facing != 0.0F) {
    obj["manoeuvre_offset"] = vector_to_json(slot.manoeuvre_offset);
    obj["manoeuvre_facing"] = static_cast<double>(slot.manoeuvre_facing);
  }
  return obj;
}

auto slot_from_json(const QJsonObject& obj) -> FormationSlot {
  FormationSlot slot;
  slot.id = obj["id"].toInt(k_invalid_slot);
  slot.role = static_cast<ArmyRole>(obj["role"].toInt(0));
  slot.local_offset = vector_from_json(obj["local"].toArray());
  slot.world_position = vector_from_json(obj["world"].toArray());
  slot.facing = static_cast<float>(obj["facing"].toDouble(0.0));
  slot.local_facing = static_cast<float>(obj["local_facing"].toDouble(0.0));
  slot.rank = obj["rank"].toInt(0);
  slot.file = obj["file"].toInt(0);
  slot.status = static_cast<SlotStatus>(obj["status"].toInt(0));
  slot.occupant = static_cast<EntityID>(obj["occupant"].toVariant().toULongLong());
  slot.half_width = static_cast<float>(obj["half_width"].toDouble(0.5));
  slot.half_depth = static_cast<float>(obj["half_depth"].toDouble(0.5));
  slot.heavy = obj["heavy"].toBool(false);
  if (auto band = try_parse_battle_band(obj["band"].toString())) {
    slot.band = *band;
  }
  slot.yield_depth = static_cast<float>(obj["yield_depth"].toDouble(0.0));
  if (obj.contains("manoeuvre_offset")) {
    slot.manoeuvre_offset = vector_from_json(obj["manoeuvre_offset"].toArray());
    slot.manoeuvre_facing = static_cast<float>(obj["manoeuvre_facing"].toDouble(0.0));
  }
  return slot;
}

auto slots_to_json(const std::vector<FormationSlot>& records) -> QJsonArray {
  QJsonArray array;
  for (const auto& slot : records) {
    array.append(slot_to_json(slot));
  }
  return array;
}

void slots_from_json(const QJsonArray& array, std::vector<FormationSlot>& out) {
  for (const auto slot : array) {
    out.push_back(slot_from_json(slot.toObject()));
  }
}

auto options_to_json(const ArmyFormationOptions& options) -> QJsonObject {
  QJsonObject obj;
  obj["flank"] =
      QString::fromLatin1(flank_preference_to_string(options.flank_preference));
  obj["movement"] =
      QString::fromLatin1(movement_policy_to_string(options.movement_policy));
  obj["ranged"] =
      QString::fromLatin1(ranged_placement_to_string(options.ranged_placement));
  obj["mixed"] = QString::fromLatin1(mixed_policy_to_string(options.mixed_policy));
  obj["frontage_scale"] = static_cast<double>(options.frontage_scale);
  obj["depth_scale"] = static_cast<double>(options.depth_scale);
  obj["spacing_scale"] = static_cast<double>(options.spacing_scale);
  obj["reserve_rows"] = options.reserve_rows;
  obj["preserve_member_order"] = options.preserve_member_order;
  obj["doctrine_locked"] = options.doctrine_locked;
  return obj;
}

auto options_from_json(const QJsonObject& obj) -> ArmyFormationOptions {
  ArmyFormationOptions options;
  if (auto parsed = try_parse_flank_preference(obj["flank"].toString())) {
    options.flank_preference = *parsed;
  }
  if (auto parsed = try_parse_movement_policy(obj["movement"].toString())) {
    options.movement_policy = *parsed;
  }
  if (auto parsed = try_parse_ranged_placement(obj["ranged"].toString())) {
    options.ranged_placement = *parsed;
  }
  if (auto parsed = try_parse_mixed_policy(obj["mixed"].toString())) {
    options.mixed_policy = *parsed;
  }
  options.frontage_scale = static_cast<float>(obj["frontage_scale"].toDouble(1.0));
  options.depth_scale = static_cast<float>(obj["depth_scale"].toDouble(1.0));
  options.spacing_scale = static_cast<float>(obj["spacing_scale"].toDouble(1.0));
  options.reserve_rows = obj["reserve_rows"].toInt(-1);
  options.preserve_member_order = obj["preserve_member_order"].toBool(false);
  options.doctrine_locked = obj["doctrine_locked"].toBool(false);
  return options;
}

auto move_plan_to_json(const FormationMovePlan& plan) -> QJsonObject {
  QJsonObject move_plan;
  move_plan["active"] = plan.active;
  move_plan["corridor_index"] = static_cast<qint64>(plan.corridor_index);
  move_plan["center"] = vector_to_json(plan.formation_center);
  move_plan["facing_direction"] = vector_to_json(plan.facing_direction);
  QJsonArray corridor;
  for (const auto& waypoint : plan.corridor) {
    corridor.append(vector_to_json(waypoint));
  }
  move_plan["corridor"] = corridor;
  return move_plan;
}

void move_plan_from_json(const QJsonObject& move_plan, FormationMovePlan& plan) {
  plan.active = move_plan["active"].toBool(false);
  plan.corridor_index =
      static_cast<std::size_t>(move_plan["corridor_index"].toVariant().toULongLong());
  plan.formation_center = vector_from_json(move_plan["center"].toArray());
  if (move_plan.contains("facing_direction")) {
    plan.facing_direction = vector_from_json(move_plan["facing_direction"].toArray());
  }
  for (const auto waypoint : move_plan["corridor"].toArray()) {
    plan.corridor.push_back(vector_from_json(waypoint.toArray()));
  }
}

void write_shape_fields(const ArmyFormation& formation, QJsonObject& obj) {
  obj["id"] = static_cast<qint64>(formation.id);
  obj["doctrine"] = QString::fromStdString(formation.doctrine);
  obj["intent"] = QString::fromLatin1(intent_to_string(formation.intent));
  obj["anchor"] = vector_to_json(formation.anchor);
  obj["facing"] = static_cast<double>(formation.facing);
  obj["frontage"] = static_cast<double>(formation.frontage);
  obj["depth"] = static_cast<double>(formation.depth);
  obj["spacing"] = static_cast<double>(formation.spacing);
  obj["slot_spacing"] = static_cast<double>(formation.slot_spacing);
  obj["options"] = options_to_json(formation.options);
  obj["requested_frontage"] = static_cast<double>(formation.requested_frontage);
  obj["compressed"] = formation.compressed;
}

void write_state_fields(const ArmyFormation& formation, QJsonObject& obj) {
  obj["phase"] = static_cast<int>(formation.phase);
  obj["cohesion"] = static_cast<double>(formation.cohesion);
  obj["cohesion_pace"] = static_cast<double>(formation.cohesion_pace);
  obj["plan_revision"] = static_cast<qint64>(formation.plan_revision);
  obj["needs_replan"] = formation.needs_replan;
  obj["moves_pending"] = formation.moves_pending;
  obj["destination_facing"] = static_cast<double>(formation.destination_facing);
  obj["has_destination"] = formation.has_destination;
  obj["destination"] = vector_to_json(formation.destination);
  obj["advance_progress"] = static_cast<double>(formation.advance_progress);
  obj["move_plan"] = move_plan_to_json(formation.move_plan);
  if (is_battle_order_intent(formation.intent)) {
    QJsonObject manoeuvre;
    manoeuvre["centre_yield"] = static_cast<double>(formation.manoeuvre.centre_yield);
    manoeuvre["wing_wheel"] = static_cast<double>(formation.manoeuvre.wing_wheel);
    manoeuvre["wheel_ordered"] = formation.manoeuvre.wheel_ordered;
    manoeuvre["yielding"] = formation.manoeuvre.yielding;
    manoeuvre["lane_shift"] = static_cast<double>(formation.manoeuvre.lane_shift);
    manoeuvre["lanes_opened"] = formation.manoeuvre.lanes_opened;
    obj["manoeuvre"] = manoeuvre;
  }
}

void read_shape_fields(const QJsonObject& obj, ArmyFormation& formation) {
  formation.id = static_cast<FormationGroupID>(obj["id"].toVariant().toULongLong());
  formation.doctrine = obj["doctrine"].toString().toStdString();
  if (auto parsed = try_parse_intent(obj["intent"].toString())) {
    formation.intent = *parsed;
  }
  formation.anchor = vector_from_json(obj["anchor"].toArray());
  formation.facing = static_cast<float>(obj["facing"].toDouble(0.0));
  formation.frontage = static_cast<float>(obj["frontage"].toDouble(0.0));
  formation.depth = static_cast<float>(obj["depth"].toDouble(0.0));
  formation.spacing = static_cast<float>(obj["spacing"].toDouble(1.0));
  formation.slot_spacing = static_cast<float>(
      obj["slot_spacing"].toDouble(static_cast<double>(formation.spacing)));
  formation.options = options_from_json(obj["options"].toObject());
  formation.requested_frontage =
      static_cast<float>(obj["requested_frontage"].toDouble(0.0));
  formation.compressed = obj["compressed"].toBool(false);
}

void read_state_fields(const QJsonObject& obj, ArmyFormation& formation) {
  formation.phase = static_cast<FormationPhase>(obj["phase"].toInt(0));
  formation.cohesion = static_cast<float>(obj["cohesion"].toDouble(1.0));
  formation.cohesion_pace = static_cast<float>(obj["cohesion_pace"].toDouble(0.0));
  formation.plan_revision =
      static_cast<std::uint32_t>(obj["plan_revision"].toVariant().toUInt());
  formation.needs_replan = obj["needs_replan"].toBool(false);
  formation.moves_pending = obj["moves_pending"].toBool(false);
  formation.destination_facing = static_cast<float>(
      obj["destination_facing"].toDouble(static_cast<double>(formation.facing)));
  formation.has_destination = obj["has_destination"].toBool(false);
  formation.destination = vector_from_json(obj["destination"].toArray());
  formation.advance_progress =
      static_cast<float>(obj["advance_progress"].toDouble(0.0));
  move_plan_from_json(obj["move_plan"].toObject(), formation.move_plan);
  auto const manoeuvre = obj["manoeuvre"].toObject();
  formation.manoeuvre.centre_yield =
      static_cast<float>(manoeuvre["centre_yield"].toDouble(0.0));
  formation.manoeuvre.wing_wheel =
      static_cast<float>(manoeuvre["wing_wheel"].toDouble(0.0));
  formation.manoeuvre.wheel_ordered = manoeuvre["wheel_ordered"].toBool(false);
  formation.manoeuvre.yielding = manoeuvre["yielding"].toBool(false);
  formation.manoeuvre.lane_shift =
      static_cast<float>(manoeuvre["lane_shift"].toDouble(0.0));
  formation.manoeuvre.lanes_opened = manoeuvre["lanes_opened"].toBool(false);
}

void read_member_ids(const QJsonObject& obj, ArmyFormation& formation) {
  for (const auto member : obj["members"].toArray()) {
    formation.members.push_back(
        static_cast<EntityID>(member.toVariant().toULongLong()));
  }
}

void drop_transient_state(ArmyFormation& formation) {
  formation.morph.clear();
  if (!formation.has_destination) {
    formation.moves_pending = false;
    formation.move_plan.clear();
    formation.advance_progress = 0.0F;
  } else {
    formation.needs_replan = true;
  }
}

} // namespace

auto formation_to_json(const ArmyFormation& formation) -> QJsonObject {
  QJsonObject obj;
  write_shape_fields(formation, obj);
  write_state_fields(formation, obj);

  QJsonArray members;
  for (auto const member : formation.members) {
    members.append(static_cast<qint64>(member));
  }
  obj["members"] = members;
  obj["slot_list"] = slots_to_json(formation.slot_list);
  obj["reference_slots"] = slots_to_json(formation.reference_slots);
  return obj;
}

auto formation_from_json(const QJsonObject& obj) -> ArmyFormation {
  ArmyFormation formation;
  read_shape_fields(obj, formation);
  read_state_fields(obj, formation);
  read_member_ids(obj, formation);
  slots_from_json(obj["slot_list"].toArray(), formation.slot_list);
  slots_from_json(obj["reference_slots"].toArray(), formation.reference_slots);
  drop_transient_state(formation);
  return formation;
}

} // namespace Game::Formation::Codec
