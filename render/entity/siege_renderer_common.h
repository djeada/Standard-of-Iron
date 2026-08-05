#pragma once

#include <QVector3D>

#include <string_view>

#include "registry.h"

namespace Render::GL {
class Mesh;
class Texture;

using SiegeBodyDrawer =
    void (*)(const DrawContext&, ISubmitter&, Mesh*, Texture*, const QVector3D&);

struct SiegeRendererConfig {
  std::string_view renderer_key;
  QVector3D default_team;
  SiegeBodyDrawer draw_body;
};

void register_siege_renderer_variant(EntityRendererRegistry& registry,
                                     const SiegeRendererConfig& config);

} // namespace Render::GL
