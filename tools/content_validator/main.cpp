#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>

#include "../../game/map/campaign_loader.h"
#include "../../game/map/commander_message_grammar.h"
#include "../../game/map/map_loader.h"
#include "../../game/map/mission_loader.h"
#include "../../game/map/terrain_service.h"
#include "../../game/map/undead_shrine_placement.h"
#include "../../game/systems/building_collision_registry.h"
#include "game/formation/formation_data_loader.h"
#include "game/mission/commander_voice_bank.h"
#include "game/session/session_context.h"
#include "game/units/commander_catalog.h"
#include "game/units/troop_catalog_loader.h"
#include "game/units/troop_type.h"

namespace {

struct ValidationResult {
  bool success = true;
  std::vector<QString> errors;
  std::vector<QString> warnings;

  void addError(const QString& error) {
    success = false;
    errors.push_back(error);
  }

  void addWarning(const QString& warning) { warnings.push_back(warning); }
};

auto lastAiOwnerId(const Game::Mission::MissionDefinition& mission) -> int {
  return 1 + static_cast<int>(mission.ai_setups.size());
}

auto countMissionAuthoredEnemyBarracks(const Game::Mission::MissionDefinition& mission)
    -> int {
  int count = 0;
  for (const auto& ai_setup : mission.ai_setups) {
    for (const auto& building : ai_setup.starting_buildings) {
      if (building.type.trimmed().toLower() == QLatin1String("barracks")) {
        count += 1;
      }
    }
  }
  return count;
}

auto countWavePhases(const Game::Mission::MissionDefinition& mission) -> int {
  std::set<int> authored_phases;
  std::set<float> trigger_times;
  for (const auto& ai_setup : mission.ai_setups) {
    for (const auto& wave : ai_setup.waves) {
      if (wave.phase.has_value()) {
        authored_phases.insert(std::max(1, *wave.phase));
      } else {
        trigger_times.insert(wave.timing);
      }
    }
  }
  return static_cast<int>(authored_phases.size() + trigger_times.size());
}

void validateAgainstMap(const QString& file_path,
                        const Game::Mission::MissionDefinition& mission,
                        const QString& map_path,
                        ValidationResult& result) {

  QFile map_file(map_path);
  if (!map_file.open(QIODevice::ReadOnly)) {
    result.addWarning(QString("Mission %1: could not open map '%2' for objective "
                              "cross-check")
                          .arg(file_path)
                          .arg(map_path));
    return;
  }

  QJsonParseError parse_error;
  const QJsonDocument map_document =
      QJsonDocument::fromJson(map_file.readAll(), &parse_error);
  if (parse_error.error != QJsonParseError::NoError || !map_document.isObject()) {
    result.addWarning(QString("Mission %1: could not parse map '%2' for objective "
                              "cross-check: %3")
                          .arg(file_path)
                          .arg(map_path)
                          .arg(parse_error.errorString()));
    return;
  }

  const int last_ai_owner = lastAiOwnerId(mission);
  int enemy_barracks = countMissionAuthoredEnemyBarracks(mission);
  std::set<int> orphaned_owners;

  const QJsonArray structures = map_document.object().value("structures").toArray();
  for (const auto value : structures) {
    const QJsonObject structure = value.toObject();
    if (structure.value("type").toString().trimmed().toLower() !=
        QLatin1String("barracks")) {
      continue;
    }
    const int player_id = structure.value("player_id").toInt(0);
    if (player_id < 2) {
      continue;
    }
    if (player_id > last_ai_owner) {
      orphaned_owners.insert(player_id);
      continue;
    }
    enemy_barracks += 1;
  }

  for (int const owner_id : orphaned_owners) {
    result.addWarning(
        QString("Mission %1: map has barracks for player_id %2 but the mission only "
                "declares %3 AI setup(s) (owner ids 2-%4) - that camp is unowned")
            .arg(file_path)
            .arg(owner_id)
            .arg(mission.ai_setups.size())
            .arg(last_ai_owner));
  }

  for (const auto& condition : mission.victory_conditions) {
    if (condition.type.trimmed().toLower() != QLatin1String("capture_structures")) {
      continue;
    }
    const int required = condition.min_count.value_or(1);
    if (required > enemy_barracks) {
      result.addError(
          QString("Mission %1: capture_structures needs %2 barracks but only %3 are "
                  "enemy-owned - the mission can never be won")
              .arg(file_path)
              .arg(required)
              .arg(enemy_barracks));
    }
  }
}

auto validateMissionFile(const QString& file_path) -> ValidationResult {
  ValidationResult result;

  QFileInfo const file_info(file_path);
  if (!file_info.exists()) {
    result.addError(QString("Mission file not found: %1").arg(file_path));
    return result;
  }

  Game::Mission::MissionDefinition mission;
  QString error_msg;

  if (!Game::Mission::MissionLoader::load_from_json_file(
          file_path, mission, &error_msg)) {
    result.addError(
        QString("Failed to parse mission %1: %2").arg(file_path).arg(error_msg));
    return result;
  }

  if (mission.id.isEmpty()) {
    result.addError(QString("Mission %1: missing 'id' field").arg(file_path));
  }

  if (mission.title.isEmpty()) {
    result.addError(QString("Mission %1: missing 'title' field").arg(file_path));
  }

  if (mission.map_path.isEmpty()) {
    result.addError(QString("Mission %1: missing 'map_path' field").arg(file_path));
  } else {

    QString map_path = mission.map_path;

    if (map_path.startsWith(":/")) {
      map_path = map_path.mid(2);
    }

    QString const abs_map_path =
        QDir::currentPath() + "/" + map_path.replace("assets/", "");

    QString resolved_map_path;
    QStringList const search_paths = {abs_map_path,
                                      QDir::currentPath() + "/assets/maps/" +
                                          QFileInfo(map_path).fileName(),
                                      mission.map_path};

    for (const auto& search_path : search_paths) {
      if (QFile::exists(search_path)) {
        resolved_map_path = search_path;
        break;
      }
    }

    if (resolved_map_path.isEmpty()) {
      result.addWarning(QString("Mission %1: referenced map '%2' not found "
                                "(this may be OK if it's a Qt resource)")
                            .arg(file_path)
                            .arg(mission.map_path));
    } else {
      validateAgainstMap(file_path, mission, resolved_map_path, result);
    }
  }

  if (mission.player_setup.nation.isEmpty()) {
    result.addWarning(
        QString("Mission %1: player_setup missing 'nation'").arg(file_path));
  }

  if (mission.victory_conditions.empty()) {
    result.addError(
        QString("Mission %1: no victory conditions defined").arg(file_path));
  }

  if (mission.defeat_conditions.empty()) {
    result.addWarning(
        QString("Mission %1: no defeat conditions defined").arg(file_path));
  }

  QString const victory_mode = mission.victory_mode.trimmed().toLower();
  if (victory_mode != "any" && victory_mode != "all") {
    result.addError(QString("Mission %1: victory_mode '%2' is not 'any' or 'all'")
                        .arg(file_path)
                        .arg(mission.victory_mode));
  }

  if (victory_mode == "any" && mission.victory_conditions.size() > 1) {
    result.addWarning(
        QString("Mission %1: %2 victory conditions under victory_mode 'any' - any one "
                "of them wins on its own. Use 'all' if they are meant to be required "
                "together.")
            .arg(file_path)
            .arg(mission.victory_conditions.size()));
  }

  int const authored_wave_phases = countWavePhases(mission);
  for (const auto& condition : mission.victory_conditions) {
    QString const type = condition.type.trimmed().toLower();

    if (type == "accumulate_resources") {
      if (!condition.resources.has_value() || condition.resources->empty()) {
        result.addError(
            QString("Mission %1: accumulate_resources declares no positive resource "
                    "amounts")
                .arg(file_path));
      }
      continue;
    }

    if (type == "survive_waves") {
      int const required = condition.wave_count.value_or(0);
      if (required <= 0) {
        result.addError(QString("Mission %1: survive_waves requires a positive "
                                "'wave_count'")
                            .arg(file_path));
      } else if (required > authored_wave_phases) {
        result.addError(
            QString("Mission %1: survive_waves needs %2 assault phases but the AI "
                    "setups only define %3 - the mission can never be won")
                .arg(file_path)
                .arg(required)
                .arg(authored_wave_phases));
      }
      continue;
    }

    if (type == "survive_duration" && !condition.duration.has_value()) {
      result.addError(
          QString("Mission %1: survive_duration is missing 'duration'").arg(file_path));
    }
  }

  for (const auto& condition : mission.defeat_conditions) {
    if (condition.type.trimmed().toLower() == "time_limit" &&
        !condition.duration.has_value()) {
      result.addError(
          QString("Mission %1: time_limit is missing 'duration'").arg(file_path));
    }
  }

  std::set<QString> message_ids;
  for (const auto& message : mission.commander_messages) {
    if (message.id.trimmed().isEmpty()) {
      result.addError(
          QString("Mission %1: a commander message has no 'id'").arg(file_path));
    } else if (!message_ids.insert(message.id).second) {
      result.addError(QString("Mission %1: two commander messages share the id '%2'; "
                              "saves record spent lines by id")
                          .arg(file_path, message.id));
    }

    if (message.text.trimmed().isEmpty()) {
      result.addError(QString("Mission %1: commander message '%2' has no text")
                          .arg(file_path, message.id));
    }

    Game::Units::TroopType speaker{};
    if (!Game::Units::try_parse_troop_type(message.speaker, speaker) ||
        Game::Units::commander_definition(speaker) == nullptr) {
      result.addError(
          QString("Mission %1: commander message '%2' names speaker '%3', which is not "
                  "a commander in the catalogue")
              .arg(file_path, message.id, message.speaker));
    }

    if (message.duration <= 0.0F) {
      result.addError(QString("Mission %1: commander message '%2' holds for %3 seconds")
                          .arg(file_path, message.id)
                          .arg(static_cast<double>(message.duration)));
    }
  }

  return result;
}

constexpr int k_commander_chatter_max_chars = 260;

auto isAllowedCommanderPose(const QString& pose) -> bool {
  return pose.isEmpty() || pose == QStringLiteral("dismissive") ||
         pose == QStringLiteral("cynical");
}

auto commanderIdIsCatalogued(const QString& commander_id) -> bool {
  Game::Units::TroopType troop{};
  return Game::Units::try_parse_troop_type(commander_id, troop) &&
         Game::Units::commander_definition(troop) != nullptr;
}

auto validateCommanderVoices(const QString& assets_dir,
                             Game::Mission::CommanderVoiceLibrary& out_library)
    -> ValidationResult {
  ValidationResult result;
  const QDir voices_dir(QDir(assets_dir).filePath("data/commanders/voices"));
  if (!voices_dir.exists()) {
    result.addError(
        QString("Commander voices: directory %1 is missing").arg(voices_dir.path()));
    return result;
  }

  std::set<QString> line_ids;
  for (const QString& file_name :
       voices_dir.entryList(QStringList() << "*.json", QDir::Files, QDir::Name)) {
    const QString path = voices_dir.filePath(file_name);
    QString error;
    auto bank = Game::Mission::CommanderVoiceLibrary::load_from_file(path, &error);
    if (!bank.has_value()) {
      result.addError(QString("Commander voices: %1").arg(error));
      continue;
    }
    if (!commanderIdIsCatalogued(bank->commander_id)) {
      result.addError(
          QString("Commander voices %1: '%2' is not a commander in the catalogue")
              .arg(file_name, bank->commander_id));
    }
    if (QFileInfo(file_name).completeBaseName() != bank->commander_id) {
      result.addError(QString("Commander voices %1: file is named for a different "
                              "commander than '%2'")
                          .arg(file_name, bank->commander_id));
    }
    if (bank->chatter_per_match <= 0) {
      result.addError(QString("Commander voices %1: chatter_per_match must be positive")
                          .arg(file_name));
    }
    for (const auto& line : bank->lines) {
      if (!line_ids.insert(line.id).second) {
        result.addError(
            QString("Commander voices %1: line id '%2' is used twice across "
                    "the banks; saves record spent lines by id")
                .arg(file_name, line.id));
      }
      if (line.variants.isEmpty()) {
        result.addError(QString("Commander voices %1: line '%2' has no text")
                            .arg(file_name, line.id));
      }
      if (!isAllowedCommanderPose(line.pose)) {
        result.addError(QString("Commander voices %1: line '%2' uses pose '%3'; only "
                                "dismissive and cynical are baked")
                            .arg(file_name, line.id, line.pose));
      }
      if (!line.once && line.condition.cooldown <= 0.0F) {
        result.addError(QString("Commander voices %1: line '%2' repeats but has no "
                                "cooldown")
                            .arg(file_name, line.id));
      }
      if (line.duration <= 0.0F) {
        result.addError(QString("Commander voices %1: line '%2' holds for %3 seconds")
                            .arg(file_name, line.id)
                            .arg(static_cast<double>(line.duration)));
      }
      const bool is_bookend =
          line.trigger == Game::Mission::CommanderMessageTrigger::MissionStart ||
          Game::Mission::commander_message_trigger_is_outcome(line.trigger);
      if (!is_bookend) {
        for (const QString& variant : line.variants) {
          if (variant.length() > k_commander_chatter_max_chars) {
            result.addError(
                QString("Commander voices %1: line '%2' runs to %3 characters; chatter "
                        "must read inside the %4 second panel cap")
                    .arg(file_name, line.id)
                    .arg(variant.length())
                    .arg(static_cast<double>(
                        Game::Mission::k_commander_message_max_seconds)));
          }
        }
      }
    }
    out_library.add(std::move(*bank));
  }

  for (const auto& definition : Game::Units::all_commander_definitions()) {
    const QString commander_id = QString::fromStdString(definition.id);
    if (out_library.bank_for(commander_id) == nullptr) {
      result.addError(
          QString("Commander voices: %1 has no voice bank").arg(commander_id));
    }
  }
  return result;
}

auto validateMissionVoicePolicy(const QString& file_path,
                                const Game::Mission::MissionDefinition& mission,
                                const Game::Mission::CommanderVoiceLibrary& library)
    -> ValidationResult {
  ValidationResult result;
  for (const QString& muted : mission.commander_voices.muted_lines) {
    bool found = false;
    for (const auto& bank : library.banks()) {
      for (const auto& line : bank.lines) {
        if (line.id == muted) {
          found = true;
        }
      }
    }
    if (!found) {
      result.addError(
          QString("Mission %1: commander_voices.muted_lines names '%2', which "
                  "no voice bank defines")
              .arg(file_path, muted));
    }
  }
  return result;
}

auto validateMapFile(Game::Session::SessionContext& session,
                     const QString& file_path) -> ValidationResult {
  ValidationResult result;

  Game::Map::MapDefinition map_definition;
  QString error_msg;
  if (!Game::Map::MapLoader::load_from_json_file(
          file_path, map_definition, &error_msg)) {
    result.addError(QString("Failed to parse map %1: %2").arg(file_path, error_msg));
    return result;
  }

  if (map_definition.undead_zones.empty()) {
    return result;
  }

  session.building_collision().clear();
  auto& terrain = session.terrain();
  terrain.clear();
  terrain.initialize(map_definition);

  const auto placements = Game::Map::plan_undead_zone_shrines(terrain, map_definition);
  for (std::size_t index = 0; index < placements.size(); ++index) {
    const auto& placement = placements[index];
    const auto& zone = map_definition.undead_zones[index];

    if (!placement.placed) {
      result.addError(QString("Map %1: undead zone '%2' has no clear ground for its "
                              "magic shrine")
                          .arg(file_path, placement.zone_id));
      continue;
    }

    const QVector3D center = Game::Map::undead_zone_center_world(map_definition, zone);
    const float offset = std::hypot(placement.world_position.x() - center.x(),
                                    placement.world_position.z() - center.z());
    if (offset > zone.radius) {
      result.addWarning(
          QString("Map %1: undead zone '%2' is so crowded that its shrine landed "
                  "%3 cells from the centre, outside the zone radius")
              .arg(file_path, placement.zone_id)
              .arg(offset, 0, 'f', 1));
    }
  }

  terrain.clear();
  session.building_collision().clear();
  return result;
}

auto validateCampaignFile(const QString& file_path,
                          const std::set<QString>& available_missions)
    -> ValidationResult {
  ValidationResult result;

  QFileInfo const file_info(file_path);
  if (!file_info.exists()) {
    result.addError(QString("Campaign file not found: %1").arg(file_path));
    return result;
  }

  Game::Campaign::CampaignDefinition campaign;
  QString error_msg;

  if (!Game::Campaign::CampaignLoader::load_from_json_file(
          file_path, campaign, &error_msg)) {
    result.addError(
        QString("Failed to parse campaign %1: %2").arg(file_path).arg(error_msg));
    return result;
  }

  if (campaign.id.isEmpty()) {
    result.addError(QString("Campaign %1: missing 'id' field").arg(file_path));
  }

  if (campaign.title.isEmpty()) {
    result.addError(QString("Campaign %1: missing 'title' field").arg(file_path));
  }

  if (campaign.missions.empty()) {
    result.addError(QString("Campaign %1: no missions defined").arg(file_path));
    return result;
  }

  std::set<int> order_indices;
  for (const auto& mission : campaign.missions) {
    if (order_indices.contains(mission.order_index)) {
      result.addError(QString("Campaign %1: duplicate order_index %2")
                          .arg(file_path)
                          .arg(mission.order_index));
    }
    order_indices.insert(mission.order_index);

    if (static_cast<unsigned int>(available_missions.contains(mission.mission_id)) ==
        0U) {
      result.addError(QString("Campaign %1: references unknown mission '%2'")
                          .arg(file_path)
                          .arg(mission.mission_id));
    }
  }

  if (!order_indices.empty()) {
    const int min_index = *order_indices.begin();
    const int max_index = *order_indices.rbegin();
    const int expected_count = max_index - min_index + 1;

    if (static_cast<int>(order_indices.size()) != expected_count) {
      result.addError(
          QString("Campaign %1: order_index values are not contiguous").arg(file_path));
    }

    if (min_index != 0 && min_index != 1) {
      result.addWarning(
          QString("Campaign %1: order_index starts at %2 (expected 0 or 1)")
              .arg(file_path)
              .arg(min_index));
    }
  }

  return result;
}

void printResults(const ValidationResult& result, const QString& file_name) {
  if (!result.warnings.empty()) {
    for (const auto& warning : result.warnings) {
      std::cout << "[WARNING] " << warning.toStdString() << '\n';
    }
  }

  if (!result.errors.empty()) {
    for (const auto& error : result.errors) {
      std::cerr << "[ERROR] " << error.toStdString() << '\n';
    }
  }

  if (result.success && result.warnings.empty()) {
    std::cout << "[OK] " << file_name.toStdString() << '\n';
  }
}

} // namespace

auto validateFormationContent(const QString& assets_dir) -> bool {
  const QString root = QDir(assets_dir).filePath(QStringLiteral("data/formations"));
  if (!QDir(root).exists()) {
    std::cout << "\nNo data/formations directory found (this is OK)" << '\n';
    return true;
  }

  std::cout << "\nValidating formation content" << '\n';
  std::cout << "----------------------------------------" << '\n';

  Game::Units::TroopCatalogLoader::load_default_catalog();
  Game::Formation::FormationDataLoader::reset_to_builtin_defaults();
  const auto report = Game::Formation::FormationDataLoader::load_all(root);

  for (const auto& issue : report.issues) {
    std::cout << (issue.fatal ? "  ERROR   " : "  WARNING ") << issue.file.toStdString()
              << ": " << issue.message.toStdString() << '\n';
  }
  std::cout << "  " << report.summary().toStdString() << '\n';
  return !report.has_errors();
}

auto main(int argc, char* argv[]) -> int {
  QCoreApplication const app(argc, argv);

  Game::Session::SessionContext session;
  Game::Session::ScopedSession const active_session(session);

  if (argc == 3 && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--mission")) {
    const QString mission_path = QString::fromLocal8Bit(argv[2]);
    const ValidationResult result = validateMissionFile(mission_path);
    printResults(result, mission_path);
    return result.success ? 0 : 1;
  }

  if (argc < 2) {
    std::cerr << "Usage: content_validator <assets_directory>\n"
                 "       content_validator --mission <mission.json>"
              << '\n';
    std::cerr << "  Validates all mission and campaign JSON files in the "
                 "assets directory"
              << '\n';
    return 1;
  }

  const QString assets_dir = argv[1];
  const QDir base_dir(assets_dir);

  if (!base_dir.exists()) {
    std::cerr << "Error: Assets directory not found: " << assets_dir.toStdString()
              << '\n';
    return 1;
  }

  std::cout << "Validating content in: " << assets_dir.toStdString() << '\n';
  std::cout << "========================================" << '\n';

  bool all_valid = true;
  std::set<QString> mission_ids;

  Game::Mission::CommanderVoiceLibrary voices;
  const QDir voices_dir = base_dir.filePath("data/commanders/voices");
  if (voices_dir.exists()) {
    std::cout << "\nValidating commander voice banks..." << '\n';
    const ValidationResult result = validateCommanderVoices(assets_dir, voices);
    printResults(result, QStringLiteral("data/commanders/voices"));
    if (!result.success) {
      all_valid = false;
    }
  } else {
    std::cout << "\nNo data/commanders/voices directory found (this is OK)" << '\n';
  }

  const QDir missions_dir = base_dir.filePath("missions");
  if (missions_dir.exists()) {
    const QStringList mission_files =
        missions_dir.entryList(QStringList() << "*.json", QDir::Files);

    std::cout << "\nValidating " << mission_files.size() << " mission(s)..." << '\n';

    for (const auto& mission_file : mission_files) {
      const QString mission_path = missions_dir.filePath(mission_file);
      const ValidationResult result = validateMissionFile(mission_path);

      printResults(result, QString("missions/") + mission_file);

      if (result.success) {

        Game::Mission::MissionDefinition mission;
        if (Game::Mission::MissionLoader::load_from_json_file(mission_path, mission)) {
          mission_ids.insert(mission.id);
          const ValidationResult policy =
              validateMissionVoicePolicy(mission_path, mission, voices);
          if (!policy.success) {
            printResults(policy, QString("missions/") + mission_file);
            all_valid = false;
          }
        }
      } else {
        all_valid = false;
      }
    }
  } else {
    std::cout << "\nNo missions directory found (this is OK)" << '\n';
  }

  const QDir maps_dir = base_dir.filePath("maps");
  if (maps_dir.exists()) {
    const QStringList map_files =
        maps_dir.entryList(QStringList() << "*.json", QDir::Files);

    std::cout << "\nValidating " << map_files.size() << " map(s)..." << '\n';

    for (const auto& map_file : map_files) {
      const ValidationResult result =
          validateMapFile(session, maps_dir.filePath(map_file));
      printResults(result, QString("maps/") + map_file);
      if (!result.success) {
        all_valid = false;
      }
    }
  } else {
    std::cout << "\nNo maps directory found (this is OK)" << '\n';
  }

  const QDir campaigns_dir = base_dir.filePath("campaigns");
  if (campaigns_dir.exists()) {
    const QStringList campaign_files =
        campaigns_dir.entryList(QStringList() << "*.json", QDir::Files);

    std::cout << "\nValidating " << campaign_files.size() << " campaign(s)..." << '\n';

    for (const auto& campaign_file : campaign_files) {
      const QString campaign_path = campaigns_dir.filePath(campaign_file);
      const ValidationResult result = validateCampaignFile(campaign_path, mission_ids);

      printResults(result, QString("campaigns/") + campaign_file);

      if (!result.success) {
        all_valid = false;
      }
    }
  } else {
    std::cout << "\nNo campaigns directory found (this is OK)" << '\n';
  }

  if (!validateFormationContent(assets_dir)) {
    all_valid = false;
  }

  std::cout << "\n========================================" << '\n';
  if (all_valid) {
    std::cout << "✓ All content validation passed!" << '\n';
    return 0;
  }
  std::cerr << "✗ Content validation failed!" << '\n';
  return 1;
}
