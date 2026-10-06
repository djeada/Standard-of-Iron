#include "generator_client.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QSet>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>

namespace MapEditor::Generator {

namespace {

auto parse_object(const QByteArray& json,
                  QString* error) -> std::optional<QJsonObject> {
  QJsonParseError parse_error;
  const QJsonDocument doc = QJsonDocument::fromJson(json, &parse_error);
  if (parse_error.error != QJsonParseError::NoError || !doc.isObject()) {
    if (error != nullptr) {
      *error = parse_error.error != QJsonParseError::NoError
                   ? parse_error.errorString()
                   : QStringLiteral("expected a JSON object");
    }
    return std::nullopt;
  }
  return doc.object();
}

auto string_list(const QJsonValue& value) -> QStringList {
  QStringList list;
  for (const QJsonValue item : value.toArray()) {
    if (item.isString()) {
      list.append(item.toString());
    }
  }
  return list;
}

auto parameter_type(const QString& name) -> std::optional<ParameterType> {
  if (name == QLatin1String("float")) {
    return ParameterType::Float;
  }
  if (name == QLatin1String("int")) {
    return ParameterType::Int;
  }
  if (name == QLatin1String("bool")) {
    return ParameterType::Bool;
  }
  if (name == QLatin1String("choice")) {
    return ParameterType::Choice;
  }
  return std::nullopt;
}

auto check_status(const QString& name) -> CheckStatus {
  if (name == QLatin1String("fail")) {
    return CheckStatus::Fail;
  }
  if (name == QLatin1String("warn")) {
    return CheckStatus::Warn;
  }
  return CheckStatus::Pass;
}

auto argument_value(const QJsonValue& value) -> QString {
  if (value.isBool()) {
    return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
  }
  if (value.isDouble()) {
    const double number = value.toDouble();
    if (std::floor(number) == number && std::abs(number) < 1e15) {
      return QString::number(static_cast<qint64>(number));
    }
    return QString::number(number, 'g', QLocale::FloatingPointShortest);
  }
  return value.toString();
}

} // namespace

auto Schema::stage(const QString& id) const -> const StageSpec* {
  const auto it = std::find_if(
      stages.begin(), stages.end(), [&id](const StageSpec& s) { return s.id == id; });
  return it != stages.end() ? &*it : nullptr;
}

auto Schema::preset(const QString& id) const -> const PresetSpec* {
  const auto it = std::find_if(presets.begin(),
                               presets.end(),
                               [&id](const PresetSpec& p) { return p.id == id; });
  return it != presets.end() ? &*it : nullptr;
}

auto Schema::parameter(const QString& key) const -> const ParameterSpec* {
  const auto it = std::find_if(parameters.begin(),
                               parameters.end(),
                               [&key](const ParameterSpec& p) { return p.key == key; });
  return it != parameters.end() ? &*it : nullptr;
}

auto Schema::parameters_for(const QString& stage) const
    -> QVector<const ParameterSpec*> {
  QVector<const ParameterSpec*> found;
  for (const ParameterSpec& spec : parameters) {
    if (spec.stage == stage) {
      found.append(&spec);
    }
  }
  return found;
}

auto Schema::parameter_groups() const -> QStringList {
  QStringList groups;
  const auto add = [&groups](const QString& group) {
    if (!groups.contains(group)) {
      groups.append(group);
    }
  };
  for (const ParameterSpec& spec : parameters) {
    if (spec.stage == QLatin1String("general")) {
      add(spec.stage);
    }
  }
  for (const StageSpec& stage : stages) {
    if (!parameters_for(stage.id).isEmpty()) {
      add(stage.id);
    }
  }
  for (const ParameterSpec& spec : parameters) {
    add(spec.stage);
  }
  return groups;
}

auto parse_schema(const QByteArray& json, QString* error) -> std::optional<Schema> {
  const auto root = parse_object(json, error);
  if (!root.has_value()) {
    return std::nullopt;
  }
  Schema schema;
  schema.version = root->value(QStringLiteral("version")).toInt();
  if (schema.version != k_schema_version) {
    if (error != nullptr) {
      *error = QStringLiteral("unsupported generator schema version %1 (expected %2)")
                   .arg(schema.version)
                   .arg(k_schema_version);
    }
    return std::nullopt;
  }

  for (const QJsonValue value : root->value(QStringLiteral("stages")).toArray()) {
    const QJsonObject obj = value.toObject();
    StageSpec stage;
    stage.id = obj.value(QStringLiteral("id")).toString();
    stage.label = obj.value(QStringLiteral("label")).toString(stage.id);
    stage.forced_by = string_list(obj.value(QStringLiteral("forced_by")));
    if (!stage.id.isEmpty()) {
      schema.stages.append(stage);
    }
  }

  for (const QJsonValue value : root->value(QStringLiteral("parameters")).toArray()) {
    const QJsonObject obj = value.toObject();
    const auto type = parameter_type(obj.value(QStringLiteral("type")).toString());
    ParameterSpec spec;
    spec.key = obj.value(QStringLiteral("key")).toString();
    if (spec.key.isEmpty() || !type.has_value()) {
      continue;
    }
    spec.type = *type;
    spec.stage = obj.value(QStringLiteral("stage"))
                     .toString(spec.key.section(QLatin1Char('.'), 0, 0));
    spec.label = obj.value(QStringLiteral("label")).toString(spec.key);
    spec.help = obj.value(QStringLiteral("help")).toString();
    spec.min = obj.value(QStringLiteral("min")).toDouble(0.0);
    spec.max = obj.value(QStringLiteral("max"))
                   .toDouble(spec.type == ParameterType::Int ? 100.0 : 1.0);
    spec.step = obj.value(QStringLiteral("step")).toDouble(0.0);
    spec.default_value = obj.value(QStringLiteral("default"));
    for (const QJsonValue choice : obj.value(QStringLiteral("choices")).toArray()) {
      if (choice.isString()) {
        spec.choices.append({choice.toString(), choice.toString()});
      } else {
        const QJsonObject c = choice.toObject();
        const QString id = c.value(QStringLiteral("id")).toString();
        spec.choices.append({id, c.value(QStringLiteral("label")).toString(id)});
      }
    }
    schema.parameters.append(spec);
  }

  for (const QJsonValue value : root->value(QStringLiteral("presets")).toArray()) {
    const QJsonObject obj = value.toObject();
    PresetSpec preset;
    preset.id = obj.value(QStringLiteral("id")).toString();
    preset.label = obj.value(QStringLiteral("label")).toString(preset.id);
    preset.description = obj.value(QStringLiteral("description")).toString();
    preset.width = obj.value(QStringLiteral("width")).toInt(650);
    preset.height = obj.value(QStringLiteral("height")).toInt(650);
    preset.parameters = obj.value(QStringLiteral("parameters")).toObject();
    if (!preset.id.isEmpty()) {
      schema.presets.append(preset);
    }
  }
  return schema;
}

auto request_to_json(const Request& request) -> QJsonObject {
  QJsonObject json{{QStringLiteral("version"), k_schema_version},
                   {QStringLiteral("seed"), static_cast<double>(request.seed)},
                   {QStringLiteral("preset"), request.preset},
                   {QStringLiteral("width"), request.width},
                   {QStringLiteral("height"), request.height},
                   {QStringLiteral("parameters"), request.parameters}};
  json[QStringLiteral("locks")] = QJsonArray::fromStringList(request.locks);
  return json;
}

auto request_from_json(const QJsonObject& json) -> Request {
  Request request;
  request.seed = static_cast<qint64>(json.value(QStringLiteral("seed")).toDouble(1.0));
  request.preset = json.value(QStringLiteral("preset")).toString();
  request.width = json.value(QStringLiteral("width")).toInt(request.width);
  request.height = json.value(QStringLiteral("height")).toInt(request.height);
  request.parameters = json.value(QStringLiteral("parameters")).toObject();
  request.locks = string_list(json.value(QStringLiteral("locks")));
  return request;
}

auto resolved_parameters(const Schema& schema, const Request& request) -> QJsonObject {
  QJsonObject resolved;
  for (const ParameterSpec& spec : schema.parameters) {
    resolved[spec.key] = spec.default_value;
  }
  if (const PresetSpec* preset = schema.preset(request.preset)) {
    for (auto it = preset->parameters.begin(); it != preset->parameters.end(); ++it) {
      resolved[it.key()] = it.value();
    }
  }
  for (auto it = request.parameters.begin(); it != request.parameters.end(); ++it) {
    resolved[it.key()] = it.value();
  }
  return resolved;
}

auto build_arguments(const Request& request,
                     const Invocation& invocation) -> QStringList {
  QStringList args;
  args << invocation.script;
  if (!invocation.request_path.isEmpty()) {
    args << QStringLiteral("--request") << invocation.request_path;
  }
  if (!request.preset.isEmpty()) {
    args << QStringLiteral("--preset") << request.preset;
  }
  args << QStringLiteral("--seed") << QString::number(request.seed);
  args << QStringLiteral("--width") << QString::number(request.width);
  args << QStringLiteral("--height") << QString::number(request.height);

  QStringList keys = request.parameters.keys();
  keys.sort();
  for (const QString& key : keys) {
    args << QStringLiteral("--set")
         << QStringLiteral("%1=%2").arg(key, argument_value(request.parameters[key]));
  }
  if (!invocation.base_path.isEmpty()) {
    args << QStringLiteral("--base") << invocation.base_path;
    if (!request.locks.isEmpty()) {
      args << QStringLiteral("--lock") << request.locks.join(QLatin1Char(','));
    }
  }
  args << QStringLiteral("--output") << invocation.output_path;
  if (!invocation.report_path.isEmpty()) {
    args << QStringLiteral("--report") << invocation.report_path;
  }
  if (invocation.progress) {
    args << QStringLiteral("--progress");
  }
  return args;
}

auto forced_stages(const Schema& schema,
                   const QStringList& locks) -> QMap<QString, QStringList> {
  QSet<QString> locked(locks.begin(), locks.end());
  QMap<QString, QStringList> forced;
  bool changed = true;
  while (changed) {
    changed = false;
    for (const StageSpec& stage : schema.stages) {
      if (!locked.contains(stage.id)) {
        continue;
      }
      QStringList forcing;
      for (const QString& upstream : stage.forced_by) {
        if (!locked.contains(upstream)) {
          forcing.append(upstream);
        }
      }
      if (!forcing.isEmpty()) {
        locked.remove(stage.id);
        forced.insert(stage.id, forcing);
        changed = true;
      }
    }
  }
  return forced;
}

auto effective_locks(const Schema& schema, const QStringList& locks) -> QStringList {
  const QMap<QString, QStringList> forced = forced_stages(schema, locks);
  QStringList kept;
  for (const StageSpec& stage : schema.stages) {
    if (locks.contains(stage.id) && !forced.contains(stage.id)) {
      kept.append(stage.id);
    }
  }
  return kept;
}

auto parse_progress_line(const QByteArray& line) -> std::optional<ProgressEvent> {
  const QByteArray trimmed = line.trimmed();
  if (!trimmed.startsWith('{')) {
    return std::nullopt;
  }
  const auto obj = parse_object(trimmed, nullptr);
  if (!obj.has_value()) {
    return std::nullopt;
  }
  const QString event = obj->value(QStringLiteral("event")).toString();
  ProgressEvent parsed;
  parsed.message = obj->value(QStringLiteral("message")).toString();
  parsed.attempt = std::max(1, obj->value(QStringLiteral("attempt")).toInt(1));
  if (event == QLatin1String("stage")) {
    parsed.kind = ProgressEvent::Kind::Stage;
    parsed.stage = obj->value(QStringLiteral("stage")).toString();
    parsed.status = obj->value(QStringLiteral("status")).toString();
    parsed.seconds = obj->value(QStringLiteral("seconds")).toDouble(-1.0);
    if (parsed.stage.isEmpty() || parsed.status.isEmpty()) {
      return std::nullopt;
    }
    return parsed;
  }
  if (event == QLatin1String("done")) {
    parsed.kind = ProgressEvent::Kind::Done;
    parsed.ok = obj->value(QStringLiteral("ok")).toBool(false);
    parsed.output = obj->value(QStringLiteral("output")).toString();
    parsed.report = obj->value(QStringLiteral("report")).toString();
    return parsed;
  }
  if (event == QLatin1String("error")) {
    parsed.kind = ProgressEvent::Kind::Error;
    return parsed;
  }
  return std::nullopt;
}

void ProgressParser::feed(const QByteArray& chunk) {
  m_buffer += chunk;
  qsizetype newline = m_buffer.indexOf('\n');
  while (newline >= 0) {
    consume_line(m_buffer.left(newline));
    m_buffer.remove(0, newline + 1);
    newline = m_buffer.indexOf('\n');
  }
}

void ProgressParser::finish() {
  if (!m_buffer.trimmed().isEmpty()) {
    consume_line(m_buffer);
  }
  m_buffer.clear();
}

auto ProgressParser::take_events() -> QVector<ProgressEvent> {
  QVector<ProgressEvent> events;
  events.swap(m_events);
  return events;
}

void ProgressParser::consume_line(const QByteArray& line) {
  if (line.trimmed().isEmpty()) {
    return;
  }
  if (auto event = parse_progress_line(line)) {
    m_events.append(*event);
  } else {
    ++m_ignored;
  }
}

auto Report::count(CheckStatus status) const -> int {
  return static_cast<int>(
      std::count_if(checks.begin(), checks.end(), [status](const ReportCheck& check) {
        return check.status == status;
      }));
}

auto parse_report(const QByteArray& json, QString* error) -> std::optional<Report> {
  const auto root = parse_object(json, error);
  if (!root.has_value()) {
    return std::nullopt;
  }
  Report report;
  report.version = root->value(QStringLiteral("version")).toInt();
  report.ok = root->value(QStringLiteral("ok")).toBool(false);
  report.request = root->value(QStringLiteral("request")).toObject();
  report.metrics = root->value(QStringLiteral("metrics")).toObject();

  for (const QJsonValue value : root->value(QStringLiteral("stages")).toArray()) {
    const QJsonObject obj = value.toObject();
    ReportStage stage;
    stage.id = obj.value(QStringLiteral("id")).toString();
    stage.status = obj.value(QStringLiteral("status")).toString();
    stage.seed = static_cast<qint64>(obj.value(QStringLiteral("seed")).toDouble());
    stage.seconds = obj.value(QStringLiteral("seconds")).toDouble();
    stage.forced_by = obj.value(QStringLiteral("forced_by")).toString();
    stage.notes = string_list(obj.value(QStringLiteral("notes")));
    report.stages.append(stage);
  }

  for (const QJsonValue value : root->value(QStringLiteral("checks")).toArray()) {
    const QJsonObject obj = value.toObject();
    ReportCheck check;
    check.id = obj.value(QStringLiteral("id")).toString();
    check.status = check_status(obj.value(QStringLiteral("status")).toString());
    check.message = obj.value(QStringLiteral("message")).toString(check.id);
    for (const QJsonValue item : obj.value(QStringLiteral("objects")).toArray()) {
      const QJsonObject o = item.toObject();
      ReportObject object;
      object.kind = o.value(QStringLiteral("kind")).toString();
      object.index = o.value(QStringLiteral("index")).toInt(-1);
      object.has_position =
          o.contains(QStringLiteral("x")) && o.contains(QStringLiteral("z"));
      object.x = o.value(QStringLiteral("x")).toDouble();
      object.z = o.value(QStringLiteral("z")).toDouble();
      check.objects.append(object);
    }
    report.checks.append(check);
  }
  return report;
}

auto find_repository_root(const QStringList& start_directories) -> QString {
  for (const QString& start : start_directories) {
    if (start.isEmpty()) {
      continue;
    }
    QDir directory(start);
    for (int depth = 0; depth < 8; ++depth) {
      if (QFileInfo::exists(directory.filePath(QStringLiteral("CMakeLists.txt"))) &&
          QDir(directory.filePath(QStringLiteral("assets"))).exists()) {
        return directory.absolutePath();
      }
      if (!directory.cdUp()) {
        break;
      }
    }
  }
  return {};
}

auto script_path(const QString& repository_root) -> QString {
  if (repository_root.isEmpty()) {
    return {};
  }
  const QString path =
      QDir(repository_root).filePath(QStringLiteral("scripts/soi-mapgen.py"));
  return QFileInfo::exists(path) ? path : QString();
}

auto find_python() -> std::optional<PythonCommand> {
#ifdef Q_OS_WIN
  if (const QString py = QStandardPaths::findExecutable(QStringLiteral("py"));
      !py.isEmpty()) {
    return PythonCommand{py, {QStringLiteral("-3")}};
  }
  const QStringList names = {QStringLiteral("python"), QStringLiteral("python3")};
#else
  const QStringList names = {QStringLiteral("python3"), QStringLiteral("python")};
#endif
  for (const QString& name : names) {
    if (const QString found = QStandardPaths::findExecutable(name); !found.isEmpty()) {
      return PythonCommand{found, {}};
    }
  }
  return std::nullopt;
}

} // namespace MapEditor::Generator
