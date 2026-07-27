#pragma once

#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QVector>

namespace MapEditor {

// One documented key of an authored JSON object: what it is called, what it holds
// and what the loader falls back to when it is missing.
struct JsonFieldSpec {
  QString key;
  QString type;          // number, integer, string, bool, [x, z], array, object
  QString default_value; // shown verbatim; empty for required keys
  QString description;
  QStringList allowed; // enumerated values, empty when free-form
  bool required = false;
  QJsonValue placeholder; // inserted by "add missing keys"
};

// The set of keys the game reads for one kind of authored object, so the JSON
// editor can show what is valid instead of leaving the user guessing.
struct JsonSchema {
  QString title;
  QString summary;
  QVector<JsonFieldSpec> fields;

  [[nodiscard]] auto is_empty() const -> bool { return fields.isEmpty(); }
  [[nodiscard]] auto find(const QString& key) const -> const JsonFieldSpec*;
};

// element_kind matches ElementKind; sub_type is the object's own "type" value so
// the schema can narrow down (mountain vs hill, firecamp vs static prop, ...).
[[nodiscard]] auto schema_for_element(int element_kind,
                                      const QString& sub_type) -> JsonSchema;
[[nodiscard]] auto schema_for_biome() -> JsonSchema;

} // namespace MapEditor
