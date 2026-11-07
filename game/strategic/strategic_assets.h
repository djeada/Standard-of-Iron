#pragma once

#include <QString>

#include <memory>
#include <optional>

#include "strategic_geometry.h"
#include "strategic_validation.h"
#include "strategic_world.h"

namespace Game::Strategic {

inline constexpr const char* k_default_world_path =
    "assets/world/second_punic_war.json";

struct WorldBundle {
  WorldDefinition definition;
  std::shared_ptr<RegionGeometry> geometry;
  ValidationContext context;
  QString root;
  QString path;
};

[[nodiscard]] auto find_repository_root(const QString& start) -> QString;

[[nodiscard]] auto resolve_asset_path(const QString& root,
                                      const QString& path) -> QString;

[[nodiscard]] auto
build_validation_context(const QString& root,
                         const WorldDefinition& def,
                         const RegionGeometry* geometry) -> ValidationContext;

[[nodiscard]] auto
load_world_bundle(const QString& world_path,
                  const QString& root,
                  QString* error = nullptr) -> std::optional<WorldBundle>;

} // namespace Game::Strategic
