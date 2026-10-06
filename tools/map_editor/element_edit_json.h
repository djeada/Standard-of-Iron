#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

#include "element_ops.h"

namespace MapEditor::ElementEditJson {

struct EditDocument {
  QJsonObject json;
  QString title;
  QString schema_sub_type;
  bool hill_projection = false;
};

struct EditResult {
  ElementSnapshot element;
  QString description;
  QStringList notes;
};

[[nodiscard]] auto
to_document(const ElementSnapshot& element) -> std::optional<EditDocument>;

[[nodiscard]] auto
from_json(const ElementSnapshot& before,
          const QJsonObject& json,
          const QVector<LinearElement>& linear_elements) -> EditResult;

[[nodiscard]] auto prettify_identifier(const QString& value) -> QString;

} // namespace MapEditor::ElementEditJson
