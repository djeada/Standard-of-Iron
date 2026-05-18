#pragma once

#include <QString>
#include <QVector3D>

#include <string>
#include <unordered_map>

#include "../core/component.h"

namespace Game::Visuals {

struct VisualDef {
  Engine::Core::RenderableComponent::MeshKind mesh =
      Engine::Core::RenderableComponent::MeshKind::Cube;
  QVector3D color{1.0F, 1.0F, 1.0F};
  QString texture;
};

class VisualCatalog {
public:
  auto load_from_json_file(const QString& path, QString* out_error = nullptr) -> bool;
  auto lookup(const std::string& unit_type, VisualDef& out) const -> bool;

private:
  std::unordered_map<std::string, VisualDef> m_units;
};

auto mesh_kind_from_string(const QString& s)
    -> Engine::Core::RenderableComponent::MeshKind;

void apply_to_renderable(const VisualDef& def, Engine::Core::RenderableComponent& r);

} // namespace Game::Visuals
