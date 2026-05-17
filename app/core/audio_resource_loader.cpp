#include "audio_resource_loader.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

#include <unordered_set>

#include "game/audio/audio_system.h"
#include "utils/resource_utils.h"

namespace {

auto audio_root_candidates() -> QStringList {
  const QString app_dir = QCoreApplication::applicationDirPath();
  return {
      app_dir + "/assets/audio/",
      app_dir + "/../Resources/assets/audio/",
      app_dir + "/../../assets/audio/",
  };
}

auto resolve_audio_root() -> QString {
  for (const QString& candidate : audio_root_candidates()) {
    if (QDir(candidate).exists()) {
      return candidate;
    }
  }
  return {};
}

auto resolve_manifest_path(const QString& audio_root) -> QString {
  const QStringList candidates = {
      audio_root + "audio_manifest.json",
      QStringLiteral(":/assets/audio/audio_manifest.json"),
  };

  for (const QString& candidate : candidates) {
    const QString resolved = Utils::Resources::resolve_resource_path(candidate);
    if (QFile::exists(resolved)) {
      return resolved;
    }
  }
  return {};
}

auto normalize(const QString& value) -> QString {
  return value.trimmed().toLower();
}

auto parse_category(const QString& value) -> AudioCategory {
  const QString category = normalize(value);
  if (category == QStringLiteral("music")) {
    return AudioCategory::MUSIC;
  }
  if (category == QStringLiteral("voice")) {
    return AudioCategory::VOICE;
  }
  return AudioCategory::SFX;
}

auto parse_policy(const QString& value) -> AudioLoadPolicy {
  const QString policy = normalize(value);
  if (policy == QStringLiteral("mission")) {
    return AudioLoadPolicy::Mission;
  }
  if (policy == QStringLiteral("screen")) {
    return AudioLoadPolicy::Screen;
  }
  if (policy == QStringLiteral("lazy")) {
    return AudioLoadPolicy::Lazy;
  }
  if (policy == QStringLiteral("stream")) {
    return AudioLoadPolicy::Stream;
  }
  if (policy == QStringLiteral("all")) {
    return AudioLoadPolicy::All;
  }
  return AudioLoadPolicy::Startup;
}

auto policy_matches(AudioLoadPolicy requested, AudioLoadPolicy entry) -> bool {
  return (requested == AudioLoadPolicy::All) || (entry == AudioLoadPolicy::All) ||
         (requested == entry);
}

auto load_entry(AudioSystem& audio_sys,
                const QString& manifest_dir,
                const QJsonObject& track_object,
                AudioLoadPolicy requested_policy,
                std::unordered_set<std::string>& loaded_ids) -> bool {
  const QString id = track_object.value(QStringLiteral("id")).toString().trimmed();
  const QString path = track_object.value(QStringLiteral("path")).toString().trimmed();
  if (id.isEmpty() || path.isEmpty()) {
    qWarning() << "Audio manifest entry missing id or path";
    return false;
  }

  const AudioLoadPolicy entry_policy =
      parse_policy(track_object.value(QStringLiteral("load_policy")).toString());
  if (!policy_matches(requested_policy, entry_policy)) {
    return false;
  }

  const QString category_value =
      track_object.value(QStringLiteral("category")).toString(QStringLiteral("sfx"));
  const AudioCategory category = parse_category(category_value);

  const QString resolved_path =
      Utils::Resources::resolve_resource_path(QDir(manifest_dir).filePath(path));
  if (resolved_path.isEmpty() || !QFile::exists(resolved_path)) {
    qWarning() << "Audio asset missing for manifest entry" << id << "->"
               << resolved_path;
    return false;
  }

  const std::string primary_id = id.toStdString();
  bool ok = true;
  if (loaded_ids.insert(primary_id).second) {
    if (category == AudioCategory::MUSIC) {
      ok = audio_sys.load_music(primary_id, resolved_path.toStdString());
    } else {
      ok = audio_sys.load_sound(primary_id, resolved_path.toStdString(), category);
    }
  }

  if (!ok) {
    qWarning() << "Failed to load audio asset" << id << "from" << resolved_path;
    return false;
  }

  const QJsonArray aliases = track_object.value(QStringLiteral("aliases")).toArray();
  for (const QJsonValue& alias_value : aliases) {
    const QString alias = alias_value.toString().trimmed();
    if (alias.isEmpty()) {
      continue;
    }
    audio_sys.register_alias(alias.toStdString(), primary_id);
  }
  return ok;
}

} // namespace

namespace {

auto manifest_directory(const QString& manifest_path) -> QString {
  if (manifest_path.startsWith(QStringLiteral(":/"))) {
    const int slash = manifest_path.lastIndexOf('/');
    return (slash > 0) ? manifest_path.left(slash) : QString{};
  }
  return QFileInfo(manifest_path).absolutePath();
}

} // namespace

void AudioResourceLoader::load_audio_resources(AudioLoadPolicy load_policy) {
  auto& audio_sys = AudioSystem::get_instance();

  const QString audio_root = resolve_audio_root();
  if (audio_root.isEmpty()) {
    qWarning() << "Audio assets directory not found.";
    return;
  }

  const QString manifest_path = resolve_manifest_path(audio_root);
  if (manifest_path.isEmpty()) {
    qWarning() << "Audio manifest not found under" << audio_root;
    return;
  }

  QFile manifest_file(manifest_path);
  if (!manifest_file.open(QIODevice::ReadOnly)) {
    qWarning() << "Failed to open audio manifest:" << manifest_path;
    return;
  }

  const QJsonDocument document = QJsonDocument::fromJson(manifest_file.readAll());
  if (document.isNull()) {
    qWarning() << "Failed to parse audio manifest:" << manifest_path;
    return;
  }

  QJsonArray tracks;
  if (document.isArray()) {
    tracks = document.array();
  } else if (document.isObject()) {
    tracks = document.object().value(QStringLiteral("tracks")).toArray();
  }

  if (tracks.isEmpty()) {
    qWarning() << "Audio manifest contains no tracks:" << manifest_path;
    return;
  }

  std::unordered_set<std::string> loaded_ids;
  const QString manifest_dir = manifest_directory(manifest_path);
  int loaded_count = 0;
  for (const QJsonValue& value : tracks) {
    if (!value.isObject()) {
      continue;
    }
    if (load_entry(audio_sys,
                   QFileInfo(manifest_path).absolutePath(),
                   value.toObject(),
                   load_policy,
                   loaded_ids)) {
      ++loaded_count;
    }
  }

  qInfo() << "Loaded" << loaded_count << "audio manifest entries from" << manifest_path;
}
