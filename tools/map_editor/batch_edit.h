#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include "element_ops.h"
#include "map_data.h"

namespace MapEditor::BatchEdit {

struct Report {
  int applied = 0;
  QStringList log;
  QStringList errors;

  [[nodiscard]] auto ok() const -> bool { return errors.isEmpty(); }
};

[[nodiscard]] auto kind_name(int kind) -> QString;
[[nodiscard]] auto kind_from_name(const QString& name) -> int;

[[nodiscard]] auto element_json(const ElementSnapshot& snapshot) -> QJsonObject;

[[nodiscard]] auto inventory(const MapData& data,
                             int only_kind = -1,
                             bool include_json = false) -> QJsonArray;

[[nodiscard]] auto summary(const MapData& data) -> QJsonObject;

[[nodiscard]] auto find_by_id(const MapData& data, int kind, const QString& id) -> int;

auto apply(MapData& data, const QJsonArray& operations) -> Report;

} // namespace MapEditor::BatchEdit
