#pragma once

#include <QVector3D>

#include <string_view>

#include "registry.h"

namespace Render::GL {
class Mesh;
class Texture;

using CatapultBodyDrawer =
    void (*)(const DrawContext&, ISubmitter&, Mesh*, Texture*, const QVector3D&);

struct CatapultRendererConfig {
  std::string_view renderer_key;
  QVector3D default_team;
  CatapultBodyDrawer draw_body;
};

void register_catapult_renderer_variant(EntityRendererRegistry& registry,
                                        const CatapultRendererConfig& config);

} // namespace Render::GL
