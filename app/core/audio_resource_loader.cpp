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

#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "utils/resource_utils.h"

namespace {

struct CachedAudioEntry {
  QString id;
  QString path;
  QString resolved_path;
  AudioResourceConfig config;
  AudioLoadPolicy load_policy = AudioLoadPolicy::Startup;
  bool loop = false;
  QMap<QString, QString> tags;
  QStringList aliases;
};

struct ManifestRegistry {
  QString manifest_path;
  QString manifest_dir;
  std::vector<CachedAudioEntry> entries;
  std::unordered_map<std::string, size_t> lookup;
  bool loaded = false;
};

auto registry_mutex() -> std::mutex& {
  static std::mutex mutex;
  return mutex;
}

auto manifest_registry() -> ManifestRegistry& {
  static ManifestRegistry registry;
  return registry;
}

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
  QStringList candidates;
  if (!audio_root.isEmpty()) {
    candidates.push_back(audio_root + "audio_manifest.json");
  }
  candidates.push_back(QStringLiteral(":/assets/audio/audio_manifest.json"));

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
  if (category == QStringLiteral("ambience") || category == QStringLiteral("ambient")) {
    return AudioCategory::AMBIENCE;
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

auto parse_resource_config(const QJsonObject& track_object) -> AudioResourceConfig {
  AudioResourceConfig config;
  config.category =
      parse_category(track_object.value(QStringLiteral("category")).toString());
  config.volume = std::max(0.0F,
                           float(track_object.value(QStringLiteral("volume"))
                                     .toDouble(AudioConstants::DEFAULT_VOLUME)));
  config.priority = track_object.value(QStringLiteral("priority"))
                        .toInt(AudioConstants::DEFAULT_PRIORITY);
  config.cooldown_ms =
      std::max(0, track_object.value(QStringLiteral("cooldown_ms")).toInt(0));
  config.max_instances = static_cast<size_t>(
      std::max(0, track_object.value(QStringLiteral("max_instances")).toInt(0)));
  return config;
}

auto parse_tags(const QJsonObject& track_object) -> QMap<QString, QString> {
  QMap<QString, QString> tags;
  const QJsonValue tags_value = track_object.value(QStringLiteral("tags"));
  if (!tags_value.isObject()) {
    return tags;
  }

  const QJsonObject tags_object = tags_value.toObject();
  for (auto it = tags_object.begin(); it != tags_object.end(); ++it) {
    if (!it.value().isString()) {
      continue;
    }
    tags.insert(normalize(it.key()), normalize(it.value().toString()));
  }
  return tags;
}

auto policy_matches(AudioLoadPolicy requested, AudioLoadPolicy entry) -> bool {
  if (requested == AudioLoadPolicy::All) {
    return entry != AudioLoadPolicy::Lazy && entry != AudioLoadPolicy::Stream;
  }
  return (entry == AudioLoadPolicy::All) || (requested == entry);
}

auto manifest_directory(const QString& manifest_path) -> QString {
  if (manifest_path.startsWith(QStringLiteral(":/"))) {
    const int slash = manifest_path.lastIndexOf('/');
    return (slash > 0) ? manifest_path.left(slash) : QString{};
  }
  return QFileInfo(manifest_path).absolutePath();
}

void cache_manifest_locked() {
  auto& registry = manifest_registry();
  if (registry.loaded) {
    return;
  }

  const QString audio_root = resolve_audio_root();
  const QString manifest_path = resolve_manifest_path(audio_root);
  if (manifest_path.isEmpty()) {
    qWarning() << "Audio manifest not found";
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

  registry.manifest_path = manifest_path;
  registry.manifest_dir = manifest_directory(manifest_path);
  registry.entries.clear();
  registry.lookup.clear();

  for (const QJsonValue& value : tracks) {
    if (!value.isObject()) {
      continue;
    }

    const QJsonObject track_object = value.toObject();
    CachedAudioEntry entry;
    entry.id = track_object.value(QStringLiteral("id")).toString().trimmed();
    entry.path = track_object.value(QStringLiteral("path")).toString().trimmed();
    if (entry.id.isEmpty() || entry.path.isEmpty()) {
      qWarning() << "Audio manifest entry missing id or path";
      continue;
    }

    entry.config = parse_resource_config(track_object);
    entry.load_policy =
        parse_policy(track_object.value(QStringLiteral("load_policy")).toString());
    entry.loop = track_object.value(QStringLiteral("loop")).toBool(false);
    entry.tags = parse_tags(track_object);
    entry.resolved_path = Utils::Resources::resolve_resource_path(
        QDir(registry.manifest_dir).filePath(entry.path));

    const QJsonArray aliases = track_object.value(QStringLiteral("aliases")).toArray();
    for (const QJsonValue& alias_value : aliases) {
      const QString alias = alias_value.toString().trimmed();
      if (!alias.isEmpty()) {
        entry.aliases.push_back(alias);
      }
    }

    const size_t index = registry.entries.size();
    registry.lookup[entry.id.toStdString()] = index;
    for (const QString& alias : entry.aliases) {
      registry.lookup[alias.toStdString()] = index;
    }
    registry.entries.push_back(std::move(entry));
  }

  registry.loaded = true;
}

auto find_cached_entry_locked(const QString& resource_id) -> const CachedAudioEntry* {
  auto& registry = manifest_registry();
  cache_manifest_locked();
  auto it = registry.lookup.find(resource_id.toStdString());
  if (it == registry.lookup.end()) {
    return nullptr;
  }
  return &registry.entries[it->second];
}

auto load_cached_entry(AudioSystem& audio_sys, const CachedAudioEntry& entry) -> bool {
  if (entry.resolved_path.isEmpty() || !QFile::exists(entry.resolved_path)) {
    qWarning() << "Audio asset missing for manifest entry" << entry.id << "->"
               << entry.resolved_path;
    return false;
  }

  if (audio_sys.has_resource(entry.id.toStdString())) {
    return true;
  }

  bool ok = false;
  if (entry.config.category == AudioCategory::MUSIC) {
    ok = audio_sys.load_music(
        entry.id.toStdString(), entry.resolved_path.toStdString(), entry.config);
  } else {
    ok = audio_sys.load_sound(
        entry.id.toStdString(), entry.resolved_path.toStdString(), entry.config);
  }
  if (!ok) {
    qWarning() << "Failed to load audio asset" << entry.id << "from"
               << entry.resolved_path;
    return false;
  }

  for (const QString& alias : entry.aliases) {
    audio_sys.register_alias(alias.toStdString(), entry.id.toStdString());
  }
  return true;
}

auto tags_match(const CachedAudioEntry& entry,
                const QMap<QString, QString>& tags) -> bool {
  for (auto it = tags.begin(); it != tags.end(); ++it) {
    auto entry_it = entry.tags.find(normalize(it.key()));
    if (entry_it == entry.tags.end() || entry_it.value() != normalize(it.value())) {
      return false;
    }
  }
  return true;
}

} // namespace

void AudioResourceLoader::load_audio_resources(AudioLoadPolicy load_policy) {
  auto& audio_sys = AudioSystem::get_instance();

  std::lock_guard<std::mutex> const lock(registry_mutex());
  cache_manifest_locked();

  auto& registry = manifest_registry();
  int loaded_count = 0;
  for (const auto& entry : registry.entries) {
    if (!policy_matches(load_policy, entry.load_policy)) {
      continue;
    }
    if (load_cached_entry(audio_sys, entry)) {
      ++loaded_count;
    }
  }

  if (!registry.manifest_path.isEmpty()) {
    qInfo() << "Loaded" << loaded_count << "audio manifest entries from"
            << registry.manifest_path;
  }
}

void AudioResourceLoader::unload_audio_resources(AudioLoadPolicy load_policy) {
  auto& audio_sys = AudioSystem::get_instance();

  std::lock_guard<std::mutex> const lock(registry_mutex());
  cache_manifest_locked();

  bool stop_music = false;
  auto& registry = manifest_registry();
  for (const auto& entry : registry.entries) {
    if (!policy_matches(load_policy, entry.load_policy)) {
      continue;
    }

    if (entry.config.category == AudioCategory::MUSIC) {
      stop_music = true;
      audio_sys.unload_music(entry.id.toStdString());
    } else {
      audio_sys.unload_sound(entry.id.toStdString());
    }
  }

  if (stop_music) {
    audio_sys.stop_music();
  }
}

auto AudioResourceLoader::ensure_audio_resource_loaded(const QString& resource_id)
    -> bool {
  std::lock_guard<std::mutex> const lock(registry_mutex());
  const CachedAudioEntry* entry = find_cached_entry_locked(resource_id);
  if (entry == nullptr) {
    return false;
  }
  return load_cached_entry(AudioSystem::get_instance(), *entry);
}

auto AudioResourceLoader::find_first_resource_id(
    AudioCategory category, const QMap<QString, QString>& tags) -> QString {
  std::lock_guard<std::mutex> const lock(registry_mutex());
  cache_manifest_locked();

  const auto& registry = manifest_registry();
  for (const auto& entry : registry.entries) {
    if (entry.config.category != category) {
      continue;
    }
    if (tags_match(entry, tags)) {
      return entry.id;
    }
  }
  return {};
}
