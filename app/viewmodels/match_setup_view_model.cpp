#include "app/viewmodels/match_setup_view_model.h"

#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <algorithm>
#include <optional>

#include "app/core/client_context.h"
#include "game/game_config.h"
#include "game/map/map_catalog.h"
#include "game/mission/campaign_manager.h"
#include "game/mission/mission_commander_setup.h"
#include "game/mission/mission_definition_view.h"
#include "game/render_bridge/minimap/map_preview_generator.h"
#include "game/session/session_context.h"
#include "game/systems/nation_id.h"
#include "game/systems/nation_registry.h"
#include "game/systems/save_load_service.h"
#include "game/units/commander_catalog.h"
#include "game/units/troop_type.h"
#include "game/util/asset_text.h"
#include "utils/resource_utils.h"

namespace App::ViewModels {
namespace {

auto commander_entry(const Game::Units::CommanderDefinition& definition,
                     bool is_default) -> QVariantMap {
  QVariantMap entry;
  entry["id"] = QString::fromStdString(definition.id);
  entry["troop"] =
      QString::fromStdString(Game::Units::troop_typeToString(definition.troop_type));
  entry["display_name"] =
      Game::Util::tr_asset(Game::Util::k_commanders_context, definition.display_name);
  entry["battlefield_role"] = Game::Util::tr_asset(Game::Util::k_commanders_context,
                                                   definition.battlefield_role);
  entry["bonus_summary"] =
      Game::Util::tr_asset(Game::Util::k_commanders_context, definition.bonus_summary);
  entry["passive_aura"] =
      Game::Util::tr_asset(Game::Util::k_commanders_context, definition.passive_aura);
  entry["rally_ability"] =
      Game::Util::tr_asset(Game::Util::k_commanders_context, definition.rally_ability);
  entry["is_default"] = is_default;
  return entry;
}

auto map_slot_player_ids(const QString& map_path) -> QList<int> {
  const QString resolved = Utils::Resources::resolve_resource_path(map_path);
  QFile file(resolved);
  if (!file.open(QIODevice::ReadOnly)) {
    qWarning() << "Observer setup: cannot read map" << resolved;
    return {};
  }
  QJsonParseError error{};
  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
  file.close();
  if (error.error != QJsonParseError::NoError || !doc.isObject()) {
    qWarning() << "Observer setup: cannot parse map" << resolved << error.errorString();
    return {};
  }

  const QJsonObject root = doc.object();
  QSet<int> ids;
  for (const auto* collection : {"spawns", "structures"}) {
    const QJsonArray entries = root.value(QLatin1String(collection)).toArray();
    for (const QJsonValue entry_value : entries) {
      if (!entry_value.isObject()) {
        continue;
      }
      const int player_id = entry_value.toObject().value("player_id").toInt(0);
      if (player_id > 0) {
        ids.insert(player_id);
      }
    }
  }

  QList<int> sorted = ids.values();
  std::sort(sorted.begin(), sorted.end());
  return sorted;
}

} // namespace

MatchSetupViewModel::MatchSetupViewModel(const App::Core::ClientContext& context,
                                         App::Core::ClientHost& host,
                                         QObject* parent)
    : QObject(parent)
    , m_context(context)
    , m_host(host) {
}

void MatchSetupViewModel::start_loading_maps() {
  m_maps.begin_loading();
  emit maps_changed();
  if (auto* catalog = m_context.map_catalog) {
    catalog->load_maps_async();
  }
  load_campaigns();
}

void MatchSetupViewModel::set_maps(const QVariantList& maps) {
  m_maps.replace(maps);
  emit maps_changed();
}

void MatchSetupViewModel::append_map(const QVariantMap& map) {
  m_maps.append(map);
  emit maps_changed();
}

void MatchSetupViewModel::set_maps_loading(bool loading) {
  if (m_maps_loading == loading) {
    return;
  }
  m_maps_loading = loading;

  if (loading) {
    m_maps.begin_loading();
    emit maps_changed();
  } else {
    m_maps.end_loading();
  }
  emit maps_loading_changed();
}

auto MatchSetupViewModel::nations() const -> QVariantList {
  const auto frame_lock = m_host.lock_frame();

  if (m_context.session == nullptr) {
    return {};
  }
  QList<QVariantMap> ordered;
  const auto& all = m_context.session->nations().get_all_nations();
  ordered.reserve(static_cast<int>(all.size()));
  for (const auto& nation : all) {
    if (!nation.playable || !nation.selectable_in_skirmish) {
      continue;
    }
    QVariantMap entry;
    entry.insert(QStringLiteral("id"),
                 QString::fromStdString(Game::Systems::nation_id_to_string(nation.id)));
    entry.insert(
        QStringLiteral("name"),
        Game::Util::tr_asset(Game::Util::k_nations_context, nation.display_name));
    ordered.append(entry);
  }
  std::sort(
      ordered.begin(), ordered.end(), [](const QVariantMap& a, const QVariantMap& b) {
        return a.value(QStringLiteral("name"))
                   .toString()
                   .localeAwareCompare(b.value(QStringLiteral("name")).toString()) < 0;
      });

  QVariantList nations;
  nations.reserve(ordered.size());
  for (const auto& entry : ordered) {
    nations.append(entry);
  }
  return nations;
}

auto MatchSetupViewModel::commanders_for_nation(const QString& nation_id) const
    -> QVariantList {
  const auto frame_lock = m_host.lock_frame();

  if (m_context.session == nullptr) {
    return {};
  }
  const auto parsed = Game::Systems::nation_id_from_string(nation_id.toStdString());
  const auto nation = parsed.value_or(m_context.session->nations().default_nation_id());
  const QString default_troop =
      Game::Mission::resolve_commander_troop(nation_id, std::nullopt);

  auto definitions = Game::Units::commander_definitions_for_nation(nation);
  std::stable_sort(
      definitions.begin(),
      definitions.end(),
      [&default_troop](const Game::Units::CommanderDefinition* lhs,
                       const Game::Units::CommanderDefinition* rhs) {
        const auto is_default =
            [&default_troop](const Game::Units::CommanderDefinition* entry) {
              return entry != nullptr &&
                     QString::fromStdString(Game::Units::troop_typeToString(
                         entry->troop_type)) == default_troop;
            };
        return is_default(lhs) && !is_default(rhs);
      });

  QVariantList commanders;
  commanders.reserve(static_cast<int>(definitions.size()));
  for (const auto* definition : definitions) {
    if (definition == nullptr) {
      continue;
    }
    const QString troop =
        QString::fromStdString(Game::Units::troop_typeToString(definition->troop_type));
    commanders.append(commander_entry(
        *definition, troop.compare(default_troop, Qt::CaseInsensitive) == 0));
  }
  return commanders;
}

auto MatchSetupViewModel::map_preview(
    const QString& map_path, const QVariantList& player_configs) const -> QImage {
  Game::Map::Minimap::MapPreviewGenerator generator;
  return generator.generate_preview(map_path, player_configs);
}

auto MatchSetupViewModel::map_bases(const QString& map_path) const -> QVariantList {
  return Game::Map::Minimap::MapPreviewGenerator::base_markers(
      Utils::Resources::resolve_resource_path(map_path));
}

void MatchSetupViewModel::load_campaigns() {
  auto* saves = m_context.saves;
  auto* campaign = m_context.campaign;
  if (saves == nullptr || campaign == nullptr) {
    return;
  }

  QString error;
  auto campaigns = saves->list_campaigns(&error);
  if (!error.isEmpty()) {
    qWarning() << "Failed to load campaigns:" << error;
    return;
  }
  campaign->set_available_campaigns(campaigns);
}

auto MatchSetupViewModel::campaigns() const -> QVariantList {
  return m_context.campaign != nullptr ? m_context.campaign->available_campaigns()
                                       : QVariantList{};
}

auto MatchSetupViewModel::campaign_completed() const -> bool {
  auto* campaign = m_context.campaign;
  if (campaign == nullptr) {
    return false;
  }
  const QString campaign_id = campaign->current_campaign_id();
  if (campaign_id.isEmpty()) {
    return false;
  }
  if (campaign->campaign_completed()) {
    return true;
  }
  for (const QVariant& entry : campaign->available_campaigns()) {
    const QVariantMap row = entry.toMap();
    if (row.value(QStringLiteral("campaign_id")).toString() == campaign_id ||
        row.value(QStringLiteral("id")).toString() == campaign_id) {
      return row.value(QStringLiteral("completed")).toBool();
    }
  }
  return false;
}

auto MatchSetupViewModel::is_campaign_mission() const -> bool {
  return m_context.campaign != nullptr &&
         m_context.campaign->current_mission_context().is_campaign();
}

void MatchSetupViewModel::mark_current_mission_completed() {
  auto* campaign = m_context.campaign;
  if (campaign == nullptr) {
    return;
  }
  if (campaign->current_campaign_id().isEmpty()) {
    qWarning() << "No active campaign mission to mark as completed";
    return;
  }
  if (m_context.saves == nullptr) {
    qWarning() << "Save/Load service not initialized";
    return;
  }
  campaign->mark_current_mission_completed();
  load_campaigns();
}

auto MatchSetupViewModel::current_mission_objectives() const -> QVariantMap {
  auto* campaign = m_context.campaign;
  if (campaign == nullptr) {
    return {};
  }
  const auto& definition = campaign->current_mission_definition();
  if (!definition.has_value()) {
    return {};
  }
  return build_mission_objectives_map(*definition);
}

auto MatchSetupViewModel::mission_definition(const QString& mission_id) const
    -> QVariantMap {
  return load_mission_definition_map(mission_id);
}

auto MatchSetupViewModel::starting_gold() const -> int {
  return Game::GameConfig::instance().get_starting_gold();
}

void MatchSetupViewModel::set_starting_gold(int gold) {

  const int treasury = std::clamp(gold, 0, 10000);
  if (treasury == Game::GameConfig::instance().get_starting_gold()) {
    return;
  }
  Game::GameConfig::instance().set_starting_gold(treasury);
  emit starting_gold_changed();
}

void MatchSetupViewModel::start_skirmish(const QString& map_path,
                                         const QVariantList& player_configs) {
  const App::Core::MatchLaunch launch{.kind = QStringLiteral("skirmish"),
                                      .reference = map_path,
                                      .map_path = map_path,
                                      .player_configs = player_configs,
                                      .set_skirmish_context = true};
  emit launch_requested(launch);
}

auto MatchSetupViewModel::build_observer_player_configs(const QString& map_path) const
    -> QVariantList {
  QVariantList configs;
  const QList<int> slot_ids = map_slot_player_ids(map_path);
  if (slot_ids.size() < 2) {
    return configs;
  }

  QStringList nation_ids;
  if (m_context.session != nullptr) {
    for (const auto& nation : m_context.session->nations().get_all_nations()) {
      if (nation.playable && nation.selectable_in_skirmish) {
        nation_ids.append(
            QString::fromStdString(Game::Systems::nation_id_to_string(nation.id)));
      }
    }
  }
  if (nation_ids.isEmpty()) {
    nation_ids.append(QStringLiteral("roman_republic"));
    nation_ids.append(QStringLiteral("carthaginian_empire"));
  }
  if (nation_ids.size() == 1) {
    nation_ids.append(nation_ids.first());
  }

  int index = 0;
  for (const int player_id : slot_ids) {
    const int side = index % 2;
    const QString nation_id = nation_ids.at(side % nation_ids.size());
    QVariantMap config;
    config["player_id"] = player_id;
    config["playerName"] = tr("CPU %1").arg(player_id);
    config["colorIndex"] = index;
    config["team_id"] = side + 1;
    config["nationId"] = nation_id;
    config["commanderTroop"] =
        Game::Mission::resolve_commander_troop(nation_id, std::nullopt);
    config["isHuman"] = false;
    configs.append(config);
    ++index;
  }

  return configs;
}

auto MatchSetupViewModel::start_observed_skirmish(const QString& map_path) -> bool {
  const QVariantList configs = build_observer_player_configs(map_path);
  if (configs.size() < 2) {
    emit failed(tr("This battlefield has fewer than two slots to observe"));
    return false;
  }
  start_skirmish(map_path, configs);
  return true;
}

void MatchSetupViewModel::start_campaign_mission(const QString& mission_path) {
  auto* campaign = m_context.campaign;
  if (campaign == nullptr) {
    emit failed(tr("Campaign manager not initialized"));
    return;
  }

  int selected_player_id = 1;
  campaign->start_campaign_mission(mission_path, selected_player_id);
  if (!campaign->current_mission_definition().has_value()) {
    emit failed(tr("Failed to load mission"));
    return;
  }
  launch_current_mission(QStringLiteral("campaign-mission"), mission_path);
}

void MatchSetupViewModel::start_mission_file(const QString& file_path) {
  auto* campaign = m_context.campaign;
  if (campaign == nullptr) {
    emit failed(tr("Campaign manager not initialized"));
    return;
  }

  int selected_player_id = m_context.local_owner_id;
  QString error;
  if (!campaign->start_mission_file(file_path, selected_player_id, &error)) {
    emit failed(tr("Failed to load mission preview: %1").arg(error));
    return;
  }
  launch_current_mission(QStringLiteral("mission-file"), file_path);
}

void MatchSetupViewModel::start_tutorial() {
  start_mission_file(Utils::Resources::resolve_resource_path(
      QStringLiteral(":/assets/missions/tutorial.json")));
}

void MatchSetupViewModel::launch_current_mission(const QString& kind,
                                                 const QString& reference) {
  const auto& mission = *m_context.campaign->current_mission_definition();
  const App::Core::MatchLaunch launch{.kind = kind,
                                      .reference = reference,
                                      .map_path = mission.map_path,
                                      .player_configs =
                                          build_campaign_player_configs(mission),
                                      .set_skirmish_context = false};
  emit launch_requested(launch);
  emit current_mission_changed();
}

} // namespace App::ViewModels
