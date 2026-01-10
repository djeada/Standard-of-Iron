#include "audio_resource_loader.h"

#include "game/audio/audio_system.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>

void AudioResourceLoader::load_audio_resources() {
  auto &audio_sys = AudioSystem::getInstance();

  QString const base_path =
      QCoreApplication::applicationDirPath() + "/assets/audio/";
  qInfo() << "Loading audio resources from:" << base_path;

  QDir const audio_dir(base_path);
  if (!audio_dir.exists()) {
    qWarning() << "Audio assets directory does not exist:" << base_path;
    qWarning() << "Application directory:"
               << QCoreApplication::applicationDirPath();
    return;
  }

  if (audio_sys.loadSound("archer_voice",
                          (base_path + "voices/archer_voice.wav").toStdString(),
                          AudioCategory::VOICE)) {
    qInfo() << "Loaded archer voice";
  } else {
    qWarning() << "Failed to load archer voice from:"
               << (base_path + "voices/archer_voice.wav");
  }

  if (audio_sys.loadSound(
          "swordsman_voice",
          (base_path + "voices/swordsman_voice.wav").toStdString(),
          AudioCategory::VOICE)) {
    qInfo() << "Loaded swordsman voice";
  } else {
    qWarning() << "Failed to load swordsman voice from:"
               << (base_path + "voices/swordsman_voice.wav");
  }

  if (audio_sys.loadSound(
          "spearman_voice",
          (base_path + "voices/spearman_voice.wav").toStdString(),
          AudioCategory::VOICE)) {
    qInfo() << "Loaded spearman voice";
  } else {
    qWarning() << "Failed to load spearman voice from:"
               << (base_path + "voices/spearman_voice.wav");
  }

  if (audio_sys.loadMusic("music_peaceful",
                          (base_path + "music/peaceful.wav").toStdString())) {
    qInfo() << "Loaded peaceful music";
  } else {
    qWarning() << "Failed to load peaceful music from:"
               << (base_path + "music/peaceful.wav");
  }

  if (audio_sys.loadMusic("music_tense",
                          (base_path + "music/tense.wav").toStdString())) {
    qInfo() << "Loaded tense music";
  } else {
    qWarning() << "Failed to load tense music from:"
               << (base_path + "music/tense.wav");
  }

  if (audio_sys.loadMusic("music_combat",
                          (base_path + "music/combat.wav").toStdString())) {
    qInfo() << "Loaded combat music";
  } else {
    qWarning() << "Failed to load combat music from:"
               << (base_path + "music/combat.wav");
  }

  if (audio_sys.loadMusic("music_victory",
                          (base_path + "music/victory.wav").toStdString())) {
    qInfo() << "Loaded victory music";
  } else {
    qWarning() << "Failed to load victory music from:"
               << (base_path + "music/victory.wav");
  }

  if (audio_sys.loadMusic("music_defeat",
                          (base_path + "music/defeat.wav").toStdString())) {
    qInfo() << "Loaded defeat music";
  } else {
    qWarning() << "Failed to load defeat music from:"
               << (base_path + "music/defeat.wav");
  }

  qInfo() << "Audio resources loading complete";
}
