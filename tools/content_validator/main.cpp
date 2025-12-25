#include "../../game/map/campaign_loader.h"
#include "../../game/map/mission_loader.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>
#include <set>

namespace {

struct ValidationResult {
  bool success = true;
  std::vector<QString> errors;
  std::vector<QString> warnings;

  void addError(const QString &error) {
    success = false;
    errors.push_back(error);
  }

  void addWarning(const QString &warning) { warnings.push_back(warning); }
};

auto validateMissionFile(const QString &file_path) -> ValidationResult {
  ValidationResult result;

  QFileInfo file_info(file_path);
  if (!file_info.exists()) {
    result.addError(QString("Mission file not found: %1").arg(file_path));
    return result;
  }

  Game::Mission::MissionDefinition mission;
  QString error_msg;

  if (!Game::Mission::MissionLoader::loadFromJsonFile(file_path, mission,
                                                      &error_msg)) {
    result.addError(QString("Failed to parse mission %1: %2")
                        .arg(file_path)
                        .arg(error_msg));
    return result;
  }

  if (mission.id.isEmpty()) {
    result.addError(QString("Mission %1: missing 'id' field").arg(file_path));
  }

  if (mission.title.isEmpty()) {
    result.addError(
        QString("Mission %1: missing 'title' field").arg(file_path));
  }

  if (mission.map_path.isEmpty()) {
    result.addError(
        QString("Mission %1: missing 'map_path' field").arg(file_path));
  } else {

    QString map_path = mission.map_path;

    if (map_path.startsWith(":/")) {
      map_path = map_path.mid(2);
    }

    QString abs_map_path =
        QDir::currentPath() + "/" + map_path.replace("assets/", "");

    bool map_found = false;
    QStringList search_paths = {abs_map_path,
                                QDir::currentPath() + "/assets/maps/" +
                                    QFileInfo(map_path).fileName(),
                                mission.map_path};

    for (const auto &search_path : search_paths) {
      if (QFile::exists(search_path)) {
        map_found = true;
        break;
      }
    }

    if (!map_found) {
      result.addWarning(QString("Mission %1: referenced map '%2' not found "
                                "(this may be OK if it's a Qt resource)")
                            .arg(file_path)
                            .arg(mission.map_path));
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

  return result;
}

auto validateCampaignFile(const QString &file_path,
                          const std::set<QString> &available_missions)
    -> ValidationResult {
  ValidationResult result;

  QFileInfo file_info(file_path);
  if (!file_info.exists()) {
    result.addError(QString("Campaign file not found: %1").arg(file_path));
    return result;
  }

  Game::Campaign::CampaignDefinition campaign;
  QString error_msg;

  if (!Game::Campaign::CampaignLoader::loadFromJsonFile(file_path, campaign,
                                                        &error_msg)) {
    result.addError(QString("Failed to parse campaign %1: %2")
                        .arg(file_path)
                        .arg(error_msg));
    return result;
  }

  if (campaign.id.isEmpty()) {
    result.addError(QString("Campaign %1: missing 'id' field").arg(file_path));
  }

  if (campaign.title.isEmpty()) {
    result.addError(
        QString("Campaign %1: missing 'title' field").arg(file_path));
  }

  if (campaign.missions.empty()) {
    result.addError(QString("Campaign %1: no missions defined").arg(file_path));
    return result;
  }

  std::set<int> order_indices;
  for (const auto &mission : campaign.missions) {
    if (order_indices.count(mission.order_index) > 0) {
      result.addError(QString("Campaign %1: duplicate order_index %2")
                          .arg(file_path)
                          .arg(mission.order_index));
    }
    order_indices.insert(mission.order_index);

    if (!available_missions.count(mission.mission_id)) {
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
          QString("Campaign %1: order_index values are not contiguous")
              .arg(file_path));
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

void printResults(const ValidationResult &result, const QString &file_name) {
  if (!result.warnings.empty()) {
    for (const auto &warning : result.warnings) {
      std::cout << "[WARNING] " << warning.toStdString() << std::endl;
    }
  }

  if (!result.errors.empty()) {
    for (const auto &error : result.errors) {
      std::cerr << "[ERROR] " << error.toStdString() << std::endl;
    }
  }

  if (result.success && result.warnings.empty()) {
    std::cout << "[OK] " << file_name.toStdString() << std::endl;
  }
}

} 

auto main(int argc, char *argv[]) -> int {
  QCoreApplication app(argc, argv);

  if (argc < 2) {
    std::cerr << "Usage: content_validator <assets_directory>" << std::endl;
    std::cerr << "  Validates all mission and campaign JSON files in the "
                 "assets directory"
              << std::endl;
    return 1;
  }

  const QString assets_dir = argv[1];
  const QDir base_dir(assets_dir);

  if (!base_dir.exists()) {
    std::cerr << "Error: Assets directory not found: "
              << assets_dir.toStdString() << std::endl;
    return 1;
  }

  std::cout << "Validating content in: " << assets_dir.toStdString()
            << std::endl;
  std::cout << "========================================" << std::endl;

  bool all_valid = true;
  std::set<QString> mission_ids;

  const QDir missions_dir = base_dir.filePath("missions");
  if (missions_dir.exists()) {
    const QStringList mission_files =
        missions_dir.entryList(QStringList() << "*.json", QDir::Files);

    std::cout << "\nValidating " << mission_files.size() << " mission(s)..."
              << std::endl;

    for (const auto &mission_file : mission_files) {
      const QString mission_path = missions_dir.filePath(mission_file);
      const ValidationResult result = validateMissionFile(mission_path);

      printResults(result, QString("missions/") + mission_file);

      if (result.success) {

        Game::Mission::MissionDefinition mission;
        if (Game::Mission::MissionLoader::loadFromJsonFile(mission_path,
                                                           mission)) {
          mission_ids.insert(mission.id);
        }
      } else {
        all_valid = false;
      }
    }
  } else {
    std::cout << "\nNo missions directory found (this is OK)" << std::endl;
  }

  const QDir campaigns_dir = base_dir.filePath("campaigns");
  if (campaigns_dir.exists()) {
    const QStringList campaign_files =
        campaigns_dir.entryList(QStringList() << "*.json", QDir::Files);

    std::cout << "\nValidating " << campaign_files.size() << " campaign(s)..."
              << std::endl;

    for (const auto &campaign_file : campaign_files) {
      const QString campaign_path = campaigns_dir.filePath(campaign_file);
      const ValidationResult result =
          validateCampaignFile(campaign_path, mission_ids);

      printResults(result, QString("campaigns/") + campaign_file);

      if (!result.success) {
        all_valid = false;
      }
    }
  } else {
    std::cout << "\nNo campaigns directory found (this is OK)" << std::endl;
  }

  std::cout << "\n========================================" << std::endl;
  if (all_valid) {
    std::cout << "✓ All content validation passed!" << std::endl;
    return 0;
  }
  std::cerr << "✗ Content validation failed!" << std::endl;
  return 1;
}
