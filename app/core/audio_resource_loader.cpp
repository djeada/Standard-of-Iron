#include "audio_resource_loader.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>

#include "game/audio/audio_system.h"

void AudioResourceLoader::load_audio_resources() {
  auto& audio_sys = AudioSystem::get_instance();

  const QString app_dir = QCoreApplication::applicationDirPath();
  const QStringList candidates = {
      app_dir + "/assets/audio/",
      app_dir + "/../Resources/assets/audio/",
      app_dir + "/../../assets/audio/",
  };

  QString base_path;
  for (const QString& candidate : candidates) {
    if (QDir(candidate).exists()) {
      base_path = candidate;
      break;
    }
  }

  if (base_path.isEmpty()) {
    qWarning() << "Audio assets directory not found. Searched:";
    for (const QString& c : candidates) {
      qWarning() << " " << c;
    }
    qWarning() << "Application directory:" << app_dir;
    return;
  }

  qInfo() << "Loading audio resources from:" << base_path;

  if (audio_sys.load_sound("archer_voice",
                           (base_path + "voices/archer_voice.wav").toStdString(),
                           AudioCategory::VOICE)) {
    qInfo() << "Loaded archer voice";
  } else {
    qWarning() << "Failed to load archer voice from:"
               << (base_path + "voices/archer_voice.wav");
  }

  if (audio_sys.load_sound("swordsman_voice",
                           (base_path + "voices/swordsman_voice.wav").toStdString(),
                           AudioCategory::VOICE)) {
    qInfo() << "Loaded swordsman voice";
  } else {
    qWarning() << "Failed to load swordsman voice from:"
               << (base_path + "voices/swordsman_voice.wav");
  }

  if (audio_sys.load_sound("spearman_voice",
                           (base_path + "voices/spearman_voice.wav").toStdString(),
                           AudioCategory::VOICE)) {
    qInfo() << "Loaded spearman voice";
  } else {
    qWarning() << "Failed to load spearman voice from:"
               << (base_path + "voices/spearman_voice.wav");
  }

  if (audio_sys.load_music("music_peaceful",
                           (base_path + "music/peaceful.wav").toStdString())) {
    qInfo() << "Loaded peaceful music";
  } else {
    qWarning() << "Failed to load peaceful music from:"
               << (base_path + "music/peaceful.wav");
  }

  if (audio_sys.load_music("music_tense",
                           (base_path + "music/tense.wav").toStdString())) {
    qInfo() << "Loaded tense music";
  } else {
    qWarning() << "Failed to load tense music from:" << (base_path + "music/tense.wav");
  }

  if (audio_sys.load_music("music_combat",
                           (base_path + "music/combat.wav").toStdString())) {
    qInfo() << "Loaded combat music";
  } else {
    qWarning() << "Failed to load combat music from:"
               << (base_path + "music/combat.wav");
  }

  if (audio_sys.load_music("music_victory",
                           (base_path + "music/victory.wav").toStdString())) {
    qInfo() << "Loaded victory music";
  } else {
    qWarning() << "Failed to load victory music from:"
               << (base_path + "music/victory.wav");
  }

  if (audio_sys.load_music("music_defeat",
                           (base_path + "music/defeat.wav").toStdString())) {
    qInfo() << "Loaded defeat music";
  } else {
    qWarning() << "Failed to load defeat music from:"
               << (base_path + "music/defeat.wav");
  }

  qInfo() << "Audio resources loading complete";
}
