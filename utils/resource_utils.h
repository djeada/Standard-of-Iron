#pragma once

#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QStringList>

namespace Utils::Resources {

// Resolve resources that may have been relocated under a Qt QML module prefix.
inline QString resolveResourcePath(const QString &path) {
  if (path.isEmpty())
    return path;

  auto exists = [](const QString &candidate) {
    QFileInfo info(candidate);
    if (info.exists())
      return true;
    QDir dir(candidate);
    return dir.exists();
  };

  if (exists(path))
    return path;

  if (!path.startsWith(QStringLiteral(":/")))
    return path;

  static const QStringList kAlternateRoots = {
      QStringLiteral(":/StandardOfIron"),
      QStringLiteral(":/qt/qml/StandardOfIron"),
      QStringLiteral(":/qt/qml/default")};

  const QString relative = path.mid(2); // strip ":/"
  for (const auto &root : kAlternateRoots) {
    QString candidate = root;
    if (!candidate.endsWith('/'))
      candidate.append('/');
    candidate.append(relative);
    if (exists(candidate))
      return candidate;
  }

  return path;
}

} // namespace Utils::Resources
