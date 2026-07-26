#include "mission_data.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>
#include <QSet>

#include <algorithm>

namespace MapEditor {

namespace {

auto contains(const QStringList& values, const QString& value) -> bool {
  return values.contains(value.trimmed().toLower());
}

void require_string(const QJsonObject& object,
                    const QString& key,
                    const QString& context,
                    QStringList& errors) {
  if (object.value(key).toString().trimmed().isEmpty()) {
    errors.append(QStringLiteral("%1 requires %2.").arg(context, key));
  }
}

void validate_position(const QJsonObject& position,
                       const QString& context,
                       QStringList& errors) {
  if (!position.value(QStringLiteral("x")).isDouble() ||
      !position.value(QStringLiteral("z")).isDouble()) {
    errors.append(context + QStringLiteral(" requires a numeric x/z position."));
  }
}

void validate_setup_units(const QJsonArray& units,
                          const QString& context,
                          QStringList& errors) {
  for (qsizetype i = 0; i < units.size(); ++i) {
    const QJsonObject unit = units[i].toObject();
    const QString item_context = QStringLiteral("%1 unit %2").arg(context).arg(i + 1);
    if (!contains(MissionData::supported_troops(), unit.value("type").toString())) {
      errors.append(item_context + QStringLiteral(" has an unsupported troop type."));
    }
    if (unit.value("count").toInt(0) < 1) {
      errors.append(item_context + QStringLiteral(" requires count >= 1."));
    }
    validate_position(unit.value("position").toObject(), item_context, errors);
  }
}

void validate_setup_buildings(const QJsonArray& buildings,
                              const QString& context,
                              QStringList& errors) {
  for (qsizetype i = 0; i < buildings.size(); ++i) {
    const QJsonObject building = buildings[i].toObject();
    const QString item_context =
        QStringLiteral("%1 building %2").arg(context).arg(i + 1);
    if (!contains(MissionData::supported_structures(),
                  building.value("type").toString())) {
      errors.append(item_context +
                    QStringLiteral(" has an unsupported structure type."));
    }
    validate_position(building.value("position").toObject(), item_context, errors);
  }
}

void validate_conditions(const QJsonArray& conditions,
                         const QStringList& supported,
                         const QString& context,
                         QStringList& errors) {
  for (qsizetype i = 0; i < conditions.size(); ++i) {
    const QJsonObject condition = conditions[i].toObject();
    const QString type = condition.value("type").toString().trimmed().toLower();
    const QString item_context =
        QStringLiteral("%1 condition %2").arg(context).arg(i + 1);
    if (!supported.contains(type)) {
      errors.append(item_context +
                    QStringLiteral(" has unsupported type '%1'.").arg(type));
      continue;
    }
    if (condition.value("description").toString().trimmed().isEmpty()) {
      errors.append(item_context + QStringLiteral(" requires objective text."));
    }
    if (type == "survive_duration" &&
        condition.value("duration").toDouble(0.0) <= 0.0) {
      errors.append(item_context + QStringLiteral(" requires duration > 0."));
    }
    if ((type == "clear_undead_zone" || type == "purify_shrine" ||
         type == "survive_undead_wave") &&
        condition.value("zone_id").toString().trimmed().isEmpty()) {
      errors.append(item_context + QStringLiteral(" requires a zone_id."));
    }
  }
}

} // namespace

MissionData::MissionData(QObject* parent)
    : QObject(parent) {
  clear();
}

void MissionData::clear() {
  m_root = QJsonObject{
      {"id", "new_mission"},
      {"title", "New Mission"},
      {"summary", "Describe the mission objective."},
      {"map_path", ":/assets/maps/map_grasslands.json"},
      {"player_setup",
       QJsonObject{{"nation", "roman_republic"},
                   {"faction", "roman"},
                   {"color", "red"},
                   {"starting_units", QJsonArray{}},
                   {"starting_buildings", QJsonArray{}},
                   {"starting_resources", QJsonObject{{"gold", 500}, {"food", 500}}}}},
      {"ai_setups", QJsonArray{}},
      {"victory_conditions",
       QJsonArray{QJsonObject{{"type", "destroy_all_enemies"},
                              {"description", "Destroy all enemy forces."}}}},
      {"defeat_conditions",
       QJsonArray{QJsonObject{{"type", "lose_commander"},
                              {"description", "Your commander must survive."}},
                  QJsonObject{{"type", "only_commander_remaining"},
                              {"description", "Keep an army in the field."}}}},
      {"optional_objectives", QJsonArray{}},
      {"events", QJsonArray{}},
      {"include_ambient_undead", false}};
  m_modified = false;
  emit data_changed();
  emit modified_changed(false);
}

bool MissionData::load_from_json(const QString& file_path, QString* out_error) {
  QFile file(file_path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    if (out_error != nullptr) {
      *out_error = file.errorString();
    }
    return false;
  }

  QJsonParseError parse_error;
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parse_error);
  if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
    if (out_error != nullptr) {
      *out_error = parse_error.error != QJsonParseError::NoError
                       ? QStringLiteral("JSON parse error at byte %1: %2")
                             .arg(parse_error.offset)
                             .arg(parse_error.errorString())
                       : QStringLiteral("Mission JSON root must be an object.");
    }
    return false;
  }

  const QJsonObject root = document.object();
  if (!root.contains("map_path") || !root.contains("player_setup")) {
    if (out_error != nullptr) {
      *out_error = QStringLiteral("The selected file is not a mission definition.");
    }
    return false;
  }

  m_root = root;
  set_modified(false);
  emit data_changed();
  return true;
}

bool MissionData::save_to_json(const QString& file_path, QString* out_error) const {
  if (file_path.trimmed().isEmpty()) {
    if (out_error != nullptr) {
      *out_error = QStringLiteral("No output path was provided.");
    }
    return false;
  }

  QSaveFile file(file_path);
  if (!file.open(QIODevice::WriteOnly)) {
    if (out_error != nullptr) {
      *out_error = file.errorString();
    }
    return false;
  }

  const QByteArray payload = QJsonDocument(m_root).toJson(QJsonDocument::Indented);
  if (file.write(payload) != payload.size() || !file.commit()) {
    if (out_error != nullptr) {
      *out_error = file.errorString().isEmpty() ? QStringLiteral("Write failed.")
                                                : file.errorString();
    }
    return false;
  }
  return true;
}

QJsonValue MissionData::value(const QString& key) const {
  return m_root.value(key);
}

QJsonArray MissionData::array(const QString& key) const {
  return m_root.value(key).toArray();
}

QString MissionData::to_json_string() const {
  return QString::fromUtf8(QJsonDocument(m_root).toJson(QJsonDocument::Indented));
}

QString MissionData::id() const {
  return m_root.value("id").toString();
}

QString MissionData::title() const {
  return m_root.value("title").toString();
}

QString MissionData::map_path() const {
  return m_root.value("map_path").toString();
}

void MissionData::set_value(const QString& key, const QJsonValue& value) {
  if (m_root.value(key) == value) {
    return;
  }
  m_root.insert(key, value);
  set_modified(true);
  emit data_changed();
}

void MissionData::set_array(const QString& key, const QJsonArray& value) {
  set_value(key, value);
}

void MissionData::set_root(const QJsonObject& root) {
  if (m_root == root) {
    return;
  }
  m_root = root;
  set_modified(true);
  emit data_changed();
}

QStringList MissionData::validate() const {
  QStringList errors;
  require_string(m_root, "id", "Mission", errors);
  require_string(m_root, "title", "Mission", errors);
  require_string(m_root, "summary", "Mission", errors);
  require_string(m_root, "map_path", "Mission", errors);

  const QJsonObject player = m_root.value("player_setup").toObject();
  const QString player_nation = player.value("nation").toString();
  if (!contains(supported_nations(), player_nation)) {
    errors.append(QStringLiteral("Player setup has an unsupported nation."));
  }
  validate_setup_units(player.value("starting_units").toArray(), "Player", errors);
  validate_setup_buildings(
      player.value("starting_buildings").toArray(), "Player", errors);

  QSet<QString> ai_ids;
  const QJsonArray ai_setups = array("ai_setups");
  for (qsizetype i = 0; i < ai_setups.size(); ++i) {
    const QJsonObject ai = ai_setups[i].toObject();
    const QString context = QStringLiteral("AI setup %1").arg(i + 1);
    const QString ai_id = ai.value("id").toString().trimmed();
    if (ai_id.isEmpty()) {
      errors.append(context + QStringLiteral(" requires an id."));
    } else if (ai_ids.contains(ai_id)) {
      errors.append(QStringLiteral("AI id '%1' is duplicated.").arg(ai_id));
    }
    ai_ids.insert(ai_id);
    if (!contains(supported_nations(), ai.value("nation").toString())) {
      errors.append(context + QStringLiteral(" has an unsupported nation."));
    }
    if (!contains(supported_strategies(), ai.value("strategy").toString("balanced"))) {
      errors.append(context + QStringLiteral(" has an unsupported strategy."));
    }
    if (!contains(supported_difficulties(),
                  ai.value("difficulty").toString("normal"))) {
      errors.append(context + QStringLiteral(" has an unsupported difficulty."));
    }
    const QJsonObject personality = ai.value("personality").toObject();
    for (const QString& field : {"aggression", "defense", "harassment"}) {
      const double score = personality.value(field).toDouble(0.5);
      if (score < 0.0 || score > 1.0) {
        errors.append(context +
                      QStringLiteral(" personality %1 must be 0–1.").arg(field));
      }
    }
    validate_setup_units(ai.value("starting_units").toArray(), context, errors);
    validate_setup_buildings(ai.value("starting_buildings").toArray(), context, errors);

    const QJsonArray waves = ai.value("waves").toArray();
    for (qsizetype wave_index = 0; wave_index < waves.size(); ++wave_index) {
      const QJsonObject wave = waves[wave_index].toObject();
      const QString wave_context =
          QStringLiteral("%1 wave %2").arg(context).arg(wave_index + 1);
      if (wave.value("timing").toDouble(-1.0) < 0.0) {
        errors.append(wave_context + QStringLiteral(" requires timing >= 0."));
      }
      validate_position(wave.value("entry_point").toObject(), wave_context, errors);
      const QJsonArray composition = wave.value("composition").toArray();
      if (composition.isEmpty()) {
        errors.append(wave_context + QStringLiteral(" requires at least one troop."));
      }
      for (const QJsonValue& entry_value : composition) {
        const QJsonObject entry = entry_value.toObject();
        if (!contains(supported_troops(), entry.value("type").toString()) ||
            entry.value("count").toInt(0) < 1) {
          errors.append(wave_context +
                        QStringLiteral(" contains an unsupported troop or count."));
        }
      }
    }
  }

  if (array("victory_conditions").isEmpty()) {
    errors.append(QStringLiteral("Mission requires at least one victory condition."));
  }
  validate_conditions(
      array("victory_conditions"), supported_victory_conditions(), "Victory", errors);
  validate_conditions(
      array("defeat_conditions"), supported_defeat_conditions(), "Defeat", errors);
  validate_conditions(array("optional_objectives"),
                      supported_optional_objectives(),
                      "Optional objective",
                      errors);

  const QJsonArray events = array("events");
  for (qsizetype i = 0; i < events.size(); ++i) {
    const QJsonObject event = events[i].toObject();
    const QJsonObject trigger = event.value("trigger").toObject();
    if (trigger.value("type").toString() != "timer" ||
        trigger.value("time").toDouble(-1.0) < 0.0) {
      errors.append(
          QStringLiteral("Battlefield phase %1 requires a timer >= 0.").arg(i + 1));
    }
    const QJsonArray actions = event.value("actions").toArray();
    if (actions.isEmpty()) {
      errors.append(
          QStringLiteral("Battlefield phase %1 requires a message.").arg(i + 1));
    }
    for (const QJsonValue& action_value : actions) {
      const QJsonObject action = action_value.toObject();
      if (action.value("type").toString() != "show_message" ||
          action.value("text").toString().trimmed().isEmpty()) {
        errors.append(QStringLiteral("Battlefield phase %1 has an unsupported action.")
                          .arg(i + 1));
      }
    }
  }
  return errors;
}

void MissionData::set_modified(bool modified) {
  if (m_modified == modified) {
    return;
  }
  m_modified = modified;
  emit modified_changed(modified);
}

QStringList MissionData::supported_nations() {
  return {"roman_republic", "carthage", "iron_sepulcher"};
}

QStringList MissionData::supported_strategies() {
  return {"balanced",
          "aggressive",
          "defensive",
          "expansionist",
          "economic",
          "harasser",
          "rusher"};
}

QStringList MissionData::supported_difficulties() {
  return {"normal", "easy", "medium", "hard", "very_hard"};
}

QStringList MissionData::supported_colors() {
  return {"red", "blue", "green", "yellow", "orange", "brown"};
}

QStringList MissionData::supported_troops() {
  return {"archer",
          "swordsman",
          "spearman",
          "skeleton_swordsman",
          "skeleton_archer",
          "grave_priest",
          "horse_swordsman",
          "horse_archer",
          "horse_spearman",
          "healer",
          "catapult",
          "ballista",
          "elephant",
          "roman_legion_organizer",
          "roman_veteran_consul",
          "roman_field_commander",
          "carthage_mercenary_broker",
          "carthage_cavalry_patron",
          "carthage_elephant_master",
          "civilian",
          "builder"};
}

QStringList MissionData::supported_structures() {
  return {"barracks", "defense_tower", "home", "marketplace"};
}

QStringList MissionData::supported_victory_conditions() {
  return {"destroy_all_enemies",
          "survive_duration",
          "survive_waves",
          "accumulate_resources",
          "control_structures",
          "capture_structures",
          "clear_undead_zone",
          "purify_shrine",
          "survive_undead_wave"};
}

QStringList MissionData::supported_defeat_conditions() {
  return {"lose_all_units",
          "lose_structure",
          "lose_commander",
          "only_commander_remaining",
          "time_limit"};
}

QStringList MissionData::supported_optional_objectives() {
  return supported_victory_conditions();
}

} // namespace MapEditor
