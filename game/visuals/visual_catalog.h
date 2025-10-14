#pragma once

#include <QString>
#include <QVector3D>
#include <string>
#include <unordered_map>

namespace Engine {
namespace Core {
class RenderableComponent;
}
} 

namespace Game::Visuals {

struct VisualDef {

  enum class MeshKind { None, Quad, Plane, Cube, Capsule, Ring };
  MeshKind mesh = MeshKind::Cube;
  QVector3D color{1.0f, 1.0f, 1.0f};
  QString texture;
};

class VisualCatalog {
public:
  bool loadFromJsonFile(const QString &path, QString *outError = nullptr);
  bool lookup(const std::string &unitType, VisualDef &out) const;

private:
  std::unordered_map<std::string, VisualDef> m_units;
};

VisualDef::MeshKind meshKindFromString(const QString &s);

void applyToRenderable(const VisualDef &def,
                       Engine::Core::RenderableComponent &r);

} 
