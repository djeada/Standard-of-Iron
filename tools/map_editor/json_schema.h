#pragma once

#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QVector>

namespace MapEditor {

struct JsonFieldSpec {
  QString key;
  QString type;
  QString default_value;
  QString description;
  QStringList allowed;
  bool required = false;
  QJsonValue placeholder;
};

struct JsonSchema {
  QString title;
  QString summary;
  QVector<JsonFieldSpec> fields;

  [[nodiscard]] auto is_empty() const -> bool { return fields.isEmpty(); }
  [[nodiscard]] auto find(const QString& key) const -> const JsonFieldSpec*;
};

[[nodiscard]] auto schema_for_element(int element_kind,
                                      const QString& sub_type) -> JsonSchema;
[[nodiscard]] auto schema_for_biome() -> JsonSchema;

} // namespace MapEditor
