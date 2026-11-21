#pragma once

#include <QByteArray>
#include <QJsonValue>
#include <QString>

#include <utility>
#include <vector>

namespace MapEditor::JsonText {

struct Node {
  enum class Type {
    Null,
    Bool,
    Number,
    String,
    Array,
    Object
  };

  Type type = Type::Null;

  QString text;
  std::vector<std::pair<QString, Node>> members;
  std::vector<Node> items;

  [[nodiscard]] auto find(const QString& key) const -> const Node*;
};

[[nodiscard]] auto parse(const QByteArray& utf8, Node& out, QString* error) -> bool;

[[nodiscard]] auto to_value(const Node& node) -> QJsonValue;
[[nodiscard]] auto from_value(const QJsonValue& value) -> Node;

struct Style {
  int indent = 2;
  bool trailing_newline = true;
};

[[nodiscard]] auto detect_style(const QByteArray& utf8) -> Style;

[[nodiscard]] auto write(const Node& node, const Style& style = {}) -> QByteArray;

[[nodiscard]] auto merge(const Node& original,
                         const QJsonValue& canonical,
                         const QJsonValue& current) -> Node;

} // namespace MapEditor::JsonText
