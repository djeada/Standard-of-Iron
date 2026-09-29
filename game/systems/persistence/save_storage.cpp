#include "save_storage.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QVariant>

#include <utility>

#include "save_sql.h"

namespace Game::Systems {

namespace {
constexpr const char* k_driver_name = "QSQLITE";

auto build_connection_name(const SaveStorage* instance) -> QString {
  return QStringLiteral("SaveStorage_%1")
      .arg(reinterpret_cast<quintptr>(instance), 0, 16);
}

} // namespace

using SaveSql::fail;

SaveStorage::SaveStorage(QString database_path)
    : m_database_path(std::move(database_path))
    , m_connection_name(build_connection_name(this)) {
}

SaveStorage::~SaveStorage() {
  if (m_database.isValid()) {
    if (m_database.isOpen()) {
      m_database.close();
    }
    const QString connection_name = m_connection_name;
    m_database = QSqlDatabase();
    QSqlDatabase::removeDatabase(connection_name);
  }
}

auto SaveStorage::initialize(QString* out_error) const -> bool {
  if (m_initialized && m_database.isValid() && m_database.isOpen()) {
    return true;
  }

  QString error;
  bool ready = open(&error);

  if (!ready && !is_memory_database() && QFile::exists(m_database_path)) {
    QString quarantine_error;
    if (quarantine_and_recreate(error, &quarantine_error)) {
      m_health_error.clear();
      m_initialized = true;
      return true;
    }
    error = quarantine_error;
  }

  if (!ready || !ensure_schema(&error)) {
    m_health_error = error.isEmpty()
                         ? QCoreApplication::translate(
                               "SaveStorage", "The save database could not be opened.")
                         : error;
    if (out_error != nullptr) {
      *out_error = m_health_error;
    }
    return false;
  }

  m_health_error.clear();
  m_initialized = true;
  return true;
}

auto SaveStorage::is_memory_database() const -> bool {
  return m_database_path.startsWith(QStringLiteral(":memory:")) ||
         m_database_path.contains(QStringLiteral("mode=memory"));
}

void SaveStorage::close_connection() const {
  if (m_database.isValid()) {
    if (m_database.isOpen()) {
      m_database.close();
    }
    m_database = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connection_name);
  }
  m_initialized = false;
}

auto SaveStorage::open(QString* out_error) const -> bool {
  if (m_database.isValid() && m_database.isOpen()) {
    return true;
  }

  if (!m_database.isValid()) {
    m_database = QSqlDatabase::addDatabase(k_driver_name, m_connection_name);
    m_database.setDatabaseName(m_database_path);
    m_database.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=15000"));
  }

  if (!m_database.open()) {
    return fail(
        out_error,
        QCoreApplication::translate("SaveStorage", "Failed to open save database"),
        m_database.lastError());
  }

  QSqlQuery pragma(m_database);
  if (pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL")) && pragma.next()) {
    const QString mode = pragma.value(0).toString();
    if (mode.compare(QStringLiteral("wal"), Qt::CaseInsensitive) != 0 &&
        !is_memory_database()) {
      qWarning() << "SaveStorage: journal_mode is" << mode
                 << "not WAL; a crash mid-save may leave the database behind";
    }
  }
  pragma.finish();
  pragma.exec(QStringLiteral("PRAGMA synchronous=FULL"));
  pragma.finish();
  constexpr int k_synchronous_full = 2;
  if (pragma.exec(QStringLiteral("PRAGMA synchronous")) && pragma.next() &&
      pragma.value(0).toInt() != k_synchronous_full) {
    qWarning() << "SaveStorage: synchronous is" << pragma.value(0).toInt()
               << "not FULL; a commit may not have reached the disk when it "
                  "reports success";
  }
  pragma.finish();
  pragma.exec(QStringLiteral("PRAGMA foreign_keys=ON"));
  return true;
}

} // namespace Game::Systems
