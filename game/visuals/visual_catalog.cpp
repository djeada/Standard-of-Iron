#include "visual_catalog.h"
#include "../core/component.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace Game::Visuals {

VisualDef::MeshKind meshKindFromString(const QString &s) {
  const QString t = s.trimmed().toLower();
  if (t == "quad") {
    return VisualDef::MeshKind::Quad;
  }
  if (t == "plane") {
    return VisualDef::MeshKind::Plane;
  }
  if (t == "cube") {
    return VisualDef::MeshKind::Cube;
  }
  if (t == "capsule") {
    return VisualDef::MeshKind::Capsule;
  }
  if (t == "ring") {
    return VisualDef::MeshKind::Ring;
  }
  return VisualDef::MeshKind::None;
}

static Engine::Core::RenderableComponent::MeshKind
toRenderableMesh(VisualDef::MeshKind k) {
  using RM = Engine::Core::RenderableComponent::MeshKind;
  switch (k) {
  case VisualDef::MeshKind::Quad:
    return RM::Quad;
  case VisualDef::MeshKind::Plane:
    return RM::Plane;
  case VisualDef::MeshKind::Cube:
    return RM::Cube;
  case VisualDef::MeshKind::Capsule:
    return RM::Capsule;
  case VisualDef::MeshKind::Ring:
    return RM::Ring;
  default:
    return RM::None;
  }
}

bool VisualCatalog::loadFromJsonFile(const QString &path, QString *outError) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) {
    if (outError) {
      *outError = QString("Failed to open visuals file: %1").arg(path);
    }
    return false;
  }
  const QByteArray data = f.readAll();
  f.close();
  QJsonParseError perr{};
  const QJsonDocument doc = QJsonDocument::fromJson(data, &perr);
  if (doc.isNull()) {
    if (outError) {
      *outError = QString("JSON parse error at %1: %2")
                      .arg(perr.offset)
                      .arg(perr.errorString());
    }
    return false;
  }
  QJsonObject root = doc.object();
  QJsonObject units = root.value("units").toObject();
  for (auto it = units.begin(); it != units.end(); ++it) {
    VisualDef def;
    QJsonObject o = it.value().toObject();
    def.mesh = meshKindFromString(o.value("mesh").toString("cube"));
    QJsonArray col = o.value("color").toArray();
    if (col.size() == 3) {
      def.color =
          QVector3D(float(col[0].toDouble(1.0)), float(col[1].toDouble(1.0)),
                    float(col[2].toDouble(1.0)));
    }
    def.texture = o.value("texture").toString("");
    m_units.emplace(it.key().toStdString(), def);
  }
  return true;
}

bool VisualCatalog::lookup(const std::string &unitType, VisualDef &out) const {
  auto it = m_units.find(unitType);
  if (it == m_units.end()) {
    return false;
  }
  out = it->second;
  return true;
}

void applyToRenderable(const VisualDef &def,
                       Engine::Core::RenderableComponent &r) {
  r.mesh = toRenderableMesh(def.mesh);
  r.color[0] = def.color.x();
  r.color[1] = def.color.y();
  r.color[2] = def.color.z();
  if (!def.texture.isEmpty()) {
    r.texturePath = def.texture.toStdString();
  }
}

} // namespace Game::Visuals
