#pragma once

#include <QMap>
#include <QString>
#include <QStringList>

#include "game/audio/audio_system.h"

enum class AudioLoadPolicy {
  Startup,
  Mission,
  Screen,
  Lazy,
  Stream,
  All
};

class AudioResourceLoader {
public:
  static void
  load_audio_resources(AudioLoadPolicy load_policy = AudioLoadPolicy::Startup);
  static void unload_audio_resources(AudioLoadPolicy load_policy);
  static auto ensure_audio_resource_loaded(const QString& resource_id) -> bool;
  static auto find_resource_ids(AudioCategory category,
                                const QMap<QString, QString>& tags) -> QStringList;
  static auto find_first_resource_id(AudioCategory category,
                                     const QMap<QString, QString>& tags) -> QString;
};
