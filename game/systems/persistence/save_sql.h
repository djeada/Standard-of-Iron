#pragma once

#include <QCoreApplication>
#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlError>
#include <QString>

namespace Game::Systems::SaveSql {

inline auto last_error_string(const QSqlError& error) -> QString {
  if (error.type() == QSqlError::NoError) {
    return {};
  }
  return error.text();
}

inline auto
fail(QString* out_error, const QString& context, const QSqlError& error) -> bool {
  if (out_error != nullptr) {
    *out_error = QStringLiteral("%1: %2").arg(context, last_error_string(error));
  }
  return false;
}

inline auto now_iso() -> QString {
  return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

inline auto text(const QString& value) -> QString {
  return value.isNull() ? QString::fromLatin1("") : value;
}

class TransactionGuard {
public:
  explicit TransactionGuard(QSqlDatabase& database)
      : m_database(database) {}

  TransactionGuard(const TransactionGuard&) = delete;
  auto operator=(const TransactionGuard&) -> TransactionGuard& = delete;

  auto begin(QString* out_error) -> bool {
    if (!m_database.transaction()) {
      return fail(
          out_error,
          QCoreApplication::translate("SaveStorage", "Failed to begin transaction"),
          m_database.lastError());
    }
    m_active = true;
    return true;
  }

  auto commit(QString* out_error) -> bool {
    if (!m_active) {
      return true;
    }

    if (!m_database.commit()) {
      const bool result = fail(
          out_error,
          QCoreApplication::translate("SaveStorage", "Failed to commit transaction"),
          m_database.lastError());
      rollback();
      return result;
    }

    m_active = false;
    return true;
  }

  void rollback() {
    if (m_active) {
      m_database.rollback();
      m_active = false;
    }
  }

  ~TransactionGuard() { rollback(); }

private:
  QSqlDatabase& m_database;
  bool m_active = false;
};

} // namespace Game::Systems::SaveSql
