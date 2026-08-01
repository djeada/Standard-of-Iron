#pragma once

#include <QString>

#include "promo_spec.h"

class ArenaViewport;

namespace Arena::Promo {

struct RunOptions {
  QString output_directory;

  bool write_posters{true};
};

[[nodiscard]] auto run(ArenaViewport& viewport,
                       const Spec& spec,
                       const RunOptions& options,
                       QString* error) -> int;

} // namespace Arena::Promo
