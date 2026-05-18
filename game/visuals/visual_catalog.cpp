#include "visual_catalog.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <qfiledevice.h>
#include <qjsonarray.h>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <qstringview.h>

#include <string>

namespace Game::Visuals {

auto mesh_kind_from_string(const QString& s)
    -> Engine::Core::RenderableComponent::MeshKind {
  using MeshKind = Engine::Core::RenderableComponent::MeshKind;
  const QString t = s.trimmed().toLower();
  if (t == "quad") {
    return MeshKind::Quad;
  }
  if (t == "plane") {
    return MeshKind::Plane;
  }
  if (t == "cube") {
    return MeshKind::Cube;
  }
  if (t == "capsule") {
    return MeshKind::Capsule;
  }
  if (t == "ring") {
    return MeshKind::Ring;
  }
  return MeshKind::None;
}

auto VisualCatalog::load_from_json_file(const QString& path,
                                        QString* out_error) -> bool {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) {
    if (out_error != nullptr) {
      *out_error = QString("Failed to open visuals file: %1").arg(path);
    }
    return false;
  }
  const QByteArray data = f.readAll();
  f.close();
  QJsonParseError perr{};
  const QJsonDocument doc = QJsonDocument::fromJson(data, &perr);
  if (doc.isNull()) {
    if (out_error != nullptr) {
      *out_error = QString("JSON parse error at %1: %2")
                       .arg(perr.offset)
                       .arg(perr.errorString());
    }
    return false;
  }
  QJsonObject const root = doc.object();
  QJsonObject units = root.value("units").toObject();
  for (auto it = units.begin(); it != units.end(); ++it) {
    VisualDef def;
    QJsonObject const o = it.value().toObject();
    def.mesh = mesh_kind_from_string(o.value("mesh").toString("cube"));
    QJsonArray col = o.value("color").toArray();
    if (col.size() == 3) {
      def.color = QVector3D(float(col[0].toDouble(1.0)),
                            float(col[1].toDouble(1.0)),
                            float(col[2].toDouble(1.0)));
    }
    def.texture = o.value("texture").toString("");
    m_units.emplace(it.key().toStdString(), def);
  }
  return true;
}

auto VisualCatalog::lookup(const std::string& unit_type, VisualDef& out) const -> bool {
  auto it = m_units.find(unit_type);
  if (it == m_units.end()) {
    return false;
  }
  out = it->second;
  return true;
}

void apply_to_renderable(const VisualDef& def, Engine::Core::RenderableComponent& r) {
  r.mesh = def.mesh;
  r.color[0] = def.color.x();
  r.color[1] = def.color.y();
  r.color[2] = def.color.z();
  if (!def.texture.isEmpty()) {
    r.texture_path = def.texture.toStdString();
  }
}

} // namespace Game::Visuals
