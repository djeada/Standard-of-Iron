#pragma once

#include <QObject>

#include <atomic>
#include <memory>
#include <string>

#include "audio_mastering.h"

class MiniaudioBackend;

class Sound : public QObject {
  Q_OBJECT
public:
  static constexpr float DEFAULT_VOLUME = 1.0F;

  Sound(const std::string& resource_id,
        const std::string& file_path,
        Game::Audio::Mastering::Material material,
        MiniaudioBackend* backend = nullptr);
  ~Sound() override;

  [[nodiscard]] auto track_id() const -> const QString& { return m_track_id; }
  [[nodiscard]] auto is_registered() const -> bool;
  [[nodiscard]] auto is_playing() const -> bool;
  void play(float volume = DEFAULT_VOLUME, bool loop = false, float pan = 0.0F);
  void stop();
  void set_volume(float volume);
  void set_playing_volume(float volume, int fade_ms);

  void set_backend(MiniaudioBackend* backend);

private:
  std::string m_file_path;
  Game::Audio::Mastering::Material m_material;
  QString m_track_id;
  MiniaudioBackend* m_backend;
  std::atomic<bool> m_registered;
  std::atomic<float> m_volume;
};
