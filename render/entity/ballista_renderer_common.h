#pragma once

#include <QVector3D>

#include <string_view>

#include "registry.h"

namespace Render::GL {
class Mesh;
class Texture;

using BallistaBodyDrawer =
    void (*)(const DrawContext&, ISubmitter&, Mesh*, Texture*, const QVector3D&);

struct BallistaRendererConfig {
  std::string_view renderer_key;
  QVector3D default_team;
  BallistaBodyDrawer draw_body;
};

void register_ballista_renderer_variant(EntityRendererRegistry& registry,
                                        const BallistaRendererConfig& config);

} // namespace Render::GL
