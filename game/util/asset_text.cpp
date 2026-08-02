#include "asset_text.h"

#include <QByteArray>
#include <QCoreApplication>

namespace Game::Util {

auto tr_asset(const char* context, const QString& source) -> QString {
  if (source.isEmpty()) {
    return source;
  }
  const QByteArray key = source.toUtf8();
  return QCoreApplication::translate(context, key.constData());
}

auto tr_asset(const char* context, const std::string& source) -> QString {
  if (source.empty()) {
    return {};
  }
  return QCoreApplication::translate(context, source.c_str());
}

auto tr_asset_std(const char* context, const std::string& source) -> std::string {
  if (source.empty()) {
    return source;
  }
  return QCoreApplication::translate(context, source.c_str()).toStdString();
}

} // namespace Game::Util
