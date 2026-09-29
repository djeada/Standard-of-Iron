#include "save_campaign_catalog.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <utility>

#include "map/campaign_loader.h"
#include "util/asset_text.h"

namespace Game::Systems::SaveCampaignCatalog {

namespace {

auto load_from_directories() -> std::vector<Game::Campaign::CampaignDefinition> {
  std::vector<Game::Campaign::CampaignDefinition> campaigns;

  const QStringList search_paths = {
      QStringLiteral("assets/campaigns"),
      QStringLiteral("../assets/campaigns"),
      QStringLiteral("../../assets/campaigns"),
      QCoreApplication::applicationDirPath() + QStringLiteral("/assets/campaigns"),
      QCoreApplication::applicationDirPath() + QStringLiteral("/../assets/campaigns")};

  for (const QString& campaigns_path : search_paths) {
    QDir const campaigns_dir(campaigns_path);
    if (!campaigns_dir.exists()) {
      continue;
    }

    const QStringList campaign_files =
        campaigns_dir.entryList(QStringList() << QStringLiteral("*.json"), QDir::Files);
    if (campaign_files.isEmpty()) {
      continue;
    }

    qInfo() << "Loading campaigns from filesystem:" << campaigns_dir.absolutePath();
    for (const auto& campaign_file : campaign_files) {
      Game::Campaign::CampaignDefinition campaign;
      QString error;
      if (!Game::Campaign::CampaignLoader::load_from_json_file(
              campaigns_dir.filePath(campaign_file), campaign, &error)) {
        qWarning() << "Failed to load campaign" << campaign_file << ":" << error;
        continue;
      }
      campaigns.push_back(std::move(campaign));
    }

    if (!campaigns.empty()) {
      return campaigns;
    }
  }

  return campaigns;
}

auto load_from_resources() -> std::vector<Game::Campaign::CampaignDefinition> {
  std::vector<Game::Campaign::CampaignDefinition> campaigns;

  qInfo() << "Loading campaigns from Qt resources";
  const QStringList known_campaigns = {QStringLiteral("second_punic_war")};
  for (const auto& campaign_name : known_campaigns) {
    const QString campaign_path =
        QStringLiteral(":/assets/campaigns/%1.json").arg(campaign_name);
    if (!QFile::exists(campaign_path)) {
      qWarning() << "Campaign resource does not exist:" << campaign_path;
      continue;
    }

    Game::Campaign::CampaignDefinition campaign;
    QString error;
    if (!Game::Campaign::CampaignLoader::load_from_json_file(
            campaign_path, campaign, &error)) {
      qWarning() << "Failed to load campaign from resources" << campaign_name << ":"
                 << error;
      continue;
    }
    campaigns.push_back(std::move(campaign));
  }

  return campaigns;
}

} // namespace

auto load_campaign_definitions() -> std::vector<Game::Campaign::CampaignDefinition> {
  auto campaigns = load_from_directories();
  if (!campaigns.empty()) {
    return campaigns;
  }
  return load_from_resources();
}

auto build_campaign_entry(const Game::Campaign::CampaignDefinition& campaign,
                          const QVariantList& missions_progress) -> QVariantMap {
  QVariantMap campaign_map;
  campaign_map.insert(QStringLiteral("id"), campaign.id);
  campaign_map.insert(
      QStringLiteral("title"),
      Game::Util::tr_asset(Game::Util::k_campaigns_context, campaign.title));
  campaign_map.insert(
      QStringLiteral("description"),
      Game::Util::tr_asset(Game::Util::k_campaigns_context, campaign.description));
  campaign_map.insert(QStringLiteral("unlocked"), true);

  bool all_completed = true;
  QVariantList missions_list;
  for (const auto& mission : campaign.missions) {
    QVariantMap mission_map;
    mission_map.insert(QStringLiteral("mission_id"), mission.mission_id);
    mission_map.insert(QStringLiteral("order_index"), mission.order_index);
    if (mission.intro_text.has_value()) {
      mission_map.insert(
          QStringLiteral("intro_text"),
          Game::Util::tr_asset(Game::Util::k_campaigns_context, *mission.intro_text));
    }
    if (mission.outro_text.has_value()) {
      mission_map.insert(
          QStringLiteral("outro_text"),
          Game::Util::tr_asset(Game::Util::k_campaigns_context, *mission.outro_text));
    }
    if (mission.difficulty_modifier.has_value()) {
      mission_map.insert(QStringLiteral("difficulty_modifier"),
                         *mission.difficulty_modifier);
    }

    if (mission.world_region_id.has_value()) {
      mission_map.insert(QStringLiteral("world_region_id"), *mission.world_region_id);
    }

    bool unlocked = mission.order_index == 0;
    bool completed = false;
    for (const QVariant& progress_var : missions_progress) {
      const QVariantMap progress = progress_var.toMap();
      if (progress[QStringLiteral("mission_id")].toString() == mission.mission_id) {
        unlocked = progress[QStringLiteral("unlocked")].toBool();
        completed = progress[QStringLiteral("completed")].toBool();
        break;
      }
    }

    mission_map.insert(QStringLiteral("unlocked"), unlocked);
    mission_map.insert(QStringLiteral("completed"), completed);
    missions_list.append(mission_map);

    if (!completed) {
      all_completed = false;
    }
  }

  campaign_map.insert(QStringLiteral("completed"), all_completed);
  campaign_map.insert(QStringLiteral("missions"), missions_list);
  return campaign_map;
}
} // namespace Game::Systems::SaveCampaignCatalog
