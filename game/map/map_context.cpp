#include "map/map_context.h"

#include <QDateTime>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>

#include <algorithm>
#include <deque>
#include <utility>

#include "map/map_loader.h"
#include "utils/resource_utils.h"

namespace Game::Map {
namespace {

constexpr int k_retained_maps = 3;

struct CacheEntry {
  QString resolved_path;
  qint64 size = -1;
  qint64 modified_ms = -1;
  std::shared_ptr<const MapDefinition> definition;
};

struct Cache {
  QMutex mutex;
  std::deque<CacheEntry> entries;
  MapContextStore::Statistics statistics;
};

auto cache() -> Cache& {
  static Cache instance;
  return instance;
}

struct FileStamp {
  qint64 size = -1;
  qint64 modified_ms = -1;
};

auto stamp_of(const QString& resolved_path) -> FileStamp {
  const QFileInfo info(resolved_path);
  if (!info.exists()) {
    return {};
  }
  return {.size = info.size(), .modified_ms = info.lastModified().toMSecsSinceEpoch()};
}

} // namespace

MapContext::MapContext(QString source_path,
                       QString resolved_path,
                       std::shared_ptr<const MapDefinition> definition)
    : m_source_path(std::move(source_path))
    , m_resolved_path(std::move(resolved_path))
    , m_definition(std::move(definition)) {
}

auto MapContextStore::acquire(const QString& map_path, QString* error) -> MapContext {
  if (map_path.isEmpty()) {
    if (error != nullptr) {
      *error = QStringLiteral("empty map path");
    }
    return {};
  }

  const QString resolved = Utils::Resources::resolve_resource_path(map_path);
  const FileStamp stamp = stamp_of(resolved);

  auto& store = cache();
  {
    const QMutexLocker locker(&store.mutex);
    ++store.statistics.requests;
    const auto found = std::find_if(
        store.entries.begin(), store.entries.end(), [&](const CacheEntry& entry) {
          return entry.resolved_path == resolved && entry.size == stamp.size &&
                 entry.modified_ms == stamp.modified_ms;
        });
    if (found != store.entries.end()) {
      ++store.statistics.reuses;
      auto definition = found->definition;
      CacheEntry promoted = *found;
      store.entries.erase(found);
      store.entries.push_front(std::move(promoted));
      return {map_path, resolved, std::move(definition)};
    }
  }

  auto parsed = std::make_shared<MapDefinition>();
  QString parse_error;
  const bool ok = MapLoader::load_from_json_file(resolved, *parsed, &parse_error);

  {
    const QMutexLocker locker(&store.mutex);
    ++store.statistics.parses;
    if (ok) {
      store.entries.push_front({.resolved_path = resolved,
                                .size = stamp.size,
                                .modified_ms = stamp.modified_ms,
                                .definition = parsed});
      while (store.entries.size() > k_retained_maps) {
        store.entries.pop_back();
      }
    }
  }

  if (!ok) {
    if (error != nullptr) {
      *error = parse_error;
    }
    return {};
  }

  return {map_path, resolved, std::const_pointer_cast<const MapDefinition>(parsed)};
}

void MapContextStore::clear() {
  auto& store = cache();
  const QMutexLocker locker(&store.mutex);
  store.entries.clear();
}

auto MapContextStore::statistics() -> Statistics {
  auto& store = cache();
  const QMutexLocker locker(&store.mutex);
  return store.statistics;
}

void MapContextStore::reset_statistics() {
  auto& store = cache();
  const QMutexLocker locker(&store.mutex);
  store.statistics = {};
}

} // namespace Game::Map
