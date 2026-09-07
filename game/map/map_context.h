#pragma once

#include <QString>

#include <cstdint>
#include <memory>

#include "map/map_definition.h"

namespace Game::Map {

class MapContext {
public:
  MapContext() = default;
  MapContext(QString source_path,
             QString resolved_path,
             std::shared_ptr<const MapDefinition> definition);

  [[nodiscard]] auto valid() const -> bool { return m_definition != nullptr; }

  [[nodiscard]] auto definition() const -> const MapDefinition* {
    return m_definition.get();
  }

  [[nodiscard]] auto
  shared_definition() const -> const std::shared_ptr<const MapDefinition>& {
    return m_definition;
  }

  [[nodiscard]] auto source_path() const -> const QString& { return m_source_path; }
  [[nodiscard]] auto resolved_path() const -> const QString& { return m_resolved_path; }

private:
  QString m_source_path;
  QString m_resolved_path;
  std::shared_ptr<const MapDefinition> m_definition;
};

class MapContextStore {
public:
  struct Statistics {
    std::uint64_t requests = 0;
    std::uint64_t parses = 0;
    std::uint64_t reuses = 0;
  };

  [[nodiscard]] static auto acquire(const QString& map_path,
                                    QString* error = nullptr) -> MapContext;

  static void clear();

  [[nodiscard]] static auto statistics() -> Statistics;
  static void reset_statistics();
};

} // namespace Game::Map
