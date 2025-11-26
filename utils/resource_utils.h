#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QStringList>

namespace Utils::Resources {

// Resolve resources that may have been relocated under a Qt QML module prefix.
inline auto resolveResourcePath(const QString &path) -> QString {
  if (path.isEmpty()) {
    return path;
  }

  auto exists = [](const QString &candidate) {
    QFileInfo const info(candidate);
    if (info.exists()) {
      return true;
    }
    QDir const dir(candidate);
    return dir.exists();
  };

  if (exists(path)) {
    return path;
  }

  if (!path.startsWith(QStringLiteral(":/"))) {
    return path;
  }

  const QString relative = path.mid(2); // strip ":/"

  // Fallbacks for development and packaging where resources live on disk
  auto search_upwards = [&](const QString &startDir) -> QString {
    if (startDir.isEmpty()) {
      return {};
    }
    QDir dir(startDir);
    for (int i = 0; i < 5; ++i) {
      QString candidate = dir.filePath(relative);
      if (exists(candidate)) {
        return candidate;
      }
      if (!dir.cdUp()) {
        break;
      }
    }
    return {};
  };

  if (QString candidate =
          search_upwards(QCoreApplication::applicationDirPath());
      !candidate.isEmpty()) {
    return candidate;
  }
  if (QString candidate = search_upwards(QDir::currentPath());
      !candidate.isEmpty()) {
    return candidate;
  }

  static const QStringList kAlternateRoots = {
      QStringLiteral(":/StandardOfIron"),
      QStringLiteral(":/qt/qml/StandardOfIron"),
      QStringLiteral(":/qt/qml/default")};

  for (const auto &root : kAlternateRoots) {
    QString candidate = root;
    if (!candidate.endsWith('/')) {
      candidate.append('/');
    }
    candidate.append(relative);
    if (exists(candidate)) {
      return candidate;
    }
  }

  return path;
}

} // namespace Utils::Resources
