#include "commander_voice_bank.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>

#include <utility>

#include "game/map/commander_message_grammar.h"

namespace Game::Mission {

namespace {

auto resolve_data_path(const QString& relative) -> QString {
  const QString direct = QDir::current().filePath(relative);
  if (QFile::exists(direct)) {
    return direct;
  }
  const QString app_dir = QCoreApplication::applicationDirPath();
  if (!app_dir.isEmpty()) {
    const QString from_app = QDir(app_dir).filePath(relative);
    if (QFile::exists(from_app)) {
      return from_app;
    }
    const QString parent = QDir(app_dir).filePath("../" + relative);
    if (QFile::exists(parent)) {
      return QDir(parent).canonicalPath();
    }
  }
  const QString resource_path = QStringLiteral(":/") + relative;
  if (QFile::exists(resource_path)) {
    return resource_path;
  }
  return {};
}

void set_error(QString* error, const QString& text) {
  if (error != nullptr) {
    *error = text;
  }
}

} // namespace

auto CommanderVoiceLibrary::parse_bank(const QJsonObject& root, QString* error)
    -> std::optional<CommanderVoiceBank> {
  CommanderVoiceBank bank;
  bank.commander_id = root["commander"].toString().trimmed();
  if (bank.commander_id.isEmpty()) {
    set_error(error, QStringLiteral("voice bank has no 'commander'"));
    return std::nullopt;
  }
  bank.chatter_per_match = root["chatter_per_match"].toInt(bank.chatter_per_match);

  for (const auto value : root["lines"].toArray()) {
    const QJsonObject obj = value.toObject();
    CommanderVoiceLine line;
    line.id = obj["id"].toString().trimmed();
    if (line.id.isEmpty()) {
      set_error(
          error,
          QStringLiteral("voice bank %1: a line has no 'id'").arg(bank.commander_id));
      return std::nullopt;
    }
    if (!parse_commander_relationship(obj["relationship"].toString(),
                                      line.relationship)) {
      set_error(error,
                QStringLiteral("voice bank %1: line '%2' has no valid 'relationship'")
                    .arg(bank.commander_id, line.id));
      return std::nullopt;
    }
    line.pose = obj["pose"].toString();
    line.voice_cue = obj["voice_cue"].toString();

    const QJsonObject trigger = obj["trigger"].toObject();
    if (!parse_commander_message_trigger(trigger["type"].toString(), line.trigger)) {
      set_error(error,
                QStringLiteral("voice bank %1: line '%2' names unknown trigger '%3'")
                    .arg(bank.commander_id, line.id, trigger["type"].toString()));
      return std::nullopt;
    }
    line.condition = parse_commander_message_condition(trigger);
    line.delay = static_cast<float>(trigger["delay"].toDouble(line.delay));
    line.duration = static_cast<float>(obj["duration"].toDouble(line.duration));
    line.priority = obj["priority"].toInt(line.priority);
    line.once = obj["once"].toBool(line.once);

    if (obj.contains("variants")) {
      for (const auto variant : obj["variants"].toArray()) {
        const QString text = variant.toString();
        if (!text.trimmed().isEmpty()) {
          line.variants.push_back(text);
        }
      }
    }
    if (obj.contains("text")) {
      const QString text = obj["text"].toString();
      if (!text.trimmed().isEmpty()) {
        line.variants.push_front(text);
      }
    }
    bank.lines.push_back(std::move(line));
  }
  return bank;
}

auto CommanderVoiceLibrary::load_from_file(const QString& path, QString* error)
    -> std::optional<CommanderVoiceBank> {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    set_error(error, QStringLiteral("cannot open %1").arg(path));
    return std::nullopt;
  }
  QJsonParseError parse_error{};
  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parse_error);
  if (parse_error.error != QJsonParseError::NoError || !doc.isObject()) {
    set_error(error, QStringLiteral("%1: %2").arg(path, parse_error.errorString()));
    return std::nullopt;
  }
  QString bank_error;
  auto bank = parse_bank(doc.object(), &bank_error);
  if (!bank.has_value()) {
    set_error(error, QStringLiteral("%1: %2").arg(path, bank_error));
  }
  return bank;
}

auto CommanderVoiceLibrary::load_from_directory(
    const QString& directory, QString* error) -> CommanderVoiceLibrary {
  CommanderVoiceLibrary library;
  const QDir dir(directory);
  if (!dir.exists()) {
    set_error(error,
              QStringLiteral("voice bank directory %1 does not exist").arg(directory));
    return library;
  }
  QStringList problems;
  for (const QString& name : dir.entryList({"*.json"}, QDir::Files, QDir::Name)) {
    QString file_error;
    auto bank = load_from_file(dir.absoluteFilePath(name), &file_error);
    if (!bank.has_value()) {
      problems.push_back(file_error);
      continue;
    }
    library.add(std::move(*bank));
  }
  if (!problems.isEmpty()) {
    set_error(error, problems.join(QStringLiteral("; ")));
  }
  return library;
}

auto CommanderVoiceLibrary::default_directory() -> QString {
  return resolve_data_path(QLatin1String(k_commander_voices_relative_dir));
}

auto CommanderVoiceLibrary::load_default(QString* error) -> CommanderVoiceLibrary {
  const QString directory = default_directory();
  if (directory.isEmpty()) {
    set_error(
        error,
        QStringLiteral("could not locate %1").arg(k_commander_voices_relative_dir));
    return {};
  }
  return load_from_directory(directory, error);
}

void CommanderVoiceLibrary::add(CommanderVoiceBank bank) {
  for (auto& existing : m_banks) {
    if (existing.commander_id == bank.commander_id) {
      existing = std::move(bank);
      return;
    }
  }
  m_banks.push_back(std::move(bank));
}

auto CommanderVoiceLibrary::bank_for(const QString& commander_id) const
    -> const CommanderVoiceBank* {
  for (const auto& bank : m_banks) {
    if (bank.commander_id == commander_id) {
      return &bank;
    }
  }
  return nullptr;
}

auto commander_voice_rule_id(int owner_id,
                             const QString& line_id,
                             int variant_index) -> QString {
  return QStringLiteral("%1:%2.%3").arg(owner_id).arg(line_id).arg(variant_index + 1);
}

auto expand_commander_voice_line(const CommanderVoiceLine& line,
                                 const QString& commander_id,
                                 int owner_id) -> std::vector<CommanderMessage> {
  std::vector<CommanderMessage> messages;
  messages.reserve(static_cast<std::size_t>(line.variants.size()));
  for (int index = 0; index < line.variants.size(); ++index) {
    CommanderMessage message;
    message.id = commander_voice_rule_id(owner_id, line.id, index);
    message.speaker = commander_id;
    message.pose = line.pose;
    message.text = line.variants.at(index);
    message.voice_cue = line.voice_cue;
    message.trigger = line.trigger;
    message.condition = line.condition;
    message.delay = line.delay;
    message.duration = line.duration;
    message.priority = line.priority;
    message.once = line.once;
    messages.push_back(std::move(message));
  }
  return messages;
}

} // namespace Game::Mission
