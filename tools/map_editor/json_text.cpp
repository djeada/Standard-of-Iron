#include "json_text.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cmath>
#include <deque>

namespace MapEditor::JsonText {

namespace {

class Parser {
public:
  explicit Parser(const QByteArray& bytes)
      : m_bytes(bytes) {}

  auto parse_document(Node& out, QString* error) -> bool {
    skip_space();
    if (!parse_value(out)) {
      return fail(error);
    }
    skip_space();
    if (m_at != m_bytes.size()) {
      m_error = QStringLiteral("unexpected text after the document");
      return fail(error);
    }
    return true;
  }

private:
  auto fail(QString* error) const -> bool {
    if (error != nullptr) {
      *error = QStringLiteral("JSON parse error at byte %1: %2")
                   .arg(m_at)
                   .arg(m_error.isEmpty() ? QStringLiteral("invalid value") : m_error);
    }
    return false;
  }

  void skip_space() {
    while (m_at < m_bytes.size()) {
      const char c = m_bytes[m_at];
      if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
        break;
      }
      ++m_at;
    }
  }

  auto literal(const char* word, Node::Type type, Node& out) -> bool {
    const qsizetype length = static_cast<qsizetype>(qstrlen(word));
    if (m_bytes.mid(m_at, length) != word) {
      return false;
    }
    out.type = type;
    out.text = QString::fromLatin1(word);
    m_at += length;
    return true;
  }

  auto string_token(QString& raw) -> bool {
    if (m_at >= m_bytes.size() || m_bytes[m_at] != '"') {
      m_error = QStringLiteral("expected a string");
      return false;
    }
    const qsizetype begin = m_at;
    ++m_at;
    while (m_at < m_bytes.size()) {
      const char c = m_bytes[m_at];
      if (c == '\\') {
        m_at += 2;
        continue;
      }
      if (c == '"') {
        ++m_at;
        raw = QString::fromUtf8(m_bytes.mid(begin, m_at - begin));
        return true;
      }
      ++m_at;
    }
    m_error = QStringLiteral("unterminated string");
    return false;
  }

  auto parse_value(Node& out) -> bool {
    if (m_at >= m_bytes.size()) {
      m_error = QStringLiteral("unexpected end of document");
      return false;
    }
    const char c = m_bytes[m_at];
    if (c == '{') {
      return parse_object(out);
    }
    if (c == '[') {
      return parse_array(out);
    }
    if (c == '"') {
      out.type = Node::Type::String;
      return string_token(out.text);
    }
    if (c == 't') {
      return literal("true", Node::Type::Bool, out);
    }
    if (c == 'f') {
      return literal("false", Node::Type::Bool, out);
    }
    if (c == 'n') {
      return literal("null", Node::Type::Null, out);
    }
    const qsizetype begin = m_at;
    while (m_at < m_bytes.size()) {
      const char d = m_bytes[m_at];
      if ((d >= '0' && d <= '9') || d == '-' || d == '+' || d == '.' || d == 'e' ||
          d == 'E') {
        ++m_at;
      } else {
        break;
      }
    }
    if (m_at == begin) {
      m_error = QStringLiteral("unexpected character");
      return false;
    }
    out.type = Node::Type::Number;
    out.text = QString::fromLatin1(m_bytes.mid(begin, m_at - begin));
    bool ok = false;
    (void)out.text.toDouble(&ok);
    if (!ok) {
      m_error = QStringLiteral("malformed number");
    }
    return ok;
  }

  auto parse_array(Node& out) -> bool {
    out.type = Node::Type::Array;
    ++m_at;
    const qsizetype opened = m_at;
    skip_space();
    if (m_at < m_bytes.size() && m_bytes[m_at] == ']') {
      out.text = m_bytes.mid(opened, m_at - opened).contains('\n')
                     ? QStringLiteral("\n")
                     : QString();
      ++m_at;
      return true;
    }
    while (true) {
      Node item;
      skip_space();
      if (!parse_value(item)) {
        return false;
      }
      out.items.push_back(std::move(item));
      skip_space();
      if (m_at < m_bytes.size() && m_bytes[m_at] == ',') {
        ++m_at;
        continue;
      }
      if (m_at < m_bytes.size() && m_bytes[m_at] == ']') {
        ++m_at;
        return true;
      }
      m_error = QStringLiteral("expected ',' or ']'");
      return false;
    }
  }

  auto parse_object(Node& out) -> bool {
    out.type = Node::Type::Object;
    ++m_at;
    const qsizetype opened = m_at;
    skip_space();
    if (m_at < m_bytes.size() && m_bytes[m_at] == '}') {
      out.text = m_bytes.mid(opened, m_at - opened).contains('\n')
                     ? QStringLiteral("\n")
                     : QString();
      ++m_at;
      return true;
    }
    while (true) {
      skip_space();
      QString raw_key;
      if (!string_token(raw_key)) {
        return false;
      }
      skip_space();
      if (m_at >= m_bytes.size() || m_bytes[m_at] != ':') {
        m_error = QStringLiteral("expected ':'");
        return false;
      }
      ++m_at;
      skip_space();
      Node member;
      if (!parse_value(member)) {
        return false;
      }
      out.members.emplace_back(std::move(raw_key), std::move(member));
      skip_space();
      if (m_at < m_bytes.size() && m_bytes[m_at] == ',') {
        ++m_at;
        continue;
      }
      if (m_at < m_bytes.size() && m_bytes[m_at] == '}') {
        ++m_at;
        return true;
      }
      m_error = QStringLiteral("expected ',' or '}'");
      return false;
    }
  }

  const QByteArray& m_bytes;
  qsizetype m_at = 0;
  QString m_error;
};

auto decode_string(const QString& token) -> QString {
  const QByteArray wrapped = QByteArray("[") + token.toUtf8() + QByteArray("]");
  return QJsonDocument::fromJson(wrapped).array().at(0).toString();
}

auto encode_string(const QString& value) -> QString {
  QString out;
  out.reserve(value.size() + 2);
  out.append(QLatin1Char('"'));
  for (const QChar ch : value) {
    switch (ch.unicode()) {
    case '"':
      out.append(QStringLiteral("\\\""));
      break;
    case '\\':
      out.append(QStringLiteral("\\\\"));
      break;
    case '\n':
      out.append(QStringLiteral("\\n"));
      break;
    case '\r':
      out.append(QStringLiteral("\\r"));
      break;
    case '\t':
      out.append(QStringLiteral("\\t"));
      break;
    default:
      if (ch.unicode() < 0x20) {
        out.append(
            QStringLiteral("\\u%1").arg(ch.unicode(), 4, 16, QLatin1Char('0')));
      } else {
        out.append(ch);
      }
      break;
    }
  }
  out.append(QLatin1Char('"'));
  return out;
}

auto encode_number(double value) -> QString {
  constexpr double k_tidy_scale = 10000.0;
  const double tidy = std::round(value * k_tidy_scale) / k_tidy_scale;
  const double scale = std::max(1.0, std::abs(value));
  if (std::abs(tidy - value) <= 1.0e-6 * scale) {
    value = tidy;
  }
  if (value == std::floor(value) && std::abs(value) < 1.0e15) {
    return QString::number(static_cast<qint64>(value));
  }
  return QString::number(value, 'g', 15);
}

auto compact(const QJsonValue& value) -> QByteArray {
  return QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact);
}

void write_node(const Node& node, int depth, int unit, QByteArray& out) {
  auto indent = [unit](QByteArray& target, int level) {
    target.append(QByteArray(level * unit, ' '));
  };
  switch (node.type) {
  case Node::Type::Array:
    if (node.items.empty()) {
      if (node.text.isEmpty()) {
        out.append("[]");
      } else {
        out.append("[\n");
        indent(out, depth);
        out.append("]");
      }
      return;
    }
    out.append("[\n");
    for (std::size_t i = 0; i < node.items.size(); ++i) {
      indent(out, depth + 1);
      write_node(node.items[i], depth + 1, unit, out);
      out.append(i + 1 < node.items.size() ? ",\n" : "\n");
    }
    indent(out, depth);
    out.append("]");
    return;
  case Node::Type::Object:
    if (node.members.empty()) {
      if (node.text.isEmpty()) {
        out.append("{}");
      } else {
        out.append("{\n");
        indent(out, depth);
        out.append("}");
      }
      return;
    }
    out.append("{\n");
    for (std::size_t i = 0; i < node.members.size(); ++i) {
      indent(out, depth + 1);
      out.append(node.members[i].first.toUtf8());
      out.append(": ");
      write_node(node.members[i].second, depth + 1, unit, out);
      out.append(i + 1 < node.members.size() ? ",\n" : "\n");
    }
    indent(out, depth);
    out.append("}");
    return;
  default:
    out.append(node.text.toUtf8());
    return;
  }
}

auto merge_array(const Node& original,
                 const QJsonArray& canonical,
                 const QJsonArray& current) -> Node {
  Node result;
  result.type = Node::Type::Array;
  const auto canonical_count = static_cast<int>(canonical.size());
  const auto current_count = static_cast<int>(current.size());
  if (static_cast<int>(original.items.size()) != canonical_count) {
    return from_value(current);
  }

  std::vector<int> source(static_cast<std::size_t>(current_count), -1);
  std::vector<bool> used(static_cast<std::size_t>(canonical_count), false);
  for (int i = 0; i < current_count && i < canonical_count; ++i) {
    if (current[i] == canonical[i]) {
      source[static_cast<std::size_t>(i)] = i;
      used[static_cast<std::size_t>(i)] = true;
    }
  }
  bool unmatched = false;
  for (int i = 0; i < current_count; ++i) {
    unmatched = unmatched || source[static_cast<std::size_t>(i)] < 0;
  }
  if (unmatched) {
    QHash<QByteArray, std::deque<int>> by_value;
    for (int j = 0; j < canonical_count; ++j) {
      if (!used[static_cast<std::size_t>(j)]) {
        by_value[compact(canonical[j])].push_back(j);
      }
    }
    for (int i = 0; i < current_count; ++i) {
      if (source[static_cast<std::size_t>(i)] >= 0) {
        continue;
      }
      auto found = by_value.find(compact(current[i]));
      if (found != by_value.end() && !found->empty()) {
        const int j = found->front();
        found->pop_front();
        source[static_cast<std::size_t>(i)] = j;
        used[static_cast<std::size_t>(j)] = true;
      }
    }
  }

  result.items.reserve(static_cast<std::size_t>(current_count));
  for (int i = 0; i < current_count; ++i) {
    const int from = source[static_cast<std::size_t>(i)];
    if (from >= 0) {
      result.items.push_back(original.items[static_cast<std::size_t>(from)]);
    } else if (i < canonical_count && !used[static_cast<std::size_t>(i)]) {
      used[static_cast<std::size_t>(i)] = true;
      result.items.push_back(
          merge(original.items[static_cast<std::size_t>(i)], canonical[i], current[i]));
    } else {
      result.items.push_back(from_value(current[i]));
    }
  }
  return result;
}

auto merge_object(const Node& original,
                  const QJsonObject& canonical,
                  const QJsonObject& current) -> Node {
  Node result;
  result.type = Node::Type::Object;
  QStringList seen;
  for (const auto& [raw_key, child] : original.members) {
    const QString key = decode_string(raw_key);
    seen.append(key);
    if (current.contains(key)) {
      if (canonical.contains(key)) {
        result.members.emplace_back(
            raw_key, merge(child, canonical.value(key), current.value(key)));
      } else if (to_value(child) == current.value(key)) {
        result.members.emplace_back(raw_key, child);
      } else {
        result.members.emplace_back(raw_key, from_value(current.value(key)));
      }
    } else if (!canonical.contains(key)) {
      result.members.emplace_back(raw_key, child);
    }
  }
  for (auto it = current.begin(); it != current.end(); ++it) {
    if (seen.contains(it.key())) {
      continue;
    }
    if (canonical.contains(it.key()) && canonical.value(it.key()) == it.value()) {
      continue;
    }
    result.members.emplace_back(encode_string(it.key()), from_value(it.value()));
  }
  return result;
}

} // namespace

auto Node::find(const QString& key) const -> const Node* {
  for (const auto& [raw_key, child] : members) {
    if (decode_string(raw_key) == key) {
      return &child;
    }
  }
  return nullptr;
}

auto parse(const QByteArray& utf8, Node& out, QString* error) -> bool {
  out = Node{};
  Parser parser(utf8);
  return parser.parse_document(out, error);
}

auto to_value(const Node& node) -> QJsonValue {
  switch (node.type) {
  case Node::Type::Null:
    return QJsonValue(QJsonValue::Null);
  case Node::Type::Bool:
    return QJsonValue(node.text == QLatin1String("true"));
  case Node::Type::Number:
    return QJsonValue(node.text.toDouble());
  case Node::Type::String:
    return QJsonValue(decode_string(node.text));
  case Node::Type::Array: {
    QJsonArray array;
    for (const Node& item : node.items) {
      array.append(to_value(item));
    }
    return array;
  }
  case Node::Type::Object: {
    QJsonObject object;
    for (const auto& [raw_key, child] : node.members) {
      object.insert(decode_string(raw_key), to_value(child));
    }
    return object;
  }
  }
  return {};
}

auto from_value(const QJsonValue& value) -> Node {
  Node node;
  if (value.isBool()) {
    node.type = Node::Type::Bool;
    node.text = value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
  } else if (value.isDouble()) {
    node.type = Node::Type::Number;
    node.text = encode_number(value.toDouble());
  } else if (value.isString()) {
    node.type = Node::Type::String;
    node.text = encode_string(value.toString());
  } else if (value.isArray()) {
    node.type = Node::Type::Array;
    const QJsonArray array = value.toArray();
    node.items.reserve(static_cast<std::size_t>(array.size()));
    for (const QJsonValue item : array) {
      node.items.push_back(from_value(item));
    }
  } else if (value.isObject()) {
    node.type = Node::Type::Object;
    const QJsonObject object = value.toObject();
    for (auto it = object.begin(); it != object.end(); ++it) {
      node.members.emplace_back(encode_string(it.key()), from_value(it.value()));
    }
  } else {
    node.type = Node::Type::Null;
    node.text = QStringLiteral("null");
  }
  return node;
}

auto detect_style(const QByteArray& utf8) -> Style {
  Style style;
  style.trailing_newline = utf8.endsWith('\n');
  const qsizetype first_break = utf8.indexOf('\n');
  if (first_break >= 0) {
    int spaces = 0;
    for (qsizetype i = first_break + 1; i < utf8.size() && utf8[i] == ' '; ++i) {
      ++spaces;
    }
    if (spaces > 0) {
      style.indent = spaces;
    }
  }
  return style;
}

auto write(const Node& node, const Style& style) -> QByteArray {
  QByteArray out;
  write_node(node, 0, std::max(style.indent, 1), out);
  if (style.trailing_newline) {
    out.append('\n');
  }
  return out;
}

auto merge(const Node& original,
           const QJsonValue& canonical,
           const QJsonValue& current) -> Node {
  if (current == canonical) {
    return original;
  }
  if (original.type == Node::Type::Object && canonical.isObject() &&
      current.isObject()) {
    return merge_object(original, canonical.toObject(), current.toObject());
  }
  if (original.type == Node::Type::Array && canonical.isArray() && current.isArray()) {
    return merge_array(original, canonical.toArray(), current.toArray());
  }
  return from_value(current);
}

} // namespace MapEditor::JsonText
