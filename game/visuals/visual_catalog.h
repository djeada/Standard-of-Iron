#pragma once

#include <QString>
#include <QVector3D>
#include <string>
#include <unordered_map>

namespace Engine::Core {
class RenderableComponent;
}

namespace Game::Visuals {

struct VisualDef {

  enum class MeshKind { None, Quad, Plane, Cube, Capsule, Ring };
  MeshKind mesh = MeshKind::Cube;
  QVector3D color{1.0F, 1.0F, 1.0F};
  QString texture;
};

class VisualCatalog {
public:
  auto loadFromJsonFile(const QString &path,
                        QString *out_error = nullptr) -> bool;
  auto lookup(const std::string &unitType, VisualDef &out) const -> bool;

private:
  std::unordered_map<std::string, VisualDef> m_units;
};

auto meshKindFromString(const QString &s) -> VisualDef::MeshKind;

void apply_to_renderable(const VisualDef &def,
                       Engine::Core::RenderableComponent &r);

} // namespace Game::Visuals
