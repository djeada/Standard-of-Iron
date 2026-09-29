#include <QCoreApplication>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include "map/campaign_definition.h"
#include "save_campaign_catalog.h"
#include "save_sql.h"
#include "save_storage.h"

namespace Game::Systems {

namespace {

using SaveCampaignCatalog::build_campaign_entry;
using SaveCampaignCatalog::load_campaign_definitions;
using SaveSql::fail;
using SaveSql::now_iso;
using SaveSql::text;
using SaveSql::TransactionGuard;

auto register_campaign_missions(const QSqlDatabase& database,
                                const Game::Campaign::CampaignDefinition& campaign,
                                QString* out_error) -> bool {
  for (const auto& mission : campaign.missions) {
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "INSERT INTO campaign_missions (campaign_id, mission_id, order_index, "
        "unlocked, completed) VALUES (:campaign_id, :mission_id, :order_index, "
        ":unlocked, 0) "
        "ON CONFLICT(campaign_id, mission_id) DO UPDATE SET "
        "order_index = excluded.order_index"));
    query.bindValue(QStringLiteral(":campaign_id"), campaign.id);
    query.bindValue(QStringLiteral(":mission_id"), mission.mission_id);
    query.bindValue(QStringLiteral(":order_index"), mission.order_index);
    query.bindValue(QStringLiteral(":unlocked"), mission.order_index == 0 ? 1 : 0);

    if (!query.exec()) {
      fail(out_error,
           QCoreApplication::translate("SaveStorage",
                                       "Failed to register campaign mission"),
           query.lastError());
      return false;
    }
  }
  return true;
}

auto prune_removed_missions(const QSqlDatabase& database,
                            const Game::Campaign::CampaignDefinition& campaign,
                            QString* out_error) -> bool {
  QStringList placeholders;
  placeholders.reserve(static_cast<int>(campaign.missions.size()));
  for (int i = 0; i < static_cast<int>(campaign.missions.size()); ++i) {
    placeholders.append(QStringLiteral(":mission%1").arg(i));
  }

  QSqlQuery prune(database);
  const QString sql =
      campaign.missions.empty()
          ? QStringLiteral("DELETE FROM campaign_missions WHERE campaign_id = "
                           ":campaign_id AND completed = 0")
          : QStringLiteral("DELETE FROM campaign_missions WHERE campaign_id = "
                           ":campaign_id AND completed = 0 AND mission_id NOT IN (%1)")
                .arg(placeholders.join(QStringLiteral(", ")));
  prune.prepare(sql);
  prune.bindValue(QStringLiteral(":campaign_id"), campaign.id);
  for (int i = 0; i < static_cast<int>(campaign.missions.size()); ++i) {
    prune.bindValue(placeholders[i],
                    campaign.missions[static_cast<std::size_t>(i)].mission_id);
  }

  if (!prune.exec()) {
    fail(out_error,
         QCoreApplication::translate("SaveStorage",
                                     "Failed to prune removed campaign missions"),
         prune.lastError());
    return false;
  }
  return true;
}

auto look_up_mission(const QSqlDatabase& database,
                     const QString& campaign_id,
                     const QString& mission_id,
                     int& completed_order,
                     bool& was_completed,
                     QString* out_error) -> bool {
  QSqlQuery order_query(database);
  order_query.prepare(
      QStringLiteral("SELECT order_index, completed FROM campaign_missions "
                     "WHERE campaign_id = :campaign_id AND mission_id = :mission_id"));
  order_query.bindValue(QStringLiteral(":campaign_id"), campaign_id);
  order_query.bindValue(QStringLiteral(":mission_id"), mission_id);

  if (!order_query.exec()) {
    fail(out_error,
         QCoreApplication::translate("SaveStorage",
                                     "Failed to look up the completed mission"),
         order_query.lastError());
    return false;
  }

  if (!order_query.next()) {
    if (out_error != nullptr) {
      *out_error = QCoreApplication::translate("SaveStorage",
                                               "Mission %1 is not part of campaign %2")
                       .arg(mission_id, campaign_id);
    }
    return false;
  }
  completed_order = order_query.value(0).toInt();
  was_completed = order_query.value(1).toInt() != 0;
  return true;
}

auto mark_mission_completed(const QSqlDatabase& database,
                            const QString& campaign_id,
                            const QString& mission_id,
                            QString* out_error) -> bool {
  QSqlQuery complete_query(database);
  complete_query.prepare(
      QStringLiteral("UPDATE campaign_missions SET completed = 1, unlocked = 1, "
                     "completed_at = :completed_at "
                     "WHERE campaign_id = :campaign_id AND mission_id = :mission_id"));
  complete_query.bindValue(QStringLiteral(":completed_at"), now_iso());
  complete_query.bindValue(QStringLiteral(":campaign_id"), campaign_id);
  complete_query.bindValue(QStringLiteral(":mission_id"), mission_id);

  if (!complete_query.exec()) {
    fail(out_error,
         QCoreApplication::translate("SaveStorage",
                                     "Failed to mark mission as completed"),
         complete_query.lastError());
    return false;
  }
  return true;
}

auto unlock_next_mission(const QSqlDatabase& database,
                         const QString& campaign_id,
                         int completed_order,
                         QString& unlocked_mission_id,
                         QString* out_error) -> bool {
  QSqlQuery next_query(database);
  next_query.prepare(
      QStringLiteral("SELECT mission_id FROM campaign_missions "
                     "WHERE campaign_id = :campaign_id AND order_index > :order "
                     "ORDER BY order_index ASC LIMIT 1"));
  next_query.bindValue(QStringLiteral(":campaign_id"), campaign_id);
  next_query.bindValue(QStringLiteral(":order"), completed_order);

  if (!next_query.exec()) {
    fail(out_error,
         QCoreApplication::translate("SaveStorage", "Failed to find the next mission"),
         next_query.lastError());
    return false;
  }

  if (next_query.next()) {
    unlocked_mission_id = next_query.value(0).toString();

    QSqlQuery unlock_query(database);
    unlock_query.prepare(QStringLiteral(
        "UPDATE campaign_missions SET unlocked = 1 "
        "WHERE campaign_id = :campaign_id AND mission_id = :mission_id"));
    unlock_query.bindValue(QStringLiteral(":campaign_id"), campaign_id);
    unlock_query.bindValue(QStringLiteral(":mission_id"), unlocked_mission_id);

    if (!unlock_query.exec()) {
      fail(out_error,
           QCoreApplication::translate("SaveStorage", "Failed to unlock next mission"),
           unlock_query.lastError());
      return false;
    }
  }
  return true;
}

auto campaign_has_no_open_mission(const QSqlDatabase& database,
                                  const QString& campaign_id,
                                  bool& campaign_completed,
                                  QString* out_error) -> bool {
  QSqlQuery remaining_query(database);
  remaining_query.prepare(
      QStringLiteral("SELECT COUNT(*) FROM campaign_missions "
                     "WHERE campaign_id = :campaign_id AND completed = 0"));
  remaining_query.bindValue(QStringLiteral(":campaign_id"), campaign_id);

  if (!remaining_query.exec() || !remaining_query.next()) {
    fail(out_error,
         QCoreApplication::translate("SaveStorage",
                                     "Failed to count remaining missions"),
         remaining_query.lastError());
    return false;
  }

  campaign_completed = remaining_query.value(0).toInt() == 0;
  return true;
}

auto record_campaign_completion(const QSqlDatabase& database,
                                const QString& campaign_id,
                                QString* out_error) -> bool {
  QSqlQuery campaign_query(database);
  campaign_query.prepare(
      QStringLiteral("INSERT INTO campaign_progress (campaign_id, completed, "
                     "unlocked, completed_at) VALUES (:campaign_id, 1, 1, "
                     ":completed_at) "
                     "ON CONFLICT(campaign_id) DO UPDATE SET "
                     "completed = 1, unlocked = 1, "
                     "completed_at = COALESCE(campaign_progress.completed_at, "
                     "excluded.completed_at)"));
  campaign_query.bindValue(QStringLiteral(":campaign_id"), campaign_id);
  campaign_query.bindValue(QStringLiteral(":completed_at"), now_iso());

  if (!campaign_query.exec()) {
    fail(out_error,
         QCoreApplication::translate("SaveStorage",
                                     "Failed to mark campaign as completed"),
         campaign_query.lastError());
    return false;
  }
  return true;
}

} // namespace

auto SaveStorage::list_campaigns(QString* out_error) -> QVariantList {
  QVariantList result;
  if (!initialize(out_error)) {
    return result;
  }

  const auto campaigns = load_campaign_definitions();
  for (const auto& campaign : campaigns) {
    QString db_error;
    if (!ensure_campaign_missions_in_db(campaign, &db_error)) {
      qWarning() << "Failed to initialize campaign missions in DB for" << campaign.id
                 << ":" << db_error;
      continue;
    }
    result.append(
        build_campaign_entry(campaign, get_campaign_mission_progress(campaign.id)));
  }

  if (result.isEmpty()) {
    if (out_error != nullptr) {
      *out_error = QStringLiteral("No campaigns found");
    }
    qWarning() << "No campaigns found in filesystem or Qt resources";
  } else {
    qInfo() << "Successfully loaded" << result.size() << "campaign(s)";
  }

  return result;
}

auto SaveStorage::get_campaign_progress(const QString& campaign_id,
                                        QString* out_error) const -> QVariantMap {
  QVariantMap result;
  if (!initialize(out_error)) {
    return result;
  }

  QSqlQuery query(m_database);
  query.prepare(
      QStringLiteral("SELECT completed, unlocked, completed_at FROM campaign_progress "
                     "WHERE campaign_id = :campaign_id"));
  query.bindValue(QStringLiteral(":campaign_id"), campaign_id);

  if (!query.exec()) {
    fail(out_error,
         QCoreApplication::translate("SaveStorage", "Failed to get campaign progress"),
         query.lastError());
    return result;
  }

  if (query.next()) {
    result.insert(QStringLiteral("completed"), query.value(0).toInt() != 0);
    result.insert(QStringLiteral("unlocked"), query.value(1).toInt() != 0);
    result.insert(QStringLiteral("completedAt"), query.value(2).toString());
  }

  return result;
}

auto SaveStorage::mark_campaign_completed(const QString& campaign_id,
                                          QString* out_error) -> bool {
  if (!initialize(out_error)) {
    return false;
  }

  TransactionGuard transaction(m_database);
  if (!transaction.begin(out_error)) {
    return false;
  }

  QSqlQuery query(m_database);
  query.prepare(QStringLiteral("INSERT INTO campaign_progress (campaign_id, completed, "
                               "unlocked, completed_at) "
                               "VALUES (:campaign_id, 1, 1, :completed_at) "
                               "ON CONFLICT(campaign_id) DO UPDATE SET "
                               "completed = 1, unlocked = 1, "
                               "completed_at = excluded.completed_at"));
  query.bindValue(QStringLiteral(":campaign_id"), campaign_id);
  query.bindValue(QStringLiteral(":completed_at"), now_iso());

  if (!query.exec()) {
    fail(out_error,
         QCoreApplication::translate("SaveStorage",
                                     "Failed to mark campaign as completed"),
         query.lastError());
    transaction.rollback();
    return false;
  }

  return transaction.commit(out_error);
}

auto SaveStorage::save_mission_result(const QString& mission_id,
                                      const QString& mode,
                                      const QString& campaign_id,
                                      bool completed,
                                      const QString& result,
                                      const QString& difficulty,
                                      float completion_time,
                                      QString* out_error) -> bool {
  if (!initialize(out_error)) {
    return false;
  }

  TransactionGuard transaction(m_database);
  if (!transaction.begin(out_error)) {
    return false;
  }

  const QString timestamp = now_iso();

  QSqlQuery query(m_database);
  if (!query.prepare(QStringLiteral(
          "INSERT INTO mission_results (mission_id, mode, campaign_id, completed, "
          "completion_time, difficulty, result, completed_at, created_at, "
          "updated_at) "
          "VALUES (:mission_id, :mode, :campaign_id, :completed, :completion_time, "
          ":difficulty, :result, :completed_at, :created_at, :updated_at) "
          "ON CONFLICT(mission_id, mode, campaign_id) DO UPDATE SET "
          "completed = excluded.completed, "
          "completion_time = excluded.completion_time, "
          "difficulty = excluded.difficulty, "
          "result = excluded.result, "
          "completed_at = excluded.completed_at, "
          "updated_at = excluded.updated_at"))) {
    return fail(out_error,
                QCoreApplication::translate("SaveStorage",
                                            "Failed to prepare mission result insert"),
                query.lastError());
  }

  query.bindValue(QStringLiteral(":mission_id"), text(mission_id));
  query.bindValue(QStringLiteral(":mode"), text(mode));
  query.bindValue(QStringLiteral(":campaign_id"), text(campaign_id));
  query.bindValue(QStringLiteral(":completed"), completed ? 1 : 0);
  query.bindValue(QStringLiteral(":completion_time"), completion_time);
  query.bindValue(QStringLiteral(":difficulty"), difficulty);
  query.bindValue(QStringLiteral(":result"), result);
  query.bindValue(QStringLiteral(":completed_at"), completed ? timestamp : QString());
  query.bindValue(QStringLiteral(":created_at"), timestamp);
  query.bindValue(QStringLiteral(":updated_at"), timestamp);

  if (!query.exec()) {
    fail(out_error,
         QCoreApplication::translate("SaveStorage", "Failed to save mission result"),
         query.lastError());
    transaction.rollback();
    return false;
  }

  return transaction.commit(out_error);
}

auto SaveStorage::get_mission_progress(const QString& mission_id,
                                       QString* out_error) const -> QVariantMap {
  QVariantMap result;
  if (!initialize(out_error)) {
    return result;
  }

  QSqlQuery query(m_database);
  query.prepare(QStringLiteral(
      "SELECT mode, campaign_id, completed, completion_time, difficulty, "
      "result, completed_at FROM mission_results "
      "WHERE mission_id = :mission_id ORDER BY updated_at DESC LIMIT 1"));
  query.bindValue(QStringLiteral(":mission_id"), mission_id);

  if (!query.exec()) {
    fail(out_error,
         QCoreApplication::translate("SaveStorage", "Failed to get mission progress"),
         query.lastError());
    return result;
  }

  if (query.next()) {
    result.insert(QStringLiteral("mode"), query.value(0).toString());
    result.insert(QStringLiteral("campaign_id"), query.value(1).toString());
    result.insert(QStringLiteral("completed"), query.value(2).toInt() != 0);
    result.insert(QStringLiteral("completion_time"), query.value(3).toDouble());
    result.insert(QStringLiteral("difficulty"), query.value(4).toString());
    result.insert(QStringLiteral("result"), query.value(5).toString());
    result.insert(QStringLiteral("completed_at"), query.value(6).toString());
  }

  return result;
}

auto SaveStorage::get_campaign_mission_progress(
    const QString& campaign_id, QString* out_error) const -> QVariantList {
  QVariantList result;
  if (!initialize(out_error)) {
    return result;
  }

  QSqlQuery query(m_database);
  query.prepare(QStringLiteral(
      "SELECT mission_id, order_index, unlocked, completed, completed_at "
      "FROM campaign_missions "
      "WHERE campaign_id = :campaign_id ORDER BY order_index ASC"));
  query.bindValue(QStringLiteral(":campaign_id"), campaign_id);

  if (!query.exec()) {
    fail(out_error,
         QCoreApplication::translate("SaveStorage",
                                     "Failed to get campaign mission progress"),
         query.lastError());
    return result;
  }

  while (query.next()) {
    QVariantMap mission;
    mission.insert(QStringLiteral("mission_id"), query.value(0).toString());
    mission.insert(QStringLiteral("order_index"), query.value(1).toInt());
    mission.insert(QStringLiteral("unlocked"), query.value(2).toInt() != 0);
    mission.insert(QStringLiteral("completed"), query.value(3).toInt() != 0);
    mission.insert(QStringLiteral("completed_at"), query.value(4).toString());
    result.append(mission);
  }

  return result;
}

auto SaveStorage::ensure_campaign_missions_in_db(
    const Game::Campaign::CampaignDefinition& campaign, QString* out_error) -> bool {
  if (!initialize(out_error)) {
    return false;
  }

  TransactionGuard transaction(m_database);
  if (!transaction.begin(out_error)) {
    return false;
  }

  if (!register_campaign_missions(m_database, campaign, out_error) ||
      !prune_removed_missions(m_database, campaign, out_error)) {
    transaction.rollback();
    return false;
  }

  return transaction.commit(out_error);
}

auto SaveStorage::complete_campaign_mission(const QString& campaign_id,
                                            const QString& mission_id,
                                            QString* out_error)
    -> std::optional<CampaignAdvance> {
  if (!initialize(out_error)) {
    return std::nullopt;
  }

  TransactionGuard transaction(m_database);
  if (!transaction.begin(out_error)) {
    return std::nullopt;
  }

  int completed_order = 0;
  bool was_completed = false;
  if (!look_up_mission(m_database,
                       campaign_id,
                       mission_id,
                       completed_order,
                       was_completed,
                       out_error) ||
      !mark_mission_completed(m_database, campaign_id, mission_id, out_error)) {
    transaction.rollback();
    return std::nullopt;
  }

  CampaignAdvance advance;
  advance.newly_completed = !was_completed;

  if (!unlock_next_mission(m_database,
                           campaign_id,
                           completed_order,
                           advance.unlocked_mission_id,
                           out_error) ||
      !campaign_has_no_open_mission(
          m_database, campaign_id, advance.campaign_completed, out_error)) {
    transaction.rollback();
    return std::nullopt;
  }

  if (advance.campaign_completed &&
      !record_campaign_completion(m_database, campaign_id, out_error)) {
    transaction.rollback();
    return std::nullopt;
  }

  if (!transaction.commit(out_error)) {
    return std::nullopt;
  }
  return advance;
}

} // namespace Game::Systems
