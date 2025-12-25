#pragma once

#include "nation_registry.h"
#include <QString>
#include <optional>
#include <vector>

namespace Game::Systems {

class NationLoader {
public:
  static auto load_from_file(const QString &path) -> std::optional<Nation>;
  static auto
  load_from_directory(const QString &directory) -> std::vector<Nation>;
  static auto load_default_nations() -> std::vector<Nation>;

private:
  static auto resolve_data_path(const QString &relative) -> QString;
};

} 
