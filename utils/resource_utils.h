#pragma once

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <atomic>

namespace Utils::Resources {

struct ResourceResolution {
  QString path;
  bool found = false;
  QStringList checked;
};

inline auto resource_diagnostics_flag() -> std::atomic<bool>& {
  static std::atomic<bool> enabled{
      qEnvironmentVariableIntValue("SOI_RESOURCE_DIAGNOSTICS") != 0};
  return enabled;
}

[[nodiscard]] inline auto resource_diagnostics_enabled() -> bool {
  return resource_diagnostics_flag().load(std::memory_order_relaxed);
}

inline void set_resource_diagnostics_enabled(bool enabled) {
  resource_diagnostics_flag().store(enabled, std::memory_order_relaxed);
}

namespace Detail {

inline auto resolve_resource_path(const QString& path,
                                  bool collect) -> ResourceResolution {
  ResourceResolution result;
  result.path = path;
  if (path.isEmpty()) {
    return result;
  }

  const bool is_resource = path.startsWith(QStringLiteral(":/"));
  const QString relative = is_resource ? path.mid(2) : QString{};

  auto exists = [&](const QString& candidate) {
    if (collect && !result.checked.contains(candidate)) {
      result.checked.append(candidate);
    }
    QFileInfo const info(candidate);
    if (info.exists()) {
      return true;
    }
    QDir const dir(candidate);
    return dir.exists();
  };

  auto found = [&](const QString& candidate) -> ResourceResolution {
    result.path = candidate;
    result.found = true;
    return result;
  };

  if (is_resource) {
    auto search_upwards = [&](const QString& start_dir) -> QString {
      if (start_dir.isEmpty()) {
        return {};
      }
      QDir dir(start_dir);
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

    if (QString candidate = search_upwards(QCoreApplication::applicationDirPath());
        !candidate.isEmpty()) {
      return found(candidate);
    }
    if (QString candidate = search_upwards(QDir::currentPath()); !candidate.isEmpty()) {
      return found(candidate);
    }
  }

  if (exists(path)) {
    return found(path);
  }

  if (!is_resource) {
    return result;
  }

  static const QStringList k_alternate_roots = {
      QStringLiteral(":/StandardOfIron"),
      QStringLiteral(":/qt/qml/StandardOfIron"),
      QStringLiteral(":/qt/qml/default")};

  for (const auto& root : k_alternate_roots) {
    QString candidate = root;
    if (!candidate.endsWith('/')) {
      candidate.append('/');
    }
    candidate.append(relative);
    if (exists(candidate)) {
      return found(candidate);
    }
  }

  return result;
}

} // namespace Detail

[[nodiscard]] inline auto
resolve_resource_path_with_diagnostics(const QString& path) -> ResourceResolution {
  return Detail::resolve_resource_path(path, true);
}

[[nodiscard]] inline auto
format_resolution_failure(const ResourceResolution& resolution) -> QString {
  if (resolution.found || resolution.path.isEmpty()) {
    return {};
  }
  QString message =
      QStringLiteral("Could not resolve resource: %1\nChecked:").arg(resolution.path);
  for (const auto& candidate : resolution.checked) {
    message.append(QStringLiteral("\n- "));
    message.append(candidate);
  }
  return message;
}

inline auto resolve_resource_path(const QString& path) -> QString {
  if (!resource_diagnostics_enabled()) {
    return Detail::resolve_resource_path(path, false).path;
  }
  ResourceResolution resolution = resolve_resource_path_with_diagnostics(path);
  if (const QString message = format_resolution_failure(resolution);
      !message.isEmpty()) {
    qWarning().noquote() << message;
  }
  return resolution.path;
}

} // namespace Utils::Resources
