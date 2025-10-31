#pragma once

#include <QString>

namespace Game::Units {

class TroopCatalogLoader {
public:
  static auto load_from_file(const QString &path) -> bool;
  static auto load_default_catalog() -> bool;
  static auto resolve_data_path(const QString &relative) -> QString;
};

} // namespace Game::Units
